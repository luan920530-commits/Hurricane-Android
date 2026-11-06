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
 * dedos; el juego sigue avanzando mientras el panel esta abierto.
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#include "DebugPanel.hpp"

#if defined(ANDROID)

#  include <algorithm>
#  include <string>

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
    TouchControls.CancelTouches();

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

    const float w = static_cast<float>(screen_rect.w) * 0.52f;
    const float h = static_cast<float>(screen_rect.h) * 0.66f;
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

    // Fondo semitransparente y barra de titulo.
    quad(panel, 0xE0141820);
    quad(SDL_FRect{panel.x, panel.y, panel.w, panel.h * 0.10f}, 0xF0304870);

    // Filas: rectangulo + indicador de estado (cuadrado verde = activado).
    const float filas = static_cast<float>(OPT_COUNT) + 2.0f;
    const float alto_fila = panel.h / filas;
    const Option options[OPT_COUNT] = {
        {"VIDAS", &InfiniteLives_},
        {"MUNICION", &InfiniteAmmo_},
        {"ARMAS MAX", &MaxWeapons_},
        {"INVULNERABLE", &GodMode_},
        {"FPS", &CommandLineParams.ShowFPS},
        {"EDITAR", nullptr},
    };

    for (int i = 0; i < OPT_COUNT; i++) {
        const float y = panel.y + alto_fila * static_cast<float>(i + 1);

        // Fila alterna para que se distingan.
        quad(SDL_FRect{panel.x + alto_fila * 0.15f, y, panel.w - alto_fila * 0.30f, alto_fila * 0.88f},
             (i % 2 == 0) ? 0x30FFFFFF : 0x1CFFFFFF);

        // Interruptor.
        const bool on = options[i].value != nullptr ? *options[i].value : TouchControls.IsEditing();
        const std::uint32_t color = on ? 0xFF50E070 : 0xFF505A68;
        const float lado = alto_fila * 0.52f;
        quad(SDL_FRect{panel.x + panel.w - alto_fila * 1.10f, y + (alto_fila - lado) * 0.5f, lado, lado},
             color);

        TouchControls.DrawLabel(options[i].label, panel.x + panel.w * 0.40f, y + alto_fila * 0.44f,
                                panel.w * 0.68f, alto_fila * 0.36f);
    }

    // Boton de cerrar.
    const float y_close = panel.y + alto_fila * static_cast<float>(OPT_COUNT + 1);
    quad(SDL_FRect{panel.x + panel.w * 0.25f, y_close + alto_fila * 0.12f, panel.w * 0.5f,
                   alto_fila * 0.62f},
         0xF0A03030);
    TouchControls.DrawLabel("CERRAR", panel.x + panel.w * 0.5f, y_close + alto_fila * 0.43f,
                            panel.w * 0.4f, alto_fila * 0.30f);
}

#endif /* ANDROID */
