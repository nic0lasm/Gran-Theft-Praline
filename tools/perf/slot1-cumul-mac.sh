#!/usr/bin/env bash
# Perf slot 1: cumulative gains (light function cloud shadow + facade LODs +
# traffic LOD/ghost) against the healthy reference build, in one command.
#
# Usage (from the root of the GTAL-build clone, nothing else running):
#   caffeinate -dimsu tools/perf/slot1-cumul-mac.sh
# Environment:
#   DRY_RUN=1   print the steps and check the prerequisites, run nothing heavy
#   REF_BUILD   reference build dir (default Builds/city-slot/0.3-ref)
#   DURATION    seconds per bench run (default 90)
# Output: Builds/perf-cumul/slot1-<stamp>/ (REPORT.md, heartbeat.txt, bench folders)
#
# Every step has a time limit; a heartbeat line is written every 10 minutes.
# The clone's branch is restored on exit. The map on disk is NOT regenerated:
# it must be the healthy 799c8d5 regeneration (md5 recorded in REPORT.md).
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$repo/Builds/perf-cumul/slot1-$stamp"
ref_build="${REF_BUILD:-$repo/Builds/city-slot/0.3-ref}"
duration="${DURATION:-90}"
branch="feat/perf-cumul"
map="$repo/GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
dry="${DRY_RUN:-0}"
mkdir -p "$out"
report="$out/REPORT.md"

log() { printf '%s %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$out/steps.log"; }
die() { log "ABORT: $*"; printf '\n**ABORTED:** %s\n' "$*" >> "$report"; exit 1; }

# Time limit without coreutils `timeout` (macOS): perl alarm + exec.
limit() { local s="$1"; shift; perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$s" "$@"; }

# Heartbeat every 10 minutes until the script ends.
( while sleep 600; do printf "%s alive\n" "$(date +%H:%M:%S)" >> "$out/heartbeat.txt"; done ) > /dev/null 2>&1 &
hb=$!
orig_head="$(git rev-parse --abbrev-ref HEAD)"
test "$orig_head" != "HEAD" || orig_head="$(git rev-parse HEAD)"
cleanup() {
  pkill -P "$hb" 2>/dev/null || true; kill "$hb" 2>/dev/null || true
  pkill -f "GranTheftPraline-Mac" 2>/dev/null || true
  if test "$dry" != "1"; then git checkout -q "$orig_head" 2>/dev/null || log "could not restore $orig_head"; fi
  log "end (clone back on $orig_head)"
}
trap cleanup EXIT

{
  echo "# Perf slot 1 - cumulative gains ($stamp)"
  echo
  echo "Host $(hostname -s); $(uptime | sed 's/^ *//')"
  echo "Clone HEAD before: $orig_head"
  echo "Map md5: $(md5 -q "$map" 2>/dev/null || echo missing)  size $(stat -f %z "$map" 2>/dev/null || echo ?)"
  echo "Reference build: $ref_build"
} > "$report"

log "prerequisites"
if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac" > /dev/null; then die "an Unreal editor or the game is running"; fi
test -f "$map" || die "map missing: $map"
test -d "$ref_build/GranTheftPraline.app" || die "reference build missing: $ref_build"
test -z "$(git status --porcelain --untracked-files=no)" || die "tracked changes in the clone, refusing to switch branch"
git fetch -q origin "$branch" || die "fetch $branch failed"

if test "$dry" = "1"; then
  log "DRY RUN: would check out origin/$branch ($(git rev-parse --short "origin/$branch")), run setup-environment-mac.sh, package to Builds/perf-cumul/0.3, 30 s control launch, then bench ref + cumul (lightfunction, volumetric)"
  bash -n tools/perf/bench.sh || die "bench.sh syntax"
  echo "DRY RUN OK" >> "$report"
  exit 0
fi

git checkout -q --detach "origin/$branch" || die "checkout"
echo "Code: $(git rev-parse --short HEAD) ($branch)" >> "$report"

log "setup environment assets (M_CloudShadowLF), limit 15 min"
limit 900 tools/environment/setup-environment-mac.sh > "$out/setup.log" 2>&1 || die "setup-environment-mac.sh failed (see setup.log)"

log "package Shipping, limit 45 min"
BUILDS_DIR="$repo/Builds/perf-cumul" limit 2700 tools/build/package-mac.sh 0.3 Shipping > "$out/package.log" 2>&1 || die "package failed (see package.log)"
build="$repo/Builds/perf-cumul/0.3"
test -d "$build/GranTheftPraline.app" || die "no app after packaging"

log "30 s control launch (freeze check), limit 120 s"
exe="$(find "$build/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
limit 120 "$exe" -SkipTitle -FrameLogFile="$out/control.csv" -ExitAfter=30 > "$out/control.log" 2>&1 || true
test -s "$out/control.csv" || die "control launch produced no frame: the build freezes"
echo "Control launch: $(wc -l < "$out/control.csv" | tr -d ' ') frames in 30 s" >> "$report"

run_bench() { # name build extra_args
  log "bench $1, limit 20 min"
  CAPTURE=1 EXTRA_ARGS="$3" SCENARIOS="street-fixed street-demo" DURATION="$duration" \
    limit 1200 tools/perf/bench.sh "$2" "$out/$1" > "$out/$1.log" 2>&1 || log "bench $1 ended with an error (see $1.log)"
  { echo; echo "## $1 ($3)"; cat "$out/$1/summary.md" 2>/dev/null | sed -n '/^| Scenario/,$p' || echo "no summary"; } >> "$report"
}
run_bench ref "$ref_build" ""
run_bench cumul-lf "$build" "-Sky=lightfunction"
run_bench cumul-vol "$build" "-Sky=volumetric"
run_bench cumul-traffic12 "$build" "-Sky=lightfunction -Traffic=12 -VehicleLod=1"

if test -s "$out/ref/results.csv" && test -s "$out/cumul-lf/results.csv"; then
  { echo; echo "## Gate cumul-lf vs ref"; python3 -I tools/perf/compare.py "$out/ref/results.csv" "$out/cumul-lf/results.csv" 2 || true; } >> "$report"
fi
log "done, report: $report"
cat "$report"
