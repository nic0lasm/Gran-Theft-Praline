#!/usr/bin/env bash
# Grouped Mac pass (version 0.4): one data preparation, one package, then
# every measurement without supervision. Each step is logged; a failed step
# is recorded and the pass carries on (a failed package stops it).
#
# Usage, from the root of the clone, nothing else running:
#   caffeinate -dimsu tools/build/grouped-pass-mac.sh <version> [duration_s]
#   caffeinate -dimsu tools/build/grouped-pass-mac.sh <version> --release
#     playable build of a release (0.3): lanes, one package, one measure with the
#     default settings and two films towards the river; the path of the app to
#     launch is written at the top of PASS.md.
#   tools/build/grouped-pass-mac.sh <version> --compile-only
#     builds the editor and the Shipping game only (about 20 min), no data,
#     no package, no measure; errors grouped per source file in
#     compile-errors.txt so each thread gets its own.
# Output: Builds/<version>/evidence/pass-<stamp>/PASS.md and one log per step.
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
version="${1:?usage: grouped-pass-mac.sh <version> [duration_s|--compile-only]}"
compile_only=0
release=0
if test "${2:-}" = "--compile-only"; then
  compile_only=1
  duration=0
elif test "${2:-}" = "--release"; then
  release=1
  duration=300
else
  duration="${2:-300}"
fi
map="L_City_RueDeLaRepublique"
builds_dir="${BUILDS_DIR:-$repo_root/Builds}"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$builds_dir/$version/evidence/pass-$stamp"
mkdir -p "$out"
report="$out/PASS.md"
cd "$repo_root" || exit 1

if pgrep -fl "UnrealEditor|GranTheftPraline" >/dev/null; then
  echo "grouped-pass: an Unreal process is already running, stop it first" >&2
  exit 1
fi

echo "# Grouped pass $version, $(date '+%Y-%m-%d %H:%M %Z'), commit $(git rev-parse --short HEAD)" > "$report"
echo >> "$report"

step() {
  local name="$1"
  shift
  local start status
  start=$(date +%s)
  echo "=== $name"
  "$@" > "$out/$name.log" 2>&1
  status=$?
  echo "- $name: $( test $status -eq 0 && echo OK || echo "FAILED ($status)" ), $(( $(date +%s) - start )) s, log $name.log" >> "$report"
  return $status
}

# Runs a game test, stopped after LIMIT seconds if it does not quit by itself
# (macOS has no timeout command).
limited() {
  local limit="$1"
  shift
  "$@" &
  local pid=$!
  local waited=0
  while kill -0 "$pid" 2>/dev/null; do
    if test "$waited" -ge "$limit"; then
      echo "stopped after ${limit} s"
      kill "$pid" 2>/dev/null
      sleep 5
      kill -9 "$pid" 2>/dev/null
      return 124
    fi
    sleep 1
    waited=$((waited + 1))
  done
  wait "$pid"
}

if test "$compile_only" = "1"; then
  ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
  build_sh="$ue_root/Engine/Build/BatchFiles/Mac/Build.sh"
  project="$repo_root/GranTheftPraline/GranTheftPraline.uproject"
  status=0
  step compile-editor "$build_sh" GranTheftPralineEditor Mac Development "-project=$project" -WaitMutex || status=1
  step compile-game "$build_sh" GranTheftPraline Mac Shipping "-project=$project" -WaitMutex || status=1
  # Errors grouped by file, so each thread gets its own lines.
  grep -hE ": (fatal )?error:|Error: " "$out"/compile-*.log | sed -E 's#^.*/Source/GranTheftPraline/##' | sort -u > "$out/compile-errors.txt"
  {
    echo
    echo "## Errors by file"
    echo
    echo '```'
    cut -d'(' -f1 "$out/compile-errors.txt" | cut -d: -f1 | sort | uniq -c
    echo '```'
  } >> "$report"
  echo "Done ($( test $status -eq 0 && echo compiled || echo errors )): $report"
  exit "$status"
fi

if test "$release" = "1"; then
  step lanes python3 tools/city/traffic.py --map "$map"
  if ! step package tools/build/package-mac.sh "$version"; then
    grep -hE ": (fatal )?error:|Error: " "$out/package.log" | head -n 60 > "$out/compile-errors.txt"
    echo "- package failed: first errors in compile-errors.txt" >> "$report"
    exit 1
  fi
  app="$builds_dir/$version/GranTheftPraline.app"
  # The line to read first tomorrow morning.
  { echo; echo "**Build to play: \`$app\`** (double-click, or \`open \"$app\"\`)"; echo; } >> "$report"
  step measure tools/build/test-build-mac.sh "$version" "$duration" 0
  GAME_ARGS="-DemoCarjack -DemoToRiver -DemoDriveTime=180" step film-carjack-river tools/build/test-build-mac.sh "$version" 30 200
  GAME_ARGS="-DemoToRiver -DemoDriveTime=180" step film-drive-river tools/build/test-build-mac.sh "$version" 30 200
  for log in measure film-carjack-river film-drive-river; do
    { echo; echo "### $log"; echo '```'; grep -E "^(verdict|p50|p95|frames|game_args)|ms" "$out/$log.log" | head -n 12; echo '```'; } >> "$report"
  done
  echo "Done: $report"
  exit 0
fi

# 1. Data the game reads at run time (not in Git).
step lanes python3 tools/city/traffic.py --map "$map"
step walkways python3 tools/city/pedestrians.py --map "$map"
step minimap python3 tools/ui/minimap.py --map "$map"
step environment tools/environment/setup-environment-mac.sh
step audio-prepare tools/audio/mac_audio_pass.sh prepare

# 2. One package.
app="$builds_dir/$version/GranTheftPraline.app"
{ echo; echo "**Build: \`$app\`**"; echo; } >> "$report"
if ! step package tools/build/package-mac.sh "$version"; then
  grep -nE "error|Error" "$out/package.log" | head -n 60 > "$out/compile-errors.txt"
  echo "- package failed: first errors in compile-errors.txt" >> "$report"
  exit 1
fi

# 3. Measurements, all in the same session.
step systems tools/build/measure-systems-mac.sh "$version" "$duration"
step ambiences tools/environment/measure-ambiences-mac.sh "$version" 120 20
step audio-check tools/audio/mac_audio_pass.sh check "$version"

app="$builds_dir/$version/GranTheftPraline.app"
exe="$(find "$app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
step ui-write limited 240 "$exe" -SkipTitle -UITest=write -windowed
step ui-verify limited 240 "$exe" -SkipTitle -UITest=verify -windowed
step ui-reset limited 240 "$exe" -SkipTitle -UITest=reset -windowed
step ragdoll-test limited 240 "$exe" -SkipTitle -RagdollTest -RagdollTestQuit -windowed

echo >> "$report"
echo "Systems table: see systems-*/SUMMARY.md next to this folder." >> "$report"
echo "Done: $report"
