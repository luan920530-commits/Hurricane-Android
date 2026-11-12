# Hurrican para Android

Port del clásico **Hurrican** (el *jump and shoot* de Poke53280 inspirado en Turrican) a
Android, sobre el código original de [HurricanGame/Hurrican](https://github.com/HurricanGame/Hurrican).

El objetivo no es reescribir el juego, sino conservarlo: se mantiene **el mismo motor y los
mismos datos originales**, y se añade lo que falta para que funcione bien con el dedo en un
móvil moderno.

---

## Estado

| Parte | Estado |
|---|---|
| Compilación nativa (SDL2 + SDL2_image + SDL2_mixer + libxmp + motor) | lista |
| OpenGL ES 2.0 + escalado con FBO a pantallas 16:9 / 18:9 / 20:9 | lista |
| Mando virtual multitáctil (joystick + botones de acción) | lista |
| Panel de trucos (vidas/munición/armas invulnerabilidad) | listo |
| Copia inicial de los datos desde el APK al almacenamiento | lista |
| Supervivencia a la pérdida del contexto GL (segundo plano) | lista |
| Log en `logcat` + diálogo de error visible | lista |
| Música `.it` con libxmp | lista |
| Empaquetado del APK (Gradle + manifiesto + icono) | lista |

**La APK se compila en GitHub Actions**, sin instalar nada en tu ordenador: pestaña
**Actions → Build APK → Run workflow**, y al terminar se descarga desde *Artifacts*. El
workflow compila el motor y las dependencias con el NDK, comprueba que la APK lleva dentro
los datos del juego (más de 600 ficheros y 60 MB de assets) y la publica como artefacto.

**Sin verificar en un dispositivo real**: la compilación está verificada de principio a fin
en los servidores de GitHub (JDK 17 + NDK 26 + CMake 3.22, las dos ABI), pero no se ha
ejecutado en un móvil, así que el aspecto del mando táctil, los tiempos de carga y el
rendimiento están sin ajustar. Para diagnosticar, el juego escribe en `logcat` con la
etiqueta `Hurrican` y en `files/save/Game_Log.txt`.

---

## Compilar

### Sin instalar nada: desde GitHub

1. Ve a la pestaña **Actions** del repositorio.
2. Elige **Build APK** en la lista de la izquierda.
3. **Run workflow**, selecciona ABI y tipo de compilación (por defecto `debug` para
   `arm64-v8a` + `armeabi-v7a`) y lánzalo.
4. Cuando termine (~20–40 min la primera vez), abre la ejecución y descarga
   **hurrican-android-debug** desde *Artifacts*: es un `.zip` con la APK dentro.

```bash
unzip hurrican-android-debug.zip
adb install -r app-debug.apk
```

> **Si no aparece el botón "Run workflow"**: los workflows de GitHub solo se pueden lanzar a
> mano cuando el fichero `.github/workflows/build.yml` existe en la rama por defecto del
> repositorio (*Settings → Branches*). Si no está, basta con fusionar esta rama en `main`, o
> hacer cualquier *push* a una rama con cambios en `Hurrican/`, `android/` o el workflow: se
> compila igualmente. Comprobado también que el workflow arranca desde cualquier rama.

La APK **no está firmada con una clave de publicación**: es una APK de depuración (o de
release firmada con la clave de depuración). Android avisará de "origen desconocido"; hay que
permitir la instalación desde el navegador o desde *adb*. Para publicar en una tienda hay que
crear un keystore propio y configurarlo en `android/app/build.gradle`.

### Con Android Studio o por línea de comandos

Esto es lo mismo que hace el workflow, pero en tu máquina:

#### Requisitos

- Android Studio (Ladybug o posterior) con el NDK y CMake instalados, o bien el SDK de
  Android por línea de comandos con `ANDROID_HOME` configurado.
- NDK 26 o posterior, CMake 3.22 o posterior, JDK 17.
- Unos 2 GB de espacio libre para los objetos intermedios.

#### Con Android Studio

1. *File → Open* y elige la carpeta `android/` (no la raíz del repositorio).
2. Deja que Gradle sincronice.
3. *Run* sobre un dispositivo o emulador.

#### Por línea de comandos

```bash
cd android
./gradlew assembleDebug          # APK de depuración
./gradlew assembleRelease        # APK de release (firmado con la clave de depuración)
```

El APK queda en `android/app/build/outputs/apk/`.

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb logcat -s Hurrican           # log del motor
```

### Qué compila y de dónde sale

```
Hurrican/                     motor original (src/, data/, lang/) sin duplicar
android/
  app/src/main/
    jni/
      CMakeLists.txt          punto de entrada nativo
      SDL/ SDL_image/ SDL_mixer/            dependencias empaquetadas
      SDL_mixer/external/libxmp/             reproductor de música .it
  app/build.gradle            empaqueta Hurrican/{data,lang} como assets y genera
                              hurrican_index.txt (lista de lo empaquetado)
    java/org/libsdl/app/      Java de SDL (sin tocar)
    java/org/hurrican/game/   HurricanActivity
    res/                      icono, tema, textos
```

Todo se compila dentro del APK: no hace falta instalar nada en el sistema.

---

## Los datos del juego

El APK no puede leer sus propios assets con rutas normales, y el motor lee cientos de
ficheros con `std::ifstream`. La primera vez que se abre el juego se copian los assets
(~91 MB) al almacenamiento interno de la aplicación.

La copia sigue `assets/hurrican_index.txt`, un índice que genera Gradle al empaquetar: el
NDK solo deja listar los **ficheros** de un directorio de assets, nunca los subdirectorios,
así que sin índice no habría manera de saber que existen `data/textures/` o `data/music/`.
Si algún día cambian los datos, sube `ANDROID_ASSET_VERSION` en
`Hurrican/src/SDLPort/AndroidPort.cpp` para forzar la recopia.

```
/data/data/org.hurrican.game/files/data   texturas, niveles, música, sonidos, shaders
/data/data/org.hurrican.game/files/lang   traducciones
/data/data/org.hurrican.game/files/save   Hurrican.cfg, partidas, récords, Game_Log.txt
```

Tarda unos segundos y solo ocurre una vez, o cuando cambia la versión de los datos
(`ANDROID_ASSET_VERSION` en `Hurrican/src/SDLPort/AndroidPort.cpp`).

---

## Los controles

El juego sólo entiende teclado, así que el mando virtual **no reimplementa la lógica**: marca
las mismas teclas que el motor ya consulta (`DIK_LEFT`, `DIK_LCTRL`, …). Por eso siguen
funcionando el menú del juego, la reasignación de controles y los gamepads.

El diseño es el de los juegos de acción de móvil (Free Fire, Call of Duty Mobile): **joystick
analógico flotante a la izquierda y botones grandes en arco a la derecha**, todo con los dos
pulgares y sin soltar el dispositivo.

```
   [II]                                              [DBG]

                     ( zona de juego )

        (joystick)                   (A)  saltar
          MOVER                (W) (B) disparar (C)
                             (P)  (S)   (G)
```

| Zona | Acción del juego |
|---|---|
| Joystick (aparece donde toques, tercio izquierdo) | moverse; arriba/abajo apuntan |
| A (grande, arriba a la derecha) | saltar |
| B (grande, abajo) | disparar |
| C | blitz |
| P | powerline |
| G | granada |
| S | smart bomb |
| W | cambiar de arma |
| II (arriba a la izquierda) | pausa / volver |
| DBG (arriba a la derecha) | panel de trucos |

Detalles pensados para jugar de verdad:

- **Joystick flotante**: no hay que buscar un círculo dibujado; nace bajo el dedo en cualquier
  punto del tercio izquierdo de la pantalla y se puede deslizar sin levantarlo.
- **Multitáctil real**: mover y disparar a la vez, o disparar y saltar.
- **Tamaño adaptativo**: los botones se dimensionan sobre la altura de la pantalla, así que
  en un móvil grande son más grandes y en uno pequeño siguen siendo alcanzables.
- **Modo edición** (desde el panel de trucos): los dedos mueven los botones y el joystick a
  donde quieras; se queda así hasta cerrar la app.
- Si se conecta un gamepad Bluetooth, ambos siguen activos.

Si sales de la app (llamada, WhatsApp, bloquear el móvil) con una partida en marcha, el juego
vuelve al menú con **Continuar partida** seleccionado: no se pierde nada y no reapareces en
mitad de un tiroteo. Los dedos que estuvieran pulsados se sueltan solos.

Los ajustes del juego se guardan en `save/Hurrican.cfg` y sobreviven a las actualizaciones.

---

## Panel de trucos y depuración (widget DBG)

El botón **DBG** (arriba a la derecha) abre un panel para probar el port sin teclado:

| Interruptor | Efecto |
|---|---|
| Vidas infinitas | no se descuenta ninguna vida al morir |
| Munición infinita | granadas, powerlines y smart bombs siempre a 9 |
| Armas al máximo | las cuatro armas al nivel 8 |
| Invulnerable | `GodMode` del motor: no recibe daño |
| Mostrar FPS | contador de fotogramas |
| Editar controles | arrastrar botones y joystick |
| Calidad de imagen | *Nítido* / *Suave* / *Máximo* (ver abajo) |

Los trucos **no se guardan**: duran mientras la app está abierta, para no quedarse activados sin
querer en una partida normal. Se aplican una vez por fotograma desde `Heartbeat()`, así que
también funcionan al cambiar de nivel.

El panel **pausa el overlay**: mientras está abierto el mando virtual no mueve al personaje, y
un toque fuera del panel lo cierra.

---

## Calidad de imagen (texturas y letras)

El juego se diseñó para 640×480 y se estira a la pantalla del móvil. Ese estirado decide casi
toda la nitidez que se ve, así que no se hace con una muestra y filtro lineal, sino con un
shader propio (`data/shaders/{100,320}/shader_upscale.frag`) con tres calidades:

| Calidad | Qué hace | Cuándo |
|---|---|---|
| Nítido | vecino más cercano: píxeles perfectos | pantallas pequeñas o poco potentes |
| Suave | escalado de calidad (tipo xBR/ScaleHQ); respeta las zonas planas | **por defecto**, escala 1.5×–2.6× |
| Máximo | calidad + muestras extra contra el parpadeo al moverse | pantallas grandes (≥2.6×) |

- Se elige **automáticamente** según la escala real de la pantalla (se recalcula al volver del
  segundo plano o si cambia la resolución).
- Se puede forzar con `--upscale 0|1|2` o cambiarla en caliente desde la fila *Calidad de
  imagen* del panel DBG, sin reiniciar.
- Si un driver no compila el shader, el juego sigue con el filtro lineal de siempre: nunca se
  queda sin imagen por esto.
- Las texturas y los datos originales **no se modifican**: la mejora es del escalado, no de
  repintar el arte de 1997.

---

## Si algo va mal: cómo diagnosticar

El juego escribe **todo** en `logcat`, con la etiqueta `Hurrican`, y además en
`files/save/Game_Log.txt`. Con el móvil conectado por USB:

```bash
adb logcat -s Hurrican                       # registro del motor en vivo
adb shell run-as org.hurrican.game cat files/save/Game_Log.txt
```

Qué buscar:

| Síntoma | Qué mirar |
|---|---|
| Se queda en el cartel de carga | última línea del log: copia de datos, texturas o shaders |
| Se cierra sin decir nada | `adb logcat` completo (no solo `-s Hurrican`): busca `FATAL EXCEPTION` o `SIGSEGV` |
| Arranca sin sonido | `AVISO: no se pudo abrir el audio` (el juego sigue funcionando) |
| Arranca sin música | `SDL_mixer no anuncia decodificador` (los `.it` necesitan libxmp) |
| Pantalla girada o descolocada | `Android: nuevo tamano de ventana` / `Window viewport` |

Durante el arranque se pinta un cartel con una barra de carga: si se queda ahí, el log dice en
qué paso se quedó. Si un fallo impide arrancar, aparece un diálogo con las últimas líneas.

---

## Arreglos respecto al código original

Errores del motor que afectan de verdad a Android:

1. **`SDL_main` no se exportaba.** `Main.cpp` hacía `#undef main`, pero en Android `SDL.h`
   define `main` como `SDL_main` y SDL lo busca con `dlsym`. Sin este arreglo la app se
   cerraba al arrancar con *"Couldn't find function SDL_main"*.
2. **No había recuperación del contexto OpenGL.** Al volver del segundo plano, Android puede
   destruir el contexto: los shaders, el FBO y todas las texturas desaparecen. Ahora se
   recompilan los shaders y se vuelven a subir las texturas (`RecoverContext()`,
   `ReloadAllTextures()`).
3. **Espera activa en el temporizador.** `Timer::wait()` giraba en vacío hasta 1/maxFPS; en un
   móvil eso es batería y calor. Ahora duerme.
4. **Fuga de joysticks.** Cada conexión o desconexión de mando volvía a llamar a
   `DirectInput.Init()` sin cerrar los handles anteriores, dejando el mismo mando abierto dos
   veces y mandos desconectados como "activos".
5. **`KeyDown(0)` activaba acciones sin asignar.** Las teclas de arma directa valen 0 y eso
   consultaba el scancode 0, que en Android es una tecla real.
6. **`std::cout` no llega a ningún sitio en Android.** Todo el log va ahora a `logcat` y a
   fichero; si el log no se puede abrir, el juego ya no aborta.
7. **El cambio de tamaño de ventana no se recalculaba** (barra de navegación, multi-ventana,
   rotación): el área de render quedaba desplazada.
8. **`FindLanguageFiles()` lanzaba excepción** si el directorio no existía.
9. **Si el arranque fallaba, sólo se veía una pantalla negra.** Ahora hay un diálogo con el
   motivo y las últimas líneas del log.
10. **El log mataba la app antes de arrancar.** El objeto de log es global y abría
    `Game_Log.txt` con ruta relativa *antes* de que existiera el directorio de guardado; en
    Android el directorio de trabajo es `/` (no escribible), el fichero no se abría y un
    `exit(EXIT_FAILURE)` cerraba la app nada más abrirla. Ahora se reabre en `files/save/` y,
    si aun así falla, se registra sólo en `logcat`.
11. **Rutas relativas heredadas del escritorio.** `temp.map` y compañía se escribían en `/`.
    Al arrancar se hace `chdir(files/)`, así que caen dentro de la app.
12. **Falta de audio cerraba el juego.** Si `Mix_OpenAudio` falla (otra app con el
    dispositivo, Bluetooth a medio emparejar, emulador), el motor hacía `GameRunning = false`.
    Ahora se juega en silencio y se avisa en el log.
13. **Los fallos de `Heartbeat()` se ignoraban.** Si la inicialización del menú fallaba, el
    bucle principal la reintentaba sin parar: pantalla negra fija, sin mensaje. Ahora se
    detecta y se muestra el motivo.
14. **Música `.it` sin comprobar el decodificador.** Si SDL_mixer no podía leer el formato, el
    fallo acababa cerrando el juego al cargar la música del menú. Se comprueba antes y, si no
    está, se sigue sin música.

Mejoras de rendimiento:

- Caché de superficies ya decodificadas (`SDLPort/texture.cpp`): al cambiar de nivel y al
  recuperar el contexto se evita volver a decodificar los PNG.
- Audio a 44.1 kHz con buffer pequeño (los `.wav` originales son de 11025/22050 Hz, múltiplos
  exactos) y sin latencia perceptible al pulsar el mando en pantalla.
- `USE_FAST_TRIG` y `USE_FAST_RNG` activados.

---

## OpenGL ES 2.0 y la relación de aspecto

El motor renderiza siempre a 640×480 en una textura (FBO) y luego la escala al panel. En
pantallas 20:9 se ven bandas negras laterales, que es lo correcto: la alternativa
(estirar) deformaría el juego. Los shaders son GLSL ES 1.00 y ya llevan la precisión
`mediump`/`highp` que exige GLES.

---

## Texturas ETC1 (opcional, desactivado)

El port de escritorio soporta texturas comprimidas ETC1. Se ha probado y **no compensa en
este juego**: los `.pkm` ocupan 58 MB frente a los 27 MB de los PNG originales, porque casi
todas las texturas son pequeñas y ETC1 no baja de 8 bytes cada bloque de 4×4.

Si aun así se quieren usar:

```bash
# 1. Compilar el compresor (requiere cmake y un compilador de escritorio)
git clone https://github.com/google/etc2comp.git && cd etc2comp
g++ -std=c++11 -O2 -I EtcLib -I EtcLib/Etc -I EtcLib/EtcCodec -I third_party/lodepng \
    EtcTool/*.cpp EtcLib/Etc/*.cpp EtcLib/EtcCodec/*.cpp third_party/lodepng/lodepng.cpp \
    -o ../tools/bin/EtcTool
# 2. Generar los .pkm
pip install Pillow
python3 tools/generate_etc1.py
# 3. Compilar con -DHURRICAN_USE_ETC1=ON
```

---

## Créditos y licencia

- Juego original: **Poke53280** (Jörg M. Winterstein) — <https://www.winterworks.de>
- Port SDL/OpenGL y mantenimiento: Pickle136, Stefan Schmidt, Leandro Nini y colaboradores
  de [HurricanGame/Hurrican](https://github.com/HurricanGame/Hurrican)
- SDL2, SDL2_image, SDL2_mixer: librerías de [libsdl.org](https://libsdl.org) (zlib)
- libxmp: Claudio Matsuoka y colaboradores (MIT)
- Arte del icono: el propio juego (`data/textures/intro1.png`)

El código original es **MIT** según `README.txt` del repositorio de origen. Los datos del
juego son *freeware*: se pueden distribuir libremente siempre que se mantengan sin modificar
y se acredite al autor; **no se puede vender**. Este port respeta esas condiciones y no
incluye ningún cambio en los datos originales.

---

Qué falta y en qué orden está en [ROADMAP.md](ROADMAP.md).
