// Datei : DX8Input.hpp

// --------------------------------------------------------------------------------------
//
// DirectInput Klasse
// zum Erfassen von Eingaben durch Tastaur, Maus und Joystick/Joypad
//
// (c) 2002 Jörg M. Winterstein
//
// --------------------------------------------------------------------------------------

#ifndef _DX8INPUT_HPP_
#define _DX8INPUT_HPP_

// --------------------------------------------------------------------------------------
// Include Dateien
// --------------------------------------------------------------------------------------

#include "DX8Joystick.hpp"

#include <bitset>

// --------------------------------------------------------------------------------------
// Defines
// --------------------------------------------------------------------------------------

constexpr int MAX_MOUSEBUTTONS = 4;
constexpr int DINPUT_BUFFERSIZE = 32;
constexpr int MAX_KEYS = 256;

#if SDL_VERSION_ATLEAST(2, 0, 0)
/*
 * "Sin tecla asignada" se representa con 0 en AktionKeyboard (ver CreateDefaultControlsConfig:
 * las teclas de seleccion directa de arma no tienen tecla por defecto).
 *
 * Ojo: SDL_GetScancodeFromKey(0) devuelve SDL_SCANCODE_UNKNOWN, que es 0, y TastaturPuffer[0]
 * es una posicion real de la tabla de teclado (en Android corresponde a la tecla A de un
 * teclado fisico o a eventos sinteticos). Sin esta comprobacion, una accion sin asignar se
 * activaba sola al pulsar esa tecla.
 */
#  define KeyDown(Taste) ((Taste) != 0 && TastaturPuffer[SDL_GetScancodeFromKey(Taste)] > 0)
#  define KeyCode(Taste) (SDL_GetKeyFromScancode(static_cast<SDL_Scancode >(i)))
#else
#  define KeyDown(Taste) ((Taste) != 0 && TastaturPuffer[Taste] > 0)
#  define KeyCode(Taste) (Taste)
#endif

// ForceFeedback Effekte
//
enum {
  FFE_SMALLRUMBLE = 0,
  FFE_MEDIUMRUMBLE = 1,
  FFE_BIGRUMBLE = 2,
  FFE_BLITZ = 3
};

constexpr int MAX_JOYSTICKS = 4;
// DKS - Fixed ambiguity: The game had ControlType and JoystickMode both using this define, but
//      that was confusing and mixed up in the code:  Now, JoystickMode uses this define and
//      ControlType uses the one following it:
constexpr int JOYMODE_JOYPAD = 1;    // NOTE: DO NOT change the values 1 & 2 here, Texts.cpp counts on these values.
constexpr int JOYMODE_JOYSTICK = 2;  // There originally was a 0 but it was removed for reasons in comment above.

constexpr int CONTROLTYPE_KEYBOARD = 0;
constexpr int CONTROLTYPE_JOY = 1;

// --------------------------------------------------------------------------------------
// Klassendeklaration
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// DirectInput Klasse
// --------------------------------------------------------------------------------------

class DirectInputClass {
  private:
#if defined(GCW)
    int InternalJoystickIndex;  // If the system has built-in joystick controls,
#endif                          // GCW
                                // this is their index, -1 otherwise.
  public:
    int JoysticksFound;
    DirectJoystickClass Joysticks[MAX_JOYSTICKS];

#if 0
    int MausX;  // Koordinaten der
    int MausY;  // der Maus und ihre Buttons
    std::bitset<MAX_MOUSEBUTTONS> MausButtons;
#endif

    DirectInputClass();   // Konstruktor
    ~DirectInputClass();  // Desktruktor

    bool Init();                      // DirectInput initialisieren
    void Exit();                      // DirectInput beenden
    bool UpdateTastatur();            // Tasten abfragen und im Puffer speichern
#if 0
    bool UpdateMaus(bool gepuffert);  // Maus abfragen gepuffert/ungepuffert
    void AcquireKeyboard();           // Keyboard akquirierens
#endif
    bool AreAllKeysReleased() const;  // Checken ob keine Taste mehr gedrückt ist
    bool AnyKeyDown() const;
    bool AnyButtonDown() const;
    void UpdateJoysticks();
    char *MapButtonToString(int joy_idx, int button);  // Returns char string "1" for button 0, etc..
                                                       // But when used on GCW Zero's internal controls
                                                       // (or other special cases) will return the custom
                                                       // string for the devices's buttons ("X", "Select", etc)

#if defined(GCW)
    // If the system has built-in joystick controls, this is their index. 0 is default.
    // On embedded systems like GCW Zero, this is set to the device's internal controls
    // so that a player is never 'locked out' and also so the game is not dependent on
    // the system always setting the internal controls to device index 0. Likely useful on other devices, too. -DKS
    int GetInternalJoystickIndex() { return InternalJoystickIndex; }

    // On GCW Zero, Main menu button is permanently mapped to START (button 5):
    int GetInternalJoystickMainMenuButton() { return 5; }

    bool InternalJoystickMainMenuButtonDown();
#endif  // GCW
    int NumberOfKeys;
#if defined(ANDROID)
    /*
     * En Android la entrada del jugador viene del mando virtual, no de una medida
     * const Uint8* del teclado. Se mantiene un buffer propio: cada frame se copia el estado
     * real del teclado (por si hay un gamepad o teclado USB) y encima se marcan las teclas
     * del mando virtual. TastaturPuffer apunta a este buffer para que todo el juego
     * (KeyDown, menus, etc.) siga funcionando sin cambios.
     */
    Uint8 TouchKeyboard[MAX_KEYS];
#endif
};

// --------------------------------------------------------------------------------------
// Externals
// --------------------------------------------------------------------------------------

extern DirectInputClass DirectInput;
extern const Uint8 *TastaturPuffer;
extern bool UseForceFeedback;  // ForceFeedback Fähigkeit nutzen ?

#endif
