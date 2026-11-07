// Datei : Main.cpp

// --------------------------------------------------------------------------------------
//
// Hurrican
//
// Shoot em up ala Turrican
// benutzt die DirectX8.1 API für Grafik, Sound und Input
//
// (c) 2002 Jörg M. Winterstein
//
// --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------
// Defines
// --------------------------------------------------------------------------------------

#ifdef NDEBUG
#  define SHOW_CRACKTRO
#endif

// --------------------------------------------------------------------------------------
// Includes
// --------------------------------------------------------------------------------------

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;

#include <iomanip>
#include <SDL.h>
#include "CCracktro.hpp"
#include "Console.hpp"
#include "DX8Sound.hpp"
#include "DX8Texture.hpp"
#include "GUISystem.hpp"
#include "Gameplay.hpp"
#include "HUD.hpp"
#include "Partikelsystem.hpp"
#include "Projectiles.hpp"
#if defined(ANDROID)
#  include "DX8Graphics.hpp"
#  include "SDLPort/AndroidError.hpp"
#  include "SDLPort/DebugPanel.hpp"
#  include "SDLPort/AndroidPort.hpp"
#  include "SDLPort/TouchControls.hpp"
#endif

// Memory Leaks

//#include <stdlib.h>
//#include <crtdbg.h>

// --------------------------------------------------------------------------------------
// externe Variablen
// --------------------------------------------------------------------------------------

extern bool DEMORecording;
extern bool DEMOPlaying;
extern DirectGraphicsSprite PartikelGrafix[MAX_PARTIKELGFX];  // Grafiken der Partikel

// --------------------------------------------------------------------------------------
// globale Variablen
// --------------------------------------------------------------------------------------

bool FixedFramerate = false;  // true = Spiel mit 50 Frames laufen lassen
                              // false = Spiel so flüssig wie möglich laufen lassen
bool GameRunning = true;         // Spiel läuft :-)
bool GamePaused = false;         // Spiel eingefroren (wenn man zb das Fenster verlässt)
bool NochKeinFullScreen = true;  // Logo noch anzeigen in Paint ?
#ifndef NDEBUG
bool DebugMode = false;              // Debug Mode ein/aus
#endif                               //NDEBUG
TexturesystemClass Textures;         // DKS - Added Texturesystem class (see DX8Sprite.cpp)
DirectGraphicsClass DirectGraphics;  // Grafik-Objekt
DirectInputClass DirectInput;        // Input-Objekt
TimerClass Timer;                    // Timer Klasse für die Framerate
#if defined(__AROS__)
Logdatei Protokoll("T:Game_Log.txt");  // Protokoll Datei
#else
Logdatei Protokoll("Game_Log.txt");  // Protokoll Datei
#endif
SoundManagerClass SoundManager;  // Sound Manager
DirectGraphicsFont *pDefaultFont = new (DirectGraphicsFont);
DirectGraphicsFont *pMenuFont = new (DirectGraphicsFont);
TileEngineClass TileEngine;          // Tile Engine
PartikelsystemClass PartikelSystem;  // Das coole Partikelsystem
ProjectileListClass Projectiles;     // Liste mit Schüssen
GegnerListClass Gegner;              // Liste mit Gegner
IntroClass *pIntro;                  // Intro-Objekt
OuttroClass *pOuttro;                // Outtro-Objekt
MenuClass *pMenu = nullptr;          // Hauptmenu-Objekt
ConsoleClass Console;                // Konsolen-Objekt
CGUISystem GUI;                      // GUI System
CCracktro *Cracktro;

std::string g_storage_ext;      // Where data files (levels, graphics, music, etc)
                                //      for the game are stored (read)
std::string g_config_ext;       // Where configuration files
                                //      are written (-DKS) (write)
std::string g_save_ext;         // Where high scores and save games
                                //      are written (-DKS) (write)

sCommandLineParams CommandLineParams;

// --------------------------------------------------------------------------------------
// Variablen für den Spielablauf
// --------------------------------------------------------------------------------------

// DKS - PlayerClass array is now static, not dynamically-allocated:
// PlayerClass				*pPlayer[2];					// Werte der Spieler
PlayerClass Player[2];  // Werte der Spieler

HUDClass HUD;                           // Das HUD
GameStateEnum SpielZustand = GameStateEnum::CRACKTRO;  // Aktueller Zustand des Spieles

