#!/usr/bin/env python3
"""Check GIF timing/disposal and detect alpha ghosts in the run-study previews.

Requires ImageMagick's identify/convert and the run-study OpenCV/NumPy packages.
This does not validate anatomy, frame order, or artistic quality.
"""
from pathlib import Path
import subprocess
import tempfile

import cv2
import numpy as np

ROOT = Path(__file__).resolve().parent / 'sheets' / 'run-study'


def run(*args: str) -> str:
    return subprocess.check_output(args, text=True).strip()


def alpha(path: Path) -> np.ndarray:
    image = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if image is None:
        raise ValueError(f'cannot read {path}')
    if image.ndim == 3 and image.shape[2] == 4:
        return image[:, :, 3]
    return np.full(image.shape[:2], 255, np.uint8)


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for location in (ROOT, ROOT / 'aligned-4frame'):
            for operator in ('male', 'female'):
                gif = location / f'{operator}-run-preview.gif'
                metadata = run('identify', '-format', '%w %h %T %D\n', str(gif)).splitlines()
                if metadata != ['70 80 11 Background'] * 4:
                    raise ValueError(f'{gif}: unexpected frame count, size, delay or disposal: {metadata}')
                run('convert', str(gif), '-coalesce', str(tmp / 'coalesced-%d.png'))
                for i in range(4):
                    single = tmp / 'single.png'
                    run('convert', f'{gif}[{i}]', '+repage', str(single))
                    if not np.array_equal(alpha(single), alpha(tmp / f'coalesced-{i}.png')):
                        raise ValueError(f'{gif}: leftover alpha detected in frame {i + 1}')
                print(f'{gif.relative_to(ROOT)}: 4 frames, disposal and alpha OK')


if __name__ == '__main__':
    main()
