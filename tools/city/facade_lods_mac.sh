#!/usr/bin/env bash
# Facade kit LODs on the existing street map, then package, for the M1 bench of the
# optimisation thread (tools/perf/bench.sh). Replays only place_facades.py and the
# materials (FACADES_ONLY), so the rest of the map stays as released: opaque shop glass,
# no street details, unless GTP_SHOP_GLASS / GTP_CITY_DETAILS say otherwise.
#
# Usage: caffeinate -dimsu tools/city/facade_lods_mac.sh [version=0.3-lods]
#   PYTHON (venv with numpy, shapely, PIL), BUILDS_DIR (default <repo>/Builds/facade-lods),
#   GTP_KIT_LODS=0 to package the same map without LODs (the "before").
#   THEN_GLASS=interior: afterwards replay once more with that shop glass (realism thread's
#   interior-mapped windows) and package it as <version>-<glass>, so one slot gives two builds.
# One Unreal process at a time: refuses to start if one runs.
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
py="${PYTHON:-python3}"
version="${1:-0.3-lods}"
t0=$(date +%s)
step() { echo "== $(( $(date +%s) - t0 )) s: $*"; }
if pgrep -f "UnrealEditor |UnrealEditor$|GranTheftPraline.app" > /dev/null; then
  echo "an Unreal editor or the game is running: one at a time" >&2
  exit 1
fi
replay() {  # $1 shop glass, $2 version
  step "facade kit with LODs (GTP_KIT_LODS=${GTP_KIT_LODS:-1}), shop glass $1, materials"
  PATH="$(dirname "$(command -v "$py")"):$PATH" FACADES_ONLY=1 SKIP_PLAY=1 GTP_CITY_RIVERSIDE=0 \
    GTP_SHOP_GLASS="$1" GTP_CITY_DETAILS="${GTP_CITY_DETAILS:-0}" \
    "$repo/tools/city/unreal/run_city_import.sh" > /dev/null
  grep -o '"kit_lods": {[^}]*}' "$repo/GranTheftPraline/Saved/CityImport/facades_report.json" || \
    grep -o '"kit_lods": "[a-z]*"' "$repo/GranTheftPraline/Saved/CityImport/facades_report.json"
  grep -o '"shop_glass": "[a-z]*"' "$repo/GranTheftPraline/Saved/CityImport/materials_report.json"
  git -C "$repo" checkout -- assets/geo/processed 2> /dev/null || true   # manifest.json counts drift
  step "package $2"
  BUILDS_DIR="${BUILDS_DIR:-$repo/Builds/facade-lods}" "$repo/tools/build/package-mac.sh" "$2"
}
replay "${GTP_SHOP_GLASS:-opaque}" "$version"
if test -n "${THEN_GLASS:-}"; then
  replay "$THEN_GLASS" "$version-$THEN_GLASS"
fi
step "done: builds in ${BUILDS_DIR:-$repo/Builds/facade-lods}/, bench them with tools/perf/bench.sh"
