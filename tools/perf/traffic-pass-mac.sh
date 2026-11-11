#!/usr/bin/env bash
# One Mac slot for the perceived traffic (12 -> 25 -> 40 vehicles), unattended,
# in one line from the root of the clone:
#
#   caffeinate -dimsu tools/perf/traffic-pass-mac.sh [version=perf-traffic] [duration_s=90]
#
# Unless PACKAGE_ONLY=1, it stops at once (exit 2) when the screen is locked.
# Steps, each under a time guard (killed past its limit, the pass stops):
#   1. package (Shipping)            tools/build/package-mac.sh         limit 50 min
#   2. 30 s control launch           tools/build/test-build-mac.sh      limit 3 min
#   3. one bench per traffic case    tools/perf/traffic-perceived-bench.sh, limit 2 x (duration + 60 s) each
# A heartbeat line goes to the terminal and to HEARTBEAT every minute: step,
# case n of N, seconds in the step.
#
# Environment:
#   CASES        default "d12 d25 d12-f13 d40 d12-f28 d16-f24-p" (see traffic-perceived-bench.sh)
#   PACKAGE_ONLY 1 compiles and packages, launches nothing (works with the screen locked)
#   SKIP_BUILD   1 measures only, on <BUILDS_DIR>/<version>/GranTheftPraline.app (SKIP_PACKAGE=1 too)
#   MAP_MD5      expected md5 of the map: the pass refuses to start on another map
#   BUILDS_DIR   default <repo>/Builds
#   DRY_STEP_S   length of each stand-in step (default 3; above 60 trips the time guard)
#   HEARTBEAT_S  seconds between heartbeat lines, default 60
#   DRY_RUN      1 runs every step as a short stand-in (no Unreal, no game): checks
#                the paths, the guards and the heartbeat, e.g. from a folder with a space
# Output: <BUILDS_DIR>/<version>/traffic-<stamp>/TRAFFIC.md, one log per step.
# Expected length: package 4 to 25 min, control 1 min, 6 cases x about 2 min.
set -uo pipefail

package_only="${PACKAGE_ONLY:-0}"
skip_build="${SKIP_BUILD:-${SKIP_PACKAGE:-0}}"

# A locked screen throttles or stops the game's rendering on macOS: no measure then.
# (Not needed to compile and package: PACKAGE_ONLY=1 skips this test.)
if test "$package_only" != "1" && command -v ioreg >/dev/null 2>&1 \
  && ioreg -n Root -d1 -a | grep -A1 'CGSSessionScreenIsLocked' | grep -q '<true/>'; then
  echo "traffic-pass: the screen of this Mac is locked (CGSSessionScreenIsLocked): unlock it, then run the pass again" >&2
  exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
version="${1:-perf-traffic}"
duration="${2:-90}"
builds_dir="${BUILDS_DIR:-$repo_root/Builds}"
export BUILDS_DIR="$builds_dir"
build_dir="$builds_dir/$version"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$build_dir/traffic-$stamp"
mkdir -p "$out"
report="$out/TRAFFIC.md"
heartbeat="$out/HEARTBEAT"
dry="${DRY_RUN:-0}"
read -r -a cases <<< "${CASES:-d12 d25 d12-f13 d40 d12-f28 d16-f24-p}"
cd "$repo_root" || exit 1

if pgrep -fl "UnrealEditor( |$)|GranTheftPraline-Mac" 2>/dev/null | grep -v UnrealEditorServices >/dev/null; then
  echo "traffic-pass: an Unreal or game process is already running, stop it first" >&2
  exit 1
fi

map="GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
map_md5="none"
if test -f "$map"; then
  map_md5="$( (md5 -q "$map" 2>/dev/null || md5sum "$map" | cut -d' ' -f1) )"
fi
if test -n "${MAP_MD5:-}" && test "$map_md5" != "$MAP_MD5"; then
  echo "traffic-pass: map md5 $map_md5, expected $MAP_MD5: not the map to measure" >&2
  exit 1
fi

{
  echo "# Traffic pass $version, $(date '+%Y-%m-%d %H:%M %Z'), commit $(git rev-parse --short HEAD 2>/dev/null || echo none)"
  echo
  echo "Profile m1, ${duration} s per case. Map md5 $map_md5.$( test "$dry" = 1 && echo " DRY RUN: stand-in steps, no numbers.")"
  echo
} > "$report"

