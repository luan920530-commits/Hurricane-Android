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
#  include "DX8Sprite.hpp"
#  include "Globals.hpp"
#  include "Logdatei.hpp"
#  include "Main.hpp"

TouchControlsClass TouchControls;

namespace {

// Paleta gris translúcida del mando antiguo de la captura.
constexpr std::uint32_t COLOR_BASE = 0x7867758A;
constexpr std::uint32_t COLOR_RIM = 0x907E8DA3;
constexpr std::uint32_t COLOR_PRESSED = 0xB0AAB6C4;
constexpr std::uint32_t COLOR_STICK_BASE = 0x5067758A;
constexpr std::uint32_t COLOR_STICK_KNOB = 0xA0AAB6C4;
constexpr std::uint32_t COLOR_ICON = 0xF0F2F4F7;
constexpr std::uint32_t COLOR_EDIT = 0xC0FFD080;
constexpr std::uint32_t COLOR_DEBUGBTN = 0x9060B88F;
constexpr std::uint32_t COLOR_DEBUG_RIM = 0xA064BF93;

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
 * Mantener las anclas del mando que ya funcionaba; cambiar solo las letras por los
 * pictogramas de la segunda referencia. Las coordenadas usan la ventana completa.
 */
const ActionDef ACTIONS[] = {
    {0.921f, 0.800f, 0.095f, SDLK_LALT, false},       // ACT_JUMP
    {0.766f, 0.849f, 0.098f, SDLK_LCTRL, false},      // ACT_FIRE
    {0.919f, 0.580f, 0.070f, SDLK_LSHIFT, false},     // ACT_BLITZ
    {0.804f, 0.580f, 0.070f, SDLK_SPACE, false},      // ACT_POWERLINE
    {0.684f, 0.581f, 0.071f, SDLK_RCTRL, false},      // ACT_GRENADE
    {0.583f, 0.578f, 0.071f, SDLK_RSHIFT, false},     // ACT_SMARTBOMB
    {0.583f, 0.826f, 0.062f, SDLK_RETURN, false},     // ACT_WEAPON
    {0.052f, 0.070f, 0.060f, SDLK_ESCAPE, false},     // ACT_ESCAPE
    {0.969f, 0.070f, 0.060f, 0, true},                // ACT_DEBUG
};

constexpr int ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);

// Iconos Game-icons.net (CC BY 3.0), excepto depuracion (Lucide ISC/MIT).
// Atribuciones y modificaciones: data/licenses/GameIcons-NOTICE.txt.
// Los iconos anteriores y el fallback vectorial permanecen disponibles.
struct ActionIconDef {
    const char *filename;
    uint16_t texture_w, texture_h;
    uint16_t frame_w, frame_h;
    uint16_t frames_x, frames_y;
};

const ActionIconDef ACTION_ICONS[ACTION_COUNT] = {
    {"touch_tactical_jump.png", 128, 128, 128, 128, 1, 1},       // salto
    {"touch_tactical_fire.png", 128, 128, 128, 128, 1, 1},       // disparo
    {"touch_tactical_blitz.png", 128, 128, 128, 128, 1, 1},      // blitz
    {"touch_tactical_powerline.png", 128, 128, 128, 128, 1, 1},  // powerline
    {"touch_tactical_grenade.png", 128, 128, 128, 128, 1, 1},    // granada
    {"touch_tactical_smartbomb.png", 128, 128, 128, 128, 1, 1},  // bomba inteligente
    {"touch_tactical_weapon.png", 128, 128, 128, 128, 1, 1},     // cambiar arma
    {"touch_tactical_pause.png", 128, 128, 128, 128, 1, 1},      // pausa
    {"touch_tactical_debug.png", 128, 128, 128, 128, 1, 1},      // depuracion
};

DirectGraphicsSprite g_action_icons[ACTION_COUNT];
DirectGraphicsSprite g_tactical_shell;
bool g_tactical_shell_loaded = false;
bool g_action_icons_loaded[ACTION_COUNT] = {};

/// Radio del joystick, como fraccion de la altura de la pantalla.
constexpr float STICK_RADIUS_FRAC = 0.135f;
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

