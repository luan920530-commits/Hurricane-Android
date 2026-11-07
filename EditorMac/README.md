# Hurrican Map Editor for macOS (Intel) — port in progress

This is the beginning of a native macOS port of the original `Editor/` program. The original editor is a Windows-only Visual Studio 6 / Win32 x86 application built on Direct3D 8, DirectInput 8, and DirectSound. It is a level editor: it loads and saves the same binary `.map` format used by the game.

The new target uses SDL2, OpenGL, and Dear ImGui. It targets Intel Macs running macOS 12 Monterey or later (`x86_64`, deployment target 12.0). It does not require Wine or a Windows virtual machine.

## Current milestone

- Portable reader/writer for the original `.map` header, tiles, objects, and appendix.
- Byte-for-byte round-trip test for all maps currently bundled under `Hurrican/data/levels`.
- SDL2/ImGui application shell with native macOS Open/Save panels, atlas-textured tile preview when the matching game data folder is present, tile-layer/art/block-flag editing, basic level metadata, and numeric object editing.
- The map format is kept compatible; the application does not rewrite source maps unless the user explicitly saves.

The editor is **not yet at full feature parity** with the Windows tool. It renders the map's tile atlases when it finds the matching game `data/textures` folder, and falls back to schematic colors if those images are unavailable. Object catalog/sprites, lighting and shadows, triggers, background/parallax and music selectors, undo/clipboard/minimap, and in-editor game testing remain to be ported.

## Build on an Intel Mac

Install CMake and SDL2. Dear ImGui 1.91.9 is vendored with its upstream license in `third_party/imgui`:

```sh
brew install cmake sdl2 sdl2_image
cmake -S EditorMac -B build/EditorMac -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/EditorMac --config Release
ctest --test-dir build/EditorMac --output-on-failure
open build/EditorMac/HurricanMapEditor.app
```

In the app, use **Open…** to choose a `.map` with the native macOS file panel (or paste a path into **Map path**). **Save** overwrites the current file; **Save As…** opens the native save panel. Back up original maps before editing.

## Format validation without SDL2

The format core and round-trip test have no SDL dependency and can be compiled with a C++17 compiler:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -IEditorMac/src EditorMac/src/MapDocument.cpp EditorMac/tests/MapFormatTests.cpp \
  -o /tmp/hurrican-map-format-tests
/tmp/hurrican-map-format-tests Hurrican/data/levels
```
