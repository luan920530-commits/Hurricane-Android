# Qué está verificado y qué falta

## La compilación: verificada de principio a fin en GitHub Actions

La APK **se compila y se publica** desde el repositorio (Actions → *Build APK*): JDK 17, NDK
26.1.10909125, CMake 3.22.1, dos ABI (`arm64-v8a` + `armeabi-v7a`). Cada compilación
comprueba además que la APK lleva dentro los datos del juego y `libmain.so`
(`.github/scripts/check_apk.py`), y que todos los `#include "..."` apuntan a ficheros que
están en el repositorio (`.github/scripts/check_includes.py`).

## El código es el original (comprobado por hash)

Comparado fichero a fichero contra `HurricanGame/Hurrican` (rama `master`):

| Parte | Resultado |
|---|---|
| `Hurrican/data` + `Hurrican/lang` (803 ficheros, 91 MB) | **801 idénticos** byte a byte; solo faltan `data/textures/pvr/PVRCompress.sh` y `PVRTexTool` (herramientas de escritorio para generar texturas PVRTC, no las usa el juego en Android) |
| `Hurrican/src` (376 ficheros) | **0 ausentes**; **18 modificados** (todos con el cambio comentado en el propio código) y **8 nuevos** (el port: `AndroidPort`, `AndroidError`, `TouchControls`, `DebugPanel`) |
| `Hurrican/3rdparty` | glm, sin tocar |

Los 18 ficheros modificados y por qué:

| Fichero | Cambio |
|---|---|
| `Main.cpp` | entrada `SDL_main`, almacenamiento de Android, cartel de carga, ciclo de vida, `std::exception` |
| `Logdatei.cpp/.hpp` | log a `logcat`; no abortar si no se puede abrir el fichero |
| `DX8Graphics.cpp/.hpp` | recuperación del contexto GL, cambio de tamaño de ventana, dibujo del overlay y del cartel de carga |
| `DX8Input.cpp/.hpp` | buffer de teclas propio para el mando virtual; `KeyDown(0)` |
| `DX8Sound.cpp` | el fallo de audio ya no cierra el juego; comprobación del decodificador |
| `DX8Texture.cpp/.hpp` | recarga de texturas tras perder el contexto; caché de superficies |
| `Player.cpp` | vidas infinitas del panel de trucos |
| `SDLPort/SDL_fmod.cpp` | decodificador de música comprobado antes de cargar |
| `SDLPort/opengl.hpp` | cabeceras GLES2 en Android |
| `SDLPort/texture.cpp/.hpp` | caché de superficies y `ClearSurfaceCache()` |
| `Texts.cpp` | `FindLanguageFiles()` no lanza excepción si falta el directorio |
| `Timer.cpp` | espera pasiva en vez de bucle activo |
| `Menu.cpp` | no incluir `sys/timeb.h` (no existe en bionic y no se usa) |

## Verificado con compilador

| Comprobación | Resultado |
|---|---|
| Los 186 `.cpp` del motor con `g++ -std=c++17` y las banderas de Android, **con cabeceras de bionic simuladas** (GLES2, `android/log.h`, `asset_manager`, `jni.h`) y con las cabeceras que no existen en Android puestas a fallar | **0 errores** (salvo `SDLPort/eglport.cpp`, excluido del build) |
| Exportación de `SDL_main` en `libmain.so` | comprobado con `nm -D` |
| Rutas de CMake (`HURRICAN_DIR`) y del árbol de dependencias | resuelven a los directorios reales |
| Configuración de CMake de SDL2 / SDL2_image / SDL2_mixer / libxmp | se genera y se compila en local (Linux): SDL2_mixer con libxmp estático enlaza |
| Mando virtual | zonas sin solape, dentro de pantalla, multitáctil |
| Teclas del mando contra los controles por defecto del motor (`Gameplay.cpp`) | coinciden |
| Coherencia de `data/` con lo que espera el motor (`data/levels`, `data/music`, `data/sfx`, `data/textures`, `data/shaders/100`) | correcta |

## Sin verificar (necesita un dispositivo)

1. **Que el juego arranque y se vea bien** (escalado, colores, shaders). La compilación está
   verificada; ejecutarlo en un móvil, no.
2. **El mando táctil en pantalla**: posiciones cómodas con el pulgar, tamaño de los botones.
   El modo edición del panel DBG permite recolocarlos sin recompilar.
3. **La música `.it`** a través de libxmp.
4. **El ciclo pausa/reanudación**: minimizar, volver, y comprobar en `logcat` si aparece
   "contexto grafico perdido, reconstruyendo".
5. **El primer arranque** (copia de ~91 MB de assets): cuánto tarda y si el progreso se ve.
6. **El panel de trucos**: que las vidas/munición se mantengan durante una partida larga.

## Cómo probarlo

La APK sale de **Actions → Build APK → Run workflow** (ver README), o en local:

```bash
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk

# En otra terminal, mientras juegas:
adb logcat -s Hurrican SDL

# Si algo falla, el log del motor (y el motivo de un cierre inesperado):
adb shell run-as org.hurrican.game cat files/save/Game_Log.txt
```

## Puntos donde mirar primero si algo no funciona

| Síntoma | Dónde mirar |
|---|---|
| Se cierra al abrir, sin mensaje | `getLibraries()` en `HurricanActivity.java`; en logcat, "Couldn't find function SDL_main" o `UnsatisfiedLinkError` |
| Se cierra al abrir y no hay log | el log se abre en `files/save/` desde `Android_BootstrapStorage()`; si tampoco aparece, mirar `logcat` completo buscando `FATAL EXCEPTION` |
| "No se pudo obtener el AssetManager" | `AndroidPort.cpp` → `Android_InitStorage()` |
| "No se encuentra hurrican_index.txt" | la tarea `copyHurricanAssets` de `app/build.gradle` no generó el índice |
| Se queda en el cartel de carga | `Game_Log.txt`: qué textura, shader o fichero no encuentra |
| Música no suena, efectos sí | el log dice `SDL_mixer no anuncia decodificador`; revisar `SDL2MIXER_MOD_XMP` en `jni/CMakeLists.txt` |
| Sin sonido en general | `AVISO: no se pudo abrir el audio` (el juego sigue, es normal en emuladores) |
| Vuelve del segundo plano en negro | `RecoverContext()` en `DX8Graphics.cpp` |
| Botones táctiles incómodos | activar *Editar controles* en el panel DBG y arrastrarlos; los valores por defecto están en `ACTIONS[]` (`SDLPort/TouchControls.cpp`) |
| Los trucos no se quedan activos | el panel los reaplica en `Heartbeat()`; si la partida está en pausa, no se aplican |
