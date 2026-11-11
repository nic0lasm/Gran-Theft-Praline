#!/usr/bin/env bash
# Short sweep of the draw cost of the driven cars: ghost distance x full-detail
# distance, on an existing build, with the default traffic (16 driven + 24 far).
# One line from the root of the clone:
#
#   caffeinate -dimsu tools/perf/traffic-lod-sweep-mac.sh <build_dir> [duration_s=75]
#
# Cases are named g<ghost m>-l<full detail m>, e.g. g60-l12 runs
# -GhostDistance=60 -LodNear=12 (docs/performance/vehicle-cost.md section 7).
# Environment:
#   CASES          default "g90-l20 g75-l20 g60-l20 g90-l12 g90-l8 g60-l8" (g90-l20 = today's defaults)
#   TRAFFIC_ARGS   default "-Traffic=16 -FarTraffic=24 -Police=0"
#   DETERMINISTIC  1: the demo route made repeatable (-DemoDeterministic=1, street-demo);
#                  default 0: fixed camera (street-fixed), the only repeatable view without it
#   HEARTBEAT_S    seconds between heartbeat lines, default 60
#   DRY_RUN        1 runs stand-in steps (no game): checks paths, guards and heartbeat
#   DRY_STEP_S     length of a stand-in step, default 2
# Stops at once (exit 2) when the screen is locked. Each case runs under a time
# guard of 2 x (duration + 60 s); past it the case is killed and the sweep stops.
# Output: <build_dir>/bench/lod-sweep-<stamp>/SWEEP.md, one folder and log per case.
# Expected length: 6 cases x about 2 min.
set -uo pipefail

if command -v ioreg >/dev/null 2>&1 && ioreg -n Root -d1 -a | grep -A1 'CGSSessionScreenIsLocked' | grep -q '<true/>'; then
  echo "lod-sweep: the screen of this Mac is locked (CGSSessionScreenIsLocked): unlock it, then run the sweep again" >&2
  exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${1:?usage: traffic-lod-sweep-mac.sh <build_dir> [duration_s]}"
duration="${2:-75}"
dry="${DRY_RUN:-0}"
read -r -a cases <<< "${CASES:-g90-l20 g75-l20 g60-l20 g90-l12 g90-l8 g60-l8}"
traffic="${TRAFFIC_ARGS:--Traffic=16 -FarTraffic=24 -Police=0}"
if test "${DETERMINISTIC:-0}" = "1"; then
  scenario="street-demo"
  traffic="$traffic -DemoDeterministic=1"
else
  scenario="street-fixed"
fi

if test "$dry" != "1" && ! test -d "$build_dir/GranTheftPraline.app"; then
  echo "lod-sweep: no GranTheftPraline.app in $build_dir" >&2
  exit 1
fi
if pgrep -fl "UnrealEditor( |$)|GranTheftPraline-Mac" 2>/dev/null | grep -v UnrealEditorServices >/dev/null; then
  echo "lod-sweep: an Unreal or game process is already running, stop it first" >&2
  exit 1
fi

stamp="$(date +%Y%m%d-%H%M%S)"
out="$build_dir/bench/lod-sweep-$stamp"
mkdir -p "$out"
report="$out/SWEEP.md"
heartbeat="$out/HEARTBEAT"
{
  echo "# Traffic draw-cost sweep, $(date '+%Y-%m-%d %H:%M %Z')"
  echo
  echo "Scenario $scenario, ${duration} s per case, \`$traffic\`.$( test "$dry" = 1 && echo " DRY RUN: no numbers.")"
  echo
} > "$report"

echo "start" > "$out/.state"
date +%s > "$out/.since"
( while true; do
    sleep "${HEARTBEAT_S:-60}"
    echo "[$(date '+%H:%M:%S')] $(cat "$out/.state"), $(( $(date +%s) - $(cat "$out/.since") )) s in this step" | tee -a "$heartbeat"
  done ) &
beat_pid=$!

kill_tree() {
  local pid="$1" child
  for child in $(pgrep -P "$pid" 2>/dev/null); do
    kill_tree "$child"
  done
  kill "$pid" 2>/dev/null
}
trap 'kill_tree "$beat_pid"' EXIT

stand_in() {
  echo "would run:" "$@"
  sleep "${DRY_STEP_S:-2}"
}

limit=$(( 2 * (duration + 60) ))
total=${#cases[@]}
n=0
for name in "${cases[@]}"; do
  n=$(( n + 1 ))
  ghost="${name#g}"; ghost="${ghost%%-*}"
  lod="${name##*-l}"
  case "$ghost$lod" in
    ''|*[!0-9]*) echo "- $name: not a g<m>-l<m> case, skipped" >> "$report"; continue ;;
  esac
  args="$traffic -GhostDistance=$ghost -LodNear=$lod"
  echo "case $n/$total $name" > "$out/.state"
  date +%s > "$out/.since"
  echo "[$(date '+%H:%M:%S')] case $n/$total $name: $args" | tee -a "$heartbeat"
  if test "$dry" = 1; then
    stand_in "$script_dir/bench.sh" "$build_dir" "$out/$name" > "$out/$name.log" 2>&1 &
  else
    EXTRA_ARGS="$args" SCENARIOS="$scenario" DURATION="$duration" \
      "$script_dir/bench.sh" "$build_dir" "$out/$name" > "$out/$name.log" 2>&1 &
  fi
  pid=$!
  waited=0
  killed=0
  while kill -0 "$pid" 2>/dev/null; do
    if test "$waited" -ge "$limit"; then
      kill_tree "$pid"
      pkill -f "GranTheftPraline.app/Contents/MacOS" 2>/dev/null
      killed=1
      break
    fi
    sleep 2
    waited=$(( waited + 2 ))
  done
  wait "$pid" 2>/dev/null
  status=$?
  {
    echo "## $name: \`-GhostDistance=$ghost -LodNear=$lod\`"
    echo
    if test "$killed" = 1; then
      echo "KILLED after $limit s (time guard): sweep stopped, see $name.log"
    else
      echo "exit status $status"
      grep '^|' "$out/$name/summary.md" 2>/dev/null || echo "(no table, see $name.log)"
      grep -h "traffic budget:" "$out/$name"/*/stdout.log 2>/dev/null | tail -n 2 | sed 's/^/    /'
    fi
    echo
  } >> "$report"
  test "$killed" = 1 && exit 1
done
echo "[$(date '+%H:%M:%S')] done" | tee -a "$heartbeat"
echo "Done: $report"
