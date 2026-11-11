#!/usr/bin/env bash
# Perf slot 6: near crowd of 20, 50 and 100 pedestrians with the cost levers (mesh LOD, cheap
# rendering, animation rate) and the far-crowd silhouette fallback, in one command. No mesh
# import, no editor, no copy outside the project.
#
# Usage (from the root of the GTAL-build clone, nothing else running; the path may contain spaces):
#   caffeinate -dimsu tools/perf/slot6-crowd-near-mac.sh
# Environment:
#   PACKAGE_ONLY=1     compile, cook and package (no screen needed), do not launch the game; stop after the package check
#   SKIP_BUILD=1       measure only, on the package already in Builds/perf-slot6/0.3; refuses to start if the
#                      screen is locked (CGSSessionScreenIsLocked), with a clear message
#   SKIP_CONTROL=1     skip the 30 s control launch (freeze check)
#   DRY_RUN=1          check prerequisites and syntax, print the plan, run nothing heavy
#   DURATION           seconds per bench run (default 90)
#   BRANCH             code to build (default feat/pedestrians-slot6d)
#   EXPECTED_MAP_MD5   map fingerprint required (default 1c92de0f813b89cb40aeb000cd92a1de)
#   UE_ROOT            engine folder, read by tools/build/package-mac.sh (its own default applies)
# Output: Builds/perf-slot6/slot6-<stamp>/ (REPORT.md, steps.log, heartbeat.txt, one folder per case)
#
# Planned 45 min at most: environment assets 5-10, package ~20 (incremental), control launch 1,
# eight 90 s cases ~20. Every step is stopped at twice its planned time; a heartbeat line every
# 10 minutes; the map fingerprint is checked before and after (restored from a copy if changed);
# the clone goes back to its original HEAD on exit.
# Cases: n0; n20, n50, n100 (all levers on); n50 and n100 with each lever off in turn (n50-nocheap,
# n50-noanimrate, n100-nospread); n50-far80 (50 near + 80 far silhouettes).
set -euo pipefail
# Any command that fails under set -e says which one, and where, before the script stops.
trap 'rc=$?; printf "%s ERROR: exit %s at line %s: %s\n" "$(date +%H:%M:%S)" "$rc" "$LINENO" "$BASH_COMMAND" >> "${out:-/dev/null}/steps.log" 2>/dev/null; printf "ERROR: exit %s at line %s: %s\n" "$rc" "$LINENO" "$BASH_COMMAND" >&2' ERR

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$repo/Builds/perf-slot6/slot6-$stamp"
branch="${BRANCH:-feat/pedestrians-slot6d}"
duration="${DURATION:-90}"
expected_md5="${EXPECTED_MAP_MD5:-1c92de0f813b89cb40aeb000cd92a1de}"
dry="${DRY_RUN:-0}"
package_only="${PACKAGE_ONLY:-0}"
skip_build="${SKIP_BUILD:-0}"
map="$repo/GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
mkdir -p "$out"
report="$out/REPORT.md"

