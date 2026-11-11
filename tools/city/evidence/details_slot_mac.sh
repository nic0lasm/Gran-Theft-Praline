#!/usr/bin/env bash
# One Mac slot to find what the street dressing costs on M1, unattended, from the root of the
# GTAL-build clone. It checks out SLOT_REF (detached), then:
#   0. compiles the editor target for that commit (the -game runs use the editor binaries);
#   1. regenerates the map WITH the details and OPAQUE glass (GTP_SHOP_GLASS, default opaque);
#   2. profile_details.sh: frame times with everything, without details, without furniture and
#      without each heavy detail group;
#   3. regenerates WITHOUT details and with the INTERIOR-MAPPED glass (GTP_SLOT_GLASS, default
#      interior), with the street walk: against the opaque, detail-free map, the gap is the glass;
#   4. (SLOT_MERGED=1, default) regenerates with the details baked to one material each
#      (GTP_DETAILS_MERGED=1, merge_detail_materials.py), then
#   5. profiles again: step 5 against step 2 is the gain of the material fusion.
# Every -game run gets GTP_PLAY_ARGS (default -Sky=lightfunction, the m1 sky of the perf
# reference), so the numbers compare with the perf-cumul bench.
#
# Leaves the clone as found: Content/{Maps,City,Environment/City,Environment/Facades} are
# snapshotted first (APFS clones, no extra space) and put back, the *_Merged assets it created
# are deleted, assets/geo/processed is reset, the branch found on arrival is checked out
# again and the map md5 is checked against the one recorded on arrival.
#
# Mac slot rules (project, 10/11): every step has a planned duration; a step that runs past
# twice that is stopped, Unreal is closed, the clone is restored and the script exits with
# "MAC LIBRE" and the cause. A heartbeat line (Paris time, step, minutes) goes to stdout and
# heartbeat.log every HEARTBEAT_S seconds, for whoever runs the slot to relay. DRY_RUN=1
# replays the whole slot off the Mac with stub steps on a scratch copy of the clone layout.
#
# Usage, from the root of the clone:
#   git fetch origin feat/street-details-12 &&
#   mkdir -p Builds/details-slot &&
#   git show origin/feat/street-details-12:tools/city/evidence/details_slot_mac.sh > Builds/details-slot/run.sh &&
#   SLOT_REPO="$PWD" caffeinate -dimsu bash Builds/details-slot/run.sh
# Everything it writes stays in the clone (Saved/details-slot-<stamp>/, Content, the map), apart
# from the editor's own logs and caches: the editor scripts run in place through
# Content/Python/gtp_run.py, with no temporary copy.
#   SLOT_MODE (profile | bench), SLOT_REF (default origin/feat/street-details-12), SLOT_MERGED (1),
#   SLOT_SKIP_PROFILE (0), GTP_PLAY_ARGS; bench mode: REFERENCE (results.csv of cumul-lf),
#   BENCH_BUILD, SKIP_BUILD=1 (bench an existing build), BENCH_SCENARIOS (street-demo), BENCH_DURATION (90),
#   PYTHON (venv for the city tools), UE_ROOT, GTP_CITY_ZONE, GTP_CITY_FOCUS,
#   DRY_RUN=1, DRY_STEP_S, HEARTBEAT_S (default 600), PLAN_SCALE (multiplies the planned durations)
# Output: Saved/details-slot-<stamp>/ (profile_*.json, play_glass.json, logs, heartbeat.log, REPORT.txt).
set -euo pipefail

