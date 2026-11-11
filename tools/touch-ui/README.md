# Iconos táctiles de Hurrican

Los nueve iconos de `lucide/` proceden de [Lucide](https://github.com/lucide-icons/lucide), paquete `lucide-static` versión **1.71.0**. Las fuentes se distribuyen bajo licencia **ISC**; el archivo `lucide/LICENSE` incluye también los avisos MIT de los iconos derivados de Feather. Se conservan las fuentes y los avisos completos.

`build-icons.py` exporta versiones blancas transparentes de 128×128 a `Hurrican/data/textures/touch_lucide_*.png`, usando `resvg-py` como herramienta de desarrollo. El motor dibuja los discos, el color al pulsar y la cruceta; estas imágenes son solo pictogramas. No cambian los controles, las zonas táctiles, multitouch ni la configuración de posiciones. Los recursos anteriores se conservan.

| Acción del juego | Icono |
| --- | --- |
| Salto | footprints |
| Disparo | crosshair |
| Blitz | chevrons-right |
| Powerline | zap |
| Granada | bomb |
| Bomba inteligente | radiation |
| Cambiar arma | refresh-cw |
| Pausa | pause |
| Depuración | bug |

Para reconstruirlos en un entorno Python con `resvg-py` instalado: `python3 tools/touch-ui/build-icons.py`. Verifica la tabla y los archivos con `python3 tools/touch-ui/test-icons.py`. La licencia también se incluye en `Hurrican/data/licenses/` para que viaje con los datos del APK. La vista `tools/sprite-lab/review.html` presenta una simulación de la disposición predeterminada; no ejecuta el juego ni prueba entrada táctil.
