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
#  include <cstdio>

#  include "DX8Graphics.hpp"
#  include "Globals.hpp"
#  include "Logdatei.hpp"
#  include "Main.hpp"

TouchControlsClass TouchControls;

namespace {

// Colores ARGB. Alfa bajo para que el overlay no tape el juego.
constexpr std::uint32_t COLOR_BASE = 0x30FFFFFF;
constexpr std::uint32_t COLOR_PRESSED = 0x90FFD24A;
constexpr std::uint32_t COLOR_STICK_BASE = 0x28FFFFFF;
constexpr std::uint32_t COLOR_STICK_KNOB = 0x50FFFFFF;
constexpr std::uint32_t COLOR_EDIT = 0x60FF5030;
constexpr std::uint32_t COLOR_DEBUGBTN = 0x4020FF50;

/*
 * El motor mapea por defecto estas teclas a las acciones del jugador
 * (vease CreateDefaultControlsConfig en Gameplay.cpp):
 *
 *   IZQUIERDA / DERECHA    mover
 *   ARRIBA                 apuntar arriba
 *   ABAJO                  agacharse
 *   DIK_LALT               saltar
 *   DIK_LCTRL              disparar (mantener)
 *   DIK_LSHIFT             blitz
 *   DIK_SPACE              powerline
 *   DIK_RCTRL              granada
 *   DIK_RSHIFT             smart bomb
 *   DIK_RETURN             cambiar de arma
 *   DIK_ESCAPE             pausa / menu
 */
struct ActionDef {
    float x, y;    // centro, normalizado dentro del area de juego
    float r;       // radio, normalizado respecto a la altura de la pantalla
    SDL_Keycode key;
    bool toggle;
    const char *label;
};

/*
 * Tres tamanos de boton:
 *   - "grande" para las dos acciones que se usan constantemente (saltar y disparar),
 *   - "medio" para las tacticas,
 *   - "pequeno" para las de menu y el panel de debug, que deben estorbar poco.
 *
 * El pulgar derecho llega comodamente a la esquina inferior derecha; los botones se colocan
 * en arco alrededor de esa esquina, no en una fila, para que se puedan pulsar dos a la vez
 * sin taparse (disparar + saltar es la combinacion mas habitual).
 */
const ActionDef ACTIONS[] = {
    //     x       y       r       tecla          toggle  etiqueta
    {0.905f, 0.760f, 0.105f, SDLK_LALT, false, "A"},       // ACT_JUMP    saltar
    {0.780f, 0.860f, 0.125f, SDLK_LCTRL, false, "B"},      // ACT_FIRE    disparar
    {0.940f, 0.900f, 0.075f, SDLK_LSHIFT, false, "C"},     // ACT_BLITZ   turbo
    {0.660f, 0.880f, 0.075f, SDLK_SPACE, false, "P"},      // ACT_POWERLINE
    {0.830f, 0.660f, 0.070f, SDLK_RCTRL, false, "G"},      // ACT_GRENADE
    {0.700f, 0.700f, 0.070f, SDLK_RSHIFT, false, "S"},     // ACT_SMARTBOMB
    {0.640f, 0.790f, 0.070f, SDLK_RETURN, false, "W"},     // ACT_WEAPON
    {0.045f, 0.055f, 0.055f, SDLK_ESCAPE, false, "II"},    // ACT_ESCAPE  pausa
    {0.945f, 0.055f, 0.055f, 0, true, "DBG"},              // ACT_DEBUG   panel de trucos
};

constexpr int ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);

/// Radio del joystick, como fraccion de la altura de la pantalla.
constexpr float STICK_RADIUS_FRAC = 0.135f;
/// Zona muerta del joystick (fraccion del radio).
constexpr float STICK_DEADZONE = 0.30f;

bool g_initialised = false;

}  // namespace

