#!/usr/bin/env bash
# Perf slot 2: per-pass GPU breakdown of the cumulative build, in one command.
#
# Packages feat/perf-cumul in CONFIG (default Development: the launcher engine cannot build Test) (the CSV profiler and the
# GPU stats exist there, not in Shipping), runs the fixed camera and the demo
# route with -csvGpuStats and prints the cost of every GPU pass.
#
# Usage (root of the GTAL-build clone, nothing else running):
#   caffeinate -dimsu tools/perf/slot2-gpu-passes-mac.sh
# Environment: DRY_RUN=1 (checks only), DURATION (default 90), AMBIENCES
#   (default "noon rain"; empty value = no -Ambience argument)
# Output: Builds/perf-cumul/slot2-<stamp>/REPORT.md
set -euo pipefail
config="${CONFIG:-Development}"

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$repo/Builds/perf-cumul/slot2-$stamp"
duration="${DURATION:-90}"
ambiences="${AMBIENCES-noon rain}"
branch="feat/perf-cumul"
map="$repo/GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
dry="${DRY_RUN:-0}"
mkdir -p "$out"
report="$out/REPORT.md"

log() { printf '%s %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$out/steps.log"; }
die() { log "ABORT: $*"; printf '\n**ABORTED:** %s\n' "$*" >> "$report"; exit 1; }
limit() { local s="$1"; shift; perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$s" "$@"; }

( while sleep 600; do printf "%s alive\n" "$(date +%H:%M:%S)" >> "$out/heartbeat.txt"; done ) > /dev/null 2>&1 &
hb=$!
orig_head="$(git rev-parse --abbrev-ref HEAD)"
test "$orig_head" != "HEAD" || orig_head="$(git rev-parse HEAD)"
switched=0
cleanup() {
  pkill -P "$hb" 2>/dev/null || true; kill "$hb" 2>/dev/null || true
  pkill -f "GranTheftPraline-Mac" 2>/dev/null || true
  pkill -f "UnrealEditor( |$)|UnrealBuildTool|AutomationTool" 2>/dev/null || true
  if test "$switched" = "1"; then git checkout -q "$orig_head" 2>/dev/null || log "could not restore $orig_head"; fi
  log "end (clone on $(git rev-parse --abbrev-ref HEAD))"
}
trap cleanup EXIT

{
  echo "# Perf slot 2 - GPU cost per pass ($stamp)"
  echo
  echo "Host $(hostname -s); $(uptime | sed 's/^ *//')"
  echo "Map md5: $(md5 -q "$map" 2>/dev/null || echo missing)"
} > "$report"

if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac|UnrealBuildTool|AutomationTool" > /dev/null; then die "Unreal, UBT/UAT or the game is running"; fi
test -f "$map" || die "map missing"
if test "$dry" != "1"; then git checkout -q -- assets/geo/processed 2>/dev/null || true; fi
test -z "$(git status --porcelain --untracked-files=no)" || die "tracked changes in the clone"
git fetch -q origin "$branch" || die "fetch failed"
if test "$dry" = "1"; then
  log "DRY RUN: would package origin/$branch ($(git rev-parse --short "origin/$branch")) in $config and capture GPU passes for: fixed + demo x [${ambiences:-none}]"
  python3 -I tools/perf/gpu_passes.py /dev/null 2>/dev/null; echo "DRY RUN OK" >> "$report"; exit 0
fi

git checkout -q --detach "origin/$branch" || die "checkout"; switched=1
echo "Code: $(git rev-parse --short HEAD)" >> "$report"
build="$repo/Builds/perf-cumul-test/0.3"
if test -d "$build/GranTheftPraline.app" && test "$(cat "$build/.commit" 2>/dev/null)" = "$(git rev-parse HEAD)"; then
  log "$config build already packaged for this commit"
else
  log "setup environment, limit 15 min"
  limit 900 tools/environment/setup-environment-mac.sh > "$out/setup.log" 2>&1 || die "setup failed"
  log "package $config, limit 50 min"
  BUILDS_DIR="$repo/Builds/perf-cumul-test" limit 3000 tools/build/package-mac.sh 0.3 "$config" > "$out/package.log" 2>&1 || die "package failed (package.log)"
  git rev-parse HEAD > "$build/.commit"
fi
exe="$(find "$build/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
test -n "$exe" || die "no executable"

capture() { # name args...
  local name="$1"; shift
  local dir="$out/$name"; mkdir -p "$dir"
  log "capture $name ($*), limit $((duration + 90)) s"
  limit $((duration + 90)) "$exe" "$@" -SkipTitle -csvGpuStats -csvCaptureFrames=$((duration * 40)) \
    -FrameLogFile="$dir/frametimes.csv" -ExitAfter="$duration" -abslog="$dir/game.log" > "$dir/stdout.log" 2>&1 || log "  $name exited with $?"
  # The CSV profiler writes under the app's Saved/Profiling/CSV (or the user Library): take the newest.
  local csvf
  csvf="$(find "$build" "$HOME/Library/Application Support/Epic" -path '*Profiling/CSV*' -name '*.csv' -newer "$dir" 2>/dev/null | head -n 1)"
  { echo; echo "## $name"; echo; } >> "$report"
  if test -n "$csvf"; then
    cp "$csvf" "$dir/profile.csv"
    python3 -I tools/perf/gpu_passes.py "$dir/profile.csv" 300 >> "$report" 2>&1 || echo "analysis failed" >> "$report"
  else
    echo "No CSV profile found (check game.log for 'CSV')." >> "$report"
  fi
  python3 -I tools/perf/summarize.py "$dir/frametimes.csv" "$name" test >> "$report" 2>/dev/null || true
}
for amb in ${ambiences:-_none}; do
  a=(); test "$amb" = "_none" || a=("-Ambience=$amb")
  capture "fixed-$amb" ${a[@]+"${a[@]}"}
  capture "demo-$amb" ${a[@]+"${a[@]}"} -DemoRoute
done
log "done: $report"
cat "$report"
