# Run-cycle pose study

- `male-run-4f-70x80.png` and `female-run-4f-70x80.png` are transparent 4-frame horizontal strips; each cell is exactly 70×80 px. They are the aligned review versions.
- `*-run-preview.gif` is the corresponding short loop for visual review, not an in-game asset.
- All four key poses for a character are fitted using one shared scale (the largest silhouette sets it), so a tucked pose cannot appear to zoom in. `aligned-4frame/` also registers the upper-body anchor horizontally while retaining vertical run bounce; it contains the final preview cells, strips, GIFs, and measured offsets.
- Key pose 2 was replaced after review. It now shows two legs and two boots, without the duplicated limb from the earlier draft.
- `cells/` retains the common-scale but not yet registered intermediate cells.
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