log() { printf '%s %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$out/steps.log"; }
die() { log "ABORT: $*"; printf '\n**ABORTED:** %s\n' "$*" >> "$report"; exit 1; }
md5of() { if command -v md5 > /dev/null 2>&1; then md5 -q "$1"; else md5sum "$1" | cut -d' ' -f1; fi; }
sizeof() { stat -f %z "$1" 2> /dev/null || stat -c %s "$1" 2> /dev/null || echo ?; }
# Time limit without coreutils `timeout`: perl alarm + exec; arguments pass as they are.
limit() { local s="$1"; shift; perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$s" "$@"; }

# True when the console session is locked: no game can create its Metal window then.
screen_locked() { ioreg -n Root -d1 -a 2> /dev/null | grep -A1 "CGSSessionScreenIsLocked" | grep -q "<true/>"; }

( while sleep 600; do printf "%s alive\n" "$(date +%H:%M:%S)" >> "$out/heartbeat.txt"; done ) > /dev/null 2>&1 &
hb=$!
orig_head="$(git rev-parse --abbrev-ref HEAD)"
test "$orig_head" != "HEAD" || orig_head="$(git rev-parse HEAD)"
map_before=""
cleanup() {
  kill "$hb" 2> /dev/null || true
  if test "$dry" != "1"; then
    pkill -f "GranTheftPraline-Mac" 2> /dev/null || true
    pkill -f "UnrealEditor( |$)|UnrealBuildTool|AutomationTool" 2> /dev/null || true
    if test -n "$map_before" && test -f "$map" && test "$(md5of "$map")" != "$map_before"; then
      cp -f "$out/map.backup" "$map" && log "MAP CHANGED during the pass: restored from the backup" \
        && echo "**Map changed during the pass and was restored from the backup.**" >> "$report"
    fi
    git checkout -q "$orig_head" 2> /dev/null || log "could not restore $orig_head"
    log "map md5 after: $(test -f "$map" && md5of "$map" || echo missing)"
  fi
  log "end (clone back on $orig_head)"
}
trap cleanup EXIT

{
  echo "# Perf slot 6 - near crowd and far silhouettes ($stamp)"
  echo
  echo "Host $(hostname -s); $(uptime | sed 's/^ *//')"
  echo "Clone HEAD before: $orig_head; code branch $branch"
} > "$report"

test "$package_only" = "1" && test "$skip_build" = "1" && die "PACKAGE_ONLY=1 and SKIP_BUILD=1 together do nothing"
log "prerequisites"
if test "$package_only" != "1" && test "$dry" != "1" && screen_locked; then
  die "the screen is locked (CGSSessionScreenIsLocked): unlock the Mac, or run with PACKAGE_ONLY=1 to build without a screen"
fi
if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac" > /dev/null; then
  test "$dry" = "1" && log "(dry run) an Unreal process is running: a real run would stop here" || die "an Unreal editor or the game is running"
fi
if test -f "$map"; then
  map_before="$(md5of "$map")"
  echo "Map md5 before: $map_before  size $(sizeof "$map")" >> "$report"
  log "map md5 before: $map_before"
  if test "$map_before" != "$expected_md5"; then
    test "$dry" = "1" && log "(dry run) map md5 differs from $expected_md5" || die "map md5 $map_before is not the expected $expected_md5 (regenerate or pass EXPECTED_MAP_MD5)"
  fi
else
  test "$dry" = "1" && log "(dry run) map missing" || die "map missing: $map"
fi
bash -n "$repo/tools/perf/bench.sh" || die "bench.sh syntax"
bash -n "${BASH_SOURCE[0]}" || die "this script's syntax"

if test "$dry" = "1"; then
  log "mode: package_only=$package_only skip_build=$skip_build; screen locked now: $(screen_locked && echo yes || echo no)"
  log "DRY RUN plan: checkout origin/$branch; setup-environment-mac.sh (limit 30 min); package Shipping to Builds/perf-slot6/0.3 (limit 90 min); 30 s control launch; cases n0 n20 n50 n100 n50-nocheap n50-noanimrate n100-nospread n50-far80, ${duration} s each (limit 25 min per case); map check and restore; report"
  echo "DRY RUN OK (path with spaces handled: \"$repo\")" >> "$report"
  cat "$report"
  exit 0
fi

test -z "$(git status --porcelain --untracked-files=no)" || { git checkout -q -- assets/geo/processed 2> /dev/null || true; }
test -z "$(git status --porcelain --untracked-files=no)" || die "tracked changes in the clone, refusing to switch branch"
cp -f "$map" "$out/map.backup"
git fetch -q origin "$branch" || die "fetch $branch failed"
git checkout -q --detach "origin/$branch" || die "checkout"
echo "Code: $(git rev-parse --short HEAD) ($branch)" >> "$report"

build="$repo/Builds/perf-slot6/0.3"
if test "$skip_build" = "1"; then
  log "SKIP_BUILD: using the existing package"
  test -d "$build/GranTheftPraline.app" || die "SKIP_BUILD but no package at $build (run PACKAGE_ONLY=1 first)"
else
  log "setup environment assets (M_CloudShadowLF), limit 30 min"
  limit 1800 tools/environment/setup-environment-mac.sh > "$out/setup.log" 2>&1 || die "setup-environment-mac.sh failed (see setup.log)"

  log "package Shipping, limit 90 min"
  BUILDS_DIR="$repo/Builds/perf-slot6" limit 5400 tools/build/package-mac.sh 0.3 Shipping > "$out/package.log" 2>&1 || die "package failed (see package.log)"
  test -d "$build/GranTheftPraline.app" || die "no app after packaging"
  echo "Package: $build (code $(git rev-parse --short HEAD))" >> "$report"
fi
if test "$package_only" = "1"; then
  log "PACKAGE_ONLY: package ready, game not launched"
  echo "Package ready at $build; measurements not run (PACKAGE_ONLY). Next: SKIP_BUILD=1 caffeinate -dimsu tools/perf/slot6-crowd-near-mac.sh" >> "$report"
  cat "$report"
  exit 0
fi
if screen_locked; then
  die "the screen is locked (CGSSessionScreenIsLocked): unlock the Mac, then rerun with SKIP_BUILD=1"
fi

if test "${SKIP_CONTROL:-0}" = "1"; then
  log "SKIP_CONTROL: no control launch"
else
log "30 s control launch (freeze check), limit 120 s"
exe="$(find "$build/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
limit 120 "$exe" -SkipTitle "-FrameLogFile=$out/control.csv" -ExitAfter=30 > "$out/control.log" 2>&1 || true
if ! test -s "$out/control.csv"; then
  # The Shipping build froze at start. Find out whether the pedestrians are to blame (same build,
  # pedestrians off), then package Development to get a log of the freeze, and stop.
  log "control launch froze: retry with -Pedestrians=0, limit 120 s"
  limit 120 "$exe" -SkipTitle -Pedestrians=0 "-FrameLogFile=$out/control-nopeds.csv" -ExitAfter=30 > "$out/control-nopeds.log" 2>&1 || true
  if test -s "$out/control-nopeds.csv"; then
    echo "Control launch FROZE with pedestrians on and ran with -Pedestrians=0: the pedestrian code is the cause" >> "$report"
  else
    echo "Control launch froze even with -Pedestrians=0: not the pedestrians (base build or map)" >> "$report"
  fi
  log "package Development for a log, limit 30 min"
  BUILDS_DIR="$repo/Builds/perf-slot6-dev" limit 1800 tools/build/package-mac.sh 0.3 Development > "$out/package-dev.log" 2>&1 || log "development package failed (see package-dev.log)"
  devexe="$(find "$repo/Builds/perf-slot6-dev/0.3/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x 2> /dev/null | head -n 1)"
  if test -n "$devexe"; then
    limit 90 "$devexe" -SkipTitle -log -stdout -FullStdOutLogOutput "-FrameLogFile=$out/control-dev.csv" -ExitAfter=30 > "$out/control-dev.log" 2>&1 || true
    echo "Development control log: control-dev.log (last lines below)" >> "$report"
    { echo '```'; tail -n 40 "$out/control-dev.log"; echo '```'; } >> "$report"
  fi
  die "control launch produced no frame: the build freezes (see the report for the isolation result)"
fi
echo "Control launch: $(wc -l < "$out/control.csv" | tr -d ' ') frames in 30 s" >> "$report"
# The control game must be gone before the bench starts (bench.sh refuses a running game): wait up to 60 s.
for _ in $(seq 1 60); do
  pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac" > /dev/null || break
  sleep 1
done
if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac" > /dev/null; then
  die "the control game is still running 60 s after its end (stuck process): stop it and rerun with SKIP_BUILD=1 SKIP_CONTROL=1"
fi
fi

off="-Traffic=0 -Police=0 -Ragdoll=0 -Minimap=0 -DayNight=0 -Audio=0"
run_case() { # name extra; never stops the pass
  run_case_body "$@" || log "case $1 failed (exit $?), continuing"
}
run_case_body() {
  log "bench $1, limit 25 min"
  CAPTURE=0 EXTRA_ARGS="$off $2" SCENARIOS="street-demo" DURATION="$duration" \
    limit 1500 tools/perf/bench.sh "$build" "$out/$1" > "$out/$1.log" 2>&1 || log "bench $1 ended with an error (see $1.log)"
  # No match from grep (n0 has no pedestrian log) must not stop the pass under set -e / pipefail.
  { echo; echo "## $1 ($2)"; sed -n '/^| Scenario/,$p' "$out/$1/summary.md" 2> /dev/null || echo "no summary"; { grep -h "LogGTPPedestrians" "$out/$1.log" 2> /dev/null || true; } | tail -n 3 | sed 's/^/    /'; } >> "$report" || log "could not write the report block for $1"
}
run_case n0 "-Pedestrians=0"
run_case n20 "-Pedestrians=20 -PedMaxVisible=20"
run_case n50 "-Pedestrians=50 -PedMaxVisible=50"
run_case n100 "-Pedestrians=100 -PedMaxVisible=100"
run_case n50-nocheap "-Pedestrians=50 -PedMaxVisible=50 -PedCheap=0"
run_case n50-noanimrate "-Pedestrians=50 -PedMaxVisible=50 -PedAnimRate=0"
run_case n100-nospread "-Pedestrians=100 -PedMaxVisible=100 -PedSpread=0"
run_case n50-far80 "-Pedestrians=50 -PedMaxVisible=20 -PedFar=80 -PedFarSilhouette=1"

for c in n20 n50 n100 n50-far80; do
  if test -s "$out/n0/results.csv" && test -s "$out/$c/results.csv"; then
    { echo; echo "## Gate $c vs n0"; python3 -I tools/perf/compare.py "$out/n0/results.csv" "$out/$c/results.csv" 2 || true; } >> "$report"
  fi
done
log "map md5 check"
test "$(md5of "$map")" = "$map_before" && echo "Map md5 after: unchanged ($map_before)" >> "$report" || echo "Map md5 after: CHANGED (restored on exit)" >> "$report"
log "done, report: $report"
cat "$report"
