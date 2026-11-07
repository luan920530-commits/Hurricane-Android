/**
 * Panel de trucos y depuracion para Android.
 *
 * Es el "widget" que se abre con el boton DBG del mando virtual: una lista de interruptores
 * grande y legible en el movil, pensada para probar el port sin teclado:
 *
 *   - Vidas infinitas      (no se descuenta ninguna vida)
 *   - Municion infinita    (granadas, powerlines y smart bombs siempre llenas)
 *   - Armas al maximo      (las cuatro armas al nivel maximo)
 *   - Modo invulnerable    (GodMode del motor: no recibe dano)
 *   - Mostrar FPS          (contador de fotogramas del motor)
 *   - Editar controles     (arrastrar los botones del mando virtual)
 *
 * Los trucos NO se guardan en disco: duran mientras la app esta abierta. Asi no se queda
 * activado sin querer en una partida normal, y no hace falta limpiar nada para volver a
 * jugar "de verdad".
 *
 * Funcionamiento: el panel se dibuja en coordenadas de ventana, encima del juego, y consume
 * los toques que caen dentro de su zona. Cuando esta abierto, el mando virtual no procesa
 * dedos (salvo el boton DBG), y el juego se dibuja pero no avanza (se pausa).
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#include "DebugPanel.hpp"

#if defined(ANDROID)

#  include <algorithm>
#  include <string>

#  include "DX8Font.hpp"
#  include "DX8Graphics.hpp"
#  include "DX8Input.hpp"
#  include "Logdatei.hpp"
#  include "Main.hpp"
#  include "Player.hpp"
#  include "SDLPort/TouchControls.hpp"

DebugPanelClass DebugPanel;

namespace {

constexpr int OPT_COUNT = 6;

enum OptionId {
    OPT_LIVES = 0,
    OPT_AMMO,
    OPT_WEAPONS,
    OPT_GODMODE,
    OPT_FPS,
    OPT_EDIT,
};

struct Option {
    const char *label;
    bool *value;
};

/// Niveles "de verdad" del motor: por encima de esto hay armas especiales que no se pueden
/// activar sin el power-up correspondiente, asi que el truco deja las armas al 8.
constexpr int MAX_SANE_WEAPON_LEVEL = 8;

}  // namespace

// --------------------------------------------------------------------------------------
// Ajustes
// --------------------------------------------------------------------------------------

bool DebugPanelClass::InfiniteLives() {
    return InfiniteLives_;
}

bool DebugPanelClass::InfiniteAmmo() {
    return InfiniteAmmo_;
}

bool DebugPanelClass::MaxWeapons() {
    return MaxWeapons_;
}

bool DebugPanelClass::IsGodMode() {
    return GodMode_;
}

void DebugPanelClass::SetOpen(bool open) {
    if (open == open_)
        return;

    open_ = open;

    /*
     * Al abrir el panel se suelta todo lo que el mando virtual tuviera pulsado: si no, el
     * jugador se queda andando o disparando solo mientras navega por los interruptores.
     */
    if (open_) {
        if (TastaturPuffer != nullptr) {
            const_cast<Uint8 *>(TastaturPuffer)[SDL_SCANCODE_LEFT] = 0;
            const_cast<Uint8 *>(TastaturPuffer)[SDL_SCANCODE_RIGHT] = 0;
            const_cast<Uint8 *>(TastaturPuffer)[SDL_SCANCODE_UP] = 0;
            const_cast<Uint8 *>(TastaturPuffer)[SDL_SCANCODE_DOWN] = 0;
            const_cast<Uint8 *>(TastaturPuffer)[SDL_SCANCODE_LCTRL] = 0;
        }
    }

    Protokoll << "Panel de depuracion: " << (open_ ? "abierto" : "cerrado") << std::endl;
}

void DebugPanelClass::Toggle() {
    SetOpen(!open_);
}

void DebugPanelClass::SetGodMode(bool enabled) {
    GodMode_ = enabled;
    ApplyGodMode();
}

bool DebugPanelClass::AnyActive() {
    return InfiniteLives_ || InfiniteAmmo_ || MaxWeapons_ || GodMode_;
}

void DebugPanelClass::ApplyGodMode() {
    // El motor consulta Player[].GodMode en cada dano (Player.cpp, PlayerClass::Damage).
    Player[0].GodMode = GodMode_;
    Player[1].GodMode = GodMode_;
}

// --------------------------------------------------------------------------------------
// Trucos que hay que reaplicar cada frame
// --------------------------------------------------------------------------------------

