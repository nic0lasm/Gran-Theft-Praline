#!/usr/bin/env bash
# One-command ragdoll measurement on a packaged build, through the perf bench
# (tools/perf/bench.sh on feat/perf-bench). Three runs of the same scenario with the
# player standing still while -RagdollTest hits bodies with cars, falls and a crowd:
#   off   : -Ragdoll=0 (reactions refused: the baseline frame times)
#   full  : -RagdollTest (cap of the profile, 3 on m1)
#   lite  : -RagdollTest -RagdollLite=1 (cheaper ragdolls, cap 6 on m1)
# Prints the bench tables and the "GTPRagdollTest:" lines (PASS/FAIL, mean, p95, max).
#
# Usage: tools/ragdoll/measure-ragdoll-mac.sh <build_dir> [out_dir]
# Environment: DURATION (default 60 s per run); anything bench.sh understands.
# Unverified until its first Mac run: the log lines are searched in the bench output folder.
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bench="$script_dir/../perf/bench.sh"
build_dir="${1:?usage: measure-ragdoll-mac.sh <build_dir> [out_dir]}"
stamp="$(date +%Y%m%d-%H%M%S)"
out_dir="${2:-$build_dir/ragdoll/$stamp}"
test -x "$bench" || { echo "measure-ragdoll: $bench not found (feat/perf-bench not merged here)" >&2; exit 1; }
mkdir -p "$out_dir"

run() {
  local label="$1"
  shift
  echo "== $label: $*"
  SCENARIOS="${SCENARIOS:-street-fixed}" DURATION="${DURATION:-60}" EXTRA_ARGS="$*" \
    "$bench" "$build_dir" "$out_dir/$label"
}

run off -RagdollTest -Ragdoll=0
run full -RagdollTest
run lite -RagdollTest -RagdollLite=1

echo
echo "== GTPRagdollTest lines"
grep -rh "GTPRagdollTest\|ragdoll: " "$out_dir" || echo "no GTPRagdollTest line found: check where bench.sh stores the game output"
