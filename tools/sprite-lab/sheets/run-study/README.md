# Run-cycle pose study

- `male-run-4f-70x80.png` and `female-run-4f-70x80.png` are transparent 4-frame horizontal strips; each cell is exactly 70×80 px.
- `cells/` keeps the four individual processed frames. `../frames/` contains the larger generated source key poses.
- `*-run-preview.gif` is a quick loop for visual review, not an in-game asset.
- The walk/run poses were generated individually from the selected operator concepts and are not bone-rigged in-betweens. Their identity, edges, frame-to-frame consistency, and ground line need visual review. The strips are an animation study, **not yet the game's 5×4 run texture** and are not installed in `Hurrican/data/textures`.

Rebuild with ImageMagick 6+:

```sh
bash tools/sprite-lab/build-run-study.sh
```