void FillCommandLineParams(int argc, char *args[]) {

    // Fullscreen en Android pero conservando la proporción original 4:3 del juego.
    // El overlay táctil sí ocupa toda la ventana y queda disponible en los laterales.
    CommandLineParams.RunWindowMode = ScreenMode::FULLSCREEN;
    CommandLineParams.Scanlines = false;
    CommandLineParams.ColorBleed = false;
    CommandLineParams.ScreenCurvature = false;
    CommandLineParams.ScreenNoise = false;
    CommandLineParams.TexFactor = 1;
    CommandLineParams.TexSizeMin = 1024;
    CommandLineParams.ScreenDepth = DEFAULT_SCREENBPP;
    CommandLineParams.VSync = true;
    CommandLineParams.ShowFPS = false;
    CommandLineParams.AllowNPotTextureSizes = false;
    CommandLineParams.LowRes = false;
    CommandLineParams.DataPath = nullptr;
    CommandLineParams.SavePath = nullptr;
    CommandLineParams.Arcade = false;

    for (int i = 1; i < argc; i++) {
        if ((strstr(args[i], "--help") != nullptr) || (strstr(args[i], "-?") != nullptr) ||
            (strstr(args[i], "-H") != nullptr) || (strstr(args[i], "-h") != nullptr)) {
            Protokoll << "Hurrican\n";
            Protokoll << "  Usage      : hurrican <arguments>\n";
            Protokoll << "  Arguments\n";
            Protokoll << "  -H,-?, --help           : Show this information\n";
            Protokoll << "  -W,    --windowmode     : Run in a window, not fullsreen\n";
            Protokoll << "  -S,    --stretch        : Run fullsreen stretched to fill the whole area\n";
            Protokoll << "  -F,    --showfps        : Show the current frames per second\n";
            Protokoll << "  -D x,  --depth x        : Set screen pixel depth to x (16, 24, 32)\n";
            Protokoll << "                            ( Default is " << DEFAULT_SCREENBPP << " )\n";
            Protokoll << "  -L,    --lowres         : Use " << LOWRES_SCREENWIDTH << "x" << LOWRES_SCREENHEIGHT
                      << " low-resolution screen dimensions\n";
            Protokoll << "  -NV,   --novsync        : Disable VSync / double-buffering\n";
            Protokoll << "  -NP,   --nonpot         : Allow non-power-of-two texture sizes\n";
            Protokoll << "                            Normally, GPUs require texture dimensions that are\n";
            Protokoll << "                            powers of two. If your GPU does not require that,\n";
            Protokoll << "                            you can reduce VRAM usage with this switch.\n";
            Protokoll << "  -TF x, --texfactor x    : Division factor for textures\n";
            Protokoll << "                            Valid values: 1, 2, 4\n";
            Protokoll << "                            If set to 2, textures dimensions will be halved.\n";
            Protokoll << "                            If set to 4, textures dimensions will be quartered.\n";
            Protokoll << "                            ( Default is 1 (no resizing) )\n";
            Protokoll << "  -TS x, --texsizemin x   : Size limitation for texture division factor\n";
            Protokoll << "                            Only textures with widths or heights above this\n";
            Protokoll << "                            value will be resized. MIN: 16  MAX: 1024\n";
            Protokoll << "                            ( Default is 1024 )\n";
            Protokoll << "  -PD x, --pathdata x     : Look in this path for the game's read-only data\n";
            Protokoll << "                            i.e. music, sound, graphics, levels, etc.\n";
            Protokoll << "  -PS x, --pathsave x     : Use this path for the game's save data\n";
            Protokoll << "                            i.e. save-games, settings, high-scores, etc.\n";
            Protokoll << "  -C,    --crt            : Simulate all CRT effects (except noise) for a retro look\n";
            Protokoll << "         --scanlines      : CRT effects: enable scanlines\n";
            Protokoll << "         --colorbleed     : CRT effects: enable color bleeding\n";
            Protokoll << "         --screencurve    : CRT effects: enable screen curvature\n";
            Protokoll << "         --screennoise    : CRT effects: enable screen noise\n";
            Protokoll << "         --custom x       : Play custom userlevel\n";
            Protokoll << "         --level x        : Load selected level map\n";
            Protokoll << "         --arcade         : Enable arcade mode\n";
            Protokoll << std::endl;
            exit(EXIT_SUCCESS);
        } else if ((strstr(args[i], "--windowmode") != nullptr) || (strstr(args[i], "-W") != nullptr)) {
            if (CommandLineParams.RunWindowMode == ScreenMode::FULLSCREEN_STRETCHED) {
                Protokoll << "Stretch mode selected, ignoring windowmode" << std::endl;
            } else {
                CommandLineParams.RunWindowMode = ScreenMode::WINDOW;
                Protokoll << "Window mode is enabled" << std::endl;
            }
        } else if ((strstr(args[i], "--stretch") != nullptr) || (strstr(args[i], "-S") != nullptr)) {
            if (CommandLineParams.RunWindowMode == ScreenMode::WINDOW) {
                Protokoll << "Window mode selected, ignoring stretch mode" << std::endl;
            } else {
                CommandLineParams.RunWindowMode = ScreenMode::FULLSCREEN_STRETCHED;
                Protokoll << "Stretched fullscreen mode is enabled" << std::endl;
            }
        } else if ((strstr(args[i], "--showfps") != nullptr) || (strstr(args[i], "-F") != nullptr)) {
            CommandLineParams.ShowFPS = true;
            Protokoll << "FPS will be displayed" << std::endl;
        } else if ((strstr(args[i], "--depth") != nullptr) || (strstr(args[i], "-D") != nullptr)) {
            i++;
            if (i < argc) {
                CommandLineParams.ScreenDepth = std::clamp(atoi(args[i]), 16, 32);
                if (CommandLineParams.ScreenDepth >= 32)
                    CommandLineParams.ScreenDepth = 32;
                else if (CommandLineParams.ScreenDepth > 24 && CommandLineParams.ScreenDepth < 32)
                    CommandLineParams.ScreenDepth = 24;
                else if (CommandLineParams.ScreenDepth > 16 && CommandLineParams.ScreenDepth < 24)
                    CommandLineParams.ScreenDepth = 16;
                Protokoll << "Screen depth (bpp) requested is " << CommandLineParams.ScreenDepth << std::endl;
            }
        } else if ((strstr(args[i], "--lowres") != nullptr) || (strstr(args[i], "-L") != nullptr)) {
            Protokoll << "Low-resolution 320x240 screen dimensions are requested" << std::endl;
            CommandLineParams.LowRes = true;
        } else if ((strstr(args[i], "--novsync") != nullptr) || (strstr(args[i], "-NV") != nullptr)) {
            Protokoll << "VSync / double-buffering will be disabled, if supported" << std::endl;
            CommandLineParams.VSync = false;
        } else if ((strstr(args[i], "--nonpot") != nullptr) || (strstr(args[i], "-NP") != nullptr)) {
            Protokoll << "Non-power-of-two textures are allowed" << std::endl;
            CommandLineParams.AllowNPotTextureSizes = true;
        } else if ((strstr(args[i], "--texfactor") != nullptr) || (strstr(args[i], "-TF") != nullptr)) {
            i++;
            if (i < argc) {
                CommandLineParams.TexFactor = std::clamp(atoi(args[i]), 1, 4);
                if (CommandLineParams.TexFactor == 3)
                    CommandLineParams.TexFactor = 4;
                Protokoll << "Texfactor set to " << CommandLineParams.TexFactor << std::endl;
            }
        } else if ((strstr(args[i], "--texsizemin") != nullptr) || (strstr(args[i], "-TS") != nullptr)) {
            i++;
            if (i < argc) {
                CommandLineParams.TexSizeMin = std::clamp(atoi(args[i]), 16, 1024);
                Protokoll << "Texsizemin set to " << CommandLineParams.TexSizeMin << std::endl;
            }
        } else if ((strstr(args[i], "--pathdata") != nullptr) || (strstr(args[i], "-PD") != nullptr)) {
            i++;
            if (i < argc) {
                if (args[i] && strlen(args[i]) > 0 && !CommandLineParams.DataPath) {
                    CommandLineParams.DataPath = static_cast<char *>(malloc(strlen(args[i]) + 1));
                    strcpy(CommandLineParams.DataPath, args[i]);
                    if (fs::is_directory(CommandLineParams.DataPath)) {
                        Protokoll << "Data path set to " << CommandLineParams.DataPath << std::endl;
                    } else {
                        Protokoll << "ERROR: could not find data path " << CommandLineParams.DataPath << std::endl;
                        free(CommandLineParams.DataPath);
                        CommandLineParams.DataPath = nullptr;
                    }
                }
            }
        } else if ((strstr(args[i], "--pathsave") != nullptr) || (strstr(args[i], "-PS") != nullptr)) {
            i++;
            if (i < argc) {
                if (args[i] && strlen(args[i]) > 0 && !CommandLineParams.SavePath) {
                    CommandLineParams.SavePath = static_cast<char *>(malloc(strlen(args[i]) + 1));
                    strcpy(CommandLineParams.SavePath, args[i]);
                    if (fs::is_directory(CommandLineParams.SavePath)) {
                        Protokoll << "Save path set to " << CommandLineParams.SavePath << std::endl;
                    } else {
                        Protokoll << "ERROR: could not find save path " << CommandLineParams.SavePath << std::endl;
                        free(CommandLineParams.SavePath);
                        CommandLineParams.SavePath = nullptr;
                    }
                }
            }
        } else if ((strstr(args[i], "--crt") != nullptr) || (strstr(args[i], "-C") != nullptr)) {
            Protokoll << "CRT emulation enabled" << std::endl;
            CommandLineParams.Scanlines = true;
            CommandLineParams.ColorBleed = true;
            CommandLineParams.ScreenCurvature = true;
        } else if (strstr(args[i], "--scanlines") != nullptr) {
            Protokoll << "CRT scanlines enabled" << std::endl;
            CommandLineParams.Scanlines = true;
        } else if (strstr(args[i], "--colorbleed") != nullptr) {
            Protokoll << "CRT color bleeding enabled" << std::endl;
            CommandLineParams.ColorBleed = true;
        } else if (strstr(args[i], "--screencurve") != nullptr) {
            Protokoll << "CRT screen curvature enabled" << std::endl;
            CommandLineParams.ScreenCurvature = true;
        } else if (strstr(args[i], "--screennoise") != nullptr) {
            Protokoll << "CRT screen noise enabled" << std::endl;
            CommandLineParams.ScreenNoise = true;
        } else if (strstr(args[i], "--custom") != nullptr) {
            i++;
            if (i < argc && strlen(args[i]) < 256) {
                strcpy(CommandLineParams.OwnLevelList, args[i]);
                CommandLineParams.RunOwnLevelList = true;
            }
        } else if (strstr(args[i], "--level") != nullptr) {
            // own single level?
            i++;
            if (i < argc && strlen(args[i]) < 256) {
                strcpy(CommandLineParams.UserLevelName, args[i]);
                CommandLineParams.RunUserLevel = true;
            }
        } else if (strstr(args[i], "--arcade") != nullptr) {
            CommandLineParams.Arcade = true;
            Protokoll << "Arcade mode enabled" << std::endl;
        }
        else Protokoll << "Uknonwn: " << args[i] << std::endl;
    }
}