void DrawDpad(TouchControlsClass &controls, float cx, float cy, float radius, const bool pressed[4]) {
    const float offset = radius * 0.52f;
    const float button_r = radius * 0.34f;
    const float stroke = std::max(2.4f, radius * 0.055f);
    const float dx[] = {-offset, offset, 0.0f, 0.0f};
    const float dy[] = {0.0f, 0.0f, -offset, offset};

    controls.DrawDisc(cx, cy + radius * 0.05f, radius * 1.04f, radius * 1.04f, 0x48000000);
    if (g_tactical_shell_loaded) {
        const int diameter = static_cast<int>(radius * 2.0f);
        g_tactical_shell.RenderSpriteScaled(cx - radius, cy - radius, diameter, diameter, 0xFFFFFFFF);
    } else {
        controls.DrawDisc(cx, cy, radius, radius, COLOR_RIM);
        controls.DrawDisc(cx, cy, radius * 0.94f, radius * 0.94f, COLOR_STICK_BASE);
    }

    for (int i = 0; i < 4; ++i) {
        const float bx = cx + dx[i];
        const float by = cy + dy[i];
        if (!g_tactical_shell_loaded) {
            controls.DrawDisc(bx, by, button_r * 1.10f, button_r * 1.10f, 0x50000000);
            controls.DrawDisc(bx, by, button_r, button_r, COLOR_RIM);
            controls.DrawDisc(bx, by, button_r * 0.84f, button_r * 0.84f,
                     pressed[i] ? COLOR_PRESSED : COLOR_BASE);
        } else if (pressed[i]) {
            controls.DrawDisc(bx, by, button_r * 0.65f, button_r * 0.65f, 0x70F0C070);
        }

        const float a = button_r * 0.22f;
        switch (i) {
            case 0: // izquierda
                DrawStroke(bx + a, by - a, bx - a, by, stroke, COLOR_ICON);
                DrawStroke(bx - a, by, bx + a, by + a, stroke, COLOR_ICON);
                break;
            case 1: // derecha
                DrawStroke(bx - a, by - a, bx + a, by, stroke, COLOR_ICON);
                DrawStroke(bx + a, by, bx - a, by + a, stroke, COLOR_ICON);
                break;
            case 2: // arriba
                DrawStroke(bx - a, by + a, bx, by - a, stroke, COLOR_ICON);
                DrawStroke(bx, by - a, bx + a, by + a, stroke, COLOR_ICON);
                break;
            case 3: // abajo
                DrawStroke(bx - a, by - a, bx, by + a, stroke, COLOR_ICON);
                DrawStroke(bx, by + a, bx + a, by - a, stroke, COLOR_ICON);
                break;
        }
    }

    if (!g_tactical_shell_loaded) {
        controls.DrawDisc(cx, cy, radius * 0.31f, radius * 0.31f, COLOR_RIM);
        controls.DrawDisc(cx, cy, radius * 0.24f, radius * 0.24f, COLOR_STICK_KNOB);
    }
}