# --- time guard and heartbeat ---------------------------------------------------
current="start"
step_start=$(date +%s)
beat() {
  local line
  line="[$(date '+%H:%M:%S')] $current, $(( $(date +%s) - step_start )) s in this step"
  echo "$line" >> "$heartbeat"
  echo "$line"
}
( while true; do sleep "${HEARTBEAT_S:-60}"; test -f "$out/.state" && current="$(cat "$out/.state")" && step_start="$(cat "$out/.since")"; beat; done ) &
beat_pid=$!
trap 'kill_tree "$beat_pid"' EXIT

kill_tree() {
  local pid="$1" child
  for child in $(pgrep -P "$pid" 2>/dev/null); do
    kill_tree "$child"
  done
  kill "$pid" 2>/dev/null
}

# guarded <name> <limit_s> <command...>: runs the command, log <name>.log, kills it past the limit.
guarded() {
  local name="$1" limit="$2" start pid status waited=0
  shift 2
  current="$name"
  start=$(date +%s)
  step_start=$start
  echo "$name" > "$out/.state"
  echo "$start" > "$out/.since"
  beat
  "$@" > "$out/$name.log" 2>&1 &
  pid=$!
  while kill -0 "$pid" 2>/dev/null; do
    if test "$waited" -ge "$limit"; then
      kill_tree "$pid"
      pkill -f "GranTheftPraline.app/Contents/MacOS" 2>/dev/null
      wait "$pid" 2>/dev/null
      echo "- $name: KILLED after $limit s (time guard), log $name.log" >> "$report"
      return 124
    fi
    sleep 2
    waited=$(( waited + 2 ))
  done
  wait "$pid"
  status=$?
  echo "- $name: $( test $status -eq 0 && echo OK || echo "FAILED ($status)" ), $(( $(date +%s) - start )) s, log $name.log" >> "$report"
  return $status
}

stand_in() {
  # DRY_RUN: what the real step would run, then a short wait.
  echo "would run:" "$@"
  sleep "${DRY_STEP_S:-3}"
}

# --- 1. package -------------------------------------------------------------------
if test "$skip_build" != "1"; then
  if test "$dry" = 1; then
    guarded package 60 stand_in tools/build/package-mac.sh "$version" && mkdir -p "$build_dir/GranTheftPraline.app"
  else
    guarded package 3000 tools/build/package-mac.sh "$version"
  fi
  status=$?
  if test $status -ne 0; then
    grep -hE ": (fatal )?error:|Error: " "$out/package.log" | sort -u | head -n 80 > "$out/compile-errors.txt"
    echo "- pass stopped: package failed, errors in compile-errors.txt" >> "$report"
    exit 1
  fi
fi
test -d "$build_dir/GranTheftPraline.app" || { echo "- no build at $build_dir" >> "$report"; exit 1; }
if test "$package_only" = "1"; then
  echo "- PACKAGE_ONLY=1: build ready at $build_dir, no game launched" >> "$report"
  echo "[$(date '+%H:%M:%S')] done (package only)" >> "$heartbeat"
  echo "Done: $report"
  exit 0
fi

# --- 2. control launch ----------------------------------------------------------
if test "$dry" = 1; then
  guarded control 60 stand_in tools/build/test-build-mac.sh "$version" 30 0
else
  GAME_ARGS="" guarded control 180 tools/build/test-build-mac.sh "$version" 30 0
fi
if test $? -ne 0; then
  echo "- pass stopped: the build does not run 30 s cleanly, see control.log" >> "$report"
  exit 1
fi

# --- 3. traffic cases -----------------------------------------------------------
total=${#cases[@]}
n=0
for name in "${cases[@]}"; do
  n=$(( n + 1 ))
  if test "$dry" = 1; then
    guarded "case-$n-of-$total-$name" 60 stand_in tools/perf/traffic-perceived-bench.sh "$build_dir" "$out/$name" "$name"
  else
    DURATION="$duration" guarded "case-$n-of-$total-$name" $(( 2 * (duration + 60) )) \
      tools/perf/traffic-perceived-bench.sh "$build_dir" "$out/$name" "$name"
  fi
done

# --- tables ---------------------------------------------------------------------
{
  echo
  for name in "${cases[@]}"; do
    sed -n '/^## /,$p' "$out/$name/traffic-perceived.md" 2>/dev/null || echo "## $name: (no table, see its log)"
    echo
  done
} >> "$report"
echo "[$(date '+%H:%M:%S')] done" >> "$heartbeat"
echo "Done: $report"
