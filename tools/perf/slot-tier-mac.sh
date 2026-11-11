#!/usr/bin/env bash
# Perf slots 4, 8, 10, 11, 12: density tiers, cold start and the long gate
# of the cumulative build, in one command each.
#
# Usage (root of the GTAL-build clone, nothing else running):
#   TIER=B caffeinate -dimsu tools/perf/slot-tier-mac.sh
# TIER:
#   B       slot 4  : 20 pedestrians drawn, 12 cars LOD+ghost, police 3;
#                     fixed + demo + night + rain, 330 s, 3 passes (~75 min)
#   C       slot 8  : 50 pedestrians (20 full + far crowd), 25 cars; 4 scenes, 330 s x 1 (~35 min)
#   target  slot 11 : 100 pedestrians (20 full + 80 far), 40 cars; 4 scenes, 330 s x 1 (~35 min)
#   cold    slot 10 : fresh package, first launch vs second launch on the demo
#                     route, frames above 100 ms (PSO / shader hitches) (~70 min)
#   long    slot 12 : tier B, 600 s, 3 passes, memory sampled every 30 s,
#                     non-regression gate against REFERENCE (~130 min)
# Environment: DRY_RUN=1 (checks only), REFERENCE=<results.csv> (gate),
#   BRANCH (default feat/perf-cumul), FORCE_PACKAGE=1, PACKAGE_ONLY=1 (stop after the package)
# Output: Builds/perf-cumul/tier-<TIER>-<stamp>/REPORT.md
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo"
tier="${TIER:?TIER=B|C|target|cold|long}"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$repo/Builds/perf-cumul/tier-$tier-$stamp"
branch="${BRANCH:-feat/perf-cumul}"
map="$repo/GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
dry="${DRY_RUN:-0}"
police="-WantedStars=2 -WantedHold -PoliceMax=3"
case "$tier" in
  B|long) world="-Sky=lightfunction -Pedestrians=20 -PedMaxVisible=20 -Traffic=12 -VehicleLod=1 $police" ;;
  C)      world="-Sky=lightfunction -Pedestrians=50 -PedMaxVisible=20 -PedFar=30 -Traffic=25 -VehicleLod=1 $police" ;;
  target) world="-Sky=lightfunction -Pedestrians=100 -PedMaxVisible=20 -PedFar=80 -Traffic=40 -VehicleLod=1 $police" ;;
  cold)   world="-Sky=lightfunction -Pedestrians=20 -PedMaxVisible=20 -Traffic=12 -VehicleLod=1" ;;
  *) echo "unknown TIER $tier" >&2; exit 2 ;;
esac
case "$tier" in
  B)      duration=330; passes=3; scenarios="street-fixed street-demo night rain" ;;
  long)   duration=600; passes=3; scenarios="street-fixed street-demo night rain" ;;
  C|target) duration=330; passes=1; scenarios="street-fixed street-demo night rain" ;;
  cold)   duration=120; passes=1; scenarios="street-demo" ;;
esac
scenarios="${SCENARIOS:-$scenarios}"
mkdir -p "$out"
report="$out/REPORT.md"

