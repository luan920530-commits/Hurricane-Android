# Editor de mapas de Hurrican para macOS (Intel) — port en desarrollo

Este es el inicio de un port nativo para macOS del programa original `Editor/`. El editor original solo funciona en Windows: es una aplicación Win32 x86 para Visual Studio 6 basada en Direct3D 8, DirectInput 8 y DirectSound. Permite editar niveles y lee y guarda el mismo formato binario `.map` que usa el juego.

La nueva versión usa SDL2, OpenGL y Dear ImGui. Está dirigida a Mac Intel con macOS 12 Monterey o posterior (`x86_64`, destino mínimo 12.0); no requiere Wine ni una máquina virtual de Windows.

## Hito actual

- Lector y escritor portables para la cabecera `.map` original, los tiles, los objetos y los datos adicionales.
- Prueba de ida y vuelta byte a byte para los mapas incluidos en `Hurrican/data/levels`.
- Editor nativo en español, con panel de propiedades fijo a la izquierda y lienzo de mapa a la derecha; incluye creación, apertura y guardado de mapas, pintura con atlas, selección rectangular, copiar/pegar/rellenar, metadatos y edición de objetos.
- Los índices se interpretan como en el juego: tilesets desde 0 y gráficos desde 1; el panel muestra el archivo, la ruta encontrada y los errores de cada atlas.
- Catálogo con búsqueda para 149 IDs de objetos y paneles específicos para varios activadores integrados.
- El formato del juego se conserva; la aplicación no modifica los mapas fuente hasta que el usuario los guarda explícitamente.

El editor **todavía no tiene todas las funciones** de la herramienta de Windows. Muestra los atlas del juego cuando encuentra la carpeta `data/textures` correspondiente; si falta alguna imagen, indica cuál no pudo cargar. Aún faltan las vistas previas de sprites, paneles especializados para todos los activadores, selectores visuales para fondos y música, pintura específica de iluminación y sombras, deshacer/rehacer, minimapa y pruebas del juego desde el editor.

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
