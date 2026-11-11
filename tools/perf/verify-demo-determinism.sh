#!/usr/bin/env bash
# Two identical deterministic demo runs, then a comparison of their positions.
#
# Usage: tools/perf/verify-demo-determinism.sh <build_dir> [duration_s] [extra game args...]
#   build_dir   folder holding GranTheftPraline.app
#   duration_s  default 90
# Example: tools/perf/verify-demo-determinism.sh Builds/perf-m1/0.3 90
# Exit code 0 on PASS (every 5 s instant within 150 cm / 10 degrees, same phases).
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${1:?usage: verify-demo-determinism.sh <build_dir> [duration_s] [extra args]}"
duration="${2:-90}"
if test "$#" -ge 2; then
  shift 2
else
  shift "$#"
fi
extra=("$@")
app="$build_dir/GranTheftPraline.app"
test -d "$app" || { echo "verify-demo: no build at $app" >&2; exit 1; }
exe="$(find "$app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
test -n "$exe" || { echo "verify-demo: no executable in $app" >&2; exit 1; }
if pgrep -fl "UnrealEditor|GranTheftPraline" > /dev/null; then
  echo "verify-demo: an Unreal or game process is already running" >&2
  exit 1
fi
out_dir="${OUT_DIR:-$build_dir/determinism/$(date +%Y%m%d-%H%M%S)}"
mkdir -p "$out_dir"

run() {
  local log="$1"
  "$exe" -SkipTitle -DemoRoute -DemoDeterministic=1 -DemoDriveTime=60 -NSAppSleepDisabled YES ${extra[@]+"${extra[@]}"} > "$log" 2>&1 &
  local pid=$!
  sleep "$duration"
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
  sleep 3
}

for n in 1 2; do
  echo "== run $n (${duration}s)"
  run "$out_dir/run$n.log"
done
python3 "$script_dir/compare_demo_positions.py" "$out_dir/run1.log" "$out_dir/run2.log"