void DebugPanelClass::ApplyFrameCheats() {
    if (!AnyActive())
        return;

    for (int p = 0; p < 2; p++) {
        PlayerClass &pl = Player[p];

        if (InfiniteLives_) {
            // El motor da el juego por terminado con Lives < 0, y en el HUD dibuja el
            // numero tal cual: se mantiene en 9 (un valor visible y creible).
            if (pl.Lives >= 0 && pl.Lives < 9)
                pl.Lives = 9;
        }

        if (InfiniteAmmo_) {
            // Granadas, powerlines y smart bombs son contadores enteros que el motor
            // descuenta al usarlos; el disparo normal no gasta municion (es un juego de
            // 1997 con armas de cadencia ilimitada), asi que esto es toda la "municion".
            if (pl.Grenades < 9)
                pl.Grenades = 9;
            if (pl.PowerLines < 9)
                pl.PowerLines = 9;
            if (pl.SmartBombs < 9)
                pl.SmartBombs = 9;
        }

        if (MaxWeapons_) {
            for (int w = 0; w < 4; w++) {
                if (pl.CurrentWeaponLevel[w] < MAX_SANE_WEAPON_LEVEL)
                    pl.CurrentWeaponLevel[w] = MAX_SANE_WEAPON_LEVEL;
            }
            if (pl.PowerLines < 3)
                pl.PowerLines = 3;
        }
    }

    if (GodMode_)
        ApplyGodMode();
}

// --------------------------------------------------------------------------------------
// Entrada
// --------------------------------------------------------------------------------------

bool DebugPanelClass::PanelRect(const SDL_Rect &screen_rect, SDL_FRect &out) const {
    if (screen_rect.w <= 0 || screen_rect.h <= 0)
        return false;

    const bool portrait = screen_rect.h > screen_rect.w;
    const float w = static_cast<float>(screen_rect.w) * (portrait ? 0.90f : 0.60f);
    const float h = static_cast<float>(screen_rect.h) * (portrait ? 0.74f : 0.72f);
    out.x = screen_rect.x + (static_cast<float>(screen_rect.w) - w) * 0.5f;
    out.y = screen_rect.y + (static_cast<float>(screen_rect.h) - h) * 0.5f;
    out.w = w;
    out.h = h;
    return true;
}

bool DebugPanelClass::HandleTouch(float nx, float ny, bool down, const SDL_Rect &screen_rect) {
    if (!open_)
        return false;

    SDL_FRect panel;
    if (!PanelRect(screen_rect, panel))
        return false;

    const float px = screen_rect.x + nx * static_cast<float>(screen_rect.w);
    const float py = screen_rect.y + ny * static_cast<float>(screen_rect.h);

    const bool inside = px >= panel.x && px <= panel.x + panel.w && py >= panel.y && py <= panel.y + panel.h;
    if (!down)
        return inside;

    // Un toque fuera del panel lo cierra: es la forma mas natural de volver al juego.
    if (!inside) {
        SetOpen(false);
        return true;
    }

    // Reparto vertical: titulo, una fila por opcion, y una ultima fila de "cerrar".
    const float filas = static_cast<float>(OPT_COUNT) + 2.0f;
    const float alto_fila = panel.h / filas;
    const int fila = static_cast<int>((py - panel.y) / alto_fila);

    if (fila >= 1 && fila <= OPT_COUNT) {
        const int opcion = fila - 1;
        switch (opcion) {
            case OPT_LIVES:
                InfiniteLives_ = !InfiniteLives_;
                break;
            case OPT_AMMO:
                InfiniteAmmo_ = !InfiniteAmmo_;
                break;
            case OPT_WEAPONS:
                MaxWeapons_ = !MaxWeapons_;
                break;
            case OPT_GODMODE:
                SetGodMode(!GodMode_);
                break;
            case OPT_FPS:
                CommandLineParams.ShowFPS = !CommandLineParams.ShowFPS;
                break;
            case OPT_EDIT:
                TouchControls.ToggleEditing();
                break;
            default:
                break;
        }
        Protokoll << "Panel de depuracion: opcion " << opcion << " conmutada" << std::endl;
    } else if (fila >= OPT_COUNT + 1) {
        SetOpen(false);
    }

    return true;
}

// --------------------------------------------------------------------------------------
// Dibujo
// --------------------------------------------------------------------------------------