void TouchControlsClass::Init() {
    available = SDL_GetNumTouchDevices() > 0;

    for (int i = 0; i < ACTION_COUNT; i++) {
        actions[i].x = ACTIONS[i].x;
        actions[i].y = ACTIONS[i].y;
        actions[i].r = ACTIONS[i].r;
        actions[i].scancode = ACTIONS[i].toggle ? 0 : static_cast<int>(SDL_GetScancodeFromKey(ACTIONS[i].key));
        actions[i].is_toggle = ACTIONS[i].toggle;
        actions[i].label = ACTIONS[i].label;
        actions[i].pressed = false;
        actions[i].finger = -1;
    }

    active = available && enabled;
    g_initialised = true;
    editing = false;
    stick_active = false;
    stick_finger = -1;
    debug_requested = false;

    Protokoll << "Mando virtual: " << (available ? "pantalla tactil detectada" : "sin pantalla tactil")
              << ", " << (active ? "activo" : "desactivado") << ", " << ACTION_COUNT << " botones"
              << std::endl;
}

void TouchControlsClass::Shutdown() {
    ClearKeys();
    active = false;
    g_initialised = false;
}

void TouchControlsClass::SetEnabled(bool value) {
    enabled = value;
    active = available && enabled;
    if (!active)
        ClearKeys();
}

void TouchControlsClass::ToggleEditing() {
    editing = !editing;
    ClearKeys();
    Protokoll << "Mando virtual: modo edicion " << (editing ? "activado" : "desactivado")
              << " (mueve los controles con el dedo)" << std::endl;
}

void TouchControlsClass::ResetLayout() {
    for (int i = 0; i < ACTION_COUNT; i++) {
        actions[i].x = ACTIONS[i].x;
        actions[i].y = ACTIONS[i].y;
        actions[i].r = ACTIONS[i].r;
    }
    stick_default_x = 0.17f;
    stick_default_y = 0.74f;
    Protokoll << "Mando virtual: disposicion restablecida" << std::endl;
}

void TouchControlsClass::ClearKeys() {
    if (TastaturPuffer != nullptr) {
        for (auto sc : pressed_scancodes) {
            if (sc > 0 && sc < MAX_KEYS)
                const_cast<Uint8 *>(TastaturPuffer)[sc] = 0;
        }
    }
    pressed_scancodes.clear();

    for (int i = 0; i < ACTION_COUNT; i++) {
        actions[i].pressed = false;
        actions[i].finger = -1;
    }

    if (TastaturPuffer != nullptr) {
        for (const int sc : {SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN}) {
            const_cast<Uint8 *>(TastaturPuffer)[sc] = 0;
        }
    }
    stick_active = false;
    stick_finger = -1;
    stick_x = stick_y = 0.0f;
}

void TouchControlsClass::ComputePixels(const SDL_Rect &screen_rect, float &scale) {
    // Los botones se miden sobre la ALTURA de la pantalla (no sobre el area 4:3 del juego)
    // para que en pantallas 20:9 no queden diminutos.
    scale = static_cast<float>(window_h);
    if (screen_rect.h <= 0)
        return;
}

void TouchControlsClass::UpdateJoystick() {
    if (TastaturPuffer == nullptr)
        return;

    const float len = std::sqrt(stick_x * stick_x + stick_y * stick_y);
    const float dead = stick_radius * STICK_DEADZONE;

    const bool left = len > dead && stick_x < 0.0f;
    const bool right = len > dead && stick_x > 0.0f;
    const bool up = len > dead && stick_y < 0.0f;
    const bool down = len > dead && stick_y > 0.0f;

    auto set = [](SDL_Scancode sc, bool on) {
        if (!on)
            return;
        const_cast<Uint8 *>(TastaturPuffer)[sc] = 1;
    };
    set(SDL_SCANCODE_LEFT, left);
    set(SDL_SCANCODE_RIGHT, right);
    set(SDL_SCANCODE_UP, up);
    set(SDL_SCANCODE_DOWN, down);
}

