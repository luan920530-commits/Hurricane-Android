#!/usr/bin/env bash
set -euo pipefail

# Make a transparent 4-pose run-cycle study from selected AI key poses.
# This deliberately does NOT write into Hurrican/data/textures.
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/sheets/run-study"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$OUT/cells"

for operator in male female; do
  max_w=0
  max_h=0
  for n in 01 02 03 04; do
    src="$ROOT/frames/$operator/run-$n.png"
    cutout="$TMP/$operator-run-$n-cutout.png"
    trimmed="$TMP/$operator-run-$n-trim.png"
    [[ -f "$src" ]] || { echo "Missing frame: $src" >&2; exit 1; }
    # Remove only near-black background connected to the outside; retain dark ink.
    convert "$src" -alpha set -fuzz 4% -fill none -draw 'matte 0,0 floodfill' "$cutout"
    convert "$cutout" -trim +repage "$trimmed"
    read -r w h <<< "$(identify -format '%w %h' "$trimmed")"
    (( w > max_w )) && max_w=$w
    (( h > max_h )) && max_h=$h
  done

  # Crucial: all four poses share one scale based on the largest silhouette.
  # Per-frame fitting made crouched poses look like a camera zoom.
  scale_pct=$(awk -v w="$max_w" -v h="$max_h" 'BEGIN { s=66/w; v=76/h; if (v<s) s=v; printf "%.8f", s*100 }')
  cells=()
  for n in 01 02 03 04; do
    trimmed="$TMP/$operator-run-$n-trim.png"
    cell="$OUT/cells/$operator-run-$n.png"
    convert "$trimmed" -resize "${scale_pct}%" -gravity south -background none -extent 70x80 "$cell"
    cells+=("$cell")
  done

  # Raw aligned-size candidates; the optional anchor pass below centers torsos.
  convert -size 280x80 xc:none \
    "${cells[0]}" -geometry +0+0 -compose over -composite \
    "${cells[1]}" -geometry +70+0 -compose over -composite \
    "${cells[2]}" -geometry +140+0 -compose over -composite \
    "${cells[3]}" -geometry +210+0 -compose over -composite "$OUT/$operator-run-4f-70x80.png"
  convert "${cells[0]}" "${cells[1]}" "${cells[2]}" "${cells[3]}" \
    -delay 11 -loop 0 "$OUT/$operator-run-preview.gif"
done

# Register each pose to a shared upper-body anchor; leave vertical run bounce intact.
if python3 -c 'import cv2, numpy' >/dev/null 2>&1; then
  PYTHONPATH="${PYTHONPATH:-}" python3 "$ROOT/align-run-study.py"
  for operator in male female; do
    cp "$OUT/aligned-4frame/$operator-run-4f-70x80.png" "$OUT/$operator-run-4f-70x80.png"
    cp "$OUT/aligned-4frame/$operator-run-preview.gif" "$OUT/$operator-run-preview.gif"
  done
else
  echo "Optional registration skipped. Install: pip install -r $ROOT/requirements-run-study.txt" >&2
fi

echo "Wrote common-scale, aligned transparent 4-frame strips under $OUT"
