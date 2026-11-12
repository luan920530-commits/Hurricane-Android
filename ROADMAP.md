# Dónde seguir mejorando

Estado de partida: la APK compila en verde, lleva los 801 ficheros de datos idénticos al
original, los 186 `.cpp` compilan con las banderas de Android y los shaders se validan en el
CI. Lo que sigue está ordenado por **cuánto se nota jugando por cada hora de trabajo**.

## 1. Lo que se nota nada más coger el móvil (prioridad alta)

### 1.1 Probar el mando en un móvil de verdad y dejar los botones finos
El diseño ya es de acción (joystick flotante + botones en arco + modo edición), pero las
posiciones son **teóricas**: se han calculado sobre la altura de pantalla, no con un pulgar
real. Es lo primero que hay que validar.

- Abrir el panel **DBG** → *Editar controles*, recolocar y anotar las coordenadas que
  funcionan bien en un móvil normal y en uno grande.
- Con esos datos, fijar los valores por defecto de `ACTIONS[]` en `SDLPort/TouchControls.cpp`.
- Añadir dos ajustes que ya están pedidos por el diseño pero no existen: **tamaño global de
  los botones** (80 / 100 / 120 %) y **zona muerta** del joystick (0.30 ahora mismo).

### 1.2 Disparo cómodo
En CoD/Free Fire el disparo se mantiene pulsado mucho rato y se combina con apuntar. Hoy el
botón B marca `DIK_LCTRL` mientras el dedo esté dentro; conviene:

- **Auto-disparo** opcional: al mantener el botón, el juego repite el disparo sin necesidad de
  acertar en el centro del botón cada vez (el motor ya tiene `AutoFire` en `Player`, se puede
  reutilizar en vez de inventar nada nuevo).
- Botón de disparo **más grande y con algo de histéresis** (si el dedo se desliza 20 px fuera,
  no soltar el disparo): es el fallo más habitual de los mandos virtuales.

### 1.3 Pausa y reanudación
Ya está hecho: al salir de la app con una partida en marcha se va al menú con *Continuar
partida* seleccionado, y se sueltan todas las teclas. Queda comprobar en dispositivo:

- salir con una llamada entrante, volver, y comprobar que el personaje no dispara solo;
- **rotar** el móvil con la partida en marcha (el cambio de tamaño ya se recalcula, pero el
  área de juego táctil se recalcula por fotograma: conviene probarlo).

## 2. Lo que se nota a los cinco minutos (prioridad media)

### 2.1 Rendimiento y batería
- El bucle duerme en `Timer::wait()`, pero **no hay tope de FPS en Android**: conviene fijar
  60 (o el `display refresh rate` del móvil) y comprobar con `--showfps`.
- Medir con `adb shell dumpsys gfxinfo org.hurrican.game` y `adb shell top -n 1 | grep hurrican`.
- El escalado nuevo tiene tres calidades: en un móvil modesto sirve para bajar coste sin
  perder nitidez (calidad *nitido* = 1 muestra por pixel).

### 2.2 Latencia de audio
El buffer de mezcla está a 1024 muestras a 44.1 kHz (~23 ms). En un juego de disparos se
puede bajar a 512 si el móvil aguanta; se puede elegir automáticamente si aparecen
`underrun` en el log.

### 2.3 Mandos Bluetooth
`data/gamecontrollerdb.txt` ya se carga y el gamepad convive con el mando virtual. Falta
probar un mando real y decidir si al conectarlo se **oculta** el overlay automáticamente
(hoy se queda, y tapa media pantalla).

## 3. Lo que se nota a la media hora (prioridad baja, pero suma)

### 3.1 Aspecto 4:3 en pantallas 16:9 o 20:9
Hoy se elige el mayor rectángulo 4:3 y se dejan bandas negras, o se estira (modo
`FULLSCREEN_STRETCHED`, que deforma). Dos alternativas mejores:

- **Recorte** (el juego original lo admite mal, porque el HUD está pegado a los bordes),
- o **16:9 con HUD recolocado**, que es lo que hacen los ports modernos de juegos de esta
  época. Es trabajo de `HUD.cpp` y `Menu.cpp`, pero es la mejora visual más grande que queda.

### 3.2 Texturas comprimidas (ETC1)
El código ya tiene la ruta `USE_ETC1` (un `.pkm` por textura + canal alfa aparte). Ventajas:
menos memoria de GPU y menos tiempo de carga. Requiere generar los `.pkm` en el CI y
activarlo solo en los móviles que lo soportan. **No toca los originales**: los `.pkm` se
generan a partir de los PNG en tiempo de compilación.

### 3.3 Escalado de la interfaz
Las fuentes son de 1997 y a 1600 px de alto se leen mal. El motor ya tiene
`DirectGraphicsFont::mScaleFactor`: subir las fuentes y el HUD un 25-50 % en pantallas
grandes mejora la legibilidad en el móvil. (El escalado de la imagen ya está hecho; esto es
el texto del juego.)

### 3.4 Guardado
Comprobar guardar/cargar partida en el móvil (`files/save`), y valorar un **guardado
automático al pasar a segundo plano**, para que cerrar la app desde el menú de tareas no
cueste el nivel.

## 4. Robustez (mantenimiento)

Quedan tres sitios donde un fallo todavía puede dejar la app sin mensaje; todos se pueden
convertir en aviso + continuar, como se hizo con el audio y los niveles:

| Sitio | Qué pasa hoy |
|---|---|
| `DirectGraphics.Init()` si un shader no compila | `Init()` devuelve false y se muestra el diálogo (aceptable), pero el motivo real solo está en el log |
| `SDL_LoadTexture()` / `loadImageSDL()` | pone `GameRunning = false`: cierra el juego si falta una textura |
| `DirectGraphics.Font` si falta una fuente | el texto desaparece sin avisar |

## 5. Cómo se comprueba cada cosa (sin herramienta local)

- **Compilación y APK**: `Actions → Build APK → Run workflow`. Ahora el CI comprueba además
  que los shaders son coherentes con el código y que van dentro de la APK.
- **Comparación con el original**: el procedimiento de `VERIFICACION.md` (descarga de
  `HurricanGame/Hurrican` + sha256 por fichero) confirma que los datos no se han tocado.
- **En el móvil**: `adb logcat -s Hurrican` y `files/save/Game_Log.txt`.
