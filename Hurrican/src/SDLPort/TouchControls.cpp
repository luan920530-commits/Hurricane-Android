/**
 * Implementacion del mando virtual de Android. Vease TouchControls.hpp.
 *
 * Las acciones se entregan directamente al jugador. En menus se agregan al buffer de teclado
 * propio de DirectInput, sin escribir sobre el estado de teclado de SDL.
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
#  include "Gameplay.hpp"
#  include "Player.hpp"
#  include "SDLPort/DebugPanel.hpp"

TouchControlsClass TouchControls;

namespace {

// Colores ARGB. Alfa bajo para que el overlay no tape el juego.
constexpr std::uint32_t COLOR_BASE = 0x30FFFFFF;
constexpr std::uint32_t COLOR_PRESSED = 0x90FFD24A;
constexpr std::uint32_t COLOR_STICK_BASE = 0x28FFFFFF;
constexpr std::uint32_t COLOR_STICK_KNOB = 0x50FFFFFF;
constexpr std::uint32_t COLOR_EDIT = 0x60FF5030;
constexpr std::uint32_t COLOR_DEBUGBTN = 0x4020FF50;

struct ActionDef {
    float x, y;    // centro, normalizado dentro del area de juego
    float r;       // radio, normalizado respecto a la altura de la pantalla
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
    // Centros en el area de juego; radios proporcionales a su altura.
    {0.90f, 0.80f, 0.090f,   false, "SALTO"},
    {0.75f, 0.86f, 0.100f,  false, "FUEGO"},
    {0.90f, 0.58f, 0.065f, false, "BLITZ"},
    {0.79f, 0.58f, 0.065f,  false, "RAYO"},
    {0.68f, 0.58f, 0.065f,  false, "GRANADA"},
    {0.57f, 0.58f, 0.065f, false, "BOMBA"},
    {0.57f, 0.81f, 0.070f, false, "ARMA"},
    {0.05f, 0.07f, 0.055f, false, "PAUSA"},
    {0.95f, 0.07f, 0.055f, true, "DBG"},
};

constexpr int ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);

/// Radio del joystick, como fraccion de la altura de la pantalla.
constexpr float STICK_RADIUS_FRAC = 0.135f;

bool g_initialised = false;

}  // namespace

void TouchControlsClass::Init() {
    // En Android algunos dispositivos solo se anuncian al recibir el primer toque.
    available = true;

    for (int i = 0; i < ACTION_COUNT; i++) {
        actions[i].x = ACTIONS[i].x;
        actions[i].y = ACTIONS[i].y;
        actions[i].r = ACTIONS[i].r;
        actions[i].is_toggle = ACTIONS[i].toggle;
        actions[i].label = ACTIONS[i].label;
        actions[i].pressed = false;
    }

    active = available && enabled;
    wait_for_release = false;
    contacts.clear();
    g_initialised = true;
    editing = false;
    stick_active = false;
    stick.Reset();

    Protokoll << "Mando virtual: " << (available ? "disponible en Android" : "no disponible")
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
    for (int i = 0; i < ACTION_COUNT; ++i) {
        actions[i].pressed = false;
        pending[i] = false;
    }
    for (bool &direction : directions) direction = false;
    stick.Reset();
    stick_active = false;
    stick_x = stick_y = 0.0f;
    dragging = false;
}

void TouchControlsClass::CancelTouches() { ClearKeys(); wait_for_release = !contacts.empty(); }
void TouchControlsClass::ResetTouches() {
    ClearKeys(); contacts.clear(); wait_for_release = false;
}

bool TouchControlsClass::PlayerAction(int action) const {
    if (!active || editing || DebugPanel.IsOpen()) return false;
    switch (action) {
        case AKTION_LINKS: return directions[0];
        case AKTION_RECHTS: return directions[1];
        case AKTION_OBEN: return directions[2];
        case AKTION_UNTEN: case AKTION_DUCKEN: return directions[3];
        case AKTION_JUMP: return actions[ACT_JUMP].pressed;
        case AKTION_SHOOT: return actions[ACT_FIRE].pressed;
        case AKTION_BLITZ: return actions[ACT_BLITZ].pressed;
        case AKTION_POWERLINE: return actions[ACT_POWERLINE].pressed;
        case AKTION_GRANATE: return actions[ACT_GRENADE].pressed;
        case AKTION_SMARTBOMB: return actions[ACT_SMARTBOMB].pressed;
        case AKTION_WAFFEN_CYCLE: return actions[ACT_WEAPON].pressed;
        default: return false;
    }
}

void TouchControlsClass::UpdateEditing(const SDL_TouchFingerEvent &finger, const SDL_Rect &screen_rect,
                                       int win_w, int win_h) {
    // Coordenadas del dedo en pixeles de ventana.
    const float px = finger.x * static_cast<float>(win_w);
    const float py = finger.y * static_cast<float>(win_h);

    if (finger.type == SDL_FINGERDOWN) {
        if (dragging) return;
        dragging = true;
        drag_finger = finger.fingerId;
        drag_device = finger.touchId;
        // Buscar el control mas cercano al toque, con un margen comodo.
        float best = 1e9f;
        int best_action = -1;
        const float escala = static_cast<float>(screen_rect.h);

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
    } else if (finger.type == SDL_FINGERMOTION && dragging && finger.fingerId == drag_finger && finger.touchId == drag_device) {
        if (drag_stick) {
            stick_default_x = std::clamp((px - screen_rect.x + drag_off_x) / screen_rect.w, 0.05f, 0.95f);
            stick_default_y = std::clamp((py - screen_rect.y + drag_off_y) / screen_rect.h, 0.05f, 0.95f);
        } else if (drag_action >= 0) {
            actions[drag_action].x =
                std::clamp((px - screen_rect.x + drag_off_x) / screen_rect.w, 0.03f, 0.97f);
            actions[drag_action].y =
                std::clamp((py - screen_rect.y + drag_off_y) / screen_rect.h, 0.03f, 0.97f);
        }
    } else if (finger.type == SDL_FINGERUP && dragging && finger.fingerId == drag_finger && finger.touchId == drag_device) {
        dragging = false;
        drag_action = -1;
        drag_stick = false;
    }
}

void TouchControlsClass::HandleFingerEvent(const SDL_TouchFingerEvent &finger, const SDL_Rect &area,
                                           int win_w, int win_h) {
    if (!g_initialised || !active || area.w <= 0 || area.h <= 0 || win_w <= 0 || win_h <= 0)
        return;
    static bool logged_first_touch = false;
    if (!logged_first_touch && finger.type == SDL_FINGERDOWN) {
        logged_first_touch = true;
        Protokoll << "Android: primer toque recibido; ventana " << win_w << "x" << win_h
                  << ", posicion normalizada " << finger.x << "," << finger.y << std::endl;
    }
    // SDL Android recibe los toques en el hilo Java. Guardar los eventos consumidos en
    // el hilo del juego evita leer simultaneamente la lista interna mutable de SDL.
    auto it = std::find_if(contacts.begin(), contacts.end(), [&](const TouchInput::Finger &f) {
        return f.device == finger.touchId && f.id == finger.fingerId;
    });
    if (finger.type == SDL_FINGERUP) {
        if (it != contacts.end()) contacts.erase(it);
    } else if (finger.type == SDL_FINGERDOWN || finger.type == SDL_FINGERMOTION) {
        if (it == contacts.end()) contacts.push_back({finger.touchId, finger.fingerId, finger.x, finger.y, finger.x, finger.y});
        else { it->x = finger.x; it->y = finger.y; }
    }

    if (DebugPanel.IsOpen()) {
        if (finger.type == SDL_FINGERDOWN) {
            const float nx = (finger.x * win_w - area.x) / area.w;
            const float ny = (finger.y * win_h - area.y) / area.h;
            DebugPanel.HandleTouch(nx, ny, true, area);
            CancelTouches();
        }
        return;
    }
    if (wait_for_release) {
        if (contacts.empty()) wait_for_release = false;
        return;
    }
    if (finger.type == SDL_FINGERDOWN && HitDebugButton(finger.x * win_w, finger.y * win_h, area)) {
        CancelTouches();
        DebugPanel.SetOpen(true);
        return;
    }
    if (editing) {
        UpdateEditing(finger, area, win_w, win_h);
        return;
    }
    // Conserva un toque rapido incluso si baja y sube entre dos frames.
    if (finger.type == SDL_FINGERDOWN) {
        std::vector<TouchInput::Button> buttons;
        for (const auto &action : actions)
            buttons.push_back({area.x + action.x * area.w, area.y + action.y * area.h, action.r * area.h});
        const int hit = TouchInput::Hit(buttons, finger.x * win_w, finger.y * win_h);
        if (hit >= 0 && hit != ACT_DEBUG) pending[hit] = true;
    }
}

void TouchControlsClass::Update(const SDL_Rect &area, int win_w, int win_h) {
    if (!g_initialised || !active || TastaturPuffer == nullptr) return;
    if (area.w <= 0 || area.h <= 0 || win_w <= 0 || win_h <= 0) return;
    if (editing || DebugPanel.IsOpen()) {
        for (auto &action : actions) action.pressed = false;
        for (bool &p : pending) p = false;
        for (bool &direction : directions) direction = false;
        stick.Reset(); stick_active = false;
        return;
    }

    std::vector<TouchInput::Button> buttons;
    for (const auto &action : actions)
        buttons.push_back({area.x + action.x * area.w, area.y + action.y * area.h, action.r * area.h});

    if (wait_for_release && contacts.empty()) wait_for_release = false;
    if (wait_for_release) return;
    std::vector<TouchInput::Finger> fingers = contacts;
    for (auto &finger : fingers) {
        finger.x *= win_w; finger.y *= win_h;
        finger.start_x *= win_w; finger.start_y *= win_h;
    }
    const TouchInput::Rect rect{static_cast<float>(area.x), static_cast<float>(area.y),
                                static_cast<float>(area.w), static_cast<float>(area.h)};
    stick.Update(fingers, rect, buttons, stick_default_x, stick_default_y);
    stick_active = stick.active;
    stick_origin_x = stick.origin_x; stick_origin_y = stick.origin_y;
    stick_x = stick.x; stick_y = stick.y;
    stick_radius = STICK_RADIUS_FRAC * area.h;
    directions[0] = stick.Left(stick_radius); directions[1] = stick.Right(stick_radius);
    directions[2] = stick.Up(stick_radius); directions[3] = stick.Down(stick_radius);

    for (int i = 0; i < ACTION_COUNT; ++i) {
        actions[i].pressed = pending[i];
        pending[i] = false;
    }
    for (const auto &finger : fingers) {
        if (stick.Owns(finger)) continue;
        const int hit = TouchInput::Hit(buttons, finger.x, finger.y);
        if (hit >= 0) actions[hit].pressed = true;
    }

    // Los menus esperan teclas; durante la partida se usan acciones directamente, sin
    // interferir con las teclas reasignadas ni con un mando fisico.
    auto key = [](SDL_Scancode sc) { const_cast<Uint8 *>(TastaturPuffer)[sc] = 1; };
    if (actions[ACT_ESCAPE].pressed) key(SDL_SCANCODE_ESCAPE);
    if (SpielZustand != GameStateEnum::GAMELOOP) {
        if (directions[0]) key(SDL_SCANCODE_LEFT);
        if (directions[1]) key(SDL_SCANCODE_RIGHT);
        if (directions[2]) key(SDL_SCANCODE_UP);
        if (directions[3]) key(SDL_SCANCODE_DOWN);
        if (actions[ACT_JUMP].pressed || actions[ACT_FIRE].pressed || actions[ACT_WEAPON].pressed)
            key(SDL_SCANCODE_RETURN);
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

// Fuente 5x7 propia: no depende de texturas del juego ni del idioma cargado.
void TouchControlsClass::DrawLabel(const char *text, float cx, float cy, float max_width, float height) {
    auto glyph = [](char c) -> const char * {
        switch (c) {
        case 'A': return "01110100011000111111100011000110001";
        case 'B': return "11110100011000111110100011000111110";
        case 'C': return "01111100001000010000100001000001111";
        case 'D': return "11110100011000110001100011000111110";
        case 'E': return "11111100001000011110100001000011111";
        case 'F': return "11111100001000011110100001000010000";
        case 'G': return "01111100001000010111100011000101111";
        case 'H': return "10001100011000111111100011000110001";
        case 'I': return "11111001000010000100001000010011111";
        case 'L': return "10000100001000010000100001000011111";
        case 'M': return "10001110111010110101100011000110001";
        case 'N': return "10001110011010110011100011000110001";
        case 'O': return "01110100011000110001100011000101110";
        case 'P': return "11110100011000111110100001000010000";
        case 'R': return "11110100011000111110101001001010001";
        case 'S': return "01111100001000001110000010000111110";
        case 'T': return "11111001000010000100001000010000100";
        case 'U': return "10001100011000110001100011000101110";
        case 'V': return "10001100011000110001010100101000100";
        case 'X': return "10001100010101000100010101000110001";
        case 'Y': return "10001100010101000100001000010000100";
        case 'Z': return "11111000010001000100010001000011111";
        default: return "00000000000000000000000000000000000";
        }
    };
    const size_t len = std::char_traits<char>::length(text);
    if (!len) return;
    const float pixel = std::min(height / 7.0f, max_width / (len * 6.0f - 1.0f));
    const float left = cx - (len * 6.0f - 1.0f) * pixel / 2.0f;
    const float top = cy - 3.5f * pixel;
    std::vector<VERTEX2D> vertices;
    for (size_t n = 0; n < len; ++n) {
        const char *bits = glyph(text[n]);
        for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col) {
            if (bits[row * 5 + col] != '1') continue;
            const float x = left + (n * 6 + col) * pixel, y = top + row * pixel;
            const HCOLOR color(0xF0FFFFFF);
            vertices.push_back(VERTEX2D{x, y, color, 0, 0});
            vertices.push_back(VERTEX2D{x, y + pixel, color, 0, 0});
            vertices.push_back(VERTEX2D{x + pixel, y, color, 0, 0});
            vertices.push_back(VERTEX2D{x + pixel, y, color, 0, 0});
            vertices.push_back(VERTEX2D{x, y + pixel, color, 0, 0});
            vertices.push_back(VERTEX2D{x + pixel, y + pixel, color, 0, 0});
        }
    }
    if (!vertices.empty()) DirectGraphics.DrawColorVertices(GL_TRIANGLES, vertices.size(), vertices.data(),
                                                             DirectGraphics.GetWindowProjection());
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
        DrawLabel(actions[i].label, cx, cy, r * 1.55f, r * 0.42f);

        if (editing)
            DrawDisc(cx, cy, r * 0.35f, r * 0.35f, COLOR_EDIT);
    }
}

#endif /* ANDROID */
