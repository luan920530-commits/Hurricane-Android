/**
 * Implementacion del mando virtual de Android. Vease TouchControls.hpp.
 *
 * Nota sobre el estado de las teclas: el motor guarda las pulsaciones en la tabla que
 * devuelve SDL_GetKeyboardState(), que es de solo lectura para nosotros. Por eso en Android
 * DirectInputClass mantiene su propio buffer (DirectInputClass::TouchKeyboard) y
 * TastaturPuffer apunta a el; aqui solo se marca/desmarca ese buffer.
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#include "TouchControls.hpp"

#if defined(ANDROID)

#  include <algorithm>
#  include <cmath>

#  include "DX8Graphics.hpp"
#  include "Globals.hpp"
#  include "Logdatei.hpp"
#  include "Main.hpp"

TouchControlsClass TouchControls;

namespace {

// Colores ARGB con alfa bajo, para que no tapen el juego.
constexpr uint32_t COLOR_BASE = 0x38FFFFFF;
constexpr uint32_t COLOR_PRESSED = 0x80FFD24A;
constexpr uint32_t COLOR_DPAD = 0x30FFFFFF;

/*
 * El motor mapea por defecto estas teclas a las acciones del jugador
 * (vease CreateDefaultControlsConfig en Gameplay.cpp):
 *
 *   DIK_LEFT / DIK_RIGHT   mover
 *   DIK_UP                 apuntar arriba
 *   DIK_DOWN               agacharse / apuntar abajo
 *   DIK_LALT               saltar
 *   DIK_LCTRL              disparar
 *   DIK_LSHIFT             blitz
 *   DIK_SPACE              powerline
 *   DIK_RETURN             cambiar de arma
 *   DIK_RCTRL              granada
 *   DIK_RSHIFT             smart bomb
 *   DIK_ESCAPE             pausa / volver
 */
struct ButtonDef {
    float x, y, w, h;  // posicion normalizada (0..1) dentro del area de juego
    SDL_Keycode key;
    bool round;
};

const ButtonDef BUTTONS[] = {
    // D-pad a la izquierda: izquierda / abajo / derecha.
    {0.015f, 0.735f, 0.080f, 0.115f, SDLK_LEFT, true},
    {0.100f, 0.845f, 0.080f, 0.115f, SDLK_DOWN, true},
    {0.185f, 0.735f, 0.080f, 0.115f, SDLK_RIGHT, true},
    // Apuntar arriba: poco usado, pero el juego original lo tiene.
    {0.100f, 0.620f, 0.070f, 0.100f, SDLK_UP, true},

    // Acciones a la derecha.
    {0.895f, 0.720f, 0.075f, 0.110f, SDLK_LALT, false},    // A - saltar
    {0.795f, 0.845f, 0.075f, 0.110f, SDLK_LCTRL, false},   // B - disparar
    {0.920f, 0.865f, 0.070f, 0.105f, SDLK_LSHIFT, false},  // C - blitz

    // Utilidades alrededor de los botones de accion.
    {0.795f, 0.720f, 0.070f, 0.105f, SDLK_SPACE, false},   // powerline
    {0.700f, 0.720f, 0.070f, 0.105f, SDLK_RETURN, false},  // cambiar de arma
    {0.895f, 0.055f, 0.075f, 0.110f, SDLK_RCTRL, false},   // granada
    {0.795f, 0.055f, 0.075f, 0.110f, SDLK_RSHIFT, false},  // smart bomb

    // Menu.
    {0.020f, 0.055f, 0.080f, 0.110f, SDLK_ESCAPE, false},  // pausa / atras
};

constexpr int BUTTON_COUNT = sizeof(BUTTONS) / sizeof(BUTTONS[0]);

bool g_initialised = false;

}  // namespace

