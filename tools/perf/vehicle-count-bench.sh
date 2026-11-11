#!/usr/bin/env bash
# One command: cost of AI cars at 3, 6 and 12 cars, with and without the
# distance tiers (-VehicleLod=1), optionally with police chasing.
# Built on tools/perf/bench.sh (same route, same duration, load guard).
#
# Usage: tools/perf/vehicle-count-bench.sh <build_dir> [out_dir]
#   TIERS     default "0 1 2": none, -VehicleLod=1, then also -VehicleGhost=1 -TrafficPool=1
#   COUNTS    space-separated car counts, default "3 6 12"
#   POLICE    1 adds -WantedStars=2 -WantedHold (police chase), default 0
#   DURATION  seconds per run, default 90
#   SCENARIO  bench.sh scenario, default street-demo (the player drives)
#
# Writes <out_dir>/vehicle-cost.md: one row per run.
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${1:?usage: vehicle-count-bench.sh <build_dir> [out_dir]}"
stamp="$(date +%Y%m%d-%H%M%S)"
out_dir="${2:-$build_dir/bench/vehicles-$stamp}"
counts="${COUNTS:-3 6 12}"
police="${POLICE:-0}"
scenario="${SCENARIO:-street-demo}"
mkdir -p "$out_dir"

police_args=""
if test "$police" = "1"; then
  police_args="-WantedStars=2 -WantedHold"
else
  police_args="-Police=0"
fi

report="$out_dir/vehicle-cost.md"
{
  echo "# Vehicle cost bench $stamp"
  echo
  echo "Scenario $scenario, police=$police, ${DURATION:-90} s per run."
  echo
} > "$report"

# Tiers: 0 = none, 1 = -VehicleLod=1, 2 = tier 1 plus ghosts of far cars and the car pool.
tiers="${TIERS:-0 1 2}"
for n in $counts; do
  for lod in $tiers; do
    run="$out_dir/cars${n}_lod${lod}"
    case "$lod" in
      0) tier_args="-VehicleLod=0 -VehicleGhost=0 -TrafficPool=0 -FarTraffic=0" ;;
      1) tier_args="-VehicleLod=1 -VehicleGhost=0 -TrafficPool=0 -FarTraffic=0" ;;
      *) tier_args="-VehicleLod=1 -VehicleGhost=1 -TrafficPool=1 -FarTraffic=0" ;;
    esac
    EXTRA_ARGS="-Traffic=$n $tier_args $police_args" SCENARIOS="$scenario" \
      "$script_dir/bench.sh" "$build_dir" "$run" > "$run.log" 2>&1 || echo "run cars=$n tier=$lod exited with an error, see $run.log" >&2
    {
      echo "## cars=$n, $tier_args"
      echo
      grep '^|' "$run/summary.md" 2>/dev/null || echo "(no data)"
      echo
    } >> "$report"
  done
done
echo "Done: $report"
cat "$report"
