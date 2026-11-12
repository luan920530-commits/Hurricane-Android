# Revisión de ramas: `main` vs. esta rama

Fecha de la revisión: 12/11/2026.

## Resumen en una frase

**No es que `main` esté "más actualizado": es una segunda implementación en paralelo del mismo
port.** Cada rama tiene cosas que la otra no tiene, y las dos tocan los mismos ficheros, así que
no se puede simplemente "adoptar la más nueva".

| | `main` (`14e1b2f`) | esta rama (`fdf8b7d`) |
|---|---|---|
| Commits propios | 25 | 21 |
| Ficheros | 3738 | 3401 |
| Mando táctil | **iconos dibujados** (arte táctico + Lucide) + **guarda la posición** | bordes de color (sin arte) + modo edición **sin guardar** |
| Acciones | **directas al jugador** (`Aktion[...]`), no solo teclas | solo escribe teclas (`DIK_...`) |
| Personajes | **skin femenina** + selección en el menú | solo los originales |
| Economía | **tienda offline** (monedas ↔ vidas/armas/bombas) | no |
| Nitidez | no | **escalado Nítido/Suave/Máximo** (`shader_upscale`) |
| Pausa al salir de la app | no | **sí** (vuelve al menú con *Continuar partida*) |
| Soltar dedos al salir del app / abrir panel | `CancelTouches()` | `ReleaseAll()` (equivalente) |
| CI | valida créditos, iconos y tests de layout/tienda | valida **shaders** y que vayan en la APK |
| Robustez (log, audio, niveles, `std::exception`) | **la misma** (ficheros idénticos) | **la misma** (ficheros idénticos) |

## Lo que `main` aporta de verdad (y esta rama no tiene)

1. **Mando táctil con arte**: `touch_tactical_shell.png` (marco metálico) + 9 iconos tácticos +
   juego alternativo de iconos Lucide. Se ve mucho mejor que los círculos semitransparentes.
2. **Diseño "táctico"**: cruceta a la izquierda dibujada, botones en tres filas, posiciones
   repartidas por toda la pantalla (no solo el arco inferior derecho).
3. **Persistencia del layout**: `TouchLayout` guarda/lee `save/touch-layout-v1.txt`; lo que
   colocas en modo edición **se queda** entre ejecuciones (aquí se perdía al cerrar).
4. **Acciones directas al motor**: `Player.cpp` consulta `TouchControls.IsActionPressed()` y marca
   `Aktion[AKTION_*]`. Esto hace que el mando funcione **aunque el perfil guardado sea "joystick"**
   (con solo teclas, si el jugador cambió el perfil en opciones, el mando dejaba de responder).
   Es la mejora de jugabilidad más sólida de `main`.
5. **Tienda offline**: monedas por diamante, vidas extra, mejora de arma permanente, bombas
   guardadas; `Store.hpp` con guardado atómico y sin sobrescribir un fichero corrupto.
6. **Personaje femenino** (`p1_female_hurri_*`, 14 hojas) con selección en el menú.
7. **Herramientas**: `tools/sprite-lab` (208 ficheros, banco de trabajo de sprites),
   `EditorMac` (27), `tools/touch-ui` con tests (`test-layout`, `test-store`, `test-icons`).
8. **CI**: comprueba que no queden referencias a Poke53280 en los textos distribuidos y que los
   iconos táctiles estén en el repositorio, y ejecuta los tests de layout y tienda.

## Lo que esta rama aporta y `main` no

1. **Escalado de imagen** (esto es lo que mejora las texturas): `shader_upscale` con tres
   calidades y elección automática. En `main` los 4 ficheros de shader nuevos **no existen** y
   el escalado es el filtro lineal de siempre.
2. **Pausa automática** al salir de la app: en `main` la partida se congela, pero al volver
   reapareces en mitad del tiroteo con el enemigo encima.
3. **Comprobación de shaders en el CI** (detecta constantes `c_...` que el motor no inyecta,
   GLSL 100 vs 320, llaves descompensadas) y **comprobación de que los shaders van en la APK**.
4. **Documentación de diagnóstico**: `README` (sección de imagen y de "si algo va mal"),
   `VERIFICACION.md` con la comparación por hash contra el original, `ROADMAP.md`.

## Problemas detectados en `main` (importante)

| Punto | Detalle |
|---|---|
| **Toca arte original** | Reemplaza `p1_hurri_idle.png` y `p1_hurri_jump.png` (100 KB → 128 KB y 59 → 83 KB). Choca con tu criterio de no alterar los datos originales. Si se quiere la skin nueva, mejor **añadir** `p1_female_*` y todo lo demás, sin sustituir |
| **Borra texto de los niveles** | `data/levels/Walkers_World/Logdatei.txt` pierde 44 líneas y `valentine/read_me.txt` una: son las líneas `poke53280.de / Eiswuxe@Poke53280.de`. Es deliberado (quitar la referencia visible), pero **es un fichero de datos original** |
| **Renombra el menú del motor** | `MENUZUSTAND_STORE` añadido a `Menu.hpp`; hay que revisar que no descuadre los índices guardados |
| **Sube `ANDROID_ASSET_VERSION` a "9"** | Fuerza recopiar los 91 MB la próxima vez; correcto, pero hay que tomar el número más alto al integrar |
| **Conflicto de integración** | Solo 3 ficheros con conflicto de texto: `check_apk.py`, `DebugPanel.cpp` y `TouchControls.hpp`. El resto (`Main.cpp`, `DX8Graphics.cpp`, `TouchControls.cpp`, `CMakeLists`, CI) se fusiona automáticamente, pero **hay que revisar a mano** que las dos cosas convivan (`OPT_UPSCALE` vs. tienda; `ReleaseAll` vs. `CancelTouches`; área táctil = ventana vs. área 4:3) |

## Cómo integrarlo bien (propuesta)

Orden recomendado, en esta misma rama, para no perder nada:

1. **Marcar `p1_hurri_idle.png` y `p1_hurri_jump.png` como originales** (restaurarlos) y quedarse
   con la skin femenina como personaje **añadido**.
2. Fusionar `main` en esta rama resolviendo los 3 conflictos:
   - `TouchControls`: adoptar el diseño de `main` (arte + persistencia + acciones directas) y
     **conservar** `ReleaseAll()`, `HitDebugButton()` y el panel DBG.
   - `DebugPanel`: conservar `OPT_UPSCALE` (calidad de imagen) e integrar el resto.
   - `check_apk.py`: conservar las dos comprobaciones (shaders + iconos/créditos).
3. Mantener en el CI las **dos** baterías: shaders, includes, APK, créditos, iconos, tests.
4. Un solo APK de prueba con todo y validar en el móvil: nitidez, mando con arte, tienda y pausa.

Si prefieres el camino rápido, la alternativa es **partir de `main`** (que ya tiene el arte, la
persistencia y la tienda) y portar encima el escalado y la pausa automática, que son 5 ficheros.
En ambos casos el resultado final es el mismo APK; cambia solo qué rama queda como base.
