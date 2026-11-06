/**
 * Controles tactiles de Android - mando virtual.
 *
 * Hurrican solo consulta teclado: la logica del jugador lee KeyDown(DIK_...). En lugar de
 * reescribir el motor, este modulo dibuja un mando en pantalla y escribe en el mismo buffer
 * de teclas que consulta el juego (DirectInputClass::TouchKeyboard). Asi el juego se comporta
 * exactamente igual que con teclado.
 *
 * DISPOSICION inspirada en un mando clasico de pantalla: d-pad y stick L a la izquierda,
 * cuatro botones A/B/X/Y y stick R a la derecha, hombros ZL/L/ZR/R, pausa y +/-.
 * Las posiciones se anclan a la ventana completa y escalan con su altura.
 *
 *  - D-pad y sticks admiten multitouch; el stick derecho dispara y permite apuntar arriba/abajo.
 *  - Los botones mapean las mismas teclas que el teclado original, sin cambiar la logica del juego.
 *  - El boton del ojo abre el panel de depuracion; la configuracion puede ocultar el overlay.
 *  - Modo edicion y restauracion de la disposicion permanecen disponibles.
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#ifndef _TOUCHCONTROLS_HPP_
#define _TOUCHCONTROLS_HPP_

#include "DX8Input.hpp"

#if defined(ANDROID)

#  include <SDL.h>
#  include <string>
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

    /** Modo edicion: los dedos mueven los controles en vez de usarlos. */
    inline bool IsEditing() const { return editing; }
    void ToggleEditing();

    /** Devuelve la disposicion a los valores por defecto. */
    void ResetLayout();

    /**
     * Avisa de una pulsacion en la zona del boton DEBUG. Se consulta desde Update, que es
     * quien reparte los dedos.
     */
    inline bool ConsumeDebugRequest() {
        const bool r = debug_requested;
        debug_requested = false;
        return r;
    }

    /**
     * true si el punto (en pixeles de ventana) cae sobre el boton DBG.
     *
     * Lo usa el bucle principal para abrir el panel de trucos: ese boton no escribe ninguna
     * tecla, solo avisa, y conviene saberlo antes de repartir los dedos.
     */
    bool HitDebugButton(float px, float py, const SDL_Rect &screen_rect) const;

    /**
     * Dibuja una forma generica (la usa el panel de trucos para sus botones): vertices en
     * coordenadas de ventana con la proyeccion de pantalla.
     */
    void DrawQuad(float x, float y, float w, float h, std::uint32_t argb);
    void DrawDisc(float cx, float cy, float rx, float ry, std::uint32_t argb);

  private:
    /// Controles de accion (los que se pueden pulsar).
    enum ActionId {
        ACT_JUMP = 0,
        ACT_FIRE,
        ACT_BLITZ,
        ACT_POWERLINE,
        ACT_GRENADE,
        ACT_SMARTBOMB,
        ACT_WEAPON,
        ACT_ESCAPE,
        ACT_DEBUG,
        ACT_ZR,
        ACT_MINUS,
        ACT_PLUS,
        ACT_COUNT
    };

    struct ActionButton {
        float x, y;          ///< centro, normalizado (0..1) respecto a la ventana completa
        float r;             ///< radio, normalizado respecto a la ALTURA de la pantalla
        int scancode;        ///< tecla que se pulsa al tocarlo
        bool is_toggle;      ///< no escribe tecla cada frame, solo avisa al pulsar
        bool pressed;
        int finger;          ///< dedo que lo esta pulsando (-1 = ninguno)
    };

    void ClearKeys();
    void ComputePixels(const SDL_Rect &screen_rect, float &scale);
    void UpdateJoystick();
    void UpdateEditing(const SDL_TouchFingerEvent &finger, const SDL_Rect &screen_rect, int win_w,
                       int win_h);
    void ApplyLayoutPersist();

    bool available = false;  ///< el dispositivo tiene pantalla tactil
    bool enabled = true;     ///< el jugador quiere el mando virtual
    bool active = false;     ///< available && enabled
    bool editing = false;    ///< modo edicion (arrastrar controles)
    bool debug_requested = false;

    int window_w = 0;
    int window_h = 0;

    /// Joystick flotante de movimiento.
    bool stick_active = false;
    SDL_FingerID stick_finger = -1;
    float stick_origin_x = 0.0f;  ///< centro cuando se toco (pixeles)
    float stick_origin_y = 0.0f;
    float stick_x = 0.0f;  ///< desplazamiento actual del pomo (pixeles)
    float stick_y = 0.0f;
    float stick_radius = 0.0f;  ///< radio en pixeles (depende de la pantalla)
    bool dpad_pressed[4] = {false, false, false, false};
    bool right_stick_active = false;
    float right_stick_x = 0.0f;
    float right_stick_y = 0.0f;
    /// Zona donde puede nacer el joystick, normalizada.
    float stick_zone_w = 0.43f;
    float stick_zone_h = 0.78f;

    ActionButton actions[ACT_COUNT];
    std::vector<int> pressed_scancodes;

    /// Control que se esta arrastrando en modo edicion.
    int drag_action = -1;
    bool drag_stick = false;
    float drag_off_x = 0.0f;
    float drag_off_y = 0.0f;
    /// Ultima posicion conocida de cada dedo, para detectar arrastres.
    struct FingerState {
        SDL_FingerID id;
        float x, y;
    };
    std::vector<FingerState> fingers;

    /// Posicion por defecto del joystick (centro), normalizada.
    float stick_default_x = 0.25f;
    float stick_default_y = 0.66f;
};

extern TouchControlsClass TouchControls;

#endif /* ANDROID */

#endif /* _TOUCHCONTROLS_HPP_ */
