# Qué está verificado y qué falta

Este entorno **no tiene SDK de Android, NDK, Java ni Gradle**, así que la APK no se ha podido
compilar ni ejecutar aquí. Lo que sí se ha hecho:

## Verificado

| Comprobación | Resultado |
|---|---|
| Compilación de los 186 `.cpp` del motor con `g++ -std=c++17` y las banderas de Android (`-DANDROID -DUSE_GL2 -DUSE_GLES2 -DUSE_FBO -DUSE_FAST_TRIG -DUSE_FAST_RNG`) | **0 errores** en todos salvo `SDLPort/eglport.cpp`, que no forma parte del build de Android (es la ruta EGL de las consolas; Android usa el contexto EGL de SDL) |
| Cabeceras de GLES falsas para simular `GLES2/gl2.h` | todos los símbolos que usa el motor existen en GLES2 real |
| Exportación del símbolo `SDL_main` en `libmain.so` | comprobado con `nm -D`: se exporta `SDL_main`, no `main` |
| Rutas del CMake (`HURRICAN_DIR`) | resuelven a la carpeta real del motor (se corrigió un `../` de más que rompía la compilación) |
| Geometría del mando virtual | script de prueba: 0 solapes entre botones, todos dentro de pantalla, reparto correcto con 4 dedos simultáneos |
| Sintaxis del script `tools/generate_etc1.py` | válida; se ejecutó de verdad para generar y medir las texturas ETC1 |
| Coherencia de las teclas del mando virtual con los controles por defecto del motor | coinciden (`Gameplay.cpp`, `CreateDefaultControlsConfig`) |

## Sin verificar (necesita un dispositivo)

1. **La compilación nativa real.** Requiere el NDK. Es lo primero que hay que probar.
2. **Que el juego arranque y se vea bien** (escalado, colores, shaders).
3. **El mando táctil en pantalla**: posiciones cómodas con el pulgar, tamaño de los botones.
4. **La música `.it`** a través de libxmp.
5. **El ciclo pausa/reanudación**: minimizar, volver, y comprobar en `logcat` si aparece
   "contexto grafico perdido, reconstruyendo".
6. **El primer arranque** (copia de ~91 MB de assets): cuánto tarda y si el progreso se ve.

## Cómo probarlo

```bash
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk

# En otra terminal, mientras juegas:
adb logcat -s Hurrican SDL
```

Si algo falla, el log del motor está en:

```bash
adb shell run-as org.hurrican.game cat files/save/Game_Log.txt
```

## Puntos donde mirar primero si algo no funciona

| Síntoma | Dónde mirar |
|---|---|
| Se cierra al abrir, sin mensaje | `getLibraries()` en `HurricanActivity.java`; en logcat, "Couldn't find function SDL_main" o un `UnsatisfiedLinkError` con la ABI |
| "No se pudo obtener el AssetManager" | `AndroidPort.cpp` → `Android_InitStorage()` |
| Se queda en la pantalla de carga | `Game_Log.txt`: qué textura o fichero no encuentra |
| Música no suena, efectos sí | `SDL2MIXER_MOD_XMP` en `jni/CMakeLists.txt` y `xmp_static` en los enlaces |
| Vuelve del segundo plano en negro | `RecoverContext()` en `DX8Graphics.cpp` |
| Botones táctiles mal colocados | tabla `BUTTONS[]` en `SDLPort/TouchControls.cpp` (posiciones normalizadas 0..1) |
