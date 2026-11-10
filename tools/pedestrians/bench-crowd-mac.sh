#!/usr/bin/env bash
# One-command crowd measurement on a packaged build (macOS): frame times with
# 0, 12, 30 and 60 pedestrians, then the cost levers one by one at 30.
# Runs tools/build/test-build-mac.sh once per case, in one session, so the
# numbers compare with each other (profile m1, demo route, nothing else on).
#
# Usage: caffeinate -dimsu tools/pedestrians/bench-crowd-mac.sh <version> [duration_s] [cases...]
#   cases (default: all)  n0 n12 n30 n60 n30-nocheap n30-noanimrate n30-nospread n30-vis6 n30-vis20 n40-vis20
# Output: Builds/<version>/evidence/crowd-<stamp>/SUMMARY.md and one folder per case.
# A case that fails is recorded and the script goes on. Needs no editor.
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
version="${1:?usage: bench-crowd-mac.sh <version> [duration_s] [cases...]}"
duration="${2:-180}"
shift $(( $# >= 2 ? 2 : 1 ))
builds_dir="${BUILDS_DIR:-$repo_root/Builds}"
evidence="$builds_dir/$version/evidence"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$evidence/crowd-$stamp"
mkdir -p "$out"

off="-Traffic=0 -Police=0 -Ragdoll=0 -Minimap=0 -DayNight=0 -Audio=0"
args_for() {
  case "$1" in
    n0)              echo "$off -Pedestrians=0" ;;
    n12)             echo "$off -Pedestrians=12" ;;
    n30)             echo "$off -Pedestrians=30" ;;
    n60)             echo "$off -Pedestrians=60" ;;
    n30-nocheap)     echo "$off -Pedestrians=30 -PedCheap=0" ;;
    n30-noanimrate)  echo "$off -Pedestrians=30 -PedAnimRate=0" ;;
    n30-nospread)    echo "$off -Pedestrians=30 -PedSpread=0" ;;
    n30-vis6)        echo "$off -Pedestrians=30 -PedMaxVisible=6" ;;
    n30-vis20)       echo "$off -Pedestrians=30 -PedMaxVisible=20" ;;
    n40-vis20)       echo "$off -Pedestrians=40 -PedMaxVisible=20" ;;
    n20-far60)       echo "$off -Pedestrians=20 -PedFar=60" ;;
    *) return 1 ;;
  esac
}

if test "$#" -gt 0; then
  cases=("$@")
else
  cases=(n0 n12 n30 n60 n30-nocheap n30-noanimrate n30-nospread n30-vis6 n30-vis20 n40-vis20)
fi

if pgrep -fl "UnrealEditor|GranTheftPraline" | grep -v UnrealEditorServices >/dev/null; then
  echo "bench-crowd: an Unreal process is already running, stop it first" >&2
  exit 1
fi

summary="$out/SUMMARY.md"
{
  echo "# Crowd cost, build $version, $(date '+%Y-%m-%d %H:%M %Z')"
  echo
  echo "Each case ${duration} s, profile m1, demo route, other systems off. Compare cases with each other only."
  echo "The 'LogGTPPedestrians: active= drawn=' lines of each run are in its log."
} > "$summary"

for name in "${cases[@]}"; do
  extra="$(args_for "$name")" || { echo "unknown case $name" >&2; continue; }
  echo "=== $name: $extra"
  before="$(ls -1 "$evidence" 2>/dev/null | sort | tail -n 1)"
  GAME_ARGS="$extra" "$script_dir/../build/test-build-mac.sh" "$version" "$duration" 0 > "$out/$name.log" 2>&1
  status=$?
  latest="$(ls -1 "$evidence" 2>/dev/null | grep -v '^crowd-\|^systems-' | sort | tail -n 1)"
  {
    echo
    echo "## $name"
    echo
    echo "Arguments: \`$extra\`; exit status $status"
    if test -n "$latest" && test "$latest" != "$before" && test -f "$evidence/$latest/frametimes.summary.txt"; then
      mkdir -p "$out/$name"
      cp -R "$evidence/$latest/." "$out/$name/"
      echo
      echo '```'
      cat "$evidence/$latest/frametimes.summary.txt"
      echo '```'
      grep -h "LogGTPPedestrians: active=" "$out/$name.log" 2>/dev/null | tail -n 2 | sed 's/^/    /'
    else
      echo
      echo "No evidence produced, see $name.log"
    fi
  } >> "$summary"
done
echo "Done: $summary"