// --------------------------------------------------------------------------------------
// XDG Functions
// --------------------------------------------------------------------------------------
#ifdef USE_HOME_DIR
std::string createDir(const char *path, const char *subdir) {

    std::string dir(path);
    dir.append(subdir);
    bool success = fs::is_directory(dir) || fs::create_directories(dir);
    if (!success) {
        Protokoll << "ERROR: unable to create or access directory." << std::endl;
        Protokoll << "\tFull path that was tried: " << dir << std::endl;
        dir.clear();
    }

    return dir;
}

std::string getXdgDir(const char *xdgVar, const char *fallback) {

    std::string xdg_ext;
    char *xdgdir = getenv(xdgVar);
    if (xdgdir) {
        xdg_ext = createDir(xdgdir, "/hurrican");
    }
    if (xdg_ext.empty()) {
        char *homedir = getenv("HOME");
        if (homedir) {
            xdg_ext = createDir(homedir, fallback);
        } else {
            Protokoll << "ERROR: unable to find $HOME environment variable" << std::endl;
        }
    }

    if (xdg_ext.empty()) {
        Protokoll << "\tUsing '.' folder as fallback." << std::endl;
        xdg_ext.assign(".");
    }

    return xdg_ext;
}
#endif
// --------------------------------------------------------------------------------------
// Win-Main Funktion
// --------------------------------------------------------------------------------------
#if defined(ANDROID)
/*
 * Android: SDL no llama a main(), sino que busca el simbolo "SDL_main" dentro de libmain.so
 * con dlsym() (SDL_android.c, nativeRunMain). SDL.h ya declara ese prototipo y define
 * "#define main SDL_main", asi que aqui NO se puede deshacer el define: hay que exportar
 * exactamente SDL_main o la app se cierra nada mas abrirse con
 * "Couldn't find function SDL_main".
 */
