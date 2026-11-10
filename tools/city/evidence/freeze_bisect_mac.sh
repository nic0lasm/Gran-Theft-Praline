#!/usr/bin/env bash
# Bisect the launch freeze of the 3883236 builds (game alive, no frame) between the code and
# the facades-only replays, unattended. Three builds, each followed by a 30 s launch:
#   A  full regeneration, sign atlas off   (code of the head, like 799c8d5 otherwise)
#   B  A then one FACADES_ONLY replay, atlas off   (the replay the bench used)
#   C  full regeneration, sign atlas on
# Read: A freezes -> the code; A runs and B freezes -> the replay; only C freezes -> the atlas.
# Development packages, so the game writes ~/Library/Logs/GranTheftPraline/GranTheftPraline.log.
#
# Usage: caffeinate -dimsu tools/city/evidence/freeze_bisect_mac.sh [steps=ABC]
#   PYTHON (venv with numpy, scipy, shapely, PIL), BUILDS_DIR (default <repo>/Builds/freeze-bisect).
# Writes <BUILDS_DIR>/FREEZE.md. One Unreal or game process at a time. The map and city
# content found at the start are restored at the end (checked by md5).
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../.." && pwd)"
steps="${1:-ABC}"
out="${BUILDS_DIR:-$repo/Builds/freeze-bisect}"
mkdir -p "$out"
report="$out/FREEZE.md"
echo "# Freeze bisect, $(date -u +%FT%TZ), commit $(git -C "$repo" rev-parse --short HEAD)" > "$report"
# Everything the city scripts write in the project is saved first and put back at the end,
# even on error, then checked by checksum: the map left by the threads before stays as it was.
proj="$repo/GranTheftPraline"
kept=(Content/Maps/L_City_RueDeLaRepublique.umap Content/Environment/City Content/Environment/Facades/LyonKit
      Content/City Plugins/CityBrands/Content)
backup="$out/backup"
sums() { (cd "$1" && for k in "${kept[@]}"; do test -e "$k" && find "$k" -type f -print0; done | sort -z | xargs -0 md5 -r) ; }
rm -rf "$backup"; mkdir -p "$backup"
for k in "${kept[@]}"; do
  if test -e "$proj/$k"; then mkdir -p "$backup/$(dirname "$k")"; cp -Rp "$proj/$k" "$backup/$k"; fi
done
sums "$proj" > "$out/before.md5"
restore() {
  for k in "${kept[@]}"; do
    if test -e "$backup/$k"; then rm -rf "${proj:?}/$k"; mkdir -p "$proj/$(dirname "$k")"; cp -Rp "$backup/$k" "$proj/$k"; fi
  done
  sums "$proj" > "$out/after.md5"
  if cmp -s "$out/before.md5" "$out/after.md5"; then echo "- restore: map and city content identical to before (md5)" >> "$report"
  else echo "- restore: MISMATCH, see before.md5 / after.md5" >> "$report"; fi
  git -C "$repo" checkout -- assets/geo/processed 2> /dev/null || true
}
trap restore EXIT
export PATH="$(dirname "$(command -v "${PYTHON:-python3}")"):$PATH"   # run_city_import.sh calls python3
common=(GTP_SHOP_GLASS=opaque GTP_CITY_DETAILS=0 SKIP_PLAY=1)

launch() {  # $1 version
  BUILDS_DIR="$out" "$repo/tools/build/package-mac.sh" "$1" Development > "$out/$1-package.log" 2>&1
  rc=0
  BUILDS_DIR="$out" "$repo/tools/build/test-build-mac.sh" "$1" 30 0 > "$out/$1-test.log" 2>&1 || rc=$?
  verdict="$(grep -m1 '^verdict=' "$out/$1-test.log" || echo "verdict=none")"
  frames="$(grep -m1 '^frame ' "$out/$1-test.log" || echo "no frame times")"
  cp ~/Library/Logs/GranTheftPraline/GranTheftPraline.log "$out/$1-game.log" 2> /dev/null || true
  echo "- $1: $verdict (test exit $rc); $frames; game log $1-game.log" >> "$report"
  git -C "$repo" checkout -- assets/geo/processed 2> /dev/null || true
}

case "$steps" in *A*|*B*)
  env "${common[@]}" GTP_SIGN_ATLAS=0 "$repo/tools/city/regen_city.sh" > "$out/A-regen.log" 2>&1
  case "$steps" in *A*) launch freeze-A ;; esac
  case "$steps" in *B*)
    env "${common[@]}" GTP_SIGN_ATLAS=0 FACADES_ONLY=1 GTP_CITY_RIVERSIDE=0 \
      "$repo/tools/city/unreal/run_city_import.sh" > "$out/B-replay.log" 2>&1
    launch freeze-B ;;
  esac ;;
esac
case "$steps" in *C*)
  env "${common[@]}" GTP_SIGN_ATLAS=1 "$repo/tools/city/regen_city.sh" > "$out/C-regen.log" 2>&1
  launch freeze-C ;;
esac
cat "$report"