void TouchControlsClass::UpdateEditing(const SDL_TouchFingerEvent &finger, const SDL_Rect &screen_rect,
                                       int win_w, int win_h) {
    // Coordenadas del dedo en pixeles de ventana.
    const float px = finger.x * static_cast<float>(win_w);
    const float py = finger.y * static_cast<float>(win_h);

    if (finger.type == SDL_FINGERDOWN) {
        // Buscar el control mas cercano al toque, con un margen comodo.
        float best = 1e9f;
        int best_action = -1;
        const float escala = static_cast<float>(win_h);

        for (int i = 0; i < ACTION_COUNT; i++) {
            const float cx = screen_rect.x + actions[i].x * screen_rect.w;
            const float cy = screen_rect.y + actions[i].y * screen_rect.h;
            const float dist = std::hypot(px - cx, py - cy);
            if (dist < actions[i].r * escala * 2.0f && dist < best) {
                best = dist;
                best_action = i;
            }
        }

        const float stick_cx = screen_rect.x + stick_default_x * screen_rect.w;
        const float stick_cy = screen_rect.y + stick_default_y * screen_rect.h;
        const float stick_dist = std::hypot(px - stick_cx, py - stick_cy);

        if (stick_dist < STICK_RADIUS_FRAC * escala * 2.0f && stick_dist < best) {
            drag_stick = true;
            drag_action = -1;
            drag_off_x = stick_default_x * screen_rect.w - (px - screen_rect.x);
            drag_off_y = stick_default_y * screen_rect.h - (py - screen_rect.y);
        } else if (best_action >= 0) {
            drag_action = best_action;
            drag_stick = false;
            drag_off_x = actions[best_action].x * screen_rect.w - (px - screen_rect.x);
            drag_off_y = actions[best_action].y * screen_rect.h - (py - screen_rect.y);
        }
    } else if (finger.type == SDL_FINGERMOTION) {
        if (drag_stick) {
            stick_default_x = std::clamp((px - screen_rect.x + drag_off_x) / screen_rect.w, 0.05f, 0.95f);
            stick_default_y = std::clamp((py - screen_rect.y + drag_off_y) / screen_rect.h, 0.05f, 0.95f);
        } else if (drag_action >= 0) {
            actions[drag_action].x =
                std::clamp((px - screen_rect.x + drag_off_x) / screen_rect.w, 0.03f, 0.97f);
            actions[drag_action].y =
                std::clamp((py - screen_rect.y + drag_off_y) / screen_rect.h, 0.03f, 0.97f);
        }
    } else if (finger.type == SDL_FINGERUP) {
        drag_action = -1;
        drag_stick = false;
    }
}

void TouchControlsClass::ReleaseAll() {
    if (TastaturPuffer != nullptr) {
        for (auto sc : pressed_scancodes) {
            if (sc > 0 && sc < MAX_KEYS)
                const_cast<Uint8 *>(TastaturPuffer)[sc] = 0;
        }

        // El joystick marca las cuatro direcciones; se sueltan explicitamente para no
        // depender de que pressed_scancodes este al dia.
        const int direcciones[4] = {SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN};
        for (const int sc : direcciones) {
            if (sc > 0 && sc < MAX_KEYS)
                const_cast<Uint8 *>(TastaturPuffer)[sc] = 0;
        }
    }

    pressed_scancodes.clear();
    for (int i = 0; i < ACTION_COUNT; i++) {
        actions[i].pressed = false;
        actions[i].finger = -1;
    }

    stick_active = false;
    stick_finger = -1;
    stick_x = stick_y = 0.0f;
}