void TouchControlsClass::Init() {
    available = SDL_GetNumTouchDevices() > 0;

    buttons.clear();
    buttons.reserve(BUTTON_COUNT);
    for (int i = 0; i < BUTTON_COUNT; i++) {
        Button b;
        b.rect = {BUTTONS[i].x, BUTTONS[i].y, BUTTONS[i].w, BUTTONS[i].h};
        b.scancode = static_cast<int>(SDL_GetScancodeFromKey(BUTTONS[i].key));
        b.pressed = false;
        b.is_circle = BUTTONS[i].round;
        buttons.push_back(b);
    }

    active = available && enabled;
    g_initialised = true;

    Protokoll << "Mando virtual: " << (available ? "pantalla tactil detectada" : "sin pantalla tactil")
              << ", " << (active ? "activo" : "desactivado") << std::endl;
}

void TouchControlsClass::Shutdown() {
    ClearKeys();
    buttons.clear();
    active = false;
    g_initialised = false;
}

void TouchControlsClass::SetEnabled(bool value) {
    enabled = value;
    active = available && enabled;
    if (!active)
        ClearKeys();
}

void TouchControlsClass::ClearKeys() {
    if (TastaturPuffer != nullptr) {
        for (auto sc : pressed_scancodes) {
            if (sc >= 0 && sc < MAX_KEYS)
                const_cast<Uint8 *>(TastaturPuffer)[sc] = 0;
        }
    }
    pressed_scancodes.clear();
    for (auto &b : buttons)
        b.pressed = false;
}

void TouchControlsClass::Update(const SDL_Rect &screen_rect, int win_w, int win_h) {
    if (!g_initialised || !active || TastaturPuffer == nullptr)
        return;

    window_w = win_w;
    window_h = win_h;

    // 1) Soltar lo que se marco en el frame anterior.
    for (auto sc : pressed_scancodes) {
        if (sc >= 0 && sc < MAX_KEYS)
            const_cast<Uint8 *>(TastaturPuffer)[sc] = 0;
    }
    pressed_scancodes.clear();
    for (auto &b : buttons)
        b.pressed = false;

    if (screen_rect.w <= 0 || screen_rect.h <= 0 || win_w <= 0 || win_h <= 0)
        return;

    // 2) Repartir los dedos que estan sobre el area de juego entre los botones.
    const SDL_TouchID device = SDL_GetTouchDevice(0);
    const int fingers = device == 0 ? 0 : SDL_GetNumTouchFingers(device);

    for (int i = 0; i < fingers; i++) {
        SDL_Finger *finger = SDL_GetTouchFinger(device, i);
        if (finger == nullptr)
            continue;

        const float px = finger->x * static_cast<float>(win_w);
        const float py = finger->y * static_cast<float>(win_h);

        if (px < screen_rect.x || py < screen_rect.y || px > screen_rect.x + screen_rect.w ||
            py > screen_rect.y + screen_rect.h)
            continue;

        const float nx = (px - screen_rect.x) / static_cast<float>(screen_rect.w);
        const float ny = (py - screen_rect.y) / static_cast<float>(screen_rect.h);

        for (auto &b : buttons) {
            if (b.pressed)
                continue;  // ese boton ya lo pulsa otro dedo
            if (nx < b.rect.x || nx > b.rect.x + b.rect.w || ny < b.rect.y || ny > b.rect.y + b.rect.h)
                continue;
            b.pressed = true;
            pressed_scancodes.push_back(b.scancode);
            break;
        }
    }

    // 3) Volcar el estado al buffer que consulta el juego.
    for (auto sc : pressed_scancodes) {
        if (sc >= 0 && sc < MAX_KEYS)
            const_cast<Uint8 *>(TastaturPuffer)[sc] = 1;
    }
}