int SDL_main(int argc, char *argv[]) {
  // (SDL.h exporta este prototipo con DECLSPEC en Android; su main() se define mas abajo)
#else
#undef main
#define main main
int main(int argc, char *argv[]) {
#endif
    GamePaused = false;

    FillCommandLineParams(argc, argv);

    // Set game's data path:
    // First, see if a command line parameter was passed:
    if (CommandLineParams.DataPath) {
        g_storage_ext = CommandLineParams.DataPath;
        free(CommandLineParams.DataPath);
        CommandLineParams.DataPath = nullptr;
    } else {
#if defined(ANDROID)
        /*
         * Android: los datos viven en los assets del APK, que no se pueden leer con rutas
         * normales. Android_InitStorage() los copia al almacenamiento interno la primera vez
         * y devuelve ese directorio.
         */
        g_storage_ext = Android_BootstrapStorage();
        g_save_ext = Android_GetSavePath();
        g_config_ext = g_save_ext;
        if (g_storage_ext.empty() || g_save_ext.empty()) {
            Protokoll << "ERROR: no se pudo preparar el almacenamiento en Android" << std::endl;
            GameRunning = false;
            return 1;
        }
#else
#  ifdef USE_STORAGE_PATH
        // A data-files storage path has been specified in the Makefile:
        g_storage_ext = USE_STORAGE_PATH;
        // Attempt to locate the dir
        if (!fs::is_directory(g_storage_ext)) {
            // Failed, print message and use "." folder as fall-back
            Protokoll << "ERROR: Failed to locate data directory " << g_storage_ext << std::endl;
            Protokoll << "\tUsing '.' folder as fallback." << std::endl;
            g_storage_ext = ".";
        }
#  else
        g_storage_ext = ".";
#  endif
#endif  // ANDROID
    }

    // Set game's save path (save games, settings, logs, high-scores, etc)
    if (CommandLineParams.SavePath) {
        g_save_ext = CommandLineParams.SavePath;
        free(CommandLineParams.SavePath);
        CommandLineParams.SavePath = nullptr;
        g_config_ext = g_save_ext;
    } else {
#if defined(ANDROID)
        // Ya se ha resuelto junto con el directorio de datos (ver Android_InitStorage()).
#else
#  ifdef USE_HOME_DIR
        // Makefile is specifying this is a UNIX machine and we should write saves, settings, etc to $XDG_CONFIG_HOME/hurrican/ dir
        g_config_ext = getXdgDir("XDG_CONFIG_HOME", "/.config/hurrican");

        // Makefile is specifying this is a UNIX machine and we should write saves, settings, etc to $XDG_DATA_HOME/hurrican/ dir
        g_save_ext = getXdgDir("XDG_DATA_HOME", "/.local/share/hurrican");
#  else
        g_save_ext = ".";
        g_config_ext = g_save_ext;
#  endif  // USE_HOME_DIR
#endif  // ANDROID
    }

    Protokoll << "--> Using external config path '" << g_config_ext << "' <--" << std::endl;
    Protokoll << "--> Using external storage path '" << g_storage_ext << "' <--" << std::endl;
    Protokoll << "--> Using save path '" << g_save_ext << "' <--\n" << std::endl;

    //----- Spiel-Initialisierung

    if (!GameInit()) {
        Protokoll << "\n-> GameInit error!\n" << std::endl;
        GameRunning = false;
#if defined(ANDROID)
        /*
         * Sin esto la app se quedaba en un bucle vacio con pantalla negra y el jugador no
         * sabia que pasaba. Se muestra el motivo y se sale (SDL cierra la actividad).
         */
        Protokoll.flush();
        Android_ShowFatalError("Hurrican no pudo arrancar",
                               "Fallo durante la inicializacion del juego.\n"
                               "Suele ocurrir si los datos no se copiaron bien: "
                               "reinstalar la aplicacion lo soluciona.");
        return 1;
#endif
    } else {
        Protokoll << "\n-> GameInit successful!\n" << std::endl;
    }

#if defined(ANDROID)
    /*
     * Logo de arranque.
     *
     * Hasta que el motor dibuja su primer fotograma la superficie esta en negro, y la
     * inicializacion de Android (copia de datos la primera vez, compilacion de shaders,
     * carga de texturas) tarda varios segundos. El jugador solo veia una pantalla negra y,
     * si algo fallaba, un cierre. Este cartel aparece en cuanto hay contexto OpenGL y se
     * mantiene hasta que el juego pinta de verdad, para que se distinga "esta cargando" de
     * "se ha quedado colgado".
     */
    Android_ShowStartupScreen("Hurrican", "Cargando...");
#endif

    //----- Main-Loop

#if defined(ANDROID)
    // Marcada cuando Android destruye el contexto OpenGL; se repara fuera del bucle de
    // eventos para no recompilar shaders en mitad del procesado.
    bool context_lost = false;
    // true mientras el dedo que abrio/interactuo con el panel sigue en la pantalla, para que
    // ese mismo toque no llegue al mando virtual.
    bool DebugPanelOpenBlock = false;
#endif

    while (GameRunning) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    GameRunning = false;
                    break;
#if defined(ANDROID)
                /*
                 * Ciclo de vida de Android.
                 *
                 * Al salir de la app (boton inicio, llamada entrante, rotacion...) Android
                 * puede destruir el contexto OpenGL y con el todas las texturas. Sin esto el
                 * juego volvia a un estado corrupto: pantalla negra o crash al reanudar.
                 *
                 *  - WILLENTERBACKGROUND: se congela el juego y se cortan musica y sonidos.
                 *  - DIDENTERFOREGROUND: hay que rehacer TODO el estado de GPU (texturas,
                 *    shaders, framebuffers) y volver a cargar el nivel en curso. Es lo mismo
                 *    que hace GameInit()/GameInit2() al arrancar, asi que se reutiliza la
                 *    misma via: terminar el bucle y reiniciar la partida no es opcion en
                 *    Android (el proceso sigue vivo), por eso se marca una bandera.
                 */
                case SDL_APP_WILLENTERBACKGROUND:
                    Protokoll << "Android: la app pasa a segundo plano, en pausa" << std::endl;
                    TouchControls.CancelTouches();
                    GamePaused = true;
                    SoundManager.PauseSongs();
                    SoundManager.PauseSounds();
                    break;
                case SDL_APP_DIDENTERFOREGROUND:
                    Protokoll << "Android: la app vuelve al primer plano" << std::endl;
                    SoundManager.UnpauseSongs();
                    SoundManager.UnpauseSounds();
                    GamePaused = false;
                    break;
                /*
                 * SDL solo envia SDL_RENDER_DEVICE_RESET cuando el contexto EGL se perdio
                 * de verdad (no en una pausa normal, donde el contexto se conserva). Es la
                 * senal fiable de que hay que rehacer shaders, FBO y texturas.
                 */
                case SDL_RENDER_DEVICE_RESET:
                    Protokoll << "Android: contexto grafico perdido, reconstruyendo" << std::endl;
                    context_lost = true;
                    break;
#endif
#if defined(ANDROID)
                /*
                 * Enrutar el ciclo completo de cada dedo (bajada, movimiento y subida).
                 * El panel de depuracion tiene prioridad y consume los toques mientras esta
                 * abierto; los demas se entregan al mando multitactil.
                 */
                case SDL_FINGERDOWN: {
                    if (DebugPanel.IsOpen()) {
                        DebugPanel.HandleTouch(event.tfinger.x, event.tfinger.y, true,
                                               DirectGraphics.GetRenderRect());
                        DebugPanelOpenBlock = true;
                        TouchControls.CancelTouches();
                    } else {
                        const float px = event.tfinger.x * static_cast<float>(DirectGraphics.GetWindowWidth());
                        const float py = event.tfinger.y * static_cast<float>(DirectGraphics.GetWindowHeight());
                        const SDL_Rect touch_area{0, 0, DirectGraphics.GetWindowWidth(),
                                                  DirectGraphics.GetWindowHeight()};
                        if (TouchControls.HitDebugButton(px, py, touch_area)) {
                            DebugPanel.SetOpen(true);
                            DebugPanelOpenBlock = true;
                            TouchControls.CancelTouches();
                        } else {
                            TouchControls.HandleTouchEvent(event.tfinger);
                        }
                    }
                    break;
                }
                case SDL_FINGERMOTION:
                    if (!DebugPanelOpenBlock && !DebugPanel.IsOpen())
                        TouchControls.HandleTouchEvent(event.tfinger);
                    break;
                case SDL_FINGERUP:
                    if (DebugPanelOpenBlock) {
                        if (DebugPanel.IsOpen())
                            DebugPanel.HandleTouch(event.tfinger.x, event.tfinger.y, false,
                                                   DirectGraphics.GetRenderRect());
                        DebugPanelOpenBlock = false;
                        TouchControls.CancelTouches();
                    } else {
                        TouchControls.HandleTouchEvent(event.tfinger);
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                        event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        Protokoll << "Android: nuevo tamano de ventana " << event.window.data1 << "x"
                                  << event.window.data2 << std::endl;
                        DirectGraphics.OnWindowResized(event.window.data1, event.window.data2);
                    }
                    break;
#endif
                case SDL_JOYDEVICEADDED:
                case SDL_JOYDEVICEREMOVED:
                    if (event.type == SDL_JOYDEVICEADDED)
                        Protokoll << "-> Device added!\n";
                    else
                        Protokoll << "-> Device removed!\n";
                    if (!DirectInput.Init()) {
                        Protokoll << "\n-> Input Initialization Failure ...!\n";
                    }
                    break;
                default:
                    break;
            }
        }

            // DKS - Exceptions can now be disabled, reducing unnecessary code-bloat:
#ifndef USE_NO_EXCEPTIONS
        try
#endif  // USE_NO_EXCEPTIONS
        {
#if defined(ANDROID)
            if (context_lost) {
                context_lost = false;
                if (!DirectGraphics.RecoverContext()) {
                    Protokoll << "ERROR: recuperacion grafica fallida, cerrando" << std::endl;
                    GameRunning = false;
                }
            }
#endif

            if (!GamePaused) {
                // Main Loop
                /*
                 * El valor de retorno importa: si GameInit2() o la carga del nivel fallan,
                 * Heartbeat() devuelve false. Antes se ignoraba y el bucle seguia llamando a
                 * la inicializacion una y otra vez: pantalla negra fija, sin mensaje y sin
                 * forma de saber que pasaba (la app parecia colgada y habia que cerrarla a
                 * mano). Ahora se avisa con el motivo.
                 */
                if (!Heartbeat()) {
                    Protokoll << "\n-> ERROR: fallo la inicializacion del juego\n" << std::endl;
                    GameRunning = false;
#if defined(ANDROID)
                    Protokoll.flush();
                    Android_ShowFatalError("Hurrican no pudo arrancar",
                                           "Fallo al inicializar el menu o el nivel.\n"
                                           "Las ultimas lineas del registro dicen por que.");
                    return 1;
#endif
                }

                // Eingabegeräte updaten
                DirectInput.UpdateTastatur();
                DirectInput.UpdateJoysticks();
                // DirectInput.UpdateMaus(false);

                // Soundchannels updaten
                SoundManager.Update();

                // Timer updaten
                Timer.update();

                // Feste Framerate ? (Spiel läuft in Zeitlupe, wenn zu langsam)
                //
                if (FixedFramerate) {
                    Timer.SetMaxFPS(60);
                    Timer.setSpeedFactor(1.0f / 60.0f * Timer.GetMoveSpeed());
                }
                Timer.wait();

                // Bei Demo immer gleichen Speedfaktor setzen
                //
                if (DEMORecording || DEMOPlaying)
                    Timer.setSpeedFactor(0.28f);
            }
        }
            // DKS - Exceptions can now be disabled, reducing unnecessary code-bloat:
