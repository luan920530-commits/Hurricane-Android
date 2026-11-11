# Experimento de carrera con rig · NO instalar en el juego

Prueba completa de **20 cuadros distintos** para cada operador, hecha con Spruppet y los conceptos pintados, no con el vector descartado de `out/`. Se aproximaron manualmente máscaras y articulaciones de las piernas; el torso y el rifle quedan como una sola pieza. Los archivos `.spruppet.json` permiten continuar corrigiendo el rig en `workbench/editor.html` → Save & load → Open project.

**Resultado: aún no aprobado.** En algunas fases hay huecos o fragmentos cerca de las piernas y las botas, y el movimiento no reproduce exactamente la carrera original. El arte base tiene zonas ocluidas que deben reconstruirse. Por eso estas hojas y GIF son **prototipos**, no texturas terminadas, y no se han copiado a `Hurrican/data/textures/`. Las celdas no son prueba de anatomía correcta.

- `*-run-raw.png`: exportación horizontal de Spruppet, con 20 celdas de igual tamaño.
- `*-experimental-5x4.png`: distribución 350×320, 5×4 celdas de 70×80 en orden fila a fila.
- `*-experimental-preview.gif`: vista continua de los 20 cuadros; 80 ms por cuadro solo para revisión, no es velocidad medida del juego.
- `build-preview.py`: recompone ambas hojas sin cambiar escala o anclaje entre celdas. No toca el juego.

Para reconstruir, con ImageMagick, OpenCV y NumPy instalados:

```sh
python3 tools/sprite-lab/experiments/rigging/build-preview.py
```

En el visor `tools/sprite-lab/review.html` elige «Rig experimental · 20 cuadros (con defectos)» para verlo en contexto. Es una simulación web, no una ejecución del juego. Antes de integrar: corregir rig y partes ocultas, comprobar rifle y anatomía en las 20 fases, revisar tamaño/recortes y probar en el motor real.
