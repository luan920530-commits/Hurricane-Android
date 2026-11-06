/**
 * Panel de trucos y depuracion (widget en pantalla) para Android.
 *
 * Se abre con el boton DBG del mando virtual. Vease DebugPanel.cpp.
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#ifndef _DEBUGPANEL_HPP_
#define _DEBUGPANEL_HPP_

#if defined(ANDROID)

#  include <SDL.h>

class DebugPanelClass {
  public:
    /// Abre o cierra el panel.
    void Toggle();
    void SetOpen(bool open);
    inline bool IsOpen() const { return open_; }

    /**
     * Reparte un dedo sobre el panel. Debe llamarse ANTES que el mando virtual: si el panel
     * esta abierto se queda con el toque y el mando no responde.
     *
     * @param nx,ny  posicion del dedo normalizada (0..1) dentro de la ventana
     * @param down   true al tocar, false al levantar
     * @param screen_rect area de juego (el panel se centra ahi)
     * @return true si el toque lo ha consumido el panel
     */
    bool HandleTouch(float nx, float ny, bool down, const SDL_Rect &screen_rect);

    /// Dibuja el panel encima del juego (llamar despues de presentar el frame).
    void Draw(const SDL_Rect &screen_rect);

    /**
     * Reaplica los trucos que el motor va recortando (vidas, granadas...). Se llama una vez
     * por frame del juego; es barato y no hace nada si no hay trucos activos.
     */
    void ApplyFrameCheats();

    // --- Consultas para el resto del motor -------------------------------------------
    static bool InfiniteLives();
    static bool InfiniteAmmo();
    static bool MaxWeapons();
    static bool IsGodMode();
    /// true si hay algun truco que haya que reaplicar cada frame.
    static bool AnyActive();

    void SetGodMode(bool enabled);

  private:
    bool PanelRect(const SDL_Rect &screen_rect, SDL_FRect &out) const;
    void ApplyGodMode();

    bool open_ = false;

    // Trucos. static para poderlos consultar desde el motor sin pasar el objeto.
    static inline bool InfiniteLives_ = false;
    static inline bool InfiniteAmmo_ = false;
    static inline bool MaxWeapons_ = false;
    static inline bool GodMode_ = false;
};

extern DebugPanelClass DebugPanel;

#endif /* ANDROID */

#endif /* _DEBUGPANEL_HPP_ */
