#!/usr/bin/env python3
"""Build preview-only tactical HUD assets; requires resvg-py and ImageMagick."""
from pathlib import Path
import subprocess
import resvg_py

HERE = Path(__file__).resolve().parent
SOURCES = {
    'jump': 'delapouite/jump-across.svg',
    'fire': 'delapouite/crosshair.svg',
    'blitz': 'lorc/run.svg',
    'powerline': 'lorc/power-lightning.svg',
    'grenade': 'lorc/grenade.svg',
    'smartbomb': 'lorc/unlit-bomb.svg',
    'weapon': 'sbed/rifle.svg',
    'pause': 'guard13007/pause-button.svg',
}


def main():
    out = HERE / 'rendered'
    out.mkdir(exist_ok=True)
    subprocess.run(['convert', str(HERE / 'tactical-button-shell.png'), '-alpha', 'set', '-fuzz', '5%',
                    '-fill', 'none', '-draw', 'matte 0,0 floodfill', '-trim', '+repage',
                    '-resize', '256x256', str(out / 'shell.png')], check=True)
    for action, name in SOURCES.items():
        svg = (HERE / 'game-icons' / name).read_text()
        background = '<path d="M0 0h512v512H0z"/>'
        if background not in svg:
            raise ValueError(f'Unexpected SVG background in {name}')
        svg = svg.replace(background, '', 1).replace('#fff', '#f4e8c8')
        (out / f'{action}.png').write_bytes(resvg_py.svg_to_bytes(svg_string=svg, width=128, height=128))


if __name__ == '__main__':
    main()
