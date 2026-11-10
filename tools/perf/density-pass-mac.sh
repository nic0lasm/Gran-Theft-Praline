#!/usr/bin/env bash
# One Mac slot for the density tests of the performance push: ONE package, then
# every density bench on it, unattended. Each bench is logged; a failed bench is
# recorded and the pass goes on (a failed package stops it).
#
#   1. package (Shipping)                 tools/build/package-mac.sh
#   2. cars 3 / 6 / 12, -VehicleLod 0/1   tools/perf/vehicle-count-bench.sh
#   3. pedestrians 0 / 12 / 30 / 60       tools/pedestrians/bench-crowd-mac.sh
#   4. parked cars as instances 0 / 1     tools/perf/bench.sh -ParkedCars
#   5. ragdolls off / full / lite         tools/ragdoll/measure-ragdoll-mac.sh
#
# Usage, from the root of the clone, nothing else running:
#   caffeinate -dimsu tools/perf/density-pass-mac.sh [version=perf-density] [duration_s=90]
# Environment: BUILDS_DIR (default <repo>/Builds), SKIP_PACKAGE=1 to reuse a build,
#   CROWD_CASES (default "n0 n12 n30 n60"), COUNTS (default "3 6 12").
# Output: <BUILDS_DIR>/<version>/density-<stamp>/DENSITY.md and one log per step.
# Expected length: package 15-25 min, cars ~25 min (3 counts x 3 tiers), crowd ~10 min,
#   parked ~4 min, ragdolls ~5 min.
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
version="${1:-perf-density}"
duration="${2:-90}"
builds_dir="${BUILDS_DIR:-$repo_root/Builds}"
export BUILDS_DIR="$builds_dir"
build_dir="$builds_dir/$version"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$build_dir/density-$stamp"
mkdir -p "$out"
report="$out/DENSITY.md"
cd "$repo_root" || exit 1

if pgrep -fl "UnrealEditor|GranTheftPraline" | grep -v UnrealEditorServices >/dev/null; then
  echo "density-pass: an Unreal or game process is already running, stop it first" >&2
  exit 1
fi

{
  echo "# Density pass $version, $(date '+%Y-%m-%d %H:%M %Z'), commit $(git rev-parse --short HEAD)"
  echo
  echo "Profile m1, ${duration} s per run. Compare runs of this pass with each other only."
  echo
} > "$report"

step() {
  local name="$1"
  shift
  local start status
  start=$(date +%s)
  echo "=== $name"
  "$@" > "$out/$name.log" 2>&1
  status=$?
  echo "- $name: $( test $status -eq 0 && echo OK || echo "FAILED ($status)" ), $(( $(date +%s) - start )) s, log $name.log" >> "$report"
  return $status
}

# Run-time data of the switches measured here (city tools need shapely: PYTHON=<venv>/bin/python).
python="${PYTHON:-python3}"
step parking-data "$python" tools/city/parking.py --map L_City_RueDeLaRepublique

if test "${SKIP_PACKAGE:-0}" != "1"; then
  if ! step package tools/build/package-mac.sh "$version"; then
    grep -hE ": (fatal )?error:|Error: " "$out/package.log" | sort -u | head -n 80 > "$out/compile-errors.txt"
    echo "- package failed: errors in compile-errors.txt" >> "$report"
    exit 1
  fi
fi
test -d "$build_dir/GranTheftPraline.app" || { echo "- no build at $build_dir" >> "$report"; exit 1; }

COUNTS="${COUNTS:-3 6 12}" DURATION="$duration" step cars tools/perf/vehicle-count-bench.sh "$build_dir" "$out/cars"
# shellcheck disable=SC2086
step crowd tools/pedestrians/bench-crowd-mac.sh "$version" "$duration" ${CROWD_CASES:-n0 n12 n30 n60}
# Parked cars as instances (needs /Game/Vehicles/Parked from import_parked_static.py before the package).
for parked in 0 1; do
  EXTRA_ARGS="-Traffic=6 -VehicleLod=1 -ParkedCars=$parked" SCENARIOS="street-demo" DURATION="$duration" \
    step "parked$parked" tools/perf/bench.sh "$build_dir" "$out/parked$parked"
done
DURATION=60 step ragdolls tools/ragdoll/measure-ragdoll-mac.sh "$build_dir" "$out/ragdolls"

# The tables, gathered in one file.
{
  echo
  echo "## Cars"
  echo
  cat "$out/cars/vehicle-cost.md" 2>/dev/null || echo "(no table, see cars.log)"
  echo
  echo "## Pedestrians"
  echo
  latest_crowd="$(ls -dt "$build_dir"/evidence/crowd-* 2>/dev/null | head -n 1)"
  cat "$latest_crowd/SUMMARY.md" 2>/dev/null || echo "(no table, see crowd.log)"
  echo
  echo "## Parked cars (6 traffic cars, -VehicleLod=1)"
  echo
  for parked in 0 1; do echo "-ParkedCars=$parked"; grep '^|' "$out/parked$parked/summary.md" 2>/dev/null || echo "(no data)"; echo; done
  grep -h "parked cars:" "$out"/parked1/*.log "$out"/parked1/*/*.log 2>/dev/null | head -n 3
  echo
  echo "## Ragdolls"
  echo
  find "$out/ragdolls" -name summary.md -exec cat {} \; 2>/dev/null || true
  grep -h "GTPRagdollTest:" "$out/ragdolls.log" 2>/dev/null || true
} >> "$report"
echo "Done: $report"
