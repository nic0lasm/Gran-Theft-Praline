#!/usr/bin/env bash
# Perf slot 5: far crowd (-PedFar, SM_PedFar_A, vertex animation) against the full
# pedestrian pool, in one command, unattended.
#
# Usage (from the root of the GTAL-build clone, nothing else running; the path may
# contain spaces):
#   caffeinate -dimsu tools/perf/slot5-crowd-mac.sh
# Environment:
#   DRY_RUN=1    check the prerequisites and the syntax, print the plan, run nothing heavy
#   REF_BUILD    unused; the cases compare with each other in one build
#   DURATION     seconds per bench run (default 90)
#   BRANCH       code to build (default feat/pedestrians-slot5 = feat/perf-cumul + this script)
#   EXPECTED_MAP_MD5   map fingerprint to require (default 1c92de0f813b89cb40aeb000cd92a1de)
# Output: Builds/perf-slot5/slot5-<stamp>/ (REPORT.md, steps.log, heartbeat.txt, one folder per case)
#
# Planned ~60 min. Every step has a time limit of twice its planned duration; a heartbeat
# line is written every 10 minutes; the map is fingerprinted before and after (and restored
# from a copy if it changed); the clone goes back to its original HEAD on exit.
# Steps: environment assets, [mesh bake if SM_PedFar_A is missing], package Shipping,
# 30 s control launch, bench cases n0 n20 n40-vis20 n60 n40-far60 n40-far100 [n40-far100-vat].
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$repo/Builds/perf-slot5/slot5-$stamp"
branch="${BRANCH:-feat/pedestrians-slot5}"
duration="${DURATION:-90}"
expected_md5="${EXPECTED_MAP_MD5:-1c92de0f813b89cb40aeb000cd92a1de}"
dry="${DRY_RUN:-0}"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
map="$repo/GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
far_dir="$repo/GranTheftPraline/Content/Characters/Far"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
ubt_build="$ue_root/Engine/Build/BatchFiles/Mac/Build.sh"
mkdir -p "$out"
report="$out/REPORT.md"

