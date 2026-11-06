/**
 * Implementacion del mando virtual de Android. Vease TouchControls.hpp.
 *
 * Nota sobre el estado de las teclas: DirectInputClass mantiene en Android un buffer propio
 * para eventos virtuales de menus. En juego, las acciones tactiles tambien se consultan
 * directamente, sin depender del perfil de entrada guardado.
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
constexpr std::uint32_t COLOR_BASE = 0x786B6B6B;
constexpr std::uint32_t COLOR_RIM = 0x907F7F7F;
constexpr std::uint32_t COLOR_PRESSED = 0xC0D8D8D8;
constexpr std::uint32_t COLOR_STICK_BASE = 0x50777777;
constexpr std::uint32_t COLOR_STICK_KNOB = 0xB0D0D0D0;
constexpr std::uint32_t COLOR_ICON = 0xD8F2F2F2;
constexpr std::uint32_t COLOR_EDIT = 0xC0FFD080;
constexpr std::uint32_t COLOR_DEBUGBTN = 0x786B6B6B;
constexpr std::uint32_t COLOR_DEBUG_RIM = 0x907F7F7F;

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
    float x, y;    // centro, normalizado con respecto a la ventana completa
    float r;       // radio, normalizado respecto a la altura de la pantalla
    SDL_Keycode key;
    bool toggle;
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
    // Distribucion inspirada en un mando clasico. Coordenadas ancladas a toda la ventana;
    // los botones de hombro se dibujan como capsulas y los de accion como discos.
    {0.850f, 0.600f, 0.055f, SDLK_LALT, false},       // ACT_JUMP    A
    {0.795f, 0.700f, 0.055f, SDLK_LCTRL, false},      // ACT_FIRE    B
    {0.795f, 0.500f, 0.055f, SDLK_LSHIFT, false},     // ACT_BLITZ   X
    {0.740f, 0.600f, 0.055f, SDLK_SPACE, false},      // ACT_POWERLINE Y
    {0.930f, 0.230f, 0.043f, SDLK_RCTRL, false},      // ACT_GRENADE R
    {0.105f, 0.105f, 0.043f, SDLK_RSHIFT, false},     // ACT_SMARTBOMB ZL
    {0.105f, 0.230f, 0.043f, SDLK_RETURN, false},     // ACT_WEAPON L
    {0.975f, 0.035f, 0.030f, SDLK_ESCAPE, false},     // ACT_ESCAPE pausa
    {0.975f, 0.940f, 0.028f, 0, true},                // ACT_DEBUG / boton GM
    {0.930f, 0.105f, 0.043f, SDLK_RSHIFT, false},     // ACT_ZR: alternativa de bomba inteligente
    {0.445f, 0.715f, 0.032f, SDLK_ESCAPE, false},      // ACT_MINUS: pausa alternativa
    {0.590f, 0.715f, 0.032f, SDLK_RETURN, false},      // ACT_PLUS: cambio de arma alternativo
};

constexpr int ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);

/// Radio del joystick, como fraccion de la altura de la pantalla.
constexpr float STICK_RADIUS_FRAC = 0.105f;
constexpr float RIGHT_STICK_X = 0.940f;
constexpr float RIGHT_STICK_Y = 0.465f;
constexpr float DPAD_X = 0.115f;
constexpr float DPAD_Y = 0.465f;
constexpr float DPAD_BUTTON_RADIUS_FRAC = 0.066f;
constexpr float DPAD_OFFSET_FRAC = 0.073f;
/// Zona muerta del joystick (fraccion del radio).
constexpr float STICK_DEADZONE = 0.30f;

bool g_initialised = false;
std::vector<VERTEX2D> g_stroke_vertices;

void DrawStroke(float x1, float y1, float x2, float y2, float width, std::uint32_t color) {
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.01f)
        return;
    const float ox = -dy * width * 0.5f / length;
    const float oy = dx * width * 0.5f / length;
    const VERTEX2D quad[4] = {
        VERTEX2D{x1 + ox, y1 + oy, HCOLOR(color), 0.0f, 0.0f},
        VERTEX2D{x1 - ox, y1 - oy, HCOLOR(color), 0.0f, 0.0f},
        VERTEX2D{x2 + ox, y2 + oy, HCOLOR(color), 0.0f, 0.0f},
        VERTEX2D{x2 - ox, y2 - oy, HCOLOR(color), 0.0f, 0.0f},
    };
    g_stroke_vertices.insert(g_stroke_vertices.end(), {quad[0], quad[1], quad[2], quad[2], quad[1], quad[3]});
}

void DrawGlyph(char glyph, float cx, float cy, float size, float width, std::uint32_t color) {
    const float half = size * 0.5f;
    const float left = cx - half;
    const float right = cx + half;
    const float top = cy - half;
    const float bottom = cy + half;
    const float mid = cy;
    switch (glyph) {
        case 'A':
            DrawStroke(left, bottom, cx, top, width, color);
            DrawStroke(cx, top, right, bottom, width, color);
            DrawStroke(cx - size * 0.24f, mid + size * 0.08f, cx + size * 0.24f, mid + size * 0.08f, width, color);
            break;
        case 'B':
            DrawStroke(left, top, left, bottom, width, color);
            DrawStroke(left, top, right - size * 0.12f, top, width, color);
            DrawStroke(left, mid, right - size * 0.08f, mid, width, color);
            DrawStroke(left, bottom, right - size * 0.12f, bottom, width, color);
            DrawStroke(right - size * 0.12f, top, right, mid, width, color);
            DrawStroke(right, mid, right - size * 0.12f, bottom, width, color);
            break;
        case 'X':
            DrawStroke(left, top, right, bottom, width, color);
            DrawStroke(right, top, left, bottom, width, color);
            break;
        case 'Y':
            DrawStroke(left, top, cx, mid, width, color);
            DrawStroke(right, top, cx, mid, width, color);
            DrawStroke(cx, mid, cx, bottom, width, color);
            break;
        case 'L':
            DrawStroke(left, top, left, bottom, width, color);
            DrawStroke(left, bottom, right, bottom, width, color);
            break;
        case 'R':
            DrawStroke(left, top, left, bottom, width, color);
            DrawStroke(left, top, right - size * 0.10f, top, width, color);
            DrawStroke(right - size * 0.10f, top, right, mid - size * 0.05f, width, color);
            DrawStroke(right, mid - size * 0.05f, left, mid, width, color);
            DrawStroke(cx - size * 0.02f, mid, right, bottom, width, color);
            break;
        case 'Z':
            DrawStroke(left, top, right, top, width, color);
            DrawStroke(right, top, left, bottom, width, color);
            DrawStroke(left, bottom, right, bottom, width, color);
            break;
        case '-':
            DrawStroke(left, cy, right, cy, width, color);
            break;
        case '+':
            DrawStroke(left, cy, right, cy, width, color);
            DrawStroke(cx, top, cx, bottom, width, color);
            break;
    }
}

}  // namespace

void TouchControlsClass::Init() {
    // Android puede registrar el touchscreen al primer evento tactil. Mantener el overlay
    // habilitado desde el arranque evita que el estado inicial de SDL oculte el mando.
    available = true;

    for (int i = 0; i < ACTION_COUNT; i++) {
        actions[i].x = ACTIONS[i].x;
        actions[i].y = ACTIONS[i].y;
        actions[i].r = ACTIONS[i].r;
        actions[i].scancode = ACTIONS[i].toggle ? 0 : static_cast<int>(SDL_GetScancodeFromKey(ACTIONS[i].key));
        actions[i].is_toggle = ACTIONS[i].toggle;
        actions[i].pressed = false;
        actions[i].finger = -1;
    }

    active = available && enabled;
    g_initialised = true;
    editing = false;
    fingers.clear();
    stick_active = false;
    stick_finger = -1;
    right_stick_active = false;
    right_stick_x = right_stick_y = 0.0f;
    std::fill(std::begin(dpad_pressed), std::end(dpad_pressed), false);
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

void TouchControlsClass::HandleTouchEvent(const SDL_TouchFingerEvent &event) {
    if (!g_initialised || !active)
        return;

    if (editing) {
        const SDL_Rect area{0, 0, window_w, window_h};
        if (window_w > 0 && window_h > 0)
            UpdateEditing(event, area, window_w, window_h);
        return;
    }

    const auto same_finger = [&](const FingerState &finger) {
        return finger.touch_id == event.touchId && finger.id == event.fingerId;
    };
    auto found = std::find_if(fingers.begin(), fingers.end(), same_finger);

    if (event.type == SDL_FINGERUP) {
        if (found != fingers.end())
            fingers.erase(found);
        return;
    }

    if (found == fingers.end()) {
        fingers.push_back(FingerState{event.touchId, event.fingerId, event.x, event.y});
    } else {
        found->x = event.x;
        found->y = event.y;
    }
}

void TouchControlsClass::CancelTouches() {
    fingers.clear();
    ClearKeys();
}

bool TouchControlsClass::IsActionPressed(VirtualAction action) const {
    if (!g_initialised || !active)
        return false;

    const float deadzone = stick_radius * STICK_DEADZONE;
    const bool stick_left = stick_active && stick_x < -deadzone;
    const bool stick_right = stick_active && stick_x > deadzone;
    const bool stick_up = stick_active && stick_y < -deadzone;
    const bool stick_down = stick_active && stick_y > deadzone;
    const float aim_deadzone = static_cast<float>(window_h) * 0.105f * 0.28f;
    const bool aim_up = right_stick_active && right_stick_y < -aim_deadzone;
    const bool aim_down = right_stick_active && right_stick_y > aim_deadzone;

    switch (action) {
        case VirtualAction::MOVE_LEFT: return dpad_pressed[0] || stick_left;
        case VirtualAction::MOVE_RIGHT: return dpad_pressed[1] || stick_right;
        case VirtualAction::DUCK: return dpad_pressed[3] || stick_down;
        case VirtualAction::LOOK_UP: return dpad_pressed[2] || stick_up || aim_up;
        case VirtualAction::LOOK_DOWN: return dpad_pressed[3] || stick_down || aim_down;
        case VirtualAction::JUMP: return actions[ACT_JUMP].pressed;
        case VirtualAction::FIRE: return actions[ACT_FIRE].pressed || right_stick_active;
        case VirtualAction::BLITZ: return actions[ACT_BLITZ].pressed;
        case VirtualAction::POWERLINE: return actions[ACT_POWERLINE].pressed;
        case VirtualAction::GRENADE: return actions[ACT_GRENADE].pressed;
        case VirtualAction::SMARTBOMB: return actions[ACT_SMARTBOMB].pressed || actions[ACT_ZR].pressed;
        case VirtualAction::CYCLE_WEAPON: return actions[ACT_WEAPON].pressed || actions[ACT_PLUS].pressed;
    }
    return false;
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
    stick_default_x = 0.25f;
    stick_default_y = 0.66f;
    Protokoll << "Mando virtual: disposicion restablecida" << std::endl;
}

void TouchControlsClass::ClearKeys() {
    fingers.clear();
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
    std::fill(std::begin(dpad_pressed), std::end(dpad_pressed), false);
    right_stick_active = false;
    right_stick_x = right_stick_y = 0.0f;
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

    auto set = [this](SDL_Scancode sc, bool on) {
        if (!on)
            return;
        const int key = static_cast<int>(sc);
        if (key <= 0 || key >= MAX_KEYS)
            return;
        if (std::find(pressed_scancodes.begin(), pressed_scancodes.end(), key) == pressed_scancodes.end()) {
            const_cast<Uint8 *>(TastaturPuffer)[key] = 1;
            pressed_scancodes.push_back(key);
        }
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
    std::fill(std::begin(dpad_pressed), std::end(dpad_pressed), false);

    stick_radius = STICK_RADIUS_FRAC * static_cast<float>(win_h);
    stick_x = stick_y = 0.0f;
    right_stick_active = false;
    right_stick_x = right_stick_y = 0.0f;
    bool stick_finger_seen = false;

    auto press_scancode = [this](SDL_Scancode scancode) {
        const int key = static_cast<int>(scancode);
        if (key <= 0 || key >= MAX_KEYS)
            return;
        if (std::find(pressed_scancodes.begin(), pressed_scancodes.end(), key) == pressed_scancodes.end()) {
            const_cast<Uint8 *>(TastaturPuffer)[key] = 1;
            pressed_scancodes.push_back(key);
        }
    };

    const float h = static_cast<float>(win_h);
    const float dpad_cx = screen_rect.x + DPAD_X * screen_rect.w;
    const float dpad_cy = screen_rect.y + DPAD_Y * screen_rect.h;
    const float dpad_offset = DPAD_OFFSET_FRAC * h;
    const float dpad_radius = DPAD_BUTTON_RADIUS_FRAC * h;
    const float right_cx = screen_rect.x + RIGHT_STICK_X * screen_rect.w;
    const float right_cy = screen_rect.y + RIGHT_STICK_Y * screen_rect.h;
    const float right_radius = 0.105f * h;

    // Los eventos SDL preservan los contactos completos, incluido el movimiento, y el
    // vector mantiene dedos simultaneos aunque SDL no exponga un dispositivo de touch.
    for (const FingerState &finger : fingers) {
        const float px = finger.x * static_cast<float>(win_w);
        const float py = finger.y * static_cast<float>(win_h);
        if (px < screen_rect.x || py < screen_rect.y || px > screen_rect.x + screen_rect.w ||
            py > screen_rect.y + screen_rect.h)
            continue;

        const float nx = (px - screen_rect.x) / static_cast<float>(screen_rect.w);
        const float ny = (py - screen_rect.y) / static_cast<float>(screen_rect.h);

        // Una vez iniciado, el stick sigue al mismo dedo aunque cruce otras zonas.
        if (stick_active && stick_finger == finger.id) {
            stick_finger_seen = true;
            float dx = px - stick_origin_x;
            float dy = py - stick_origin_y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > stick_radius) {
                dx *= stick_radius / len;
                dy *= stick_radius / len;
            }
            stick_x = dx;
            stick_y = dy;
            continue;
        }

        bool used = false;

        // D-pad de cuatro botones separados, como en el mando de referencia.
        const float dpad_x = px - dpad_cx;
        const float dpad_y = py - dpad_cy;
        const struct DpadButton { float x, y; SDL_Scancode key; } dpad[] = {
            {-dpad_offset, 0.0f, SDL_SCANCODE_LEFT}, {dpad_offset, 0.0f, SDL_SCANCODE_RIGHT},
            {0.0f, -dpad_offset, SDL_SCANCODE_UP}, {0.0f, dpad_offset, SDL_SCANCODE_DOWN},
        };
        for (int direction = 0; direction < 4; ++direction) {
            const auto &button = dpad[direction];
            if (std::hypot(dpad_x - button.x, dpad_y - button.y) <= dpad_radius) {
                dpad_pressed[direction] = true;
                press_scancode(button.key);
                used = true;
                break;
            }
        }
        if (used)
            continue;

        // El stick derecho mantiene el disparo; al arrastrarlo arriba/abajo tambien apunta.
        const float right_dx = px - right_cx;
        const float right_dy = py - right_cy;
        if (std::hypot(right_dx, right_dy) <= right_radius) {
            right_stick_active = true;
            right_stick_x = right_dx;
            right_stick_y = right_dy;
            press_scancode(SDL_SCANCODE_LCTRL);
            if (right_dy < -right_radius * 0.28f)
                press_scancode(SDL_SCANCODE_UP);
            else if (right_dy > right_radius * 0.28f)
                press_scancode(SDL_SCANCODE_DOWN);
            continue;
        }

        // Botones frontales, hombros, pausa y los botones +/-.
        for (int b = 0; b < ACTION_COUNT; b++) {
            if (actions[b].pressed)
                continue;

            const bool shoulder = b == ACT_GRENADE || b == ACT_SMARTBOMB || b == ACT_WEAPON || b == ACT_ZR;
            const float rx = actions[b].r * (shoulder ? 1.70f : 1.0f) * h / screen_rect.w;
            const float ry = actions[b].r * (shoulder ? 0.90f : 1.0f);
            const float dx = (nx - actions[b].x) / rx;
            const float dy = (ny - actions[b].y) / ry;
            if (dx * dx + dy * dy > 1.0f)
                continue;

            actions[b].pressed = true;
            used = true;
            if (actions[b].is_toggle) {
                if (b == ACT_DEBUG)
                    debug_requested = true;
            } else if (actions[b].scancode > 0) {
                press_scancode(static_cast<SDL_Scancode>(actions[b].scancode));
            }
            break;
        }
        if (used)
            continue;

        // Stick izquierdo analogo. Se puede iniciar dentro de la zona izquierda o sobre su aro.
        const bool in_zone = nx < stick_zone_w && ny < stick_zone_h;
        const bool on_drawn_stick =
            std::hypot((nx - stick_default_x) * screen_rect.w, (ny - stick_default_y) * screen_rect.h) <
            stick_radius * 1.2f;
        if (in_zone || on_drawn_stick) {
            if (!stick_active) {
                stick_active = true;
                stick_finger = finger.id;
                if (on_drawn_stick) {
                    stick_origin_x = screen_rect.x + stick_default_x * screen_rect.w;
                    stick_origin_y = screen_rect.y + stick_default_y * screen_rect.h;
                } else {
                    stick_origin_x = px;
                    stick_origin_y = py;
                }
                stick_finger_seen = true;
                stick_x = stick_y = 0.0f;
            }
        }
    }

    if (!stick_finger_seen) {
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
    if (!g_initialised || !active || screen_rect.w <= 0 || screen_rect.h <= 0)
        return;

    const float h = static_cast<float>(screen_rect.h);
    const float dpad_cx = screen_rect.x + DPAD_X * screen_rect.w;
    const float dpad_cy = screen_rect.y + DPAD_Y * screen_rect.h;
    const float offset = DPAD_OFFSET_FRAC * h;
    const float dpad_r = DPAD_BUTTON_RADIUS_FRAC * h;
    const float left_cx = screen_rect.x + stick_default_x * screen_rect.w;
    const float left_cy = screen_rect.y + stick_default_y * screen_rect.h;
    const float left_r = STICK_RADIUS_FRAC * h;
    const float right_cx = screen_rect.x + RIGHT_STICK_X * screen_rect.w;
    const float right_cy = screen_rect.y + RIGHT_STICK_Y * screen_rect.h;
    const float right_r = 0.105f * h;

    g_stroke_vertices.clear();
    g_stroke_vertices.reserve(768);

    // D-pad: four raised, softly translucent buttons with clear direction chevrons.
    const float dpad_x[] = {-offset, offset, 0.0f, 0.0f};
    const float dpad_y[] = {0.0f, 0.0f, -offset, offset};
    for (int i = 0; i < 4; ++i) {
        const float cx = dpad_cx + dpad_x[i];
        const float cy = dpad_cy + dpad_y[i];
        DrawDisc(cx, cy + dpad_r * 0.08f, dpad_r * 1.08f, dpad_r * 1.08f, 0x48000000);
        DrawDisc(cx, cy, dpad_r * 1.03f, dpad_r * 1.03f, COLOR_RIM);
        DrawDisc(cx, cy, dpad_r * 0.91f, dpad_r * 0.91f, dpad_pressed[i] ? COLOR_PRESSED : COLOR_BASE);
        const float s = dpad_r * 0.30f;
        const float w = std::max(1.5f, dpad_r * 0.12f);
        switch (i) {
            case 0: // left
                DrawStroke(cx + s * 0.45f, cy - s, cx - s * 0.45f, cy, w, COLOR_ICON);
                DrawStroke(cx - s * 0.45f, cy, cx + s * 0.45f, cy + s, w, COLOR_ICON);
                break;
            case 1: // right
                DrawStroke(cx - s * 0.45f, cy - s, cx + s * 0.45f, cy, w, COLOR_ICON);
                DrawStroke(cx + s * 0.45f, cy, cx - s * 0.45f, cy + s, w, COLOR_ICON);
                break;
            case 2: // up
                DrawStroke(cx - s, cy + s * 0.45f, cx, cy - s * 0.45f, w, COLOR_ICON);
                DrawStroke(cx, cy - s * 0.45f, cx + s, cy + s * 0.45f, w, COLOR_ICON);
                break;
            case 3: // down
                DrawStroke(cx - s, cy - s * 0.45f, cx, cy + s * 0.45f, w, COLOR_ICON);
                DrawStroke(cx, cy + s * 0.45f, cx + s, cy - s * 0.45f, w, COLOR_ICON);
                break;
        }
    }

    auto draw_stick = [&](float cx, float cy, float r, bool is_active, float dx, float dy, char label) {
        const float knob_x = cx + (is_active ? dx : 0.0f);
        const float knob_y = cy + (is_active ? dy : 0.0f);
        DrawDisc(cx, cy + r * 0.08f, r * 1.12f, r * 1.12f, 0x48000000);
        DrawDisc(cx, cy, r * 1.04f, r * 1.04f, COLOR_RIM);
        DrawDisc(cx, cy, r * 0.91f, r * 0.91f, COLOR_STICK_BASE);
        DrawDisc(knob_x, knob_y, r * 0.55f, r * 0.55f,
                 is_active ? COLOR_PRESSED : COLOR_STICK_KNOB);
        DrawGlyph(label, knob_x, knob_y, r * 0.52f, std::max(1.7f, r * 0.09f), 0xE0484848);
    };
    draw_stick(left_cx, left_cy, left_r, stick_active, stick_x, stick_y, 'L');
    draw_stick(right_cx, right_cy, right_r, right_stick_active, right_stick_x, right_stick_y, 'R');

    // Face buttons, shoulder triggers, pause, +/- and debug, positioned like the reference.
    for (int i = 0; i < ACTION_COUNT; ++i) {
        const float cx = screen_rect.x + actions[i].x * screen_rect.w;
        const float cy = screen_rect.y + actions[i].y * screen_rect.h;
        const float r = actions[i].r * h;
        const bool shoulder = i == ACT_GRENADE || i == ACT_SMARTBOMB || i == ACT_WEAPON || i == ACT_ZR;
        const float rx = r * (shoulder ? 1.70f : 1.0f);
        const float ry = r * (shoulder ? 0.90f : 1.0f);
        const std::uint32_t fill = actions[i].pressed ? COLOR_PRESSED : COLOR_BASE;
        DrawDisc(cx, cy + ry * 0.09f, rx * 1.10f, ry * 1.10f, 0x48000000);
        DrawDisc(cx, cy, rx * 1.04f, ry * 1.04f, COLOR_RIM);
        DrawDisc(cx, cy, rx * 0.91f, ry * 0.91f,
                 actions[i].is_toggle ? COLOR_DEBUGBTN : fill);

        const float glyph_size = shoulder ? r * 0.78f : r * 0.78f;
        const float glyph_width = std::max(1.5f, r * 0.12f);
        switch (i) {
            case ACT_JUMP: DrawGlyph('A', cx, cy, glyph_size, glyph_width, COLOR_ICON); break;
            case ACT_FIRE: DrawGlyph('B', cx, cy, glyph_size, glyph_width, COLOR_ICON); break;
            case ACT_BLITZ: DrawGlyph('X', cx, cy, glyph_size, glyph_width, COLOR_ICON); break;
            case ACT_POWERLINE: DrawGlyph('Y', cx, cy, glyph_size, glyph_width, COLOR_ICON); break;
            case ACT_GRENADE: DrawGlyph('R', cx, cy, glyph_size, glyph_width, COLOR_ICON); break;
            case ACT_SMARTBOMB:
                DrawGlyph('Z', cx - r * 0.19f, cy, glyph_size * 0.60f, glyph_width, COLOR_ICON);
                DrawGlyph('L', cx + r * 0.20f, cy, glyph_size * 0.60f, glyph_width, COLOR_ICON);
                break;
            case ACT_WEAPON: DrawGlyph('L', cx, cy, glyph_size, glyph_width, COLOR_ICON); break;
            case ACT_ESCAPE:
                DrawStroke(cx - r * 0.22f, cy - r * 0.34f, cx - r * 0.22f, cy + r * 0.34f,
                           glyph_width * 1.5f, COLOR_ICON);
                DrawStroke(cx + r * 0.22f, cy - r * 0.34f, cx + r * 0.22f, cy + r * 0.34f,
                           glyph_width * 1.5f, COLOR_ICON);
                break;
            case ACT_DEBUG:
                DrawStroke(cx - r * 0.46f, cy, cx, cy - r * 0.24f, glyph_width, COLOR_ICON);
                DrawStroke(cx, cy - r * 0.24f, cx + r * 0.46f, cy, glyph_width, COLOR_ICON);
                DrawStroke(cx + r * 0.46f, cy, cx, cy + r * 0.24f, glyph_width, COLOR_ICON);
                DrawStroke(cx, cy + r * 0.24f, cx - r * 0.46f, cy, glyph_width, COLOR_ICON);
                DrawDisc(cx, cy, r * 0.12f, r * 0.12f, COLOR_ICON);
                break;
            case ACT_ZR:
                DrawGlyph('Z', cx - r * 0.19f, cy, glyph_size * 0.60f, glyph_width, COLOR_ICON);
                DrawGlyph('R', cx + r * 0.20f, cy, glyph_size * 0.60f, glyph_width, COLOR_ICON);
                break;
            case ACT_MINUS: DrawGlyph('-', cx, cy, glyph_size * 0.95f, glyph_width, COLOR_ICON); break;
            case ACT_PLUS: DrawGlyph('+', cx, cy, glyph_size * 0.95f, glyph_width, COLOR_ICON); break;
        }

        if (editing)
            DrawDisc(cx, cy, r * 0.10f, r * 0.10f, COLOR_EDIT);
    }

    // Batch every line/letter in one draw call to keep the overlay light on mobile GPUs.
    if (!g_stroke_vertices.empty()) {
        DirectGraphics.DrawColorVertices(GL_TRIANGLES,
                                         static_cast<std::uint32_t>(g_stroke_vertices.size()),
                                         g_stroke_vertices.data(), DirectGraphics.GetWindowProjection());
    }
}
#endif /* ANDROID */
