#!/usr/bin/env bash
# Measure and film every ambience of the day-night cycle and weather in a packaged build.
# Each ambience is a frozen time and weather (-Ambience=<name>) passed to
# tools/build/test-build-mac.sh, which plays the demo route, films it and records frame times.
#
# Usage: tools/environment/measure-ambiences-mac.sh [version] [duration_s] [video_s] [ambience ...]
#   version     packaged build under Builds/ (default: the project version, major.minor)
#   duration_s  soak per ambience (default 120), video_s film per ambience (default 20, 0 = none)
#   ambience    subset of: noon golden dusk night rain night-rain fog (default: all)
# Environment: GAME_ARGS (added to every run, e.g. -GfxProfile=ultra), BUILDS_DIR.
# Output: Builds/<version>/evidence/ambiences-<stamp>.md, a table of the summaries,
#         next to the per-run folders of test-build-mac.sh.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
project_version="$(sed -n 's/^ProjectVersion=//p' "$repo/GranTheftPraline/Config/DefaultGame.ini" | head -n 1 | tr -d '\r')"
version="${1:-$(printf '%s' "$project_version" | cut -d. -f1-2)}"
duration="${2:-120}"
video="${3:-20}"
shift $(( $# > 3 ? 3 : $# ))
builds_dir="${BUILDS_DIR:-$repo/Builds}"
evidence="$builds_dir/$version/evidence"

# Ambiences are defined in DefaultGame.ini (Ambiences of GTPEnvironmentSubsystem).
ambience_args() {
  case "$1" in
    noon|golden|dusk|night|rain|night-rain|fog) echo "-Ambience=$1" ;;
    *) return 1 ;;
  esac
}

list=("$@")
test "${#list[@]}" -gt 0 || list=(noon golden dusk night rain night-rain fog)
stamp="$(date +%Y%m%d-%H%M%S)"
table="$evidence/ambiences-$stamp.md"
mkdir -p "$evidence"
printf '| Ambience | Arguments | Run | Summary |\n| --- | --- | --- | --- |\n' > "$table"

for name in "${list[@]}"; do
  args="$(ambience_args "$name")" || { echo "unknown ambience: $name" >&2; exit 1; }
  echo "== $name: $args"
  before="$(ls -1 "$evidence" | sort)"
  status="PASS"
  GAME_ARGS="${GAME_ARGS:-} $args" "$repo/tools/build/test-build-mac.sh" "$version" "$duration" "$video" || status="FAIL"
  run="$(comm -13 <(printf '%s\n' "$before") <(ls -1 "$evidence" | sort) | grep -v '^ambiences-' | tail -n 1 || true)"
  summary="no summary"
  if test -n "$run" && test -f "$evidence/$run/frametimes.summary.txt"; then
    summary="$(tr '\n' ' ' < "$evidence/$run/frametimes.summary.txt")"
  fi
  printf '| %s | `%s` | %s %s | %s |\n' "$name" "$args" "$status" "$run" "$summary" >> "$table"
done
cat "$table"
