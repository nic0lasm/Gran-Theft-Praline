#!/usr/bin/env bash
# Unattended A/B measurement of each gameplay system on one packaged build
# (grouped Mac pass, version 0.4). Runs test-build-mac.sh once per case, all
# in the same session so the numbers compare with each other:
#   reference   every system off
#   traffic     traffic only            police      traffic + police (2 stars held)
#   pedestrians pedestrians only        ragdoll     ragdoll self-test
#   minimap     minimap only            daynight    night ambience only
#   all         everything on (default settings, night)
# then two films: carjack with everything on, and the default day drive.
#
# Usage: caffeinate -dimsu tools/build/measure-systems-mac.sh <version> [duration_s] [cases...]
# Output: Builds/<version>/evidence/systems-<stamp>/<case>/ (copy of each run)
#         and systems-<stamp>/SUMMARY.md (one block per case).
# A case that fails is recorded and the script carries on with the next one.
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
version="${1:?usage: measure-systems-mac.sh <version> [duration_s] [cases...]}"
duration="${2:-300}"
shift $(( $# >= 2 ? 2 : 1 ))
builds_dir="${BUILDS_DIR:-$repo_root/Builds}"
evidence="$builds_dir/$version/evidence"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$evidence/systems-$stamp"
mkdir -p "$out"

off="-Traffic=0 -Police=0 -Pedestrians=0 -Ragdoll=0 -Minimap=0 -DayNight=0 -Audio=0"
args_for() {
  case "$1" in
    reference)   echo "$off" ;;
    traffic)     echo "${off/-Traffic=0/}" ;;
    police)      echo "${off/-Traffic=0 -Police=0/} -WantedStars=2 -WantedHold" ;;
    pedestrians) echo "${off/-Pedestrians=0/}" ;;
    ragdoll)     echo "${off/-Ragdoll=0/} -RagdollTest" ;;
    minimap)     echo "${off/-Minimap=0/}" ;;
    daynight)    echo "${off/-DayNight=0/} -Ambience=night" ;;
    audio)       echo "${off/-Audio=0/}" ;;
    all)         echo "-Ambience=night" ;;
    *)           return 1 ;;
  esac
}

if test "$#" -gt 0; then
  cases=("$@")
else
  cases=(reference traffic police pedestrians ragdoll minimap daynight audio all)
fi

if pgrep -fl "UnrealEditor|GranTheftPraline" >/dev/null; then
  echo "measure-systems: an Unreal process is already running, stop it first" >&2
  pgrep -fl "UnrealEditor|GranTheftPraline" >&2
  exit 1
fi

summary="$out/SUMMARY.md"
{
  echo "# Systems A/B, build $version, $(date '+%Y-%m-%d %H:%M %Z')"
  echo
  echo "Each case: ${duration} s, profile m1, demo route. Compare cases with each other only."
} > "$summary"

run_case() {
  local name="$1" extra="$2" video="$3" dur="$4"
  local before latest
  before="$(ls -1 "$evidence" 2>/dev/null | sort | tail -n 1)"
  echo "=== $name: $extra"
  GAME_ARGS="$extra" "$script_dir/test-build-mac.sh" "$version" "$dur" "$video" > "$out/$name.log" 2>&1
  local status=$?
  latest="$(ls -1 "$evidence" | grep -v '^systems-' | sort | tail -n 1)"
  {
    echo
    echo "## $name"
    echo
    echo "Arguments: \`$extra\`, exit status $status"
    echo
    echo '```'
    if test -n "$latest" && test "$latest" != "$before"; then
      mv "$evidence/$latest" "$out/$name"
      cat "$out/$name/result.txt" 2>/dev/null
      cat "$out/$name/frametimes.summary.txt" 2>/dev/null
      # System start lines and periodic stats, from the game log if it was kept.
      grep -hE "(traffic|police|pedestrians|ragdoll|minimap|daynight|river|wanted|GTPRagdollTest|Demo route)[:]|\[Audio\]" "$out/$name"/*.log 2>/dev/null | head -n 40
    else
      echo "no evidence folder written"
      tail -n 20 "$out/$name.log"
    fi
    echo '```'
  } >> "$summary"
}

for name in "${cases[@]}"; do
  extra="$(args_for "$name")" || { echo "unknown case $name" >&2; continue; }
  run_case "$name" "$extra" 0 "$duration"
done

# Films (video run only, short soak).
run_case film-carjack-all "-DemoCarjack -DemoDriveTime=180 -Ambience=golden" 200 30
run_case film-drive-day "-DemoDriveTime=180" 200 30

echo "Done: $summary"