void DrawActionIcon(TouchControlsClass &controls, int action, float cx, float cy, float radius, std::uint32_t color) {
    const float w = std::max(2.0f, radius * 0.065f);
    switch (action) {
        case 0: { // SALTO: silueta corriendo/saltando
            controls.DrawDisc(cx + radius * 0.18f, cy - radius * 0.34f, radius * 0.095f, radius * 0.095f, color);
            DrawStroke(cx + radius * 0.10f, cy - radius * 0.20f, cx - radius * 0.04f, cy + radius * 0.08f, w, color);
            DrawStroke(cx + radius * 0.03f, cy - radius * 0.12f, cx - radius * 0.30f, cy - radius * 0.26f, w, color);
            DrawStroke(cx + radius * 0.02f, cy - radius * 0.12f, cx + radius * 0.34f, cy - radius * 0.04f, w, color);
            DrawStroke(cx - radius * 0.04f, cy + radius * 0.08f, cx - radius * 0.34f, cy + radius * 0.31f, w, color);
            DrawStroke(cx - radius * 0.04f, cy + radius * 0.08f, cx + radius * 0.27f, cy + radius * 0.31f, w, color);
            DrawStroke(cx - radius * 0.47f, cy + radius * 0.43f, cx + radius * 0.47f, cy + radius * 0.43f, w, color);
        } break;
        case 1: { // FUEGO: llama
            DrawStroke(cx - radius * 0.02f, cy + radius * 0.43f, cx - radius * 0.34f, cy + radius * 0.08f, w, color);
            DrawStroke(cx - radius * 0.34f, cy + radius * 0.08f, cx - radius * 0.23f, cy - radius * 0.20f, w, color);
            DrawStroke(cx - radius * 0.23f, cy - radius * 0.20f, cx - radius * 0.06f, cy - radius * 0.02f, w, color);
            DrawStroke(cx - radius * 0.06f, cy - radius * 0.02f, cx + radius * 0.06f, cy - radius * 0.45f, w, color);
            DrawStroke(cx + radius * 0.06f, cy - radius * 0.45f, cx + radius * 0.34f, cy - radius * 0.08f, w, color);
            DrawStroke(cx + radius * 0.34f, cy - radius * 0.08f, cx + radius * 0.27f, cy + radius * 0.24f, w, color);
            DrawStroke(cx + radius * 0.27f, cy + radius * 0.24f, cx - radius * 0.02f, cy + radius * 0.43f, w, color);
            DrawStroke(cx - radius * 0.02f, cy + radius * 0.25f, cx - radius * 0.12f, cy + radius * 0.02f, w * 0.75f, color);
            DrawStroke(cx - radius * 0.12f, cy + radius * 0.02f, cx + radius * 0.08f, cy - radius * 0.12f, w * 0.75f, color);
            DrawStroke(cx + radius * 0.08f, cy - radius * 0.12f, cx + radius * 0.15f, cy + radius * 0.18f, w * 0.75f, color);
            DrawStroke(cx + radius * 0.15f, cy + radius * 0.18f, cx - radius * 0.02f, cy + radius * 0.25f, w * 0.75f, color);
        } break;
        case 2: { // BLITZ: flecha veloz
            DrawStroke(cx - radius * 0.46f, cy - radius * 0.28f, cx + radius * 0.22f, cy - radius * 0.14f, w, color);
            DrawStroke(cx - radius * 0.36f, cy, cx + radius * 0.44f, cy, w * 1.2f, color);
            DrawStroke(cx - radius * 0.46f, cy + radius * 0.28f, cx + radius * 0.22f, cy + radius * 0.14f, w, color);
            DrawStroke(cx + radius * 0.12f, cy - radius * 0.30f, cx + radius * 0.46f, cy, w, color);
            DrawStroke(cx + radius * 0.46f, cy, cx + radius * 0.12f, cy + radius * 0.30f, w, color);
        } break;
        case 3: { // RAYO: descarga eléctrica
            DrawStroke(cx + radius * 0.12f, cy - radius * 0.45f, cx - radius * 0.20f, cy - radius * 0.02f, w * 1.3f, color);
            DrawStroke(cx - radius * 0.20f, cy - radius * 0.02f, cx + radius * 0.04f, cy - radius * 0.02f, w * 1.3f, color);
            DrawStroke(cx + radius * 0.04f, cy - radius * 0.02f, cx - radius * 0.12f, cy + radius * 0.44f, w * 1.3f, color);
        } break;
        case 4: { // GRANADA: cuerpo mallado y palanca
            DrawStroke(cx - radius * 0.15f, cy - radius * 0.27f, cx + radius * 0.15f, cy - radius * 0.27f, w, color);
            DrawStroke(cx - radius * 0.15f, cy - radius * 0.27f, cx - radius * 0.25f, cy - radius * 0.12f, w, color);
            DrawStroke(cx + radius * 0.15f, cy - radius * 0.27f, cx + radius * 0.25f, cy - radius * 0.12f, w, color);
            DrawStroke(cx - radius * 0.25f, cy - radius * 0.12f, cx - radius * 0.25f, cy + radius * 0.27f, w, color);
            DrawStroke(cx + radius * 0.25f, cy - radius * 0.12f, cx + radius * 0.25f, cy + radius * 0.27f, w, color);
            DrawStroke(cx - radius * 0.25f, cy + radius * 0.27f, cx + radius * 0.25f, cy + radius * 0.27f, w, color);
            DrawStroke(cx - radius * 0.19f, cy - radius * 0.03f, cx + radius * 0.19f, cy + radius * 0.18f, w * 0.7f, color);
            DrawStroke(cx - radius * 0.19f, cy + radius * 0.18f, cx + radius * 0.19f, cy - radius * 0.03f, w * 0.7f, color);
            DrawStroke(cx - radius * 0.35f, cy - radius * 0.30f, cx - radius * 0.12f, cy - radius * 0.40f, w, color);
            DrawStroke(cx - radius * 0.12f, cy - radius * 0.40f, cx + radius * 0.10f, cy - radius * 0.34f, w, color);
        } break;
        case 5: { // BOMBA: esfera con mecha y chispa
            for (int i = 0; i < 12; ++i) {
                const float a1 = static_cast<float>(i) * TWO_PI / 12.0f;
                const float a2 = static_cast<float>(i + 1) * TWO_PI / 12.0f;
                DrawStroke(cx + cosf(a1) * radius * 0.27f, cy + sinf(a1) * radius * 0.27f,
                           cx + cosf(a2) * radius * 0.27f, cy + sinf(a2) * radius * 0.27f, w, color);
            }
            DrawStroke(cx + radius * 0.12f, cy - radius * 0.24f, cx + radius * 0.25f, cy - radius * 0.40f, w, color);
            DrawStroke(cx + radius * 0.25f, cy - radius * 0.40f, cx + radius * 0.39f, cy - radius * 0.35f, w, color);
            DrawStroke(cx + radius * 0.39f, cy - radius * 0.35f, cx + radius * 0.43f, cy - radius * 0.47f, w, color);
            DrawStroke(cx + radius * 0.35f, cy - radius * 0.49f, cx + radius * 0.48f, cy - radius * 0.49f, w, color);
        } break;
        case 6: { // ARMA: pistola
            DrawStroke(cx - radius * 0.43f, cy - radius * 0.12f, cx + radius * 0.14f, cy - radius * 0.12f, w * 1.4f, color);
            DrawStroke(cx + radius * 0.14f, cy - radius * 0.12f, cx + radius * 0.46f, cy - radius * 0.12f, w, color);
            DrawStroke(cx - radius * 0.34f, cy - radius * 0.12f, cx - radius * 0.34f, cy + radius * 0.18f, w, color);
            DrawStroke(cx - radius * 0.34f, cy + radius * 0.18f, cx + radius * 0.16f, cy + radius * 0.18f, w, color);
            DrawStroke(cx + radius * 0.16f, cy + radius * 0.18f, cx + radius * 0.27f, cy + radius * 0.04f, w, color);
            DrawStroke(cx - radius * 0.06f, cy + radius * 0.18f, cx + radius * 0.02f, cy + radius * 0.42f, w, color);
            DrawStroke(cx + radius * 0.02f, cy + radius * 0.42f, cx + radius * 0.25f, cy + radius * 0.42f, w, color);
            DrawStroke(cx + radius * 0.25f, cy + radius * 0.42f, cx + radius * 0.16f, cy + radius * 0.18f, w, color);
        } break;
        case 7: // PAUSA
            DrawStroke(cx - radius * 0.18f, cy - radius * 0.34f, cx - radius * 0.18f, cy + radius * 0.34f, w * 2.3f, color);
            DrawStroke(cx + radius * 0.18f, cy - radius * 0.34f, cx + radius * 0.18f, cy + radius * 0.34f, w * 2.3f, color);
            break;
        case 8: { // DBG / ajustes: engranaje
            constexpr int teeth = 8;
            for (int i = 0; i < teeth; ++i) {
                const float a = static_cast<float>(i) * TWO_PI / teeth;
                const float x1 = cx + cosf(a) * radius * 0.22f;
                const float y1 = cy + sinf(a) * radius * 0.22f;
                const float x2 = cx + cosf(a) * radius * 0.43f;
                const float y2 = cy + sinf(a) * radius * 0.43f;
                DrawStroke(x1, y1, x2, y2, w, color);
            }
            controls.DrawDisc(cx, cy, radius * 0.20f, radius * 0.20f, color);
            controls.DrawDisc(cx, cy, radius * 0.10f, radius * 0.10f, COLOR_DEBUGBTN);
        } break;
    }
}

}  // namespace

