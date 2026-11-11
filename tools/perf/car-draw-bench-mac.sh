#!/usr/bin/env bash
# Short bench of the draw levers of the driven cars (docs/performance/vehicle-cost.md
# section 8): fixed camera, 16/20/24 driven + 24 far, base against levers.
# One line from the root of the clone:
#
#   caffeinate -dimsu tools/perf/car-draw-bench-mac.sh <build_dir> [duration_s=75]
#
# Cases are named d<driven>-<lever>, lever one of:
#   base    no lever (today's defaults)
#   cheap   all levers: -CarDriverDraw=15 -CarLightDistance=40 -CarRenderStatic=30 -CarNearLOD=1
#   driver  -CarDriverDraw=15     lights  -CarLightDistance=40     static  -CarRenderStatic=30
#   near    -CarNearLOD=1         shadow  -CarShadows=0
# Environment:
#   CASES          default "d16-base d16-cheap d20-base d20-cheap d24-base d24-cheap
#                  d24-driver d24-lights d24-static d24-near d24-shadow"
#   TRAFFIC_ARGS   default "-FarTraffic=24 -Police=0" (-Traffic=<driven> is added per case)
#   HEARTBEAT_S    seconds between heartbeat lines, default 60
#   DRY_RUN        1 runs stand-in steps (no game): checks paths, guards and heartbeat
#   DRY_STEP_S     length of a stand-in step, default 2
# Stops at once (exit 2) when the screen is locked. Each case runs under a time
# guard of 2 x (duration + 60 s); past it the case is killed and the bench stops.
# Output: <build_dir>/bench/car-draw-<stamp>/DRAW.md, one folder and log per case.
# Expected length: 11 cases x about 2 min 20 = about 26 min.
set -uo pipefail

if command -v ioreg >/dev/null 2>&1 && ioreg -n Root -d1 -a | grep -A1 'CGSSessionScreenIsLocked' | grep -q '<true/>'; then
  echo "car-draw: the screen of this Mac is locked (CGSSessionScreenIsLocked): unlock it, then run the sweep again" >&2
  exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${1:?usage: car-draw-bench-mac.sh <build_dir> [duration_s]}"
duration="${2:-75}"
dry="${DRY_RUN:-0}"
read -r -a cases <<< "${CASES:-d16-base d16-cheap d20-base d20-cheap d24-base d24-cheap d24-driver d24-lights d24-static d24-near d24-shadow}"
traffic="${TRAFFIC_ARGS:--FarTraffic=24 -Police=0}"
scenario="street-fixed"

if test "$dry" != "1" && ! test -d "$build_dir/GranTheftPraline.app"; then
  echo "car-draw: no GranTheftPraline.app in $build_dir" >&2
  exit 1
fi
if pgrep -fl "UnrealEditor( |$)|GranTheftPraline-Mac" 2>/dev/null | grep -v UnrealEditorServices >/dev/null; then
  echo "car-draw: an Unreal or game process is already running, stop it first" >&2
  exit 1
fi

stamp="$(date +%Y%m%d-%H%M%S)"
out="$build_dir/bench/car-draw-$stamp"
mkdir -p "$out"
report="$out/DRAW.md"
heartbeat="$out/HEARTBEAT"
{
  echo "# Car draw levers, $(date '+%Y-%m-%d %H:%M %Z')"
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
  driven="${name%%-*}"; driven="${driven#d}"
  lever="${name#*-}"
  case "$lever" in
    base)   levers="" ;;
    cheap)  levers="-CarDriverDraw=15 -CarLightDistance=40 -CarRenderStatic=30 -CarNearLOD=1" ;;
    driver) levers="-CarDriverDraw=15" ;;
    lights) levers="-CarLightDistance=40" ;;
    static) levers="-CarRenderStatic=30" ;;
    near)   levers="-CarNearLOD=1" ;;
    shadow) levers="-CarShadows=0" ;;
    *)      levers="?" ;;
  esac
  case "$driven" in
    ''|*[!0-9]*) levers="?" ;;
  esac
  if test "$levers" = "?"; then
    echo "- $name: not a d<driven>-<lever> case, skipped" >> "$report"
    continue
  fi
  args="$traffic -Traffic=$driven $levers"
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
    echo "## $name: \`-Traffic=$driven $levers\`"
    echo
    if test "$killed" = 1; then
      echo "KILLED after $limit s (time guard): bench stopped, see $name.log"
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