void TouchControlsClass::DrawGeometry(const SDL_Rect &screen_rect) {
    if (!g_initialised || !active)
        return;
    if (screen_rect.w <= 0 || screen_rect.h <= 0)
        return;

    const glm::mat4x4 &window_matrix = DirectGraphics.GetWindowProjection();

    // Fondo translucido del D-pad, para que se vea donde esta la zona de movimiento.
    {
        const float cx = screen_rect.x + 0.100f * screen_rect.w;
        const float cy = screen_rect.y + 0.795f * screen_rect.h;
        const float radius = 0.115f * screen_rect.w;

        constexpr int SEGMENTS = 24;
        VERTEX2D fan[SEGMENTS + 2];
        fan[0].x = cx;
        fan[0].y = cy;
        fan[0].color = HCOLOR(COLOR_DPAD);
        fan[0].tu = 0.0f;
        fan[0].tv = 0.0f;
        for (int s = 0; s <= SEGMENTS; s++) {
            const float a = (static_cast<float>(s) / SEGMENTS) * TWO_PI;
            fan[s + 1].x = cx + cosf(a) * radius;
            fan[s + 1].y = cy + sinf(a) * radius;
            fan[s + 1].color = HCOLOR(COLOR_DPAD);
            fan[s + 1].tu = 0.0f;
            fan[s + 1].tv = 0.0f;
        }
        DirectGraphics.DrawColorVertices(GL_TRIANGLE_FAN, SEGMENTS + 2, fan, window_matrix);
    }

    for (const auto &b : buttons) {
        const float x0 = screen_rect.x + b.rect.x * screen_rect.w;
        const float y0 = screen_rect.y + b.rect.y * screen_rect.h;
        const float x1 = x0 + b.rect.w * screen_rect.w;
        const float y1 = y0 + b.rect.h * screen_rect.h;
        const uint32_t color = b.pressed ? COLOR_PRESSED : COLOR_BASE;

        if (b.is_circle) {
            const float cx = (x0 + x1) * 0.5f;
            const float cy = (y0 + y1) * 0.5f;
            const float rx = (x1 - x0) * 0.5f;
            const float ry = (y1 - y0) * 0.5f;

            constexpr int SEGMENTS = 16;
            VERTEX2D fan[SEGMENTS + 2];
            fan[0].x = cx;
            fan[0].y = cy;
            fan[0].color = HCOLOR(color);
            fan[0].tu = 0.0f;
            fan[0].tv = 0.0f;
            for (int s = 0; s <= SEGMENTS; s++) {
                const float a = (static_cast<float>(s) / SEGMENTS) * TWO_PI;
                fan[s + 1].x = cx + cosf(a) * rx;
                fan[s + 1].y = cy + sinf(a) * ry;
                fan[s + 1].color = HCOLOR(color);
                fan[s + 1].tu = 0.0f;
                fan[s + 1].tv = 0.0f;
            }
            DirectGraphics.DrawColorVertices(GL_TRIANGLE_FAN, SEGMENTS + 2, fan, window_matrix);
        } else {
            // Marco exterior (mas transparente) y relleno, para que el boton tenga relieve.
            constexpr float BORDER = 2.0f;
            VERTEX2D border[4] = {
                VERTEX2D{x0 - BORDER, y0 - BORDER, HCOLOR(COLOR_BASE), 0.0f, 0.0f},
                VERTEX2D{x0 - BORDER, y1 + BORDER, HCOLOR(COLOR_BASE), 0.0f, 0.0f},
                VERTEX2D{x1 + BORDER, y0 - BORDER, HCOLOR(COLOR_BASE), 0.0f, 0.0f},
                VERTEX2D{x1 + BORDER, y1 + BORDER, HCOLOR(COLOR_BASE), 0.0f, 0.0f},
            };
            DirectGraphics.DrawColorVertices(GL_TRIANGLE_STRIP, 4, border, window_matrix);

            VERTEX2D quad[4] = {
                VERTEX2D{x0, y0, HCOLOR(color), 0.0f, 0.0f},
                VERTEX2D{x0, y1, HCOLOR(color), 0.0f, 0.0f},
                VERTEX2D{x1, y0, HCOLOR(color), 0.0f, 0.0f},
                VERTEX2D{x1, y1, HCOLOR(color), 0.0f, 0.0f},
            };
            DirectGraphics.DrawColorVertices(GL_TRIANGLE_STRIP, 4, quad, window_matrix);
        }
    }
}

#endif /* ANDROID */
