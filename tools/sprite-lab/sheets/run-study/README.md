# Run-cycle pose study

- `male-run-4f-70x80.png` and `female-run-4f-70x80.png` are transparent 4-frame horizontal strips; each cell is exactly 70×80 px. They are the aligned review versions.
- `*-run-preview.gif` is the corresponding short loop for visual review, not an in-game asset. Each frame clears to the transparent background before the next frame, preventing leftover silhouettes.
- `original-run-reference.gif` shows all 20 frames from the original `Hurrican/data/textures/p1_hurri_laufen.png` in row-major order, at a constant illustrative 60 ms per frame (not a measurement of runtime speed). The original has wide-stride poses around indices 0/10, a passing pose around 5/15, and intermediate positions between them. The four operator poses are not a frame-for-frame recreation of that sequence; additional individually reviewed drawings are still needed.
- All four key poses for a character are fitted using one shared scale (the largest silhouette sets it), so a tucked pose cannot appear to zoom in. `aligned-4frame/` also registers the upper-body anchor horizontally while retaining vertical run bounce; it contains the final preview cells, strips, GIFs, and measured offsets.
- Key pose 2 was replaced after review. It now shows two legs and two boots, without the duplicated limb from the earlier draft.
- `cells/` retains the common-scale but not yet registered intermediate cells.
- `../frames/` contains the larger generated key poses. The poses were generated individually from the selected operator concepts; alignment does not invent or interpolate leg poses.
- This is still an animation study: four poses are not the runtime's required 20-frame (`5×4`) run sheet. At least 16 slots per character still need new or deliberately constructed, individually reviewed poses; no missing slots have been filled by silently repeating frames. The sheets remain outside `Hurrican/data/textures`.
- `../../frames/male/candidate-crossing.png` and `../../frames/female/candidate-passing-revised.png` are experimental passing poses only. The previous female crossing candidate was rejected after the user's report of a third leg. The revised candidate visibly has two boots and two separated legs, but is close to the existing run-03 and has not been registered into the animation or approved as a matching original frame. Visual anatomy and continuity review is still necessary, including at 70×80.
- Automated preview checks: `python3 tools/sprite-lab/check-run-previews.py` (requires ImageMagick, OpenCV and NumPy). These checks can detect leftover GIF alpha but cannot prove that a drawing has anatomically correct legs.

Rebuild from the repository root:

```sh
bash tools/sprite-lab/build-run-study.sh
```

ImageMagick 6+ is required. The optional registration pass uses OpenCV and NumPy:

```sh
python3 -m pip install -r tools/sprite-lab/requirements-run-study.txt
```
