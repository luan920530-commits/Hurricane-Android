/** Mando tactil Android: acciones independientes de la configuracion del teclado. */

#ifndef _TOUCHCONTROLS_HPP_
#define _TOUCHCONTROLS_HPP_

#include "DX8Input.hpp"

#if defined(ANDROID)

#  include <SDL.h>
#  include <string>
#  include <vector>
#  include "TouchInput.hpp"

/**
 * Mando virtual. Una sola instancia global, declarada al final de este fichero.
 */
class TouchControlsClass {
  public:
    void Init();                     ///< Lee la deteccion de pantalla tactil y ajustes
    void Shutdown();                 ///< Libera el estado
    void Update(const SDL_Rect &screen_rect, int window_w, int window_h);
    void DrawGeometry(const SDL_Rect &screen_rect);
    void DrawLabel(const char *text, float cx, float cy, float max_width, float height);
    void HandleFingerEvent(const SDL_TouchFingerEvent &finger, const SDL_Rect &screen_rect, int win_w, int win_h);
    void CancelTouches();
    void ResetTouches();
    bool PlayerAction(int action) const;

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
        ACT_COUNT
    };

    struct ActionButton {
        float x, y;          ///< centro, normalizado (0..1) dentro del area de juego
        float r;             ///< radio, normalizado respecto a la ALTURA de la pantalla
        bool is_toggle;      ///< no escribe tecla cada frame, solo avisa al pulsar
        bool pressed;
        const char *label;   ///< texto corto que se dibuja dentro
    };

    void ClearKeys();
    void UpdateEditing(const SDL_TouchFingerEvent &finger, const SDL_Rect &screen_rect, int win_w,
                       int win_h);

    bool available = false;  ///< el dispositivo tiene pantalla tactil
    bool enabled = true;     ///< el jugador quiere el mando virtual
    bool active = false;     ///< available && enabled
    bool editing = false;    ///< modo edicion (arrastrar controles)
    bool wait_for_release = false;
    std::vector<TouchInput::Finger> contacts; // SDL events; normalized to the surface

    /// Joystick flotante de movimiento.
    bool stick_active = false;
    TouchInput::Stick stick;
    float stick_origin_x = 0.0f;  ///< centro cuando se toco (pixeles)
    float stick_origin_y = 0.0f;
    float stick_x = 0.0f;  ///< desplazamiento actual del pomo (pixeles)
    float stick_y = 0.0f;
    float stick_radius = 0.0f;  ///< radio en pixeles (depende de la pantalla)

    ActionButton actions[ACT_COUNT];
    bool pending[ACT_COUNT] = {};
    bool directions[4] = {};

    /// Control que se esta arrastrando en modo edicion.
    SDL_FingerID drag_finger = 0;
    SDL_TouchID drag_device = 0;
    bool dragging = false;
    int drag_action = -1;
    bool drag_stick = false;
    float drag_off_x = 0.0f;
    float drag_off_y = 0.0f;
    /// Posicion por defecto del joystick (centro), normalizada.
    float stick_default_x = 0.17f;
    float stick_default_y = 0.74f;
};

extern TouchControlsClass TouchControls;

#endif /* ANDROID */

#endif /* _TOUCHCONTROLS_HPP_ */
