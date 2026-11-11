#!/usr/bin/env bash
# Traffic seen by the player, 12 -> 25 -> 40 vehicles: driven cars only, or
# driven cars plus far traffic drawn as instances (-FarTraffic, section 6 of
# docs/performance/vehicle-cost.md). Every case has the distance tiers, the
# ghost mode and the car pool on. Built on tools/perf/bench.sh.
#
# Usage: tools/perf/traffic-perceived-bench.sh <build_dir> [out_dir] [cases...]
#   cases (default: all)
#     d12        -Traffic=12                    (reference: palier B)
#     d25        -Traffic=25                    (25 driven cars)
#     d12-f13    -Traffic=12 -FarTraffic=13     (25 perceived)
#     d40        -Traffic=40                    (40 driven cars)
#     d12-f28    -Traffic=12 -FarTraffic=28     (40 perceived)
#     d16-f24-p  -Traffic=16 -FarTraffic=24 -ParkedCars=1 (40 perceived + parked cars)
#   POLICE=1 adds a police chase (-WantedStars=2 -WantedHold); DURATION (default 90),
#   SCENARIO (default street-demo).
# Far traffic is drawn with puppet cars by default (no extra mesh needed); the
# parked cars of d16-f24-p need the /Game/Vehicles/Parked meshes in the build.
#
# Writes <out_dir>/traffic-perceived.md: the bench table and the traffic budget
# lines of the game (CPU time of the drivers, network and far traffic per frame).
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${1:?usage: traffic-perceived-bench.sh <build_dir> [out_dir] [cases...]}"
stamp="$(date +%Y%m%d-%H%M%S)"
out_dir="${2:-$build_dir/bench/traffic-$stamp}"
shift $(( $# >= 2 ? 2 : 1 ))
scenario="${SCENARIO:-street-demo}"
mkdir -p "$out_dir"

tiers="-VehicleLod=1 -VehicleGhost=1 -TrafficPool=1"
police="-Police=0"
test "${POLICE:-0}" = "1" && police="-WantedStars=2 -WantedHold"

args_for() {
  case "$1" in
    d12)       echo "-Traffic=12" ;;
    d25)       echo "-Traffic=25" ;;
    d12-f13)   echo "-Traffic=12 -FarTraffic=13" ;;
    d40)       echo "-Traffic=40" ;;
    d12-f28)   echo "-Traffic=12 -FarTraffic=28" ;;
    d16-f24-p) echo "-Traffic=16 -FarTraffic=24 -ParkedCars=1" ;;
    *) return 1 ;;
  esac
}

if test "$#" -gt 0; then
  cases=("$@")
else
  cases=(d12 d25 d12-f13 d40 d12-f28 d16-f24-p)
fi

report="$out_dir/traffic-perceived.md"
{
  echo "# Perceived traffic bench $stamp"
  echo
  echo "Scenario $scenario, ${DURATION:-90} s per run, every case with \`$tiers $police\`."
  echo
} > "$report"

for name in "${cases[@]}"; do
  extra="$(args_for "$name")" || { echo "unknown case $name" >&2; continue; }
  run="$out_dir/$name"
  EXTRA_ARGS="$extra $tiers $police" SCENARIOS="$scenario" \
    "$script_dir/bench.sh" "$build_dir" "$run" > "$run.log" 2>&1 || echo "case $name exited with an error, see $run.log" >&2
  {
    echo "## $name: \`$extra\`"
    echo
    grep '^|' "$run/summary.md" 2>/dev/null || echo "(no data)"
    echo
    # The game logs one budget line every 5 s; the last three are enough.
    grep -h "traffic budget:\|far traffic:" "$run"/*/stdout.log 2>/dev/null | tail -n 4 | sed 's/^/    /'
    echo
  } >> "$report"
done
echo "Done: $report"
cat "$report"