void TouchControlsClass::Update(const SDL_Rect &screen_rect, int win_w, int win_h) {
    if (!g_initialised || !active || TastaturPuffer == nullptr)
        return;

    window_w = win_w;
    window_h = win_h;

    if (screen_rect.w <= 0 || screen_rect.h <= 0 || win_w <= 0 || win_h <= 0)
        return;

    // 1) Sueltan las teclas del frame anterior; el estado se reconstruye entero cada frame.
    for (auto sc : pressed_scancodes) {
        if (sc > 0 && sc < MAX_KEYS)
            const_cast<Uint8 *>(TastaturPuffer)[sc] = 0;
    }
    pressed_scancodes.clear();
    for (int i = 0; i < ACTION_COUNT; i++)
        actions[i].pressed = false;

    stick_radius = STICK_RADIUS_FRAC * static_cast<float>(win_h);
    stick_x = stick_y = 0.0f;

    // 2) Un solo dedo manda en el joystick: el que empezo en su zona. Los demas pueden
    //    pulsar botones a la vez (multitactil real).
    const SDL_TouchID device = SDL_GetTouchDevice(0);
    const int fingers_count = device == 0 ? 0 : SDL_GetNumTouchFingers(device);

    for (int i = 0; i < fingers_count; i++) {
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

        // 2a) Botones.
        bool used = false;
        for (int b = 0; b < ACTION_COUNT; b++) {
            if (actions[b].pressed)
                continue;

            const float rx = actions[b].r * static_cast<float>(win_h) / static_cast<float>(screen_rect.w);
            const float ry = actions[b].r;

            const float dx = (nx - actions[b].x) / (rx > 0.0f ? rx : 1.0f);
            const float dy = (ny - actions[b].y) / (ry > 0.0f ? ry : 1.0f);
            if (dx * dx + dy * dy > 1.0f)
                continue;

            actions[b].pressed = true;
            used = true;

            if (actions[b].is_toggle) {
                // El boton de debug solo avisa del toque; el panel lo gestiona DebugPanel.
                if (b == ACT_DEBUG)
                    debug_requested = true;
            } else if (actions[b].scancode > 0) {
                const_cast<Uint8 *>(TastaturPuffer)[actions[b].scancode] = 1;
                pressed_scancodes.push_back(actions[b].scancode);
            }
            break;
        }
        if (used)
            continue;

        /*
         * 2b) Joystick flotante.
         *
         * No esta atado a una posicion fija: cualquier toque en la zona de movimiento lo
         * "crea" ahi mismo (asi lo hacen los juegos de accion en movil, y es mucho mas
         * comodo que buscar un circulo dibujado). Si hay un joystick dibujado en su sitio
         * por defecto y se toca ahi, tambien funciona.
         */
        const bool in_zone = nx < stick_zone_w && ny < stick_zone_h;
        const bool on_drawn_stick =
            std::hypot((nx - stick_default_x) * screen_rect.w, (ny - stick_default_y) * screen_rect.h) <
            stick_radius * 1.2f;

        if (in_zone || on_drawn_stick) {
            if (!stick_active) {
                stick_active = true;
                stick_finger = i;
                // Si el toque fue cerca del joystick dibujado, se centra en el; si no, nace
                // bajo el dedo.
                if (on_drawn_stick) {
                    stick_origin_x = screen_rect.x + stick_default_x * screen_rect.w;
                    stick_origin_y = screen_rect.y + stick_default_y * screen_rect.h;
                } else {
                    stick_origin_x = px;
                    stick_origin_y = py;
                }
            }
            if (stick_finger == i) {
                float dx = px - stick_origin_x;
                float dy = py - stick_origin_y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > stick_radius) {
                    dx *= stick_radius / len;
                    dy *= stick_radius / len;
                }
                stick_x = dx;
                stick_y = dy;
            }
        }
    }

    if (fingers_count == 0) {
        stick_active = false;
        stick_finger = -1;
    }

    // 3) Volcar el joystick y las teclas al buffer que consulta el juego.
    if (stick_active)
        UpdateJoystick();

    for (auto sc : pressed_scancodes) {
        if (sc > 0 && sc < MAX_KEYS)
            const_cast<Uint8 *>(TastaturPuffer)[sc] = 1;
    }
}

bool TouchControlsClass::HitDebugButton(float px, float py, const SDL_Rect &screen_rect) const {
    if (!g_initialised || !active || screen_rect.w <= 0 || screen_rect.h <= 0)
        return false;

    const ActionButton &b = actions[ACT_DEBUG];
    const float cx = screen_rect.x + b.x * screen_rect.w;
    const float cy = screen_rect.y + b.y * screen_rect.h;
    const float r = b.r * static_cast<float>(screen_rect.h) * 1.2f;

    return std::hypot(px - cx, py - cy) <= r;
}

