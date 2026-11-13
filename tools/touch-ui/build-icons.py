#!/usr/bin/env python3
"""Rasterize vendored Lucide icon sources for Hurrican's touch overlay.

Install resvg-py separately (pip install resvg-py). Output remains transparent,
white, and 128x128; the game supplies the button disc and pressed tint.
"""
from pathlib import Path
import resvg_py

HERE = Path(__file__).resolve().parent
TEXTURES = HERE.parents[1] / 'Hurrican' / 'data' / 'textures'
ICONS = {
    'jump': 'footprints', 'fire': 'crosshair', 'blitz': 'chevrons-right',
    'powerline': 'zap', 'grenade': 'bomb', 'smartbomb': 'radiation',
    'weapon': 'refresh-cw', 'pause': 'pause', 'debug': 'bug',
}


def main():
    for action, icon in ICONS.items():
        svg = (HERE / 'lucide' / f'{icon}.svg').read_text(encoding='utf-8')
        svg = svg.replace('currentColor', '#f5f8f2')
        png = resvg_py.svg_to_bytes(svg_string=svg, width=128, height=128)
        output = TEXTURES / f'touch_lucide_{action}.png'
        output.write_bytes(png)
        print(output)


if __name__ == '__main__':
    main()
