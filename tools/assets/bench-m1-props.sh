#!/usr/bin/env bash
# Before/after bench of the M1 prop rules, one command (about 2 x 6 min at DURATION=90):
#   tools/assets/bench-m1-props.sh <build_before> <build_after> [out_dir]
# <build_before>: packaged build with the current (4K) props; <build_after>: the same build
# repackaged after `apply-m1-rules.sh prepare` + `import` (the 25 street props in 2K / M1 caps).
# Runs tools/perf/bench.sh (branch feat/perf-bench) on both with the same scenarios, the "after" run
# compared to the "before" one (REFERENCE): the bench fails on a p95 regression above 2 ms.
# Each bench is under tools/common/guard.py. Needs the game closed and no other Unreal.
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
before="${1:?build folder before}"; after="${2:?build folder after}"
out="${3:-$repo/assets/.m1-bench/$(date +%Y%m%d-%H%M%S)}"
dur="${DURATION:-90}"
scen="${SCENARIOS:-street-fixed street-demo}"
G=(python3 "$repo/tools/common/guard.py")
bench="$repo/tools/perf/bench.sh"
test -x "$bench" || { echo "missing $bench (merge feat/perf-bench first)" >&2; exit 1; }
for b in "$before" "$after"; do test -d "$b" || { echo "no build folder: $b" >&2; exit 1; }; done
if pgrep -x UnrealEditor >/dev/null || pgrep -x UnrealEditor-Cmd >/dev/null || pgrep -f '[G]ranTheftPraline-Mac' >/dev/null; then
  echo "an Unreal or the game is running: stop." >&2; exit 3
fi
mkdir -p "$out"
n=$(echo $scen | wc -w)
max=$(( (dur + 60) * n * 2 + 120 ))   # 2x the planned time of one bench
echo "[m1-bench] before: $before"
SCENARIOS="$scen" DURATION="$dur" "${G[@]}" --name bench-before --max "$max" -- "$bench" "$before" "$out/before"
ref="$(ls "$out"/before/results.csv 2>/dev/null || true)"
echo "[m1-bench] after: $after"
REFERENCE="$ref" SCENARIOS="$scen" DURATION="$dur" "${G[@]}" --name bench-after --max "$max" -- "$bench" "$after" "$out/after" || echo "[m1-bench] the after-bench reported a regression or failed: see $out/after/summary.md"
echo "[m1-bench] done: $out/before/summary.md and $out/after/summary.md"
