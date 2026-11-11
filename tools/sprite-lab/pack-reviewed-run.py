#!/usr/bin/env python3
"""Pack 20 reviewed 70x80 RGBA PNGs in Hurrican's row-major order.

This is a technical gate, not an anatomy or animation-quality validator. It
never installs textures into the game. Duplicate frames are rejected so that
missing poses cannot accidentally be disguised as a complete animation.
"""
import argparse
from pathlib import Path
import sys

import cv2
import numpy as np

LAB = Path(__file__).resolve().parent
OUTPUT_DIR = LAB / 'sheets' / 'run-study' / 'exports'


def pack(input_dir: Path) -> tuple[np.ndarray, list[str]]:
    paths = [input_dir / f'frame-{i:02}.png' for i in range(20)]
    missing = [p.name for p in paths if not p.is_file()]
    if missing:
        raise ValueError('Missing frames: ' + ', '.join(missing))
    frames = []
    seen = {}
    warnings = []
    for i, path in enumerate(paths):
        frame = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
        if frame is None or frame.dtype != np.uint8 or frame.shape != (80, 70, 4):
            raise ValueError(f'{path.name}: expected 70x80 8-bit RGBA PNG')
        alpha = frame[:, :, 3]
        if not (np.any(alpha == 0) and np.any(alpha > 8)):
            raise ValueError(f'{path.name}: expected transparent background and visible artwork')
        # Ignore hidden RGB values, which cannot make two visible poses distinct.
        visible = frame.copy()
        visible[alpha == 0] = 0
        fingerprint = visible.tobytes()
        if fingerprint in seen:
            raise ValueError(f'{path.name}: duplicate visible pixels with frame-{seen[fingerprint]:02}.png')
        seen[fingerprint] = i
        if any(np.any(edge > 8) for edge in (alpha[0, :], alpha[-1, :], alpha[:, 0], alpha[:, -1])):
            warnings.append(f'{path.name}: visible alpha touches cell border; check for clipping')
        frames.append(frame)
    sheet = np.zeros((320, 350, 4), dtype=np.uint8)
    for i, frame in enumerate(frames):
        x, y = (i % 5) * 70, (i // 5) * 80
        sheet[y:y + 80, x:x + 70] = frame
    return sheet, warnings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('frames', type=Path, help='directory containing frame-00.png through frame-19.png')
    parser.add_argument('operator', choices=('male', 'female'))
    args = parser.parse_args()
    try:
        sheet, warnings = pack(args.frames)
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        output = OUTPUT_DIR / f'{args.operator}-run-review-5x4.png'
        if not cv2.imwrite(str(output), sheet):
            raise ValueError(f'Could not write {output}')
        for warning in warnings:
            print('WARNING:', warning, file=sys.stderr)
        print(f'Created {output}. Visual review is still required; not installed into the game.')
    except (OSError, ValueError) as exc:
        print(f'Cannot pack: {exc}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
