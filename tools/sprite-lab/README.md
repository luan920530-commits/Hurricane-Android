# Hurrican operator sprite study

## Current status

The old Vex vector draft is **rejected and superseded**; it is not a design reference for final art. The selected painted concepts are in `concepts/`, and transparent working copies are in `painted/`. They are still static illustrations, not animation sheets, and have **not** been integrated into the game. Active Hurrican textures and game code remain unchanged.

A free, local rigging workbench is available in the live preview and preserved in `workbench/`. It is based on [Spruppet](https://github.com/HalfGlassEndless/spruppet) (MIT; upstream snapshot `1e571b1`), with a small Spanish landing page and local concept preloading. Run it from this folder with `python3 -m http.server 8000 --bind 0.0.0.0`; open `/` and choose a character. The editor runs in the browser and the character art stays in this workspace. Start with `Guess parts`, carefully correct the painted part map, then place joints and test motions. Save the Spruppet project JSON as well as motion PNGs. Those exports must still be inspected and adapted to Hurrican's loader before any integration.

## Selected artwork

- `concepts/male-operator.png` — selected male tactical-anime concept.
- `concepts/female-operator.png` — selected female tactical-anime concept.
- `painted/*-transparent.png` — dark backdrop removed; transparent PNG working copies.
- `painted/*-70x80.png` — static, aspect-preserving previews fitted inside a 70×80 game cell. They are **not** animated frames.

The male and female concepts have different source proportions and poses. Keep their hand-to-rifle contact and silhouette coherent during rigging; avoid treating simple repeated frames or crude rotations as finished animation.

## Hurrican format references

The legacy player uses 70×80 cells. Existing examples include `Hurrican/data/textures/p1_hurri_idle.png` (350×320 = 5×4 cells), `p1_hurri_laufen.png` (350×320), `p1_hurri_jump.png` (280×240), and `p1_hurri_crouch.png` (140×80). `Hurrican/src/Menu.cpp` also loads the idle sheet as a 5×4 grid. These dimensions are useful references, but each animation's frame order and runtime use must be checked in the loader before replacing anything.

## Safety / scope

- Do not overwrite `Hurrican/data/textures/p1_hurri_*.png` or modify player code until both rigs have useful, reviewed motions and the exported sheets have been checked against the runtime.
- Do not use paid tools, assets, or services.
- Keep the concepts and generated sheets isolated here until visual and technical validation passes.
- The previously committed Vex vector artwork remains in the repository history/output as a discarded experiment; do not extend it as the final character style.
