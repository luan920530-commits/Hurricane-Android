#!/usr/bin/env python3
"""Register generated run poses to a common upper-body anchor.

The source model can shift a character a few pixels from pose to pose. This
keeps the weapon/torso center from wandering while preserving the different
leg poses and their vertical running bounce. Uses only integer-pixel shifts so
this pass does not blur the 70x80 art. Requires opencv-python-headless + numpy.
"""
from pathlib import Path
import json
import shutil
import subprocess
import cv2
import numpy as np

ROOT = Path(__file__).resolve().parent
RUN_DIR = ROOT / "sheets" / "run-study"
SRC = RUN_DIR / "cells"
OUT = RUN_DIR / "aligned-4frame"
ANCHOR = (slice(6, 55), slice(8, 50))  # head/vest/arms, excluding the far muzzle


def anchor_center(frame: np.ndarray) -> tuple[float, float]:
    alpha = frame[ANCHOR[0], ANCHOR[1], 3].astype(np.float64) / 255.0
    yy, xx = np.mgrid[6:55, 8:50]
    weight = alpha
    total = weight.sum()
    if total < 1:
        raise ValueError("empty anchor region")
    return float((weight * xx).sum() / total), float((weight * yy).sum() / total)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    report = {
        "cell": [70, 80],
        "scale_normalization": "all four key poses use the same per-character scale, computed from the maximum trimmed width/height before fitting; prevents pose-dependent zoom",
        "alignment": "integer horizontal translation by alpha centroid in x=8..49,y=6..54; y preserved for run bounce",
        "operators": {},
    }
    for operator in ("male", "female"):
        frames = []
        for i in range(1, 5):
            path = SRC / f"{operator}-run-{i:02d}.png"
            image = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
            if image is None or image.shape != (80, 70, 4):
                raise ValueError(f"expected 70x80 RGBA cell: {path}")
            frames.append(image)
        anchors = [anchor_center(f) for f in frames]
        refx = anchors[0][0]
        offsets = [int(round(refx - x)) for x, _ in anchors]
        # Center the registered group in the cell without scaling or resampling.
        moved_boxes = []
        for image, dx in zip(frames, offsets):
            ys, xs = np.where(image[:, :, 3] > 8)
            moved_boxes.append((int(xs.min()) + dx, int(xs.max()) + dx))
        minx = min(a for a, _ in moved_boxes)
        maxx = max(b for _, b in moved_boxes)
        global_dx = int(round((69 - (maxx - minx)) / 2 - minx))
        offsets = [dx + global_dx for dx in offsets]

        aligned = []
        for i, (image, dx) in enumerate(zip(frames, offsets), 1):
            matrix = np.float32([[1, 0, dx], [0, 1, 0]])
            moved = cv2.warpAffine(image, matrix, (70, 80), flags=cv2.INTER_NEAREST,
                                   borderMode=cv2.BORDER_CONSTANT, borderValue=(0, 0, 0, 0))
            aligned.append(moved)
            cv2.imwrite(str(OUT / f"{operator}-run-{i:02d}.png"), moved)
        sheet = np.zeros((80, 280, 4), dtype=np.uint8)
        for i, frame in enumerate(aligned):
            sheet[:, i * 70:(i + 1) * 70] = frame
        cv2.imwrite(str(OUT / f"{operator}-run-4f-70x80.png"), sheet)
        report["operators"][operator] = {"anchor_centers_before": [[round(x, 2), round(y, 2)] for x, y in anchors], "x_offsets_px": offsets}
        print(f"{operator}: upper-body anchors registered; x offsets {offsets}")
    (OUT / "alignment.json").write_text(json.dumps(report, indent=2) + "\n")
    convert = shutil.which("convert")
    if convert:
        for operator in ("male", "female"):
            cells = [str(OUT / f"{operator}-run-{i:02d}.png") for i in range(1, 5)]
            subprocess.run([convert, "-delay", "11", *cells, "-loop", "0", str(OUT / f"{operator}-run-preview.gif")], check=True)


if __name__ == "__main__":
    main()