#ifndef USE_NO_EXCEPTIONS
        catch (const char *str) {
            Protokoll << "Failure! Unhandled exception\n" << str << std::endl;
            GameRunning = false;
        }
        /*
         * El motor lanza std::exceptions en sitios puntuales (std::stoi al leer datos de un
         * nivel, fs::directory_iterator, etc.). Sin este catch, cualquiera de ellas termina
         * el proceso de golpe: en Android la app se cierra sin mensaje, que es justo el
         * sintoma que hay que evitar. Se registra el motivo y se deja que el bucle termine
         * de forma ordenada.
         */
        catch (const std::exception &e) {
            Protokoll << "Failure! std::exception: " << e.what() << std::endl;
            GameRunning = false;
        }
#endif  // USE_NO_EXCEPTIONS
    }

    //----- Spiel verlassen

    // Timer.WriteLogValues();

    if (!GameExit()) {
        Protokoll << "-> GameExit Fehler !" << std::endl;
        GameRunning = false;
    }

    Protokoll << "\n-> Hurrican closed !\n";
    Protokoll << "\nOriginal game: https://www.winterworks.de\n";
    Protokoll << "This fork: https://github.com/HurricanGame/Hurrican\n";
    Protokoll << "\n-> logfile end" << std::endl;

    return 0;
}

// --------------------------------------------------------------------------------------
// GameInit, initialisiert die DX Objekte
// --------------------------------------------------------------------------------------

bool GameInit() {
    options_Detail = DETAIL_LOW;

    SetRandomSeed(SDL_GetTicks());

    // DKS - Added language-translation files support to SDL port:
    std::string langfilepath(!g_storage_ext.empty() ? g_storage_ext : ".");
    langfilepath.append("/lang");
    FindLanguageFiles(langfilepath.c_str());

    // Try again if needed
    if (LanguageFiles.empty()) {
        langfilepath.assign("./");
        FindLanguageFiles(langfilepath.c_str());
    }

    // One more time if needed
    if (LanguageFiles.empty() && !g_storage_ext.empty()) {
        langfilepath.assign("./lang");
        FindLanguageFiles(langfilepath.c_str());
    }

    if (LanguageFiles.empty()) {
        Protokoll << "ERROR: Failed to find any language files, aborting." << std::endl;
        return false;
    }

    Protokoll << "\n>--------------------<\n";
    Protokoll << "| GameInit started   |\n";
    Protokoll << ">--------------------<" << std::endl;

    // Direct3D initialisieren
    if (!DirectGraphics.Init(RENDERWIDTH, RENDERHEIGHT, CommandLineParams.ScreenDepth, CommandLineParams.VSync)) {
        Protokoll << "\n-> Graphics Initialization Failure ...!" << std::endl;
        GameRunning = false;
        return false;
    }

    // DirectInput initialisieren
    if (!DirectInput.Init()) {
        Protokoll << "\n-> Input Initialization Failure ...!" << std::endl;
        GameRunning = false;
        return false;
    }

    // DKS - Read texture scale factor files
    Textures.ReadScaleFactorsFiles();

    // DKS - Sound manager is now a static global, and initialized with Init()
    // Sound Manager initialisieren
    // pSoundManager = new CSoundManager();
    SoundManager.Init();

    // Splash-Screen nicht mehr anzeigen
    NochKeinFullScreen = false;

#ifdef SHOW_CRACKTRO
    Cracktro = new CCracktro();
    SpielZustand = GameStateEnum::CRACKTRO;
#endif

    return true;
}

// --------------------------------------------------------------------------------------
// GameInit2, initialisiert den Rest nach dem Cracktro
// --------------------------------------------------------------------------------------

