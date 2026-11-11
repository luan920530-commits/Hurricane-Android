# Controles tácticos metálicos

Este diseño está integrado en el mando Android: el aro metálico se generó para este proyecto y se convirtió a PNG transparente. Los símbolos son de [Game-icons.net](https://game-icons.net/), licencia [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/), procedentes del [repositorio game-icons/icons](https://github.com/game-icons/icons) en el commit `82d948812bfe3f269ef8f731dcdb07b08160edc4`. Se eliminó el fondo negro de los SVG y se cambió el color blanco a marfil; los autores originales no respaldan esta adaptación.

| Acción | Archivo original | Autor |
| --- | --- | --- |
| Salto | `delapouite/jump-across.svg` | Delapouite |
| Disparo | `delapouite/crosshair.svg` | Delapouite |
| Blitz | `lorc/run.svg` | Lorc |
| Powerline | `lorc/power-lightning.svg` | Lorc |
| Granada | `lorc/grenade.svg` | Lorc |
| Bomba | `lorc/unlit-bomb.svg` | Lorc |
| Arma | `sbed/rifle.svg` | Sbed |
| Pausa | `guard13007/pause-button.svg` | Guard13007 |

El icono de depuración usa el Lucide anterior (ver `../lucide/LICENSE`). `game-icons/LICENSE.txt` conserva la licencia del repositorio. `build-preview-assets.py` reconstruye los PNG de muestra usando ImageMagick y `resvg-py`. El selector «Aspecto de controles» en el visor permite comparar la propuesta con la versión actual. La vista es una simulación; falta validar legibilidad, estados pulsados, transparencia y multitouch en Android real. Créditos y licencia se distribuyen en `Hurrican/data/licenses/` y en los créditos del juego.
