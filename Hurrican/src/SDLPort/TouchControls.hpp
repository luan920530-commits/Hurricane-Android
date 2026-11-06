/**
 * Controles tactiles de Android.
 *
 * Hurrican solo consulta teclado (y joystick / gamepad). Para que sea jugable en un movil
 * sin mando, este modulo dibuja un mando virtual sobre la pantalla y traduce cada dedo a la
 * tecla que el motor ya entiende. Asi no hay que tocar la logica del juego: sigue leyendo
 * KeyDown(DIK_...), solo que el estado de esas teclas lo mantenemos nosotros.
 *
 * Distribucion en horizontal (landscape):
 *
 *   +--------------------------------------------------------------+
 *   | [SALIR]                                  [ARMA] [BOMBA]       |
 *   |                                                              |
 *   |                  ( zona de juego, sin overlay )               |
 *   |                                                              |
 *   |                    (D-pad)                   (A)             |
 *   |                                             (B)  (C)         |
 *   |                                              [POWERLINE]     |
 *   +--------------------------------------------------------------+
 *
 *  - D-pad a la izquierda: mover, agacharse y el "look up" del original.
 *  - A: saltar, B: disparar, C: blitz (turbo). Boton aparte para powerline.
 *  - ARMA: cambia el arma actual, BOMBA: smart bomb.
 *  - SALIR: pausa/menu.
 *
 * Todo se puede desactivar (por ejemplo si se juega con mando Bluetooth) mediante el
 * ajuste persistente que guarda el propio motor; ver IsEnabled().
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#ifndef _TOUCHCONTROLS_HPP_
#define _TOUCHCONTROLS_HPP_

#include "DX8Input.hpp"

#if defined(ANDROID)

#  include <SDL.h>
#  include <vector>

/**
 * Mando virtual. Una sola instancia global, declarada al final de este fichero.
 */
class TouchControlsClass {
  public:
    void Init();                     ///< Lee la deteccion de pantalla tactil y ajustes
    void Shutdown();                 ///< Libera el estado
    void Update(const SDL_Rect &screen_rect, int window_w, int window_h);
    void DrawGeometry(const SDL_Rect &screen_rect);

    /** true si el dispositivo tiene pantalla tactil y el jugador no lo desactivo. */
    inline bool IsActive() const { return active; }
    inline bool IsAvailable() const { return available; }
    void SetEnabled(bool enabled);
    inline bool IsEnabled() const { return enabled; }

  private:
    struct Button {
        SDL_FRect rect;      ///< posicion normalizada (0..1) dentro del area de juego
        int scancode;        ///< tecla que se pulsa al tocarlo
        bool pressed;
        bool is_circle;      ///< el D-pad se dibuja redondo
    };


    void ClearKeys();

    bool available = false;  ///< el dispositivo tiene pantalla tactil
    bool enabled = true;     ///< el jugador quiere el mando virtual
    bool active = false;     ///< available && enabled
    int window_w = 0;
    int window_h = 0;

    std::vector<Button> buttons;
    std::vector<int> pressed_scancodes;
};

extern TouchControlsClass TouchControls;

#endif /* ANDROID */

#endif /* _TOUCHCONTROLS_HPP_ */