bool GameInit2() {
    // DKS-Player[] is a static global now:
    // Player initialisieren
    // Player[0] = new PlayerClass(0);
    // Player[1] = new PlayerClass(1);

    // DKS - Now that the player sprites are stored in the class, I've disabled this
    //      in favor of actual constructors:
    // Player[0]->SoundOff = 0;
    // Player[1]->SoundOff = 1;
    // memset(Player[0], 0, sizeof(*Player[0]));
    // memset(Player[1], 0, sizeof(*Player[1]));

    // Konfiguration laden
    if (!LoadConfig()) {
        Protokoll << "\n-> No config found. Creating default" << std::endl;
        CreateDefaultConfig();
    }

    // Menumusik laden und spielen
    SoundManager.LoadSong("menu.it", MUSIC::MENU);

    // DKS - Renamed:
    // SoundManager.ResetAllSongVolumes();
    SoundManager.ResetAllSoundVolumes();

    SoundManager.PlaySong(MUSIC::MENU, false);

    // Menu initialisieren
    // DKS - Resized menufont.png and added missing glyphs to make Swedish translation work:
    // pMenuFont->LoadFont("menufont.png", 448, 256, 28, 28, 16, 7);
    pMenuFont->LoadFont("menufont.png", 448, 336, 28, 28, 16, 12, menufont_charwidths);
    // Fonts laden
    pDefaultFont->LoadFont("smallfont.png", 320, 84, 10, 12, 32, 7, smallfont_charwidths);

    pMenu = new MenuClass();

    // DKS - Added support for font scaling
    if (CommandLineParams.LowRes) {
        pDefaultFont->SetScaleFactor(2);  // On lower res, draw smallest font twice as large so it appears 1:1
    }

    pMenu->StartProgressBar(522);

    // GUISystem initialiseren
    GUI.InitGUISystem();

    // DKS Load PartikelsystemClass sprites:
    PartikelSystem.LoadSprites();

    // GegnerListe initialisieren
    // DKS - Load GegnerListClass sprites:
    Gegner.LoadSprites();

    // Tileengine initialisieren
    TileEngine.LoadSprites();  // DKS - Added this function to TileEngineClass
    TileEngine.SetScrollSpeed(1.0f, 0.0f);

    // DKS Load projectile sprites:
    Projectiles.LoadSprites();

    // DKS - Load HUD sprites:
    HUD.LoadSprites();

    InitReplacers();

    // Sounds laden
    SoundManager.LoadWave("spreadshot.wav", SOUND::SPREADSHOT, false);
    SoundManager.LoadWave("lasershot.wav", SOUND::LASERSHOT, false);
    SoundManager.LoadWave("bounceshot.wav", SOUND::BOUNCESHOT, false);
    SoundManager.LoadWave("explosion1.wav", SOUND::EXPLOSION1, false);
    SoundManager.LoadWave("explosion2.wav", SOUND::EXPLOSION2, false);
    SoundManager.LoadWave("explosion3.wav", SOUND::EXPLOSION3, false);
    SoundManager.LoadWave("explosion4.wav", SOUND::EXPLOSION4, false);
    SoundManager.LoadWave("walkergiggle.wav", SOUND::WALKERGIGGLE, false);
    SoundManager.LoadWave("collect.wav", SOUND::COLLECT, false);
    SoundManager.LoadWave("hit.wav", SOUND::SPREADHIT, false);
    SoundManager.LoadWave("canon.wav", SOUND::CANON, false);
    SoundManager.LoadWave("click.wav", SOUND::CLICK, false);
    SoundManager.LoadWave("blitzstart.wav", SOUND::BLITZSTART, false);
    SoundManager.LoadWave("blitzende.wav", SOUND::BLITZENDE, false);
    SoundManager.LoadWave("blitz.wav", SOUND::BLITZ, true);
    SoundManager.LoadWave("blitzstart.wav", SOUND::BLITZSTART_P2, false);
    SoundManager.LoadWave("blitzende.wav", SOUND::BLITZENDE_P2, false);
    SoundManager.LoadWave("blitz.wav", SOUND::BLITZ_P2, true);
    SoundManager.LoadWave("powerline.wav", SOUND::POWERLINE, false);
    SoundManager.LoadWave("landen.wav", SOUND::LANDEN, false);
    SoundManager.LoadWave("waterin.wav", SOUND::WATERIN, false);
    SoundManager.LoadWave("waterout.wav", SOUND::WATEROUT, false);
    SoundManager.LoadWave("dive.wav", SOUND::DIVE, false);
    SoundManager.LoadWave("feuerfalle.wav", SOUND::FEUERFALLE, false);
    SoundManager.LoadWave("abzug.wav", SOUND::ABZUG, false);
    SoundManager.LoadWave("abzug.wav", SOUND::ABZUG_P2, false);
    SoundManager.LoadWave("funke.wav", SOUND::FUNKE, false);
    SoundManager.LoadWave("funke2.wav", SOUND::FUNKE2, false);
    SoundManager.LoadWave("funke3.wav", SOUND::FUNKE3, false);
    SoundManager.LoadWave("funke4.wav", SOUND::FUNKE4, false);
    SoundManager.LoadWave("granate.wav", SOUND::GRANATE, false);
    SoundManager.LoadWave("stonefall.wav", SOUND::STONEFALL, false);
    SoundManager.LoadWave("stoneexplode.wav", SOUND::STONEEXPLODE, false);
    SoundManager.LoadWave("rocket.wav", SOUND::ROCKET, false);
    SoundManager.LoadWave("presse.wav", SOUND::PRESSE, false);
    SoundManager.LoadWave("ammo.wav", SOUND::AMMO, false);
    SoundManager.LoadWave("kotzen.wav", SOUND::KOTZEN, false);
    SoundManager.LoadWave("made.wav", SOUND::MADE, false);
    SoundManager.LoadWave("droneshot.wav", SOUND::DRONE, false);
    SoundManager.LoadWave("waterdrop.wav", SOUND::DROP, false);
    SoundManager.LoadWave("thunder.wav", SOUND::THUNDER, false);
    SoundManager.LoadWave("upgrade.wav", SOUND::UPGRADE, false);
    SoundManager.LoadWave("column.wav", SOUND::COLUMN, false);
    SoundManager.LoadWave("door.wav", SOUND::DOOR, true);
    SoundManager.LoadWave("doorstop.wav", SOUND::DOORSTOP, false);
    SoundManager.LoadWave("switch.wav", SOUND::SWITCH, false);
    SoundManager.LoadWave("schleim.wav", SOUND::SCHLEIM, false);
    SoundManager.LoadWave("message.wav", SOUND::MESSAGE, false);
    SoundManager.LoadWave("beamload.wav", SOUND::BEAMLOAD, true);
    SoundManager.LoadWave("beamload2.wav", SOUND::BEAMLOAD2, true);
    SoundManager.LoadWave("beamload.wav", SOUND::BEAMLOAD_P2, true);
    SoundManager.LoadWave("beamload2.wav", SOUND::BEAMLOAD2_P2, true);

    // DKS - This was commented out in original code, but I've added support for
    //      Trigger_Stampfstein's chain retraction sound back in:
    SoundManager.LoadWave("chain.wav", SOUND::CHAIN, true);

    SoundManager.LoadWave("mushroomjump.wav", SOUND::MUSHROOMJUMP, false);
    SoundManager.LoadWave("golemload.wav", SOUND::GOLEMLOAD, false);
    SoundManager.LoadWave("golemshot.wav", SOUND::GOLEMSHOT, false);
    SoundManager.LoadWave("dampf.wav", SOUND::STEAM, false);
    SoundManager.LoadWave("dampf2.wav", SOUND::STEAM2, false);
    SoundManager.LoadWave("hit2.wav", SOUND::HIT, false);
    SoundManager.LoadWave("hit3.wav", SOUND::HIT2, false);
    SoundManager.LoadWave("spiderlila.wav", SOUND::LILA, false);
    SoundManager.LoadWave("fireball.wav", SOUND::FIREBALL, false);
    SoundManager.LoadWave("takeoff.wav", SOUND::TAKEOFF, false);
    SoundManager.LoadWave("laugh.wav", SOUND::LAUGH, false);
    SoundManager.LoadWave("standup.wav", SOUND::STANDUP, false);
    SoundManager.LoadWave("gatling.wav", SOUND::GATLING, false);
    SoundManager.LoadWave("glassbreak.wav", SOUND::GLASSBREAK, false);
    SoundManager.LoadWave("mutant.wav", SOUND::MUTANT, false);
    SoundManager.LoadWave("heart1.wav", SOUND::HEART1, false);
    SoundManager.LoadWave("heart2.wav", SOUND::HEART2, false);
    SoundManager.LoadWave("secret.wav", SOUND::SECRET, false);
    SoundManager.LoadWave("mario.wav", SOUND::MARIO, false);
    SoundManager.LoadWave("flamethrower.wav", SOUND::FLAMETHROWER, true);
    SoundManager.LoadWave("flamethrower.wav", SOUND::FLAMETHROWER2, true);

    // Sound Trigger
    SoundManager.LoadWave("ambient_wasserfall.wav", SOUND::WASSERFALL, true);
    SoundManager.LoadWave("ambient_wind.wav", SOUND::WIND, true);

    // Voices laden
    SoundManager.LoadWave("v_spread.wav", SOUND::VOICE_SPREAD, false);
    SoundManager.LoadWave("v_laser.wav", SOUND::VOICE_LASER, false);
    SoundManager.LoadWave("v_bounce.wav", SOUND::VOICE_BOUNCE, false);
    SoundManager.LoadWave("v_lightning.wav", SOUND::VOICE_LIGHTNING, false);
    SoundManager.LoadWave("v_shield.wav", SOUND::VOICE_SHIELD, false);
    SoundManager.LoadWave("v_powerup.wav", SOUND::VOICE_POWERUP, false);
    SoundManager.LoadWave("v_wheel.wav", SOUND::VOICE_WHEELPOWER, false);
    SoundManager.LoadWave("v_grenade.wav", SOUND::VOICE_GRENADE, false);
    SoundManager.LoadWave("v_powerline.wav", SOUND::VOICE_POWERLINE, false);
    SoundManager.LoadWave("v_smartbomb.wav", SOUND::VOICE_SMARTBOMB, false);
    SoundManager.LoadWave("v_rapidfire.wav", SOUND::VOICE_RAPIDFIRE, false);
    SoundManager.LoadWave("v_supershot.wav", SOUND::VOICE_SUPERSHOT, false);
    SoundManager.LoadWave("v_extralife.wav", SOUND::VOICE_EXTRALIFE, false);

    // Endgegner Sounds
    SoundManager.LoadWave("pharaoramm.wav", SOUND::PHARAORAMM, false);
    SoundManager.LoadWave("pharaodie.wav", SOUND::PHARAODIE, false);
    SoundManager.LoadWave("spiderscream.wav", SOUND::SPIDERSCREAM, false);
    SoundManager.LoadWave("spiderwalk.wav", SOUND::SPIDERWALK, false);
    SoundManager.LoadWave("spiderlaser.wav", SOUND::SPIDERLASER, false);
    SoundManager.LoadWave("spidergrenade.wav", SOUND::SPIDERGRENADE, false);
    SoundManager.LoadWave("blitzhit.wav", SOUND::BLITZHIT, false);
    SoundManager.LoadWave("blitzhit2.wav", SOUND::BLITZHIT2, false);
    SoundManager.LoadWave("bratlaser.wav", SOUND::BRATLASER, false);
    SoundManager.LoadWave("metal.wav", SOUND::KLONG, false);

    // restliche musiken laden
    // DKS - Flugsack song is now loaded on-demand in Gegner_Helper.cpp:
    // SoundManager.LoadSong("flugsack.it",	MUSIC::FLUGSACK);

    SoundManager.LoadSong("credits.it", MUSIC::CREDITS);

    // DKS - New parameter specifies whether to loop, and stage-clear music shouldn't:
    SoundManager.LoadSong("stageclear.it", MUSIC::STAGECLEAR, false);

    // DKS - New parameter specifies whether to loop, and game-over music shouldn't:
    SoundManager.LoadSong("gameover.it", MUSIC::GAMEOVER, false);

    SoundManager.LoadSong("highscore.it", MUSIC::HIGHSCORE);

    // DKS - Punisher music is now loaded on-demand in Gegner_Punisher.cpp
    // SoundManager.LoadSong("Punisher.it", MUSIC::PUNISHER);

    if (!GameRunning)
        return false;

    // Konsole initialisieren
    // DKS - Load console sprites:
    Console.LoadSprites();

    // DKS - renamed:
    // SoundManager.ResetAllSongVolumes();
    SoundManager.ResetAllSoundVolumes();

    return true;
}