void TouchControlsClass::DrawQuad(float x, float y, float w, float h, std::uint32_t argb) {
    const glm::mat4x4 &window_matrix = DirectGraphics.GetWindowProjection();
    VERTEX2D quad[4] = {
        VERTEX2D{x, y, HCOLOR(argb), 0.0f, 0.0f},
        VERTEX2D{x, y + h, HCOLOR(argb), 0.0f, 0.0f},
        VERTEX2D{x + w, y, HCOLOR(argb), 0.0f, 0.0f},
        VERTEX2D{x + w, y + h, HCOLOR(argb), 0.0f, 0.0f},
    };
    DirectGraphics.DrawColorVertices(GL_TRIANGLE_STRIP, 4, quad, window_matrix);
}

void TouchControlsClass::DrawDisc(float cx, float cy, float rx, float ry, std::uint32_t argb) {
    const glm::mat4x4 &window_matrix = DirectGraphics.GetWindowProjection();
    constexpr int SEGMENTS = 20;
    VERTEX2D fan[SEGMENTS + 2];
    fan[0].x = cx;
    fan[0].y = cy;
    fan[0].color = HCOLOR(argb);
    fan[0].tu = 0.0f;
    fan[0].tv = 0.0f;
    for (int s = 0; s <= SEGMENTS; s++) {
        const float a = (static_cast<float>(s) / SEGMENTS) * TWO_PI;
        fan[s + 1].x = cx + cosf(a) * rx;
        fan[s + 1].y = cy + sinf(a) * ry;
        fan[s + 1].color = HCOLOR(argb);
        fan[s + 1].tu = 0.0f;
        fan[s + 1].tv = 0.0f;
    }
    DirectGraphics.DrawColorVertices(GL_TRIANGLE_FAN, SEGMENTS + 2, fan, window_matrix);
}

void TouchControlsClass::DrawGeometry(const SDL_Rect &screen_rect) {
    if (!g_initialised || !active)
        return;
    if (screen_rect.w <= 0 || screen_rect.h <= 0)
        return;

    const float escala = static_cast<float>(screen_rect.h);

    // --- Joystick -----------------------------------------------------------------------
    {
        const float cx = screen_rect.x + stick_default_x * screen_rect.w;
        const float cy = screen_rect.y + stick_default_y * screen_rect.h;
        const float r = STICK_RADIUS_FRAC * escala;

        // Si esta en uso, el joystick se dibuja donde nacio.
        const float bx = stick_active ? stick_origin_x : cx;
        const float by = stick_active ? stick_origin_y : cy;

        DrawDisc(bx, by, r, r, COLOR_STICK_BASE);
        DrawDisc(bx, by, r * 0.42f, r * 0.42f, COLOR_STICK_KNOB);

        if (stick_active) {
            // Pomos desplazado + eje X para que se vea la direccion.
            const std::uint32_t c = 0x70FFD24A;
            DrawDisc(bx + stick_x, by + stick_y, r * 0.34f, r * 0.34f, c);
        } else {
            // Flechas de referencia: izquierda / derecha.
            DrawQuad(bx - r * 1.15f, by - r * 0.12f, r * 0.30f, r * 0.24f, 0x28FFFFFF);
            DrawQuad(bx + r * 0.85f, by - r * 0.12f, r * 0.30f, r * 0.24f, 0x28FFFFFF);
        }
    }

    // --- Botones ------------------------------------------------------------------------
    for (int i = 0; i < ACTION_COUNT; i++) {
        const float cx = screen_rect.x + actions[i].x * screen_rect.w;
        const float cy = screen_rect.y + actions[i].y * screen_rect.h;
        const float r = actions[i].r * escala;

        std::uint32_t color;
        if (actions[i].is_toggle) {
            color = COLOR_DEBUGBTN;
        } else {
            color = actions[i].pressed ? COLOR_PRESSED : COLOR_BASE;
        }

        // Aro exterior mas transparente: da relieve y marca mejor el area pulsable.
        DrawDisc(cx, cy, r * 1.10f, r * 1.10f, (color & 0x00FFFFFF) | 0x18000000);
        DrawDisc(cx, cy, r, r, color);

        if (editing)
            DrawDisc(cx, cy, r * 0.35f, r * 0.35f, COLOR_EDIT);
    }
}

#endif /* ANDROID */
