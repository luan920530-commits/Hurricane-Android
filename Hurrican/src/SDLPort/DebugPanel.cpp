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

#  include "DX8Graphics.hpp"
#  include "DX8Input.hpp"
#  include "Logdatei.hpp"
#  include "Main.hpp"
#  include "Player.hpp"
#  include "SDLPort/TouchControls.hpp"

DebugPanelClass DebugPanel;

namespace {

constexpr int OPT_COUNT = 7;

enum OptionId {
    OPT_LIVES = 0,
    OPT_AMMO,
    OPT_WEAPONS,
    OPT_GODMODE,
    OPT_FPS,
    OPT_EDIT,
    OPT_UPSCALE,
};

struct Option {
    const char *label;
    bool *value;
};

/*
 * Calidades de escalado, en el mismo orden que el shader (vease shader_upscale.frag):
 *   0 nitido, 1 suave, 2 maximo.
 * El interruptor de la ultima fila recorre las tres.
 */
const char *const UPSCALE_LABELS[] = {"Nitido", "Suave", "Maximo"};

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
            case OPT_UPSCALE: {
                /*
                 * Recorre las tres calidades. Se hace en caliente (recompila un shader
                 * pequeno) para poder comparar el aspecto sin reiniciar el juego.
                 */
                const int siguiente = (DirectGraphics.GetUpscale() + 1) % 3;
                DirectGraphics.SetUpscale(siguiente);
                Protokoll << "Escalado cambiado a " << UPSCALE_LABELS[siguiente] << std::endl;
            } break;
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
        {"Vidas infinitas", &InfiniteLives_},
        {"Municion infinita", &InfiniteAmmo_},
        {"Armas al maximo", &MaxWeapons_},
        {"Invulnerable", &GodMode_},
        {"Mostrar FPS", &CommandLineParams.ShowFPS},
        {"Editar controles", nullptr},
        /* El estado de esta fila se dibuja aparte: no es un booleano sino 3 calidades. */
        {"Calidad de imagen", nullptr},
    };

    for (int i = 0; i < OPT_COUNT; i++) {
        const float y = panel.y + alto_fila * static_cast<float>(i + 1);

        // Fila alterna para que se distingan.
        quad(SDL_FRect{panel.x + alto_fila * 0.15f, y, panel.w - alto_fila * 0.30f, alto_fila * 0.88f},
             (i % 2 == 0) ? 0x30FFFFFF : 0x1CFFFFFF);

        /*
         * Interruptor. La fila de calidad de imagen no es un interruptor, sino un selector de
         * tres estados: se dibujan tres marcas y se resalta la activa.
         */
        bool on = false;

        if (i == OPT_UPSCALE) {
            /*
             * No es un interruptor, sino un selector de tres estados: se dibujan tres marcas
             * (nitido / suave / maximo) y se resalta la activa.
             */
            const int actual = DirectGraphics.GetUpscale();
            on = (actual > 0);
            const float lado = alto_fila * 0.42f;
            for (int q = 0; q < 3; q++) {
                const float x = panel.x + panel.w - alto_fila * (1.10f + (2 - q) * 0.60f);
                quad(SDL_FRect{x, y + (alto_fila - lado) * 0.5f, lado, lado},
                     (q == actual) ? 0xFF50E070 : 0xFF505A68);
            }
        } else {
            on = options[i].value != nullptr ? *options[i].value : TouchControls.IsEditing();
            const std::uint32_t color = on ? 0xFF50E070 : 0xFF505A68;
            const float lado = alto_fila * 0.52f;
            quad(SDL_FRect{panel.x + panel.w - alto_fila * 1.10f, y + (alto_fila - lado) * 0.5f, lado, lado},
                 color);
        }

        /*
         * Etiqueta: el motor no puede dibujar texto sin fuente cargada, asi que se marca el
         * numero de fila y el panel se documenta en la ayuda del boton DBG. Para que sea
         * legible de un vistazo, las filas activadas llevan una barra ancha y las apagadas
         * una fina.
         */
        quad(SDL_FRect{panel.x + alto_fila * 0.35f, y + alto_fila * 0.32f,
                       panel.w * (on ? 0.40f : 0.18f), alto_fila * 0.26f},
             on ? 0xC0FFFFFF : 0x60FFFFFF);
    }

    // Boton de cerrar.
    const float y_close = panel.y + alto_fila * static_cast<float>(OPT_COUNT + 1);
    quad(SDL_FRect{panel.x + panel.w * 0.25f, y_close + alto_fila * 0.12f, panel.w * 0.5f,
                   alto_fila * 0.62f},
         0xF0A03030);
}

#endif /* ANDROID */
