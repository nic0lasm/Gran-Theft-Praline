#!/usr/bin/env bash
# One command: cost of AI cars at 3, 6 and 12 cars, with and without the
# distance tiers (-VehicleLod=1), optionally with police chasing.
# Built on tools/perf/bench.sh (same route, same duration, load guard).
#
# Usage: tools/perf/vehicle-count-bench.sh <build_dir> [out_dir]
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

for n in $counts; do
  for lod in 0 1; do
    run="$out_dir/cars${n}_lod${lod}"
    EXTRA_ARGS="-Traffic=$n -VehicleLod=$lod $police_args" SCENARIOS="$scenario" \
      "$script_dir/bench.sh" "$build_dir" "$run" > "$run.log" 2>&1 || echo "run cars=$n lod=$lod exited with an error, see $run.log" >&2
    {
      echo "## cars=$n, VehicleLod=$lod"
      echo
      grep '^|' "$run/summary.md" 2>/dev/null || echo "(no data)"
      echo
    } >> "$report"
  done
done
echo "Done: $report"
cat "$report"
