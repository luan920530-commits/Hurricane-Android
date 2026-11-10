#!/usr/bin/env python3
"""Register a candidate's torso to run-03 using SIFT; output a 70x80 study.

ImageMagick and OpenCV/NumPy are required. This experimental method aligns
existing art, it does not fix anatomy or generate missing poses. Inspect the
result and its edges; never writes to the game textures.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

import cv2
import numpy as np

ROOT = Path(__file__).resolve().parent


def run(*cmd):
    return subprocess.check_output(cmd, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operator', choices=('male', 'female'))
    args = parser.parse_args()
    op = args.operator
    base = ROOT / 'frames' / op
    src = cv2.imread(str(base / 'study-phase08.png'))
    ref = cv2.imread(str(base / 'run-03.png'))
    if src is None or ref is None:
        raise ValueError('Could not read source images')
    sift = cv2.SIFT_create()
    ka, da = sift.detectAndCompute(cv2.cvtColor(src, cv2.COLOR_BGR2GRAY), None)
    kb, db = sift.detectAndCompute(cv2.cvtColor(ref, cv2.COLOR_BGR2GRAY), None)
    if da is None or db is None:
        raise ValueError('Not enough torso features for registration')
    pairs = cv2.BFMatcher().knnMatch(da, db, k=2)
    good = [p for pair in pairs if len(pair) == 2 for p, q in [pair]
            if p.distance < .7 * q.distance and ka[p.queryIdx].pt[1] < src.shape[0] * .5
            and kb[p.trainIdx].pt[1] < ref.shape[0] * .5]
    if len(good) < 12:
        raise ValueError('Not enough torso matches; cannot safely register')
    points = np.float32([ka[m.queryIdx].pt for m in good])
    targets = np.float32([kb[m.trainIdx].pt for m in good])
    matrix, inliers = cv2.estimateAffinePartial2D(points, targets, method=cv2.RANSAC, ransacReprojThreshold=5)
    if matrix is None or inliers is None or int(inliers.sum()) < 12:
        raise ValueError('Registration failed')
    aligned = cv2.warpAffine(src, matrix, (ref.shape[1], ref.shape[0]), borderValue=(0, 0, 0))
    # Do not use a transformed candidate that touches the large canvas edge.
    if any(np.any(edge > 10) for edge in (aligned[0], aligned[-1], aligned[:, 0], aligned[:, -1])):
        raise ValueError('Registered art touches the large canvas edge; may be clipped')
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        max_w = max_h = 0
        for n in range(1, 5):
            out = tmp / f'ref-{n}.png'
            run('convert', str(base / f'run-{n:02}.png'), '-alpha', 'set', '-fuzz', '4%', '-fill', 'none',
                '-draw', 'matte 0,0 floodfill', '-trim', '+repage', str(out))
            w, h = map(int, run('identify', '-format', '%w %h', str(out)).split())
            max_w, max_h = max(max_w, w), max(max_h, h)
        scale = min(66 / max_w, 76 / max_h) * 100
        registered = tmp / 'registered.png'
        cv2.imwrite(str(registered), aligned)
        out = ROOT / 'sheets' / 'run-study' / 'candidates' / f'{op}-phase08-70x80.png'
        candidate = tmp / 'candidate.png'
        scaled = tmp / 'scaled.png'
        run('convert', str(registered), '-alpha', 'set', '-fuzz', '4%', '-fill', 'none', '-draw',
            'matte 0,0 floodfill', '-trim', '+repage', '-resize', f'{scale:.8f}%', str(scaled))
        width, height = map(int, run('identify', '-format', '%w %h', str(scaled)).split())
        if width > 70 or height > 80:
            raise ValueError(f'Scaled candidate is {width}x{height}; fitting it would crop the art')
        run('convert', str(scaled), '-gravity', 'south', '-background', 'none', '-extent', '70x80', str(candidate))
        # Place the torso alongside the registered run-03 cell using the same
        # alpha-centroid anchor. Keep a two-pixel bottom safety margin; do not
        # silently crop if a translation goes out of bounds.
        image = cv2.imread(str(candidate), cv2.IMREAD_UNCHANGED)
        target = cv2.imread(str(ROOT / 'sheets' / 'run-study' / 'aligned-4frame' / f'{op}-run-03.png'), cv2.IMREAD_UNCHANGED)
        if image is None or target is None or image.shape != (80, 70, 4) or target.shape != (80, 70, 4):
            raise ValueError('Expected RGBA 70x80 candidate and registered reference')
        def center_x(frame):
            weights = frame[6:55, 8:50, 3].astype(np.float64)
            if not weights.sum():
                raise ValueError('Empty torso anchor')
            return float((weights * np.arange(8, 50)[None, :]).sum() / weights.sum())
        dx = int(round(center_x(target) - center_x(image)))
        ys, xs = np.where(image[:, :, 3] > 8)
        dy = min(0, 77 - int(ys.max()))
        if xs.min() + dx < 0 or xs.max() + dx >= 70 or ys.min() + dy < 0:
            raise ValueError('Registration would clip the candidate cell')
        image = cv2.warpAffine(image, np.float32([[1, 0, dx], [0, 1, dy]]), (70, 80),
                               flags=cv2.INTER_NEAREST, borderMode=cv2.BORDER_CONSTANT, borderValue=(0, 0, 0, 0))
        out.parent.mkdir(parents=True, exist_ok=True)
        if not cv2.imwrite(str(out), image):
            raise ValueError(f'Could not write {out}')
        print(f'{op}: {int(inliers.sum())}/{len(good)} torso matches, cell shift ({dx},{dy}); wrote {out}')
        print('Study only: inspect anatomy and continuity before using.')


if __name__ == '__main__':
    main()
