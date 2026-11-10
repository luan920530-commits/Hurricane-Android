# Hurrican operator sprite study

Original vector character draft built with the open-source Vex vector-art tool: https://github.com/uilianks/vex (MIT-licensed tool; this folder contains original art authored for Hurrican).

No paid service or external generation API is required. Vex exports SVG/PNG frame strips and JSON; its `--godot` option is not used here.

## Current draft

- Canvas target: 70 x 80 pixels per frame, matching the game's player-cell dimensions.
- Two coordinated designs: a female operator with tied-back hair and a male operator with a helmet; both have olive camouflage, tactical vest, and rifle.
- Draft animations: idle (8 frames), run (8), jump (8), crouch (4), shoot (5).
- This is a visual/animation study only. It does not replace the game's player textures yet: the game expects specific multi-row sheets and additional states. Those sheets must be assembled and validated against the existing loader before integration.

## Rebuild

With Vex installed and Node >= 22.18:

```sh
node /path/to/vex/bin/vex.js build tools/sprite-lab/operators.vx.js --out tools/sprite-lab/out --scales 1,2
```