// --------------------------------------------------------------------------------------
// GameExit, de-initialisieren der DX Objekte, Sprites etc.
// --------------------------------------------------------------------------------------

bool GameExit() {
    Protokoll << "\n>--------------------<\n";
    Protokoll << "| GameExit started   |\n";
    Protokoll << ">--------------------<\n" << std::endl;

    // Sprites freigeben
    delete (pDefaultFont);
    delete (pMenuFont);
    Protokoll << "-> Fonts released" << std::endl;

    // Menu beenden
    delete (pMenu);
    Protokoll << "-> Head menu released" << std::endl;

    // DKS - Sound manager is now a static global, and we use new Exit() method:
    SoundManager.Exit();
    Protokoll << "-> Sound system released" << std::endl;

    // DKS - Free any straggling textures in VRAM before closing down graphics:
    //      NOTE: this is important! Global systems that contain their own
    //      sprites might get destructed after graphics has been shut down.
    //      In the original code, many systems like TileEngineClass were accessed
    //      through global pointers to dynamically allocated classes, in the
    //      interest of speed and code-compactness, if not clarity.
    //      I changed them to be globally allocated static vars. Since some contain
    //      sprite variables, their destructors would then possible end up freeing
    //      textures after the graphics system already closed. Textures::Exit()
    //      below will prevent that.
    Textures.Exit();
    Protokoll << "-> Texture system released" << std::endl;

    DirectInput.Exit();  // DirectInput beenden

    DirectGraphics.Exit();  // Direct3D    beenden

    // PrintStatus();

    return true;
}

// --------------------------------------------------------------------------------------
// Heartbeat, der Mainloop. der bei jedem Frame durchlaufen wird
// --------------------------------------------------------------------------------------

bool Heartbeat() {
#if defined(ANDROID)
    /*
     * Trucos del panel de depuracion que el motor va gastando durante la partida (vidas,
     * granadas, powerlines, smart bombs y niveles de arma). Se reaplican aqui, una vez por
     * frame: es lo mas simple y lo que menos se mete en el codigo original.
     */
    DebugPanel.ApplyFrameCheats();
#endif

    switch (SpielZustand) {
        // Cracktro
        case GameStateEnum::CRACKTRO: {
#ifdef SHOW_CRACKTRO

            Cracktro->Run();

            if (!Cracktro->IsRunning()) {
                delete (Cracktro);
                SpielZustand = GameStateEnum::MAINMENU;

                if (!GameInit2())
                    return false;
            }
#else
            if (!GameInit2())
                return false;

            SpielZustand = GameStateEnum::MAINMENU;

#endif
            //		pOuttro = new OuttroClass();
            //		SpielZustand = GameStateEnum::OUTTRO;

            goto jump;
        }

        //----- Intro anzeigen ?
        case GameStateEnum::INTRO: {
            // Laufen lassen, bis beendet
            if (!pIntro->IsDone()) {
                pIntro->DoIntro();

                if (DirectInput.AnyKeyDown() || DirectInput.AnyButtonDown())
                    pIntro->EndIntro();
            } else {
                SoundManager.StopSong(MUSIC::INTRO, false);
                delete (pIntro);
                InitNewGame();
                InitNewGameLevel();
                SpielZustand = GameStateEnum::GAMELOOP;
            }
        } break;

        //----- Outtro anzeigen ?
        case GameStateEnum::OUTTRO: {
            pOuttro->DoOuttro();

            if (KeyDown(DIK_ESCAPE))  // Intro beenden ?
            {
                SoundManager.StopSong(MUSIC::OUTTRO, false);
                delete (pOuttro);
                Stage = MAX_LEVELS;
                pMenu->CheckForNewHighscore();
            }
        } break;

        //----- Hauptmenu
        case GameStateEnum::MAINMENU: {
            pMenu->DoMenu();
        } break;

        //---- Haupt-Gameloop
        case GameStateEnum::GAMELOOP: {
            GameLoop();
        } break;

        default:
            break;
    }

#ifndef NDEBUG

    // Debugmode ?
    if (DebugMode == true)
        ShowDebugInfo();

    // Debug-Mode ein/ausschalten
    if (KeyDown(DIK_F10)) {
        DebugMode = !DebugMode;
        while (KeyDown(DIK_F10))
            DirectInput.UpdateTastatur();
    }

#endif

    if (CommandLineParams.ShowFPS)
        ShowFPS();

    // GUI abhandeln
    GUI.Run();

    // Konsole abhandeln
    Console.DoConsole();

jump:

    DirectGraphics.DisplayBuffer();

    // Screenshot machen
#ifndef NDEBUG
    if (KeyDown(DIK_F12))
        DirectGraphics.TakeScreenshot("HurricanShot", RENDERWIDTH, RENDERHEIGHT);

    // Screenshot machen
    if (KeyDown(DIK_F9))
        GUI.HideBox();
#endif
    return true;
}

    // --------------------------------------------------------------------------------------
    // So Firlefanz wie FPS usw anzeigen
    // --------------------------------------------------------------------------------------

