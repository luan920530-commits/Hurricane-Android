# Revisión general de carrera (estudio, no integración final)

## Comparación con el juego

- `Player.cpp` carga `hurri_laufen.png` como 350×320, celdas de 70×80, 5 columnas y 4 filas. `DX8Sprite.cpp` recorre las celdas por filas y `FRAMES_RUN` es 20. El bucle del jugador usa `AnimPhase` de 0 a 19. Hay que construir y revisar 20 posiciones en ese orden antes de reemplazar la textura activa.
- Solo hay cuatro poses en cada estudio. Las poses 1 y 2 tienen zancadas muy similares; la secuencia no cubre claramente todo el retorno y cambia de manera brusca entre algunas posiciones. No equivalen a las 20 fases originales. Las candidatas son imágenes independientes, no fases adicionales integradas.
- Los GIF de estudio limpian el cuadro anterior y tienen cuatro cuadros con duración uniforme. `check-run-previews.py` verifica que la composición no conserve alfa de la imagen previa; **no identifica piernas ni valida anatomía**.

## Revisión visual

- Se examinaron las cuatro fuentes de cada operador y las tiras a escala ampliada. En las fuentes actuales se distinguen dos botas por pose, pero a 70×80 las piernas superpuestas y el cambio de silueta pueden resultar confusos; esto no constituye aprobación definitiva. La candidata femenina anterior fue descartada tras el informe de una tercera pierna.
- Hay diferencias visibles de postura, rostro/peinado y vestuario entre poses generadas por separado, particularmente entre las primeras y últimas poses de la mujer. Son necesarias correcciones de continuidad, anatomía, arma y proporciones, y validación humana.
- Las candidatas `candidate-recovery.png` muestran dos botas, pero levantan la rodilla más que las posiciones equivalentes del original. No están alineadas ni incluidas en los GIF.
- En las celdas alineadas, píxeles visibles llegan al borde inferior en las cuatro poses masculinas y en las poses femeninas 3 y 4 (umbral alfa > 8). Por sí solo eso no demuestra un recorte, pero obliga a revisar las suelas y dejar margen antes de exportar una hoja final.
- Todas las celdas de estudio miden 70×80 y las tiras 280×80, con transparencia. La escala se comparte por operador; la alineación horizontal no corrige diferencias anatómicas o de pose.

## Vista en contexto

`tools/sprite-lab/review.html` ofrece una comparación interactiva con el original y un escenario ilustrativo construido con fondo y mosaicos de Hurrican. Sirve para examinar escala y silueta, no ejecuta el motor, no simula el orden de una hoja final de 20 cuadros ni garantiza cómo se verá en Android. Las velocidades de reproducción son ilustrativas. Desde la raíz del repositorio:

```sh
python3 tools/sprite-lab/review-server.py
# Abrir http://localhost:8000/
```

**No se han reemplazado texturas activas.** Para una prueba real en el motor primero hacen falta cuadros adicionales y corregidos, transparencia y bordes revisados, una hoja 5×4 en orden, y validación del comportamiento en ejecución. No se rellenan artificialmente 20 huecos con copias para aparentar una animación completa.
