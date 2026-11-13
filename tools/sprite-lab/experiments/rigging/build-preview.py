#!/usr/bin/env python3
"""Fit the experimental 20-frame Spruppet exports for visual review only.

Requires ImageMagick, OpenCV, and NumPy. All cells of a character use the
same scale and location. No files in the game are changed.
"""
from pathlib import Path
import subprocess
import tempfile
import cv2
import numpy as np

HERE = Path(__file__).resolve().parent


def main():
    for op in ('female', 'male'):
        source = cv2.imread(str(HERE / f'{op}-run-raw.png'), cv2.IMREAD_UNCHANGED)
        if source is None or source.ndim != 3 or source.shape[2] != 4:
            raise ValueError(f'{op}: expected RGBA sprite strip')
        h, width = source.shape[:2]
        if width % 20:
            raise ValueError(f'{op}: expected exactly 20 equal cells')
        w = width // 20
        scale = min(68 / w, 76 / h)
        new_w, new_h = max(1, round(w * scale)), max(1, round(h * scale))
        sheet = np.zeros((320, 350, 4), dtype=np.uint8)
        hashes = set()
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            frames = []
            for i in range(20):
                original = tmp / 'input.png'
                scaled = tmp / 'scaled.png'
                cv2.imwrite(str(original), source[:, i*w:(i+1)*w])
                # ImageMagick resizes RGBA with alpha-aware interpolation.
                subprocess.run(['convert', str(original), '-resize', f'{new_w}x{new_h}!', str(scaled)], check=True)
                image = cv2.imread(str(scaled), cv2.IMREAD_UNCHANGED)
                cell = np.zeros((80, 70, 4), dtype=np.uint8)
                x, y = (70 - new_w)//2, 78-new_h
                cell[y:y+new_h, x:x+new_w] = image
                sheet[(i//5)*80:(i//5+1)*80, (i%5)*70:(i%5+1)*70] = cell
                hashes.add(cell.tobytes())
                frame = tmp / f'{i:02}.png'
                cv2.imwrite(str(frame), cell)
                frames.append(str(frame))
            if len(hashes) < 20:
                raise ValueError(f'{op}: only {len(hashes)} distinct cells')
            output = HERE / f'{op}-experimental-5x4.png'
            if not cv2.imwrite(str(output), sheet):
                raise ValueError(f'cannot write {output}')
            subprocess.run(['convert', '-dispose', 'background', '-delay', '8', *frames,
                            '-loop', '0', str(HERE / f'{op}-experimental-preview.gif')], check=True)
            print(f'{op}: 20 distinct frames, {w}x{h} -> {new_w}x{new_h}; {output.name} (draft only)')


if __name__ == '__main__':
    main()