log() { printf '%s %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$out/steps.log"; }
die() { log "ABORT: $*"; printf '\n**ABORTED:** %s\n' "$*" >> "$report"; exit 1; }

# Fingerprint and size that work on macOS and on the Linux cloud (for the dry-run test).
md5of() { if command -v md5 > /dev/null 2>&1; then md5 -q "$1"; else md5sum "$1" | cut -d' ' -f1; fi; }
sizeof() { stat -f %z "$1" 2> /dev/null || stat -c %s "$1" 2> /dev/null || echo ?; }

# Time limit without coreutils `timeout`: perl alarm + exec. The caller's arguments are
# passed as they are, so paths with spaces survive.
limit() { local s="$1"; shift; perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$s" "$@"; }

( while sleep 600; do printf "%s alive\n" "$(date +%H:%M:%S)" >> "$out/heartbeat.txt"; done ) > /dev/null 2>&1 &
hb=$!
orig_head="$(git rev-parse --abbrev-ref HEAD)"
test "$orig_head" != "HEAD" || orig_head="$(git rev-parse HEAD)"
map_before=""
cleanup() {
  kill "$hb" 2> /dev/null || true
  if test "$dry" != "1"; then
    # A step killed by its time limit leaves children alive: stop the game, the editor, UBT,
    # UAT; never the UnrealEditorServices helper.
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
  echo "# Perf slot 5 - far crowd ($stamp)"
  echo
  echo "Host $(hostname -s); $(uptime | sed 's/^ *//')"
  echo "Clone HEAD before: $orig_head; code branch $branch"
  echo "Clone path: $repo"
} > "$report"

log "prerequisites"
if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac" > /dev/null; then
  test "$dry" = "1" && log "(dry run) an Unreal process is running: a real run would stop here" || die "an Unreal editor or the game is running"
fi
if test -f "$map"; then
  map_before="$(md5of "$map")"
  echo "Map md5 before: $map_before  size $(sizeof "$map")" >> "$report"
  log "map md5 before: $map_before"
  test "$map_before" = "$expected_md5" || { test "$dry" = "1" && log "(dry run) map md5 differs from $expected_md5" || die "map md5 $map_before is not the expected $expected_md5 (regenerate or pass EXPECTED_MAP_MD5)"; }
else
  test "$dry" = "1" && log "(dry run) map missing: $map" || die "map missing: $map"
fi
for f in "$ue_root/Engine/Build/BatchFiles/RunUAT.sh"; do
  test -x "$f" || { test "$dry" = "1" && log "(dry run) missing $f" || die "missing $f (set UE_ROOT)"; }
done
bash -n "$repo/tools/perf/bench.sh" || die "bench.sh syntax"
bash -n "${BASH_SOURCE[0]}" || die "this script's syntax"

if test "$dry" = "1"; then
  log "DRY RUN plan: stash-free checkout of origin/$branch; setup-environment-mac.sh (limit 30 min); if $far_dir/SM_PedFar_A.uasset is missing: UBT editor build (limit 60 min), mesh bake (limit 20 min), VAT bake (limit 20 min); package Shipping to Builds/perf-slot5/0.3 (limit 90 min); 30 s control launch; bench cases n0 n20 n40-vis20 n60 n40-far60 n40-far100 [n40-far100-vat], ${duration} s each (limit 25 min per case); map md5 check and restore; report"
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

log "setup environment assets (M_CloudShadowLF), limit 30 min"
limit 1800 tools/environment/setup-environment-mac.sh > "$out/setup.log" 2>&1 || die "setup-environment-mac.sh failed (see setup.log)"

mesh_still="$far_dir/SM_PedFar_A.uasset"
vat_mesh=""
if ! test -f "$mesh_still"; then
  log "SM_PedFar_A missing: baking it here (editor build limit 60 min, each bake limit 20 min)"
  limit 3600 "$ubt_build" GranTheftPralineEditor Mac Development "-project=$project" -NoMutex > "$out/editor-build.log" 2>&1 \
    || log "editor build failed or timed out (see editor-build.log); the bakes may not run"
  tmp="$(mktemp -d)"
  cp tools/assets/unreal/make_ped_far.py "$tmp/make_ped_far.py"
  cp tools/pedestrians/make_far_vat.py "$tmp/make_far_vat.py"
  for pair in "make_ped_far:ped-far-bake" "make_far_vat:ped-far-vat"; do
    mod="${pair%%:*}"; tag="${pair##*:}"
    # Wrapper: run the bake script, then quit the editor (-ExecutePythonScript keeps it open).
    printf 'import runpy\nimport unreal\ntry:\n    runpy.run_path("%s/%s.py", run_name="__main__")\nfinally:\n    unreal.SystemLibrary.execute_console_command(None, "quit")\n' "$tmp" "$mod" > "$tmp/run_$mod.py"
    log "bake $mod, limit 20 min"
    limit 1200 "$editor" "$project" "-ExecutePythonScript=$tmp/run_$mod.py" -unattended -nosplash -stdout -FullStdOutLogOutput \
      > "$out/$tag.log" 2>&1 || log "$mod ended with an error or timed out (see $tag.log)"
    pkill -f "UnrealEditor( |$)" 2> /dev/null || true
  done
  rm -rf "$tmp"
fi
if test -f "$mesh_still"; then log "SM_PedFar_A present"; echo "SM_PedFar_A: present" >> "$report"; else log "SM_PedFar_A still missing: far cases will report the layer off"; echo "SM_PedFar_A: MISSING (far cases measure nothing)" >> "$report"; fi
vat_file="$(ls -1 "$far_dir" 2> /dev/null | grep -i 'VAT.*\.uasset$' | grep -i '^SM_' | head -n 1 || true)"
if test -n "$vat_file"; then
  vat_name="${vat_file%.uasset}"
  vat_mesh="/Game/Characters/Far/$vat_name.$vat_name"
  echo "VAT mesh: $vat_mesh" >> "$report"
else
  echo "VAT mesh: none found (the vat case is skipped)" >> "$report"
fi

log "package Shipping, limit 90 min"
BUILDS_DIR="$repo/Builds/perf-slot5" limit 5400 tools/build/package-mac.sh 0.3 Shipping > "$out/package.log" 2>&1 || die "package failed (see package.log)"
build="$repo/Builds/perf-slot5/0.3"
test -d "$build/GranTheftPraline.app" || die "no app after packaging"

log "30 s control launch (freeze check), limit 120 s"
exe="$(find "$build/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
limit 120 "$exe" -SkipTitle "-FrameLogFile=$out/control.csv" -ExitAfter=30 > "$out/control.log" 2>&1 || true
test -s "$out/control.csv" || die "control launch produced no frame: the build freezes"
echo "Control launch: $(wc -l < "$out/control.csv" | tr -d ' ') frames in 30 s" >> "$report"

off="-Traffic=0 -Police=0 -Ragdoll=0 -Minimap=0 -DayNight=0 -Audio=0"
run_case() { # name extra
  log "bench $1, limit 25 min"
  CAPTURE=0 EXTRA_ARGS="$off $2" SCENARIOS="street-demo" DURATION="$duration" \
    limit 1500 tools/perf/bench.sh "$build" "$out/$1" > "$out/$1.log" 2>&1 || log "bench $1 ended with an error (see $1.log)"
  { echo; echo "## $1 ($2)"; sed -n '/^| Scenario/,$p' "$out/$1/summary.md" 2> /dev/null || echo "no summary"; grep -h "LogGTPPedestrians" "$out/$1.log" 2> /dev/null | tail -n 3 | sed 's/^/    /'; } >> "$report"
}
run_case n0 "-Pedestrians=0"
run_case n20 "-Pedestrians=20 -PedMaxVisible=20"
run_case n40-vis20 "-Pedestrians=40 -PedMaxVisible=20"
run_case n60 "-Pedestrians=60 -PedMaxVisible=60"
run_case n40-far60 "-Pedestrians=40 -PedMaxVisible=20 -PedFar=60"
run_case n40-far100 "-Pedestrians=40 -PedMaxVisible=20 -PedFar=100"
if test -n "$vat_mesh"; then
  run_case n40-far100-vat "-Pedestrians=40 -PedMaxVisible=20 -PedFar=100 -PedFarMesh=$vat_mesh"
fi

for c in n20 n40-vis20 n60 n40-far60 n40-far100; do
  if test -s "$out/n0/results.csv" && test -s "$out/$c/results.csv"; then
    { echo; echo "## Gate $c vs n0"; python3 -I tools/perf/compare.py "$out/n0/results.csv" "$out/$c/results.csv" 2 || true; } >> "$report"
  fi
done
log "map md5 check"
test "$(md5of "$map")" = "$map_before" && echo "Map md5 after: unchanged ($map_before)" >> "$report" || echo "Map md5 after: CHANGED (restored on exit)" >> "$report"
log "done, report: $report"
cat "$report"