void TouchControlsClass::Init() {
    // Android puede registrar el touchscreen al primer evento tactil. Mantener el overlay
    // habilitado desde el arranque evita que el estado inicial de SDL oculte el mando.
    available = true;
    g_tactical_shell_loaded = g_tactical_shell.LoadImage("touch_tactical_shell.png", 256, 256, 256, 256, 1, 1);

    for (int i = 0; i < ACTION_COUNT; i++) {
        actions[i].x = ACTIONS[i].x;
        actions[i].y = ACTIONS[i].y;
        actions[i].r = ACTIONS[i].r;
        actions[i].scancode = ACTIONS[i].toggle ? 0 : static_cast<int>(SDL_GetScancodeFromKey(ACTIONS[i].key));
        actions[i].is_toggle = ACTIONS[i].toggle;
        actions[i].pressed = false;
        actions[i].finger = -1;

        const ActionIconDef &icon = ACTION_ICONS[i];
        g_action_icons_loaded[i] = g_action_icons[i].LoadImage(icon.filename, icon.texture_w, icon.texture_h,
                                                               icon.frame_w, icon.frame_h, icon.frames_x,
                                                               icon.frames_y);
    }

    active = available && enabled;
    g_initialised = true;
    editing = false;
    fingers.clear();
    stick_active = false;
    stick_finger = -1;
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

    switch (action) {
        case VirtualAction::MOVE_LEFT: return dpad_pressed[0] || stick_left;
        case VirtualAction::MOVE_RIGHT: return dpad_pressed[1] || stick_right;
        case VirtualAction::DUCK: return dpad_pressed[3] || stick_down;
        case VirtualAction::LOOK_UP: return dpad_pressed[2] || stick_up;
        case VirtualAction::LOOK_DOWN: return dpad_pressed[3] || stick_down;
        case VirtualAction::JUMP: return actions[ACT_JUMP].pressed;
        case VirtualAction::FIRE: return actions[ACT_FIRE].pressed;
        case VirtualAction::BLITZ: return actions[ACT_BLITZ].pressed;
        case VirtualAction::POWERLINE: return actions[ACT_POWERLINE].pressed;
        case VirtualAction::GRENADE: return actions[ACT_GRENADE].pressed;
        case VirtualAction::SMARTBOMB: return actions[ACT_SMARTBOMB].pressed;
        case VirtualAction::CYCLE_WEAPON: return actions[ACT_WEAPON].pressed;
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
    stick_default_x = 0.173f;
    stick_default_y = 0.741f;
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

    stick_radius = STICK_RADIUS_FRAC * static_cast<float>(win_h);
    stick_x = stick_y = 0.0f;
    std::fill(std::begin(dpad_pressed), std::end(dpad_pressed), false);
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

    // Procesar todos los dedos activos del ciclo SDL. Un dedo puede mantener un boton y
    // otro mover simultaneamente el stick, como hacia el mando anterior.
    for (const FingerState &finger : fingers) {
        const float px = finger.x * static_cast<float>(win_w);
        const float py = finger.y * static_cast<float>(win_h);
        if (px < screen_rect.x || py < screen_rect.y || px > screen_rect.x + screen_rect.w ||
            py > screen_rect.y + screen_rect.h)
            continue;

        const float nx = (px - screen_rect.x) / static_cast<float>(screen_rect.w);
        const float ny = (py - screen_rect.y) / static_cast<float>(screen_rect.h);

        // El dedo que inicio el stick sigue controlandolo aunque arrastre sobre otros botones.
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
        const float dpad_cx = screen_rect.x + stick_default_x * screen_rect.w;
        const float dpad_cy = screen_rect.y + stick_default_y * screen_rect.h;
        const float dpad_r = 0.185f * h;
        const float dpad_offset = dpad_r * 0.52f;
        const float dpad_button_r = dpad_r * 0.34f;
        const struct DpadDirection { float x, y; SDL_Scancode key; } directions[] = {
            {-dpad_offset, 0.0f, SDL_SCANCODE_LEFT}, {dpad_offset, 0.0f, SDL_SCANCODE_RIGHT},
            {0.0f, -dpad_offset, SDL_SCANCODE_UP}, {0.0f, dpad_offset, SDL_SCANCODE_DOWN},
        };
        for (int direction = 0; direction < 4; ++direction) {
            if (std::hypot(px - dpad_cx - directions[direction].x,
                           py - dpad_cy - directions[direction].y) <= dpad_button_r) {
                dpad_pressed[direction] = true;
                press_scancode(directions[direction].key);
                used = true;
                break;
            }
        }
        if (used)
            continue;

        for (int b = 0; b < ACTION_COUNT; ++b) {
            const float rx = actions[b].r * h / static_cast<float>(screen_rect.w);
            const float ry = actions[b].r;
            const float dx = (nx - actions[b].x) / rx;
            const float dy = (ny - actions[b].y) / ry;
            if (dx * dx + dy * dy > 1.0f)
                continue;

            actions[b].pressed = true;
            used = true;
            if (actions[b].is_toggle) {
                if (b == ACT_DEBUG)
                    debug_requested = true;
            } else {
                press_scancode(static_cast<SDL_Scancode>(actions[b].scancode));
            }
            break;
        }
        if (used)
            continue;

        // Stick flotante de movimiento: nace bajo el pulgar izquierdo y conserva el
        // emplazamiento grande que aparece en la captura original.
        const bool in_zone = nx < stick_zone_w && ny > 0.38f;
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
    const float dpad_cx = screen_rect.x + stick_default_x * screen_rect.w;
    const float dpad_cy = screen_rect.y + stick_default_y * screen_rect.h;
    const float dpad_r = 0.185f * h;

    g_stroke_vertices.clear();
    g_stroke_vertices.reserve(1024);

    DrawDpad(*this, dpad_cx, dpad_cy, dpad_r, dpad_pressed);

    for (int i = 0; i < ACTION_COUNT; ++i) {
        const float cx = screen_rect.x + actions[i].x * screen_rect.w;
        const float cy = screen_rect.y + actions[i].y * screen_rect.h;
        const float r = actions[i].r * h;
        const std::uint32_t fill = actions[i].is_toggle
                                       ? COLOR_DEBUGBTN
                                       : (actions[i].pressed ? COLOR_PRESSED : COLOR_BASE);

        DrawDisc(cx, cy + r * 0.08f, r * 1.10f, r * 1.10f, 0x48000000);
        if (g_tactical_shell_loaded) {
            const int diameter = static_cast<int>(r * 2.0f);
            const std::uint32_t shell_tint = actions[i].pressed ? 0xFFFFDB8A : 0xFFFFFFFF;
            g_tactical_shell.RenderSpriteScaled(cx - r, cy - r, diameter, diameter, shell_tint);
        } else {
            DrawDisc(cx, cy, r * 1.04f, r * 1.04f,
                     actions[i].is_toggle ? COLOR_DEBUG_RIM : COLOR_RIM);
            DrawDisc(cx, cy, r * 0.92f, r * 0.92f, fill);
        }

        if (g_action_icons_loaded[i]) {
            const ActionIconDef &icon = ACTION_ICONS[i];
            const float icon_w = r * (g_tactical_shell_loaded ? 1.08f : 1.42f);
            const float icon_h = icon_w * static_cast<float>(icon.frame_h) / icon.frame_w;
            const std::uint32_t tint = actions[i].pressed ? 0xFFFFE2A0 : 0xFFFFFFFF;
            g_action_icons[i].RenderSpriteScaled(static_cast<int>(cx - icon_w * 0.5f),
                                                  static_cast<int>(cy - icon_h * 0.5f),
                                                  static_cast<int>(icon_w), static_cast<int>(icon_h), tint);
        } else {
            DrawActionIcon(*this, i, cx, cy, r * 0.74f, COLOR_ICON);
        }

        if (editing)
            DrawDisc(cx, cy, r * 0.10f, r * 0.10f, COLOR_EDIT);
    }

    // Dibuja los pictogramas en un lote para mantener ligero el overlay en Android.
    if (!g_stroke_vertices.empty()) {
        DirectGraphics.DrawColorVertices(GL_TRIANGLES,
                                         static_cast<std::uint32_t>(g_stroke_vertices.size()),
                                         g_stroke_vertices.data(), DirectGraphics.GetWindowProjection());
    }
}

#endif /* ANDROID */
