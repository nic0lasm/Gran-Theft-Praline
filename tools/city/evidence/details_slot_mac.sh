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
# Usage: caffeinate -dimsu tools/city/evidence/details_slot_mac.sh
#   SLOT_REF (default origin/feat/street-details-10), SLOT_MERGED (1), GTP_PLAY_ARGS,
#   PYTHON (venv for the city tools), UE_ROOT, GTP_CITY_ZONE, GTP_CITY_FOCUS,
#   DRY_RUN=1, DRY_STEP_S, HEARTBEAT_S (default 600), PLAN_SCALE (multiplies the planned durations)
# Output: Saved/details-slot-<stamp>/ (profile_*.json, play_glass.json, logs, heartbeat.log, REPORT.txt).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$repo"
dry="${DRY_RUN:-0}"
heartbeat_s="${HEARTBEAT_S:-600}"
plan_scale="${PLAN_SCALE:-1}"
slot_ref="${SLOT_REF:-origin/feat/street-details-10}"
merged="${SLOT_MERGED:-1}"
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
           tools/city/unreal/merge_detail_materials.py; do
    test -f "$repo/$f" || { say "DRY RUN: missing $f"; exit 1; }
  done
  for f in tools/city/regen_city.sh tools/city/unreal/run_city_import.sh tools/city/unreal/profile_details.sh; do
    bash -n "$f"
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
    echo "map regenerated $GTP_CITY_DETAILS ${GTP_SHOP_GLASS:-} ${GTP_DETAILS_MERGED:-0}" > "$map"
    echo "lanes v2" > "$content/City/Traffic/L_City_RueDeLaRepublique.lanes.csv"
    if [ "${GTP_DETAILS_MERGED:-0}" = "1" ]; then echo x > "$content/Environment/Props/Wall/Plaque/SM_Plaque_Merged.uasset"; fi
    echo "{\"stub\": true, \"play_args\": \"$GTP_PLAY_ARGS\"}" > "$saved/play_report.json"
    echo '{}' > "$saved/details_report.json"
  }
  profile() { sleep "${DRY_STEP_S:-2}"; echo "{\"stub\": true, \"play_args\": \"$GTP_PLAY_ARGS\"}" > "$saved/details_profile.json"; }
else
  compile() { "$ue_root/Engine/Build/BatchFiles/Mac/Build.sh" GranTheftPralineEditor Mac Development "-project=$project" -WaitMutex; }
  regen() { "$repo/tools/city/regen_city.sh"; }
  profile() { "$repo/tools/city/unreal/profile_details.sh"; }
fi

total=$((5 + 12 + 6 + 17))
if [ "$merged" = "1" ]; then total=$((total + 14 + 6)); fi
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
step "0 compile the editor for $(git rev-parse --short HEAD)" 5 "$out/compile.log" compile
GTP_CITY_DETAILS=1 GTP_SHOP_GLASS="${GTP_SHOP_GLASS:-opaque}" SKIP_PLAY=1 \
  step "1 regenerate with details, ${GTP_SHOP_GLASS:-opaque} glass" 12 "$out/regen_details.log" regen
step "2 profile the dressing" 6 "$out/profile.log" profile
cp "$saved/details_profile.json" "$out/profile_details.json"
cp "$saved/details_report.json" "$out/" 2> /dev/null || true
GTP_CITY_DETAILS=0 GTP_SHOP_GLASS="${GTP_SLOT_GLASS:-interior}" \
  step "3 regenerate without details, ${GTP_SLOT_GLASS:-interior} glass, street walk" 17 "$out/regen_glass.log" regen
cp "$saved/play_report.json" "$out/play_glass.json"
if [ "$merged" = "1" ]; then
  GTP_CITY_DETAILS=1 GTP_DETAILS_MERGED=1 GTP_SHOP_GLASS="${GTP_SHOP_GLASS:-opaque}" SKIP_PLAY=1 \
    step "4 regenerate with details baked to one material" 14 "$out/regen_merged.log" regen
  cp "$saved/merge_details_report.json" "$out/" 2> /dev/null || true
  step "5 profile the merged dressing" 6 "$out/profile_merged.log" profile
  cp "$saved/details_profile.json" "$out/profile_merged.json"
fi
cat "$out"/profile_*.json "$out/play_glass.json"
