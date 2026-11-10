# Run-cycle pose study

- `male-run-4f-70x80.png` and `female-run-4f-70x80.png` are transparent 4-frame horizontal strips; each cell is exactly 70×80 px. They are the aligned review versions.
- `*-run-preview.gif` is the corresponding short loop for visual review, not an in-game asset.
- `cells/` retains the raw fitted poses. `aligned-4frame/` contains the registered cells, strips, GIFs, and `alignment.json` with per-frame horizontal offsets. Alignment holds the upper-body anchor steady while retaining vertical run bounce.
- `../frames/` contains the larger generated key poses. The poses were generated individually from the selected operator concepts; alignment does not invent or interpolate leg poses.
- This is still an animation study: four distinct poses are not the runtime's required 20-frame (`5×4`) run sheet, and the sheets remain outside `Hurrican/data/textures`.

Rebuild from the repository root:

```sh
bash tools/sprite-lab/build-run-study.sh
```

ImageMagick 6+ is required. The optional registration pass uses OpenCV and NumPy:

```sh
python3 -m pip install -r tools/sprite-lab/requirements-run-study.txt
```
