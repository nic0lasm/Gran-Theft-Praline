#!/usr/bin/env bash
# Static parked-car body, three steps (Blender, Python, Blender), about a minute per car.
# Usage: tools/vehicles/make_parked_static.sh <staging/vehicles> V-01 [V-02 ...]
# BLENDER=/path/to/Blender (default: blender on PATH or the macOS app).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
staging="${1:?staging/vehicles folder}"; shift
BL="${BLENDER:-$(command -v blender || echo /Applications/Blender.app/Contents/MacOS/Blender)}"
for id in "$@"; do
  s=$(date +%s)
  echo "[parked] $id: export"
  nice -n 10 "$BL" --background --python-exit-code 1 --python "$here/make_parked_static.py" -- --id "$id" --staging "$staging" --stage export
  echo "[parked] $id: decimate"
  nice -n 10 python3 "$here/decimate_parked.py" "$staging/$id/parked" --tris "${GTP_PARKED_TRIS:-8000}"
  echo "[parked] $id: finish"
  nice -n 10 "$BL" --background --python-exit-code 1 --python "$here/make_parked_static.py" -- --id "$id" --staging "$staging" --stage finish
  echo "[parked] $id: done in $(( $(date +%s) - s )) s"
done