#ifndef NDEBUG
// --------------------------------------------------------------------------------------
// FPS Werte auf dem Screen anzeigen
// --------------------------------------------------------------------------------------

void ShowFPSInfo() {
    static int updateFPS;  // Trigger für die FPS, da sonst Anzeige zu schnell
    static double FPS;
    std::string Buffer;

    updateFPS++;
    if (updateFPS > FPS / 2) {
        updateFPS = 0;
        FPS = Timer.getFrameRate();
    }

    // Aktuelle FPS
    Buffer = std::to_string(FPS);
    pDefaultFont->DrawText(0, 0, "Current FPS :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 0, Buffer.c_str(), 0xFFFFFFFF);

    // FPS Grenze
    Buffer = std::to_string(Timer.GetMaxFPS());
    pDefaultFont->DrawText(200, 0, "Max FPS :", 0xFFFFFFFF);
    pDefaultFont->DrawText(300, 0, Buffer.c_str(), 0xFFFFFFFF);

    // Durchschnittliche FPS
    Buffer = std::to_string(Timer.getAverageFPS());
    pDefaultFont->DrawText(0, 15, "Average FPS :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 15, Buffer.c_str(), 0xFFFFFFFF);

    // Maximale FPS
    Buffer = std::to_string(Timer.getMaxFrameRate());
    pDefaultFont->DrawText(0, 30, "Highest FPS :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 30, Buffer.c_str(), 0xFFFFFFFF);

    // Minimale FPS
    Buffer = std::to_string(Timer.getMinFrameRate());
    pDefaultFont->DrawText(0, 45, "Lowest FPS :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 45, Buffer.c_str(), 0xFFFFFFFF);
}

void ShowDebugInfo() {
    std::string StringBuffer;

    // Blaues durchsichtiges Rechteck zeichnen
    RenderRect(0, 0, 320, 240, 0xA00000FF);
    ShowFPSInfo();  // FPS anzeigen

    // Anzahl der aktuell aktiven Partikel anzeigen
    StringBuffer = std::to_string(PartikelSystem.GetNumPartikel());
    pDefaultFont->DrawText(0, 60, "Particles :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 60, StringBuffer.c_str(), 0xFFFFFFFF);

    // Anzahl der aktuell aktiven Schüsse anzeigen
    StringBuffer = std::to_string(Projectiles.GetNumProjectiles());
    pDefaultFont->DrawText(200, 60, "Projectiles :", 0xFFFFFFFF);
    pDefaultFont->DrawText(300, 60, StringBuffer.c_str(), 0xFFFFFFFF);

    // Benutzte Sound-Channels angeben
    StringBuffer = std::to_string(SoundManager.most_channels_used);
    pDefaultFont->DrawText(0, 75, "MaxChannels :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 75, StringBuffer.c_str(), 0xFFFFFFFF);

    // Anzahl der Gegner im Level angeben
    StringBuffer = std::to_string(Gegner.GetNumGegner());
    pDefaultFont->DrawText(200, 75, "Enemies :", 0xFFFFFFFF);
    pDefaultFont->DrawText(300, 75, StringBuffer.c_str(), 0xFFFFFFFF);

    // MoveSpeed anzeigen
    StringBuffer = std::to_string(static_cast<int>(Timer.GetMoveSpeed()));
    pDefaultFont->DrawText(0, 90, "Move Speed :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 90, StringBuffer.c_str(), 0xFFFFFFFF);

    // Blitzwinkel angeben
    StringBuffer = std::to_string(static_cast<int>(Player->BlitzWinkel));
    pDefaultFont->DrawText(0, 105, "Light angle :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 105, StringBuffer.c_str(), 0xFFFFFFFF);

    // Blitzwinkel angeben
    StringBuffer = std::to_string(Timer.getSpeedFactor());
    pDefaultFont->DrawText(0, 120, "Speed :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 120, StringBuffer.c_str(), 0xFFFFFFFF);

    // Blitzwinkel angeben
    StringBuffer = std::to_string(Player->JumpySave - Player->ypos);
    pDefaultFont->DrawText(0, 135, "yDiff :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 135, StringBuffer.c_str(), 0xFFFFFFFF);

    // Blitzwinkel angeben
    StringBuffer = std::to_string(Player->JumpAdd);
    pDefaultFont->DrawText(0, 150, "yAdd :", 0xFFFFFFFF);
    pDefaultFont->DrawText(100, 150, StringBuffer.c_str(), 0xFFFFFFFF);

    /*	for (int i=0; i<128; i++)
            for (int j=0; j<96; j++)
                if(TileEngineTiles[i][j].BackArt > 0)
                    pDefaultFont->DrawText(300+i, 100+j, ".", 0xFFFFFF00);*/
}

#endif  //NDEBUG

// DKS - added FPS reporting via command switch
void ShowFPS() {
    constexpr unsigned int FPS_UPDATE_FREQ_IN_TICKS = 500;
    static unsigned int ticks_fps_last_updated = 0;
    static int frame_ctr = 0;
    static std::stringstream char_buf;

    frame_ctr++;
    unsigned int cur_ticks = SDL_GetTicks();
    unsigned int ticks_elapsed = cur_ticks - ticks_fps_last_updated;
    if (ticks_elapsed > FPS_UPDATE_FREQ_IN_TICKS && frame_ctr > 0) {
        char_buf.str("");
        float avg_fps = static_cast<float>(frame_ctr) * (1000.0f / static_cast<float>(FPS_UPDATE_FREQ_IN_TICKS)) *
                        (static_cast<float>(FPS_UPDATE_FREQ_IN_TICKS) / static_cast<float>(ticks_elapsed));
        char_buf << std::fixed << std::setprecision(1) << "FPS: " << avg_fps;
        Protokoll << char_buf.str() << std::endl;
        frame_ctr = 0;
        ticks_fps_last_updated = cur_ticks;
    }
    pMenuFont->DrawText(0, 0, char_buf.str().c_str(), 0xFFFFFFFF);
}

//----------------------------------------------------------------------------
// Outtro starten
//----------------------------------------------------------------------------

void StartOuttro() {
    Stage = -1;
    pOuttro = new OuttroClass();
    SpielZustand = GameStateEnum::OUTTRO;
}

//----------------------------------------------------------------------------
// Intro starten
//----------------------------------------------------------------------------

void StartIntro() {
#if defined(ANDROID)
    // El toque que confirma la dificultad no debe quedar retenido en el intro. Su
    // constructor espera teclas liberadas; descartar aquí los contactos virtuales evita
    // bloquear el arranque del juego cuando Android aún no entrega FINGERUP.
    TouchControls.CancelTouches();
#endif
    pMenu->StartProgressBar(12);

    pIntro = new IntroClass();
    SpielZustand = GameStateEnum::INTRO;
}