# Run it from a copy outside the clone (it checks out another commit while it runs, and bash
# reads a script as it goes): SLOT_REPO names the clone, default the one the script sits in.
repo="$(cd "${SLOT_REPO:-$(dirname "$0")/../../..}" && pwd)"
cd "$repo"
dry="${DRY_RUN:-0}"
heartbeat_s="${HEARTBEAT_S:-600}"
plan_scale="${PLAN_SCALE:-1}"
slot_ref="${SLOT_REF:-origin/feat/street-details-12}"
merged="${SLOT_MERGED:-1}"
mode="${SLOT_MODE:-profile}"   # profile (editor -game measures) or bench (packaged build, bench.sh)
reference_csv="${REFERENCE:-$repo/Builds/perf-cumul/slot1-20261111-010738/cumul-lf/results.csv}"
bench_build="${BENCH_BUILD:-$repo/Builds/details-bench/0.3}"
export GTP_PLAY_ARGS="${GTP_PLAY_ARGS:--Sky=lightfunction}"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
content="$repo/GranTheftPraline/Content"
saved="$repo/GranTheftPraline/Saved/CityImport"
map="$content/Maps/L_City_RueDeLaRepublique.umap"
out="$repo/GranTheftPraline/Saved/details-slot-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$out"
report="$out/REPORT.txt"
snap_dirs="Maps City Environment/City Environment/Facades"

paris() { TZ=Europe/Paris date +%H:%M; }
say() { echo "[$(paris) Paris] $*" | tee -a "$out/heartbeat.log"; }
md5of() { if command -v md5 > /dev/null; then md5 -q "$1"; else md5sum "$1" | cut -d' ' -f1; fi; }
clone_copy() { cp -cR "$1" "$2" 2> /dev/null || cp -R "$1" "$2"; }   # APFS clone, plain copy elsewhere

if [ "$dry" = "1" ]; then
  # Off-Mac rehearsal: the real script against a scratch layout, stub steps.
  for f in tools/city/regen_city.sh tools/city/unreal/profile_details.sh tools/city/unreal/profile_details.py \
           tools/city/details.py tools/city/unreal/place_props.py tools/city/unreal/run_city_import.sh \
           tools/city/unreal/merge_detail_materials.py tools/environment/setup-environment-mac.sh \
           tools/build/package-mac.sh tools/perf/bench.sh tools/perf/compare.py; do
    git cat-file -e "$slot_ref:$f" 2> /dev/null || { say "DRY RUN: missing $f in $slot_ref"; exit 1; }
  done
  for f in tools/city/regen_city.sh tools/city/unreal/run_city_import.sh tools/city/unreal/profile_details.sh; do
    git show "$slot_ref:$f" | bash -n || { say "DRY RUN: syntax error in $f"; exit 1; }
  done
  content="$out/stub-content"
  saved="$out/stub-saved"
  map="$content/Maps/L_City_RueDeLaRepublique.umap"
  mkdir -p "$saved" "$content/Maps" "$content/City/Traffic" "$content/Environment/City" \
           "$content/Environment/Facades" "$content/Environment/Props/Wall/Plaque"
  echo "map v1" > "$map"
  echo "lanes v1" > "$content/City/Traffic/L_City_RueDeLaRepublique.lanes.csv"
fi

# --- arrival state -------------------------------------------------------------------------
orig_head="$(git rev-parse --abbrev-ref HEAD)"
test "$orig_head" != "HEAD" || orig_head="$(git rev-parse HEAD)"
map_md5="$(md5of "$map" 2> /dev/null || echo missing)"
{
  echo "details slot $(date -u +%FT%TZ), dry=$dry"
  echo "clone HEAD on arrival: $orig_head ($(git rev-parse --short HEAD))"
  echo "map md5 on arrival: $map_md5"
  echo "slot ref: $slot_ref, merged step: $merged, play args: $GTP_PLAY_ARGS"
} > "$report"
marker="$out/.start"
touch "$marker"
snapshotted=0
switched=0

