#!/usr/bin/env bash
# One Mac slot for the city map, unattended: regenerate the street map (regen_city.sh, with
# the street details and landmarks), package it, run the 60 s M1 test that films itself,
# then the realism before/after against the previous city build. Stops at the first error.
#
# Usage: caffeinate -dimsu tools/city/city_slot_mac.sh <before_builds_dir> [version=0.3] [soak_s=60] [video_s=60]
#   <before_builds_dir>: builds root of the previous city package (holds <version>/GranTheftPraline.app
#   and its evidence), for the before/after. Empty string: no before/after.
#   BUILDS_DIR: where this slot packages (default <repo>/Builds/city-slot).
# One Unreal or game process at a time: regen_city.sh refuses to start if one runs.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
before_dir="${1-}"
version="${2:-0.3}"
soak="${3:-60}"
video="${4:-60}"
out="${BUILDS_DIR:-$repo/Builds/city-slot}"
t0=$(date +%s)
step() { echo "== $(( $(date +%s) - t0 )) s: $*"; }

step "regenerate the street map"
SKIP_PLAY=1 "$repo/tools/city/regen_city.sh"
step "package $version into $out"
BUILDS_DIR="$out" "$repo/tools/build/package-mac.sh" "$version"
if test -n "$before_dir"; then
  step "M1 test and before/after against $before_dir"
  "$repo/tools/city/evidence/before_after.sh" "$before_dir" "$out" "$version" "$soak" "$video"
else
  step "M1 test"
  BUILDS_DIR="$out" "$repo/tools/build/test-build-mac.sh" "$version" "$soak" "$video"
fi
step "done: $out/$version (evidence/ holds the capture, frame times and stills)"
