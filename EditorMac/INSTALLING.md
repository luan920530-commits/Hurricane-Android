# Instalación de la primera versión para macOS

Esta versión preliminar es para **Mac Intel (`x86_64`) con macOS 12 Monterey o posterior**. Es un port inicial del editor de niveles de Hurrican; todavía no tiene todas las funciones del editor original.

1. Descarga `HurricanMapEditor-macOS12-Intel.dmg` o `HurricanMapEditor-macOS12-Intel.zip` desde el artefacto de GitHub Actions.
2. Abre el DMG o descomprime el ZIP y mueve **Hurrican Map Editor.app** a Aplicaciones (o a otra carpeta bajo tu control).
3. La app tiene firma ad-hoc y no está notarizada. Si macOS la bloquea al abrirla por primera vez, haz clic derecho o Control-clic sobre la app, elige **Abrir** y confirma. Puede que tengas que repetir este paso al instalar una versión nueva.
4. Abre un archivo `.map` del juego. Para que el editor encuentre el atlas correcto, conserva el mapa dentro de la estructura de datos del juego, cerca de `data/textures`.

La app incluye las bibliotecas SDL que necesita. No instala ni modifica los archivos del juego. Haz una copia de seguridad de los mapas antes de guardar cambios.

## Sobre la compilación

La GitHub Action genera una app Intel con destino mínimo macOS 12.0, ejecuta las pruebas del formato `.map`, incluye las bibliotecas SDL necesarias y publica ZIP y DMG con sumas SHA-256. Esta versión preliminar no usa firma Developer ID ni notarización de Apple.
