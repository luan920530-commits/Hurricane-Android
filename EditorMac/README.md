# Editor de mapas de Hurrican para macOS (Intel) — port en desarrollo

Este es el inicio de un port nativo para macOS del programa original `Editor/`. El editor original solo funciona en Windows: es una aplicación Win32 x86 para Visual Studio 6 basada en Direct3D 8, DirectInput 8 y DirectSound. Permite editar niveles y lee y guarda el mismo formato binario `.map` que usa el juego.

La nueva versión usa SDL2, OpenGL y Dear ImGui. Está dirigida a Mac Intel con macOS 12 Monterey o posterior (`x86_64`, destino mínimo 12.0); no requiere Wine ni una máquina virtual de Windows.

## Hito actual

- Lector y escritor portables para la cabecera `.map` original, los tiles, los objetos y los datos adicionales.
- Prueba de ida y vuelta byte a byte para los mapas incluidos en `Hurrican/data/levels`.
- Editor nativo en español con interfaz inspirada en Godot: árbol de escena a la izquierda, lienzo central, Inspector a la derecha y panel inferior con archivos/salida; los separadores permiten ajustar el tamaño de los paneles.
- Incluye creación, apertura y guardado de mapas, selección rectangular del atlas, pincel rectangular multi-tile, pincel de iluminación de tres tamaños, copiar/pegar/rellenar, metadatos, undo/redo limitado a 12 pasos o 64 MiB y edición de objetos.
- El panel inferior permite abrir mapas del directorio del juego y seleccionar texturas PNG o música IT para asignarlas a fondos, parallax, nubes o música del nivel.
- Los índices se interpretan como en el juego: tilesets desde 0 y gráficos desde 1. El atlas permite arrastrar para seleccionar varias celdas; al pintar, el patrón se repite y se recorta en los límites del mapa o de la selección.
- Cuando encuentra `data/textures` junto al juego, dibuja en el plano los sprites de 137 tipos de objeto registrados por el editor oficial. Los tipos auxiliares sin textura disponible conservan un marcador seleccionable.
- Catálogo con búsqueda para 149 IDs de objetos, edición de posición/dirección/ranura de inicio del jugador y paneles específicos para varios activadores integrados.
- El formato del juego se conserva; la aplicación no modifica los mapas fuente hasta que el usuario los guarda explícitamente.

## Comparación con el editor oficial

Se revisó el código oficial `HurricanGame/Hurrican`, en `Editor/src/Main.cpp` y sus fuentes auxiliares, hasta el commit `0d91d1f` (2025-12-31). El proyecto oficial es un editor Win32 x86 heredado, basado en Visual Studio 6 y Direct3D 8. No se encontró una versión numérica verificable del ejecutable/editor: `1.0.0.3` aparece como carpeta de mapas, no como versión confirmada de la aplicación. La tabla de sprites del port macOS sigue las asociaciones de texturas y tamaños de cuadro registradas allí.

Funciones importantes del editor oficial que siguen pendientes o son parciales en el port macOS:

- **Navegación y orientación:** el árbol de escena, zoom, desplazamiento y paneles redimensionables ya están; sigue faltando el minimapa con vista y ubicación rápida.
- **Capas y vista previa:** ya hay controles de visibilidad para fondo, frente, objetos y cuadrícula. Aún no se reproducen fielmente animaciones, agua, overlays ni la iluminación dinámica del juego.
- **Iluminación y sombras:** ya se puede pintar color/alpha en pinceles de 1, 3 × 3 o 5 × 5 tiles. Faltan la transferencia/copia de luz y una vista previa fiel de los efectos del motor.
- **Recursos del nivel:** el panel de archivos lista mapas, texturas PNG y música IT, y permite asignarlos a los campos del mapa. Falta una vista previa gráfica de los recursos.
- **Edición asistida:** undo/redo está disponible hasta 12 pasos o 64 MiB; faltan los pasos persistentes, copiar propiedades desde otro bloque y lanzar una prueba jugable del nivel.
- **Entidades:** ya se muestran sprites estáticos sobre el mapa para 137 tipos, pero faltan animaciones/variantes por estado y algunos iconos auxiliares; varios parámetros de enemigos y activadores todavía usan controles genéricos en vez de paneles especializados.

Los atlas y sprites se cargan desde los datos locales del juego; si el mapa no permite encontrar `data/textures`, se muestran marcadores de respaldo. La app no incorpora copias de las texturas del juego.

## Descargar la primera versión instalable

La GitHub Action **macOS Intel editor** genera una app `x86_64` con destino mínimo macOS 12, compila SDL para ese mismo destino, ejecuta las pruebas del formato de mapas, incluye las bibliotecas SDL y publica ZIP, DMG y sumas SHA-256. Se puede iniciar desde **Actions → macOS Intel editor → Run workflow** o se ejecuta al actualizar el port. Descarga el artefacto `HurricanMapEditor-macOS12-Intel` cuando termine la ejecución. Las instrucciones de instalación y la advertencia sobre firma/notarización están en `INSTALLING.md`.

## Compilar en un Mac Intel

Instala CMake y SDL2. Dear ImGui 1.91.9 está incluido en `third_party/imgui` junto con su licencia:

```sh
brew install cmake sdl2 sdl2_image
cmake -S EditorMac -B build/EditorMac -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/EditorMac --config Release
ctest --test-dir build/EditorMac --output-on-failure
open build/EditorMac/HurricanMapEditor.app
```

En la app, usa **Abrir…** para elegir un archivo `.map` con el selector de macOS o pega una ruta en el campo del mapa. **Guardar** reemplaza el archivo actual; **Guardar como…** abre el selector de guardado. Haz una copia de seguridad de los mapas originales antes de editarlos.

## Validar el formato sin SDL2

El núcleo del formato y la prueba de ida y vuelta no dependen de SDL y pueden compilarse con un compilador C++17:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -IEditorMac/src EditorMac/src/MapDocument.cpp EditorMac/tests/MapFormatTests.cpp \
  -o /tmp/hurrican-map-format-tests
/tmp/hurrican-map-format-tests Hurrican/data/levels
```
