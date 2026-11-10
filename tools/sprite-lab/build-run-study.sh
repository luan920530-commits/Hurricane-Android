#!/usr/bin/env bash
set -euo pipefail

# Make a transparent 4-pose run-cycle study from the selected AI key poses.
# This deliberately does NOT write into Hurrican/data/textures.
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/sheets/run-study"
mkdir -p "$OUT/cells"

for operator in male female; do
  cells=()
  for n in 01 02 03 04; do
    src="$ROOT/frames/$operator/run-$n.png"
    cell="$OUT/cells/$operator-run-$n.png"
    [[ -f "$src" ]] || { echo "Missing frame: $src" >&2; exit 1; }
    # The generated source has a flat black backdrop. Flood-fill only connected
    # near-black pixels, preserve the inked silhouette, then fit in a 70x80 cell.
    convert "$src" -alpha set -fuzz 4% -fill none -draw 'matte 0,0 floodfill' \
      -trim +repage -resize '66x76' -gravity south -background none -extent 70x80 "$cell"
    cells+=("$cell")
  done
  sheet="$OUT/$operator-run-4f-70x80.png"
  convert -size 280x80 xc:none \
    "${cells[0]}" -geometry +0+0 -compose over -composite \
    "${cells[1]}" -geometry +70+0 -compose over -composite \
    "${cells[2]}" -geometry +140+0 -compose over -composite \
    "${cells[3]}" -geometry +210+0 -compose over -composite "$sheet"
  # Convenient preview; the PNG above is the actual transparent sprite strip.
  convert "${cells[0]}" "${cells[1]}" "${cells[2]}" "${cells[3]}" \
    -set delay 11 -loop 0 "$OUT/$operator-run-preview.gif"
done

# Register each pose to a common upper-body anchor. If optional Python deps are
# unavailable, retain the raw fit and tell the artist how to run the alignment.
if python3 -c 'import cv2, numpy' >/dev/null 2>&1; then
  PYTHONPATH="${PYTHONPATH:-}" python3 "$ROOT/align-run-study.py"
  for operator in male female; do
    cp "$OUT/aligned-4frame/$operator-run-4f-70x80.png" "$OUT/$operator-run-4f-70x80.png"
    cp "$OUT/aligned-4frame/$operator-run-preview.gif" "$OUT/$operator-run-preview.gif"
  done
else
  echo "Optional alignment skipped. Install: pip install -r $ROOT/requirements-run-study.txt" >&2
fi

echo "Wrote aligned transparent 4-frame strips under $OUT"