log() { printf '%s %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$out/steps.log"; }
die() { log "ABORT: $*"; printf '\n**ABORTED:** %s\n' "$*" >> "$report"; exit 1; }
screen_locked() { ioreg -n Root -d1 -a 2>/dev/null | grep -A1 CGSSessionScreenIsLocked | grep -q "<true/>"; }
limit() { local s="$1"; shift; perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$s" "$@"; }

( while sleep 600; do printf "%s alive\n" "$(date +%H:%M:%S)" >> "$out/heartbeat.txt"; done ) > /dev/null 2>&1 &
hb=$!
mem=""
orig_head="$(git rev-parse --abbrev-ref HEAD)"
test "$orig_head" != "HEAD" || orig_head="$(git rev-parse HEAD)"
switched=0
cleanup() {
  pkill -P "$hb" 2>/dev/null || true; kill "$hb" 2>/dev/null || true
  if test -n "$mem"; then pkill -P "$mem" 2>/dev/null || true; kill "$mem" 2>/dev/null || true; fi
  pkill -f "GranTheftPraline-Mac" 2>/dev/null || true
  pkill -f "UnrealEditor( |$)|UnrealBuildTool|AutomationTool" 2>/dev/null || true
  if test "$switched" = "1"; then git checkout -q "$orig_head" 2>/dev/null || log "could not restore $orig_head"; fi
  log "end"
}
trap cleanup EXIT

{
  echo "# Perf tier $tier ($stamp)"
  echo
  echo "World: $world"
  echo "Scenarios: $scenarios ; ${duration} s ; $passes pass(es)"
  echo "Host $(hostname -s); $(uptime | sed 's/^ *//')"
  echo "Map md5: $(md5 -q "$map" 2>/dev/null || echo missing)"
} > "$report"

if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac|UnrealBuildTool|AutomationTool" > /dev/null; then die "Unreal, UBT/UAT or the game is running"; fi
test -f "$map" || die "map missing"
if test "$dry" != "1"; then git checkout -q -- assets/geo/processed 2>/dev/null || true; fi
test -z "$(git status --porcelain --untracked-files=no)" || die "tracked changes in the clone"
git fetch -q origin "$branch" || die "fetch $branch failed"
if test -n "${REFERENCE:-}" && ! test -s "$REFERENCE"; then die "REFERENCE not found: $REFERENCE"; fi
if test "$dry" = "1"; then
  screen_locked && log "WARNING: screen locked, a real run would stop"
  log "DRY RUN: tier $tier on origin/$branch ($(git rev-parse --short "origin/$branch")): $scenarios, ${duration}s x $passes, world: $world"
  bash -n tools/perf/bench.sh || die "bench.sh syntax"
  echo "DRY RUN OK" >> "$report"; exit 0
fi

git checkout -q --detach "origin/$branch" || die "checkout"; switched=1
head="$(git rev-parse HEAD)"
echo "Code: $(git rev-parse --short HEAD) ($branch)" >> "$report"
build="$repo/Builds/perf-cumul/0.3"
if test "$tier" = "cold"; then build="$repo/Builds/perf-cumul-cold/0.3"; rm -rf "$build"; fi
if test "${FORCE_PACKAGE:-0}" != "1" && test "$tier" != "cold" && test -d "$build/GranTheftPraline.app" \
   && test "$(cat "$build/.commit" 2>/dev/null)" = "$head"; then
  log "build already packaged for $head"
else
  log "setup environment, limit 15 min"
  limit 900 tools/environment/setup-environment-mac.sh > "$out/setup.log" 2>&1 || die "setup failed (setup.log)"
  log "package Shipping, limit 45 min"
  BUILDS_DIR="$(dirname "$build")" limit 2700 tools/build/package-mac.sh 0.3 Shipping > "$out/package.log" 2>&1 || die "package failed (package.log)"
  echo "$head" > "$build/.commit"
fi
exe="$(find "$build/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
test -n "$exe" || die "no executable in $build"
if test "${PACKAGE_ONLY:-0}" != "1" && screen_locked; then die "screen locked: unlock the Mac, the game cannot open its window"; fi
if test "${PACKAGE_ONLY:-0}" = "1"; then echo "Packaged only (PACKAGE_ONLY=1)" >> "$report"; log "packaged only"; exit 0; fi

if test "$tier" != "cold"; then
  log "30 s control launch, limit 120 s"
  limit 120 "$exe" $world -SkipTitle -FrameLogFile="$out/control.csv" -ExitAfter=30 > "$out/control.log" 2>&1 || true
  test -s "$out/control.csv" || die "control launch produced no frame: the build freezes with this world"
fi

if test "$tier" = "long"; then
  ( while sleep 30; do
      printf '%s rss_kb=%s %s\n' "$(date +%H:%M:%S)" \
        "$(ps -axo rss=,command= | awk '/GranTheftPraline-Mac/ && !/awk/ {print $1; exit}')" \
        "$(vm_stat | awk '/Pages free|Pages active|Pages wired|Swapouts/ {printf "%s%s ", $2$3, $NF}')" >> "$out/memory.log"
    done ) > /dev/null 2>&1 &
  mem=$!
fi

if test "$tier" = "cold"; then
  for run in first second; do
    log "cold start: $run launch, limit $((duration + 120)) s"
    limit $((duration + 120)) "$exe" $world -SkipTitle -DemoRoute -FrameLogFile="$out/$run.csv" -ExitAfter="$duration" -FrameLogWarmup=0 \
      > "$out/$run.log" 2>&1 || log "  $run exited with $?"
    { echo; echo "## $run launch"; python3 -I tools/perf/summarize.py "$out/$run.csv" cold "$run" "$out/results.csv" 2>&1 || echo "no data"; } >> "$report"
    sleep 10
  done
else
  for p in $(seq 1 "$passes"); do
    log "pass $p/$passes"
    AMBIENCE=noon CAPTURE=1 EXTRA_ARGS="$world" SCENARIOS="$scenarios" DURATION="$duration" \
      limit $(( (duration + 120) * 4 + 300 )) tools/perf/bench.sh "$build" "$out/pass$p" > "$out/pass$p.log" 2>&1 || log "  pass $p ended with an error"
    { echo; echo "## Pass $p"; sed -n '/^| Scenario/,$p' "$out/pass$p/summary.md" 2>/dev/null || echo "no summary"; } >> "$report"
    if test -n "${REFERENCE:-}" && test -s "$out/pass$p/results.csv"; then
      { echo; echo "### Gate pass $p vs reference"; python3 -I tools/perf/compare.py "$REFERENCE" "$out/pass$p/results.csv" 2 || true; } >> "$report"
    fi
  done
fi
if test -s "$out/memory.log"; then
  { echo; echo "## Memory"; echo "Peak game RSS: $(awk -F'rss_kb=' '{split($2,a," "); if (a[1]+0>m) m=a[1]+0} END {printf "%.2f GB", m/1048576}' "$out/memory.log")"; } >> "$report"
fi
log "done: $report"
cat "$report"