void DebugPanelClass::Draw(const SDL_Rect &screen_rect) {
    if (!open_)
        return;

    SDL_FRect panel;
    if (!PanelRect(screen_rect, panel))
        return;

    const glm::mat4x4 &matrix = DirectGraphics.GetWindowProjection();

    auto quad = [&](SDL_FRect r, std::uint32_t argb) {
        VERTEX2D v[4] = {
            VERTEX2D{r.x, r.y, HCOLOR(argb), 0.0f, 0.0f},
            VERTEX2D{r.x, r.y + r.h, HCOLOR(argb), 0.0f, 0.0f},
            VERTEX2D{r.x + r.w, r.y, HCOLOR(argb), 0.0f, 0.0f},
            VERTEX2D{r.x + r.w, r.y + r.h, HCOLOR(argb), 0.0f, 0.0f},
        };
        DirectGraphics.DrawColorVertices(GL_TRIANGLE_STRIP, 4, v, matrix);
    };

    const Option options[OPT_COUNT] = {
        {"Vidas infinitas", &InfiniteLives_},
        {"Municion infinita", &InfiniteAmmo_},
        {"Armas al maximo", &MaxWeapons_},
        {"Invulnerable", &GodMode_},
        {"Mostrar FPS", &CommandLineParams.ShowFPS},
        {"Editar controles", nullptr},
    };

    const float filas = static_cast<float>(OPT_COUNT) + 2.0f;
    const float alto_fila = panel.h / filas;
    const float title_h = alto_fila;
    const float switch_w = std::min(alto_fila * 1.12f, panel.w * 0.24f);
    const float switch_h = alto_fila * 0.54f;
    const float switch_x = panel.x + panel.w - switch_w - alto_fila * 0.24f;

    const int previous_scale = pDefaultFont != nullptr ? pDefaultFont->GetScaleFactor() : 1;
    if (pDefaultFont != nullptr) {
        const int scale = std::clamp(
            static_cast<int>(std::min(panel.w / 190.0f, static_cast<float>(screen_rect.h) / 360.0f)), 1, 4);
        pDefaultFont->SetScaleFactor(scale);
    }

    // Oscurecer el juego para que el panel se lea como un menu modal.
    quad(SDL_FRect{static_cast<float>(screen_rect.x), static_cast<float>(screen_rect.y),
                   static_cast<float>(screen_rect.w), static_cast<float>(screen_rect.h)}, 0x90000000);
    quad(panel, 0xF01A202A);
    quad(SDL_FRect{panel.x, panel.y, panel.w, title_h}, 0xFF304870);

    if (pDefaultFont != nullptr) {
        const float title_y = panel.y + (title_h - pDefaultFont->GetYCharSize()) * 0.5f;
        pDefaultFont->DrawTextCenterAlign(panel.x + panel.w * 0.5f, title_y,
                                          "MENU DE DESARROLLO", 0xFFFFFFFF, 0);
    }

    for (int i = 0; i < OPT_COUNT; ++i) {
        const float y = panel.y + alto_fila * static_cast<float>(i + 1);
        const SDL_FRect row{panel.x + alto_fila * 0.15f, y,
                            panel.w - alto_fila * 0.30f, alto_fila * 0.88f};
        quad(row, (i % 2 == 0) ? 0x30FFFFFF : 0x1CFFFFFF);

        const bool on = options[i].value != nullptr ? *options[i].value : TouchControls.IsEditing();
        const SDL_FRect toggle{switch_x, y + (alto_fila - switch_h) * 0.5f, switch_w, switch_h};
        quad(toggle, on ? 0xFF23864A : 0xFF46515E);

        if (pDefaultFont != nullptr) {
            const float label_x = panel.x + alto_fila * 0.35f;
            const float label_y = y + (alto_fila - pDefaultFont->GetYCharSize()) * 0.5f;
            pDefaultFont->DrawText(label_x, label_y, options[i].label, 0xFFFFFFFF);

            const char *state = on ? "ON" : "OFF";
            const float state_y = toggle.y + (toggle.h - pDefaultFont->GetYCharSize()) * 0.5f;
            pDefaultFont->DrawTextCenterAlign(toggle.x + toggle.w * 0.5f, state_y, state, 0xFFFFFFFF, 0);
        }
    }

    const float close_y = panel.y + alto_fila * static_cast<float>(OPT_COUNT + 1);
    const SDL_FRect close{panel.x + panel.w * 0.25f, close_y + alto_fila * 0.12f,
                          panel.w * 0.5f, alto_fila * 0.62f};
    quad(close, 0xFFE04A4A);
    if (pDefaultFont != nullptr) {
        const float close_text_y = close.y + (close.h - pDefaultFont->GetYCharSize()) * 0.5f;
        pDefaultFont->DrawTextCenterAlign(panel.x + panel.w * 0.5f, close_text_y, "CERRAR", 0xFFFFFFFF, 0);
        pDefaultFont->SetScaleFactor(previous_scale);
    }
}

#endif /* ANDROID */