stop_unreal() {
  [ "$dry" = "1" ] && return 0
  local sig pids
  for sig in TERM KILL; do   # the slot's own editor, UBT and UAT; never a crash reporter or UnrealEditorServices
    pids="$(ps -axo pid=,command= | grep -E "UnrealEditor( |$)|UnrealBuildTool|AutomationTool|GranTheftPraline.app" \
      | grep -vE "CrashReportClient|UnrealEditorServices|grep -E" | awk '{print $1}')"
    if [ -n "$pids" ]; then kill -"$sig" $pids 2> /dev/null || true; fi
    [ "$sig" = "TERM" ] && sleep 10
  done
}

kill_tree() {   # macOS has no setsid/timeout: walk the children by hand
  local p
  for p in $(pgrep -P "$1" 2> /dev/null); do kill_tree "$p"; done
  kill -TERM "$1" 2> /dev/null || true
}

put_back() {
  local d real
  if [ "$snapshotted" = "1" ]; then
    for d in $snap_dirs; do
      [ -d "$out/snapshot/$d" ] || continue
      real="$(cd -P "$content/$d" 2> /dev/null && pwd || echo "$content/$d")"   # Maps may link to a shared folder
      if command -v rsync > /dev/null; then
        rsync -a --delete "$out/snapshot/$d/" "$real/" || say "restore of $d failed"
      else   # same result without rsync: empty the folder, clone the snapshot back
        find "$real" -mindepth 1 -maxdepth 1 -exec rm -rf {} +
        for f in "$out/snapshot/$d"/* "$out/snapshot/$d"/.[!.]*; do
          if [ -e "$f" ]; then clone_copy "$f" "$real/$(basename "$f")" || say "restore of $f failed"; fi
        done
      fi
    done
    # merged meshes, their baked materials and textures: created by this slot only
    find "$content/Environment/Props" -newer "$marker" -name "*_Merged*" -print -delete \
      >> "$out/removed_merged.txt" 2> /dev/null || true
  fi
  git checkout -q -- assets/geo/processed 2> /dev/null || true
  if [ "$switched" = "1" ]; then git checkout -q "$orig_head" || say "could not restore $orig_head"; fi
}

current_pid=""
on_exit() {
  local rc=$? now_md5
  trap - EXIT
  set +e   # restore as much as possible, whatever fails
  if [ -n "$current_pid" ]; then kill_tree "$current_pid"; fi
  if [ "$rc" -ne 0 ]; then stop_unreal; fi
  put_back
  now_md5="$(md5of "$map" 2> /dev/null || echo missing)"
  echo "clone HEAD at the end: $(git rev-parse --abbrev-ref HEAD) ($(git rev-parse --short HEAD))" >> "$report"
  echo "map md5 at the end: $now_md5" >> "$report"
  if [ "$now_md5" != "$map_md5" ]; then
    say "WARNING: map md5 $now_md5 differs from arrival $map_md5"
    if [ "$rc" -eq 0 ]; then rc=3; fi
  else
    say "map md5 checked, same as on arrival; clone back on $orig_head"
  fi
  if [ "$rc" -ne 0 ]; then say "MAC LIBRE (slot stopped, exit $rc). Logs: $out"; else say "MAC LIBRE (slot finished). Results: $out"; fi
  exit "$rc"
}
trap on_exit EXIT
trap 'exit 130' INT TERM

# step <name> <planned minutes> <log file> <command...>
step() {
  local name="$1" plan_min="$2" log="$3"
  shift 3
  local limit_s start now elapsed next_beat rc
  limit_s=$(awk -v m="$plan_min" -v k="$plan_scale" 'BEGIN { printf "%d", m * k * 120 }')   # 2x planned
  start=$(date +%s)
  next_beat=$((start + heartbeat_s))
  say "start: $name (planned ${plan_min} min, stop past $((limit_s / 60)) min)"
  "$@" > "$log" 2>&1 &
  current_pid=$!
  while kill -0 "$current_pid" 2> /dev/null; do
    sleep 5
    now=$(date +%s)
    elapsed=$((now - start))
    if [ "$elapsed" -gt "$limit_s" ]; then
      kill_tree "$current_pid"
      current_pid=""
      say "TIMEOUT: $name ran $((elapsed / 60)) min, over twice its ${plan_min} min plan; last log lines:"
      tail -n 15 "$log" | tee -a "$out/heartbeat.log"
      exit 124
    fi
    if [ "$now" -ge "$next_beat" ]; then
      say "alive: $name, $((elapsed / 60)) min of ${plan_min} planned"
      next_beat=$((now + heartbeat_s))
    fi
  done
  rc=0
  wait "$current_pid" || rc=$?
  current_pid=""
  if [ "$rc" -ne 0 ]; then
    say "FAILED: $name (exit $rc); last log lines:"
    tail -n 15 "$log" | tee -a "$out/heartbeat.log"
    exit "$rc"
  fi
  say "done: $name in $(( ($(date +%s) - start) / 60 )) min"
  echo "$name: $(( ($(date +%s) - start) / 60 )) min" >> "$report"
}

if [ "$dry" = "1" ]; then
  # Stubs: a few seconds each, touching what the real steps touch.
  compile() { sleep "${DRY_STEP_S:-2}"; }
  regen() {
    sleep "${DRY_STEP_S:-2}"
    if [ "${DRY_FAIL_GLASS:-0}" = "1" ] && [ "$GTP_CITY_DETAILS" = "0" ]; then echo "stub failure"; return 1; fi
    echo "map regenerated $GTP_CITY_DETAILS ${GTP_SHOP_GLASS:-} ${GTP_DETAILS_MERGED:-0}" > "$map"
    echo "lanes v2" > "$content/City/Traffic/L_City_RueDeLaRepublique.lanes.csv"
    if [ "${GTP_DETAILS_MERGED:-0}" = "1" ]; then echo x > "$content/Environment/Props/Wall/Plaque/SM_Plaque_Merged.uasset"; fi
    echo "{\"stub\": true, \"play_args\": \"$GTP_PLAY_ARGS\"}" > "$saved/play_report.json"
    echo '{}' > "$saved/details_report.json"
  }
  profile() { sleep "${DRY_STEP_S:-2}"; echo "{\"stub\": true, \"play_args\": \"$GTP_PLAY_ARGS\"}" > "$saved/details_profile.json"; }
  setup_env() { sleep "${DRY_STEP_S:-2}"; }
  package() { sleep "${DRY_STEP_S:-2}"; mkdir -p "$bench_build/GranTheftPraline.app/Contents/MacOS"; }
  control() { sleep "${DRY_STEP_S:-2}"; echo "frame" > "$out/control.csv"; }
  bench() { sleep "${DRY_STEP_S:-2}"; mkdir -p "$2"; echo "scenario,variant" > "$2/results.csv"; echo "| Scenario | stub |" > "$2/summary.md"; }
else
  compile() { "$ue_root/Engine/Build/BatchFiles/Mac/Build.sh" GranTheftPralineEditor Mac Development "-project=$project" -WaitMutex; }
  regen() { "$repo/tools/city/regen_city.sh"; }
  profile() { "$repo/tools/city/unreal/profile_details.sh"; }
  setup_env() { "$repo/tools/environment/setup-environment-mac.sh"; }
  package() { BUILDS_DIR="$repo/Builds/details-bench" "$repo/tools/build/package-mac.sh" 0.3 Shipping; }
  control() {   # 30 s launch: a build that freezes produces no frame
    local exe
    exe="$(find "$bench_build/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
    "$exe" -SkipTitle -FrameLogFile="$out/control.csv" -ExitAfter=30 || true
    test -s "$out/control.csv"
  }
  bench() {   # bench <build> <out> <reference csv or ""> <extra game args>
    CAPTURE=1 EXTRA_ARGS="${GTP_PLAY_ARGS} ${4:-}" SCENARIOS="${BENCH_SCENARIOS:-street-demo}" \
      DURATION="${BENCH_DURATION:-90}" REFERENCE="${3:-}" "$repo/tools/perf/bench.sh" "$1" "$2"
  }
fi

total=$((5 + 12 + 6 + 17))
if [ "${SLOT_SKIP_PROFILE:-0}" = "1" ]; then total=$((total - 18)); fi
if [ "$merged" = "1" ]; then total=$((total + 14 + 6)); fi
if [ "$mode" = "bench" ]; then total=$((5 + 8 + 16 + 30 + 2 + 12)); fi
if [ "$mode" = "bench" ] && [ "${SKIP_BUILD:-0}" = "1" ]; then total=$((2 + 12)); fi
say "details slot, $( [ "$dry" = "1" ] && echo "DRY RUN" || echo "Mac" ), output $out; planned about $total min"

# --- prerequisites and checkout ------------------------------------------------------------
if [ "$dry" != "1" ] && ps -axo pid=,command= | grep -E "UnrealEditor( |$)|UnrealBuildTool|AutomationTool|GranTheftPraline.app" \
    | grep -vE "CrashReportClient|UnrealEditorServices|grep -E" > /dev/null; then
  say "an Unreal editor, a build (UBT/UAT) or the game is running: one at a time"; exit 1
fi
test -f "$map" || { say "map missing: $map"; exit 1; }
git checkout -q -- assets/geo/processed 2> /dev/null || true
test -z "$(git status --porcelain --untracked-files=no)" || { say "tracked changes in the clone, refusing to switch"; exit 1; }
if [ "$dry" != "1" ]; then
  git fetch -q origin "${slot_ref#origin/}" || { say "fetch ${slot_ref#origin/} failed"; exit 1; }
fi
say "snapshot of Content: $snap_dirs"
mkdir -p "$out/snapshot/Environment"
for d in $snap_dirs; do
  if [ -e "$content/$d" ]; then clone_copy "$(cd -P "$content/$d" && pwd)" "$out/snapshot/$d"; fi
done
snapshotted=1
if [ "$dry" != "1" ]; then
  git checkout -q --detach "$slot_ref"
  switched=1
fi
echo "slot commit: $(git rev-parse --short HEAD)" >> "$report"

# --- the measures ----------------------------------------------------------------------------
if [ "$mode" != "bench" ] || [ "${SKIP_BUILD:-0}" != "1" ]; then
  step "0 compile the editor for $(git rev-parse --short HEAD)" 5 "$out/compile.log" compile
fi
if [ "$mode" = "bench" ]; then
  # Official A/B on a packaged Shipping build: one build carries the details AND their
  # one-material bake (GTP_DETAILS_MERGED=ab), and UGTPCityDetailsSwitch picks the set at launch:
  # -CityDetails=0 (none), default (details), -CityDetailsMerged=1 (bake). Demo route, 90 s each,
  # the details run gated against the perf reference (REFERENCE). BENCH_BUILD=<dir> with
  # SKIP_BUILD=1 benches a build packaged elsewhere (about 15 min instead of about 75).
  if [ "${SKIP_BUILD:-0}" != "1" ]; then
    step "S setup environment assets" 8 "$out/setup.log" setup_env
    GTP_CITY_DETAILS=1 GTP_DETAILS_MERGED=ab GTP_SHOP_GLASS="${GTP_SHOP_GLASS:-opaque}" SKIP_PLAY=1 \
      step "1 regenerate with details and their bake, ${GTP_SHOP_GLASS:-opaque} glass" 16 "$out/regen_details.log" regen
    cp "$saved/details_report.json" "$saved/details_merged_report.json" "$saved/merge_details_report.json" "$out/" 2> /dev/null || true
    step "P package Shipping to $bench_build" 30 "$out/package.log" package
    git rev-parse HEAD > "$bench_build/.commit" 2> /dev/null || true
  fi
  test -d "$bench_build/GranTheftPraline.app" || { say "no build at $bench_build"; exit 1; }
  step "C 30 s control launch" 2 "$out/control.log" control
  if [ ! -s "$reference_csv" ]; then say "no reference csv at $reference_csv: bench without the gate"; reference_csv=""; fi
  echo "build: $bench_build ($(cat "$bench_build/.commit" 2> /dev/null || echo ?)); reference csv: ${reference_csv:-none}" >> "$report"
  step "B0 bench without details" 4 "$out/bench-nodetails.log" bench "$bench_build" "$out/nodetails" "" "-CityDetails=0"
  step "B1 bench with details (gate: reference)" 4 "$out/bench-details.log" bench "$bench_build" "$out/details" "$reference_csv" ""
  step "B2 bench with the one-material bake" 4 "$out/bench-merged.log" bench "$bench_build" "$out/merged" "" "-CityDetailsMerged=1"
  {
    echo
    for b in nodetails details merged; do
      if [ -f "$out/$b/summary.md" ]; then echo "## $b"; sed -n '/^| Scenario/,$p' "$out/$b/summary.md"; echo; fi
    done
    if [ "$dry" != "1" ]; then
      echo "## details vs no details (cost of the dressing)"
      python3 -I "$repo/tools/perf/compare.py" "$out/nodetails/results.csv" "$out/details/results.csv" 2 || true
      echo "## merged vs details (gain of the bake)"
      python3 -I "$repo/tools/perf/compare.py" "$out/details/results.csv" "$out/merged/results.csv" 2 || true
    fi
  } >> "$report"
  cat "$report"
  exit 0
fi
if [ "${SLOT_SKIP_PROFILE:-0}" != "1" ]; then   # SLOT_SKIP_PROFILE=1: steps 1-2 already measured
GTP_CITY_DETAILS=1 GTP_SHOP_GLASS="${GTP_SHOP_GLASS:-opaque}" SKIP_PLAY=1 \
  step "1 regenerate with details, ${GTP_SHOP_GLASS:-opaque} glass" 12 "$out/regen_details.log" regen
step "2 profile the dressing" 6 "$out/profile.log" profile
cp "$saved/details_profile.json" "$out/profile_details.json"
cp "$saved/details_report.json" "$out/" 2> /dev/null || true
fi
# Step 3 is soft: if it fails (not a time-out), steps 4-5 still run. A subshell does not
# inherit the EXIT trap, so its failure only ends the subshell.
rc3=0
( GTP_CITY_DETAILS=0 GTP_SHOP_GLASS="${GTP_SLOT_GLASS:-interior}" \
  step "3 regenerate without details, ${GTP_SLOT_GLASS:-interior} glass, street walk" 17 "$out/regen_glass.log" regen ) || rc3=$?
if [ "$rc3" -ne 0 ]; then
  if [ "$rc3" -eq 124 ]; then exit 124; fi   # a time-out still stops the slot
  stop_unreal
  say "step 3 failed (exit $rc3): going on with steps 4-5"
  echo "3 FAILED (exit $rc3), slot went on" >> "$report"
fi
cp "$saved/play_report.json" "$out/play_glass.json" 2> /dev/null || true
cp "$saved/materials_report.json" "$out/materials_glass.json" 2> /dev/null || true
if [ "$merged" = "1" ]; then
  GTP_CITY_DETAILS=1 GTP_DETAILS_MERGED=1 GTP_SHOP_GLASS="${GTP_SHOP_GLASS:-opaque}" SKIP_PLAY=1 \
    step "4 regenerate with details baked to one material" 14 "$out/regen_merged.log" regen
  cp "$saved/merge_details_report.json" "$out/" 2> /dev/null || true
  step "5 profile the merged dressing" 6 "$out/profile_merged.log" profile
  cp "$saved/details_profile.json" "$out/profile_merged.json"
fi
cat "$out"/profile_*.json "$out/play_glass.json" 2> /dev/null || true
