#!/usr/bin/env bash
# One Shipping package of an integration candidate WITH the content steps its feature
# branches need, so the merge gate measures what the branches add:
#   DETAILS=1  street details placed in the map by the editor (as details_slot_mac.sh, bench mode):
#              setup-environment-mac.sh, then regen_city.sh with GTP_CITY_DETAILS=1
#   TREES=1    leafy street trees (as tree-bench-mac.sh): prepare_trees_m1.py, pick with
#              swap_street_trees.py (map read only), the leafy mesh folders added to
#              DirectoriesToAlwaysCook in Config/DefaultGame.ini for the package only
#   INTERIORS=1 every interior declared in tools/interiors/*.json (all but catalog.json) rebuilt by
#              build_interior_level.py with its anchor in the city map (GTP_INTERIOR_ANCHOR=1), so
#              the package cooks /Game/Interiors/L_Interior_<id>; stops if a level is missing.
#              The generated levels are put back as found after the package (a level that was
#              there before is restored, one the step created is removed), the map with the rest.
# then ONE package (tools/build/package-mac.sh), then Content, the ini and the clone are put back
# as found and the map md5 is checked.
#
# Usage, from the root of a clean clone checked out on the candidate, nothing else running:
#   caffeinate -dimsu tools/perf/candidate-build-mac.sh [version=0.3]
# Environment:
#   BUILDS_DIR (default <repo>/Builds/candidate), DETAILS (1), TREES (1), INTERIORS (1), UE_ROOT
#   (default /Users/Shared/Epic Games/UE_5.8), PYTHON (python3 for regen_city.sh, with numpy,
#   scipy, shapely, PIL), GTP_SHOP_GLASS (opaque), GTP_TREE_PATTERN (/M1/), MAP_MD5 (expected
#   md5 of the city map at start; checked when set), GTP_INTERIOR_MESHES (mesh map of the
#   interiors, grey boxes without), SKIP_COMPILE=1 (editor already built for
#   this commit), HEARTBEAT_S (600), EXPECTED_S (forces every step length, to test the watchdog),
#   DRY_RUN=1 (stubs for Unreal and the package, against a scratch layout under
#   GranTheftPraline/Saved: checks the script end to end in seconds, anywhere).
# Every step runs under a watchdog: past twice its expected length it is stopped, every Unreal
# process is stopped, the clone is put back and the script exits 3. Every editor launch is
# headless (-nullrhi), so a locked screen does not stop the build; the lock state is reported,
# since the benches after it need the screen unlocked.
# Output: <BUILDS_DIR>/<version>/GranTheftPraline.app, candidate.json (commit, steps,
# play arguments) and trees_pick.json next to it; logs and CANDIDATE.md in
# GranTheftPraline/Saved/candidate-build-<stamp>/.
set -uo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$script_dir/../.." && pwd)"
cd "$repo" || exit 1
version="${1:-0.3}"
dry="${DRY_RUN:-0}"
details="${DETAILS:-1}"
trees="${TREES:-1}"
interiors="${INTERIORS:-1}"
builds_dir="${BUILDS_DIR:-$repo/Builds/candidate}"
build_dir="$builds_dir/$version"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
content="$repo/GranTheftPraline/Content"
saved="$repo/GranTheftPraline/Saved/CityImport"
game_ini="$repo/GranTheftPraline/Config/DefaultGame.ini"
out="$repo/GranTheftPraline/Saved/candidate-build-$(date +%Y%m%d-%H%M%S)"
report="$out/CANDIDATE.md"
snap_dirs="Maps City Environment/City Environment/Facades"
heartbeat_s="${HEARTBEAT_S:-600}"
mkdir -p "$out"

paris() { TZ=Europe/Paris date '+%H:%M'; }
say() { echo "[$(paris) Paris] $*" | tee -a "$out/heartbeat.log"; }
md5of() { if command -v md5 > /dev/null; then md5 -q "$1"; else md5sum "$1" | cut -d' ' -f1; fi; }
clone_copy() { cp -cR "$1" "$2" 2> /dev/null || cp -R "$1" "$2"; }   # APFS clone, plain copy elsewhere
screen_locked() { ioreg -n Root -d1 -a 2> /dev/null | grep -A1 CGSSessionScreenIsLocked | grep -q "<true/>"; }

expected() {
  if test -n "${EXPECTED_S:-}"; then echo "$EXPECTED_S"; return; fi
  case "$1" in
    compile) echo 600 ;;
    setup_env) echo 480 ;;
    details) echo 960 ;;
    prepare) echo 600 ;;
    pick) echo 420 ;;
    interiors) echo 300 ;;
    package) echo 1800 ;;
    *) echo 600 ;;
  esac
}

# --- tools, real or stubs ---------------------------------------------------------------------
if test "$dry" = "1"; then
  for f in tools/city/regen_city.sh tools/city/unreal/editor_run.sh tools/environment/setup-environment-mac.sh \
           tools/assets/unreal/prepare_trees_m1.py tools/city/unreal/swap_street_trees.py tools/build/package-mac.sh \
           tools/interiors/unreal/build_interior_level.py; do
    test -f "$repo/$f" || { say "DRY RUN: missing $f"; exit 1; }
  done
  for f in tools/city/regen_city.sh tools/city/unreal/editor_run.sh tools/environment/setup-environment-mac.sh tools/build/package-mac.sh; do
    bash -n "$repo/$f" || { say "DRY RUN: syntax error in $f"; exit 1; }
  done
  content="$out/stub-content"
  saved="$out/stub-saved"
  game_ini="$out/stub-DefaultGame.ini"
  mkdir -p "$saved" "$content/Maps" "$content/City" "$content/Environment/City" "$content/Environment/Facades"
  echo "map v1" > "$content/Maps/L_City_RueDeLaRepublique.umap"
  printf '[/Script/UnrealEd.ProjectPackagingSettings]\nBuildConfiguration=PPBC_Shipping\n' > "$game_ini"
  compile() { sleep "${DRY_SLEEP:-1}"; }
  setup_env() { sleep "${DRY_SLEEP:-1}"; }
  regen() { sleep "${DRY_SLEEP:-1}"; echo "map v2 with details" > "$content/Maps/L_City_RueDeLaRepublique.umap"; mkdir -p "$content/City/Details"; }
  run_py() {   # run_py <name> <script> <report>
    sleep "${DRY_SLEEP:-1}"
    case "$2" in
      *prepare_trees_m1.py) echo '{"errors": [], "meshes": []}' > "$3" ;;
      *build_interior_level.py)
        mkdir -p "$content/Interiors"; echo "level" > "$content/Interiors/L_Interior_$(basename "$GTP_INTERIOR_SPEC" .json).umap"
        echo "map v3 with anchor" > "$content/Maps/L_City_RueDeLaRepublique.umap"
        mkdir -p "$(dirname "$3")"; echo '{"errors": []}' > "$3" ;;
      *swap_street_trees.py) echo '{"mode": "pick", "errors": [], "swap_arg": "/Game/A/SM_Bare~/Game/B/M1/SM_Leafy", "cook_dirs": ["/Game/B/M1"], "picked": {"W14-63": {"trees": 104}, "W14-64": {"trees": 8}}}' > "$3" ;;
    esac
  }
  package_cmd() { sleep "${DRY_SLEEP:-1}"; grep -c 'DirectoriesToAlwaysCook' "$game_ini"; grep 'MapsToCook' "$game_ini"; mkdir -p "$build_dir/GranTheftPraline.app/Contents/MacOS"; printf 'version=%s\ncommit=%s\n' "$version" "$(git rev-parse --short HEAD)" > "$build_dir/build-info.txt"; }
else
  # shellcheck source=../city/unreal/editor_run.sh
  . "$repo/tools/city/unreal/editor_run.sh"
  compile() { "$ue_root/Engine/Build/BatchFiles/Mac/Build.sh" GranTheftPralineEditor Mac Development "-project=$project" -WaitMutex; }
  setup_env() { "$repo/tools/environment/setup-environment-mac.sh"; }
  regen() { GTP_CITY_DETAILS=1 GTP_DETAILS_MERGED=0 GTP_SHOP_GLASS="${GTP_SHOP_GLASS:-opaque}" SKIP_PLAY=1 "$repo/tools/city/regen_city.sh"; }
  run_py() {   # one headless editor launch per script, done when <report> is written
    rm -f "$3"
    EDITOR_DONE_FILE="$3" EDITOR_MAX_S="$(( 2 * $(expected "$1") - 30 ))" GTP_PY_SCRIPT="$repo/$2" \
      run_editor "Candidate_$1" "$editor" "$project" -nullrhi -unattended -nosplash -nosound -NSAppSleepDisabled YES \
      "-ExecutePythonScript=__import__('gtp_run').main()"
    test -f "$3"
  }
  package_cmd() { BUILDS_DIR="$builds_dir" "$repo/tools/build/package-mac.sh" "$version" Shipping; }
fi
map="$content/Maps/L_City_RueDeLaRepublique.umap"

# --- arrival state and guards -----------------------------------------------------------------
orig_head="$(git rev-parse --abbrev-ref HEAD)"
test "$orig_head" != "HEAD" || orig_head="$(git rev-parse HEAD)"
map_md5="$(md5of "$map" 2> /dev/null || echo missing)"
ini_md5="$(md5of "$game_ini")"
locked=no; test "$dry" != "1" && screen_locked && locked=yes
{
  echo "# Candidate build $version, $(TZ=Europe/Paris date '+%Y-%m-%d %H:%M') heure de Paris$(test "$dry" = 1 && echo ', DRY RUN (stubs)')"
  echo
  echo "- commit $(git rev-parse --short HEAD), DETAILS=$details, TREES=$trees, INTERIORS=$interiors, build $build_dir"
  echo "- map md5 at start: $map_md5; screen locked at start: $locked (the build steps are headless)"
} > "$report"
if test -n "${MAP_MD5:-}" && test "$map_md5" != "$MAP_MD5"; then
  echo "- map md5 is not the expected $MAP_MD5: stopped before anything ran" >> "$report"; say "map md5 $map_md5, expected $MAP_MD5"; exit 1
fi
if test "$dry" != "1"; then
  if ps -axo command= | grep -E "UnrealEditor( |$)|UnrealBuildTool|AutomationTool|GranTheftPraline.app" \
      | grep -vE "UnrealEditorServices|CrashReportClient|UnrealTraceServer|grep -E" > /dev/null; then
    say "an Unreal editor, a build or the game is running: one at a time"; exit 1
  fi
  test -x "$editor" || { say "no Unreal Editor at $editor"; exit 1; }
  test -f "$content/Python/gtp_run.py" || { say "Content/Python/gtp_run.py missing"; exit 1; }
  test -z "$(git status --porcelain --untracked-files=no)" || { say "tracked changes in the clone, refusing to run"; exit 1; }
fi

# --- put back as found, whatever happens ---------------------------------------------------------
marker="$out/.start"; touch "$marker"
snapshotted=0
current_pid=""
stop_unreal() {
  test "$dry" = "1" && return 0
  local sig pids
  for sig in TERM KILL; do   # this script's editor, UBT and UAT; never a crash reporter or a service
    pids="$(ps -axo pid=,command= | grep -E "UnrealEditor( |$)|UnrealBuildTool|AutomationTool|ShaderCompileWorker" \
      | grep -vE "CrashReportClient|UnrealEditorServices|grep -E" | awk '{print $1}')"
    if test -n "$pids"; then kill -"$sig" $pids 2> /dev/null || true; fi
    test "$sig" = "TERM" && sleep 10
  done
}
kill_tree() { local p; for p in $(pgrep -P "$1" 2> /dev/null); do kill_tree "$p"; done; kill -TERM "$1" 2> /dev/null || true; }
put_back() {
  local d real
  if test -f "$out/DefaultGame.ini.orig"; then cp "$out/DefaultGame.ini.orig" "$game_ini" && rm -f "$out/DefaultGame.ini.orig"; fi
  if test "$snapshotted" = "1"; then
    for d in $snap_dirs; do
      test -d "$out/snapshot/$d" || continue
      real="$(cd -P "$content/$d" 2> /dev/null && pwd || echo "$content/$d")"   # Maps may link to a shared folder
      rsync -a --delete "$out/snapshot/$d/" "$real/" || say "restore of $d failed"
    done
    rm -rf "$out/snapshot"
  fi
  if test -f "$out/interiors.created"; then   # generated interior levels: as found
    local f
    while IFS= read -r f; do
      test -n "$f" || continue
      if test -e "$out/interiors-snapshot/$f"; then
        rm -rf "${content:?}/$f"; clone_copy "$out/interiors-snapshot/$f" "$content/$f" || say "restore of $f failed"
      else
        rm -rf "${content:?}/$f"
      fi
    done < "$out/interiors.created"
    rm -rf "$out/interiors-snapshot" "$out/interiors.created"
  fi
  test "$dry" = "1" || git checkout -q -- assets/geo/processed 2> /dev/null || true
}
on_exit() {
  local rc=$? now_md5 now_ini
  trap - EXIT
  set +e
  if test -n "$current_pid"; then kill_tree "$current_pid"; fi
  if test "$rc" -ne 0; then stop_unreal; fi
  put_back
  now_md5="$(md5of "$map" 2> /dev/null || echo missing)"; now_ini="$(md5of "$game_ini")"
  echo "- map md5 at the end: $now_md5$(test "$now_md5" = "$map_md5" && echo ', unchanged' || echo ", CHANGED from $map_md5")" >> "$report"
  echo "- DefaultGame.ini md5 at the end: $now_ini$(test "$now_ini" = "$ini_md5" && echo ', unchanged' || echo ", CHANGED from $ini_md5")" >> "$report"
  echo "- tracked changes at the end: $(git status --porcelain --untracked-files=no | wc -l | tr -d ' '), clone on $(git rev-parse --short HEAD)" >> "$report"
  if test "$now_md5" != "$map_md5" || test "$now_ini" != "$ini_md5"; then test "$rc" -ne 0 || rc=3; say "WARNING: the clone is not as found"; fi
  say "$(test "$rc" -eq 0 && echo "done" || echo "stopped, exit $rc"); Mac libre. Report: $report"
  exit "$rc"
}
trap on_exit EXIT
trap 'exit 130' INT TERM

step() {   # step <name> <command...>: watchdog, heartbeat, one line in the report
  local name="$1"; shift
  local start limit status=0 next_beat now
  start=$(date +%s); limit=$(( 2 * $(expected "$name") )); next_beat=$(( start + heartbeat_s ))
  say "start: $name (stops after $(( limit / 60 )) min)"
  "$@" > "$out/$name.log" 2>&1 &
  current_pid=$!
  while kill -0 "$current_pid" 2> /dev/null; do
    sleep 2; now=$(date +%s)
    if test $(( now - start )) -gt "$limit"; then
      kill_tree "$current_pid"; sleep 2; kill -9 "$current_pid" 2> /dev/null; current_pid=""
      stop_unreal
      echo "- $name: STOPPED by the watchdog after $(( (now - start) / 60 )) min" >> "$report"
      exit 3
    fi
    if test "$now" -ge "$next_beat"; then say "heartbeat: $name, $(( (now - start) / 60 )) min of at most $(( limit / 60 ))"; next_beat=$(( now + heartbeat_s )); fi
  done
  wait "$current_pid" || status=$?
  current_pid=""
  echo "- $name: $(test $status -eq 0 && echo OK || echo "FAILED ($status)"), $(( $(date +%s) - start )) s, log $name.log" >> "$report"
  if test "$status" -ne 0; then say "$name failed ($status), see $out/$name.log"; exit 1; fi
}

json() { python3 -c 'import json,sys; v=json.load(open(sys.argv[1]))[sys.argv[2]]; print(" ".join(v) if isinstance(v, list) else v)' "$out/trees_pick.json" "$1"; }
add_cook_dirs() {
  test -f "$out/DefaultGame.ini.orig" || cp "$game_ini" "$out/DefaultGame.ini.orig"
  python3 - "$game_ini" $(json cook_dirs) <<'PY'
import sys
path, dirs = sys.argv[1], sys.argv[2:]
s = open(path).read()
key = "[/Script/UnrealEd.ProjectPackagingSettings]\n"
assert key in s, "no packaging section in DefaultGame.ini"
open(path, "w").write(s.replace(key, key + "".join('+DirectoriesToAlwaysCook=(Path="%s")\n' % d for d in dirs), 1))
PY
}
prepare() { run_py prepare tools/assets/unreal/prepare_trees_m1.py "$saved/trees_m1_report.json" && grep -q '"errors": \[\]' "$saved/trees_m1_report.json"; }
pick() {
  GTP_TREE_MODE=pick GTP_TREE_PATTERN="${GTP_TREE_PATTERN:-/M1/}" run_py pick tools/city/unreal/swap_street_trees.py "$saved/trees_report.json" || return 1
  cp "$saved/trees_report.json" "$out/trees_pick.json"
  grep -q '"errors": \[\]' "$out/trees_pick.json"
}

# Interior levels: the files build_interior_level.py writes for spec <id> (relative to Content).
interior_files() { echo "Interiors/L_Interior_$1.umap"; echo "__ExternalActors__/Interiors/L_Interior_$1"; echo "__ExternalObjects__/Interiors/L_Interior_$1"; }
interior_ids() { local f; for f in "$repo"/tools/interiors/*.json; do test "$(basename "$f")" = catalog.json || basename "$f" .json; done; }
build_interiors() {
  local id f
  local mesh_map="${GTP_INTERIOR_MESHES:-$repo/GranTheftPraline/Saved/InteriorBuild/mesh_map.json}"
  if test -f "$mesh_map"; then export GTP_INTERIOR_MESHES="$mesh_map"; echo "interiors dressed with $mesh_map"
  else unset GTP_INTERIOR_MESHES; echo "interiors: no mesh map ($mesh_map), grey boxes"; fi
  for id in $(interior_ids); do
    GTP_INTERIOR_SPEC="$repo/tools/interiors/$id.json" GTP_INTERIOR_ANCHOR=1 \
      run_py interiors tools/interiors/unreal/build_interior_level.py "$repo/GranTheftPraline/Saved/InteriorBuild/$id.json" || return 1
    test "$dry" = "1" || cp "$repo/GranTheftPraline/Saved/InteriorBuild/$id.json" "$out/interior-$id.json"
    grep -q '"errors": \[\]' "$out/interior-$id.json" 2> /dev/null || test "$dry" = "1" || { echo "interior $id: errors in its report"; return 1; }
    test -f "$content/Interiors/L_Interior_$id.umap" || { echo "interior $id: L_Interior_$id.umap missing after the build"; return 1; }
  done
}

# --- the steps ----------------------------------------------------------------------------------
test "${SKIP_COMPILE:-0}" = "1" || step compile compile
if test "$details" = "1"; then
  mkdir -p "$out/snapshot/Environment"
  for d in $snap_dirs; do
    if test -e "$content/$d"; then clone_copy "$(cd -P "$content/$d" && pwd)" "$out/snapshot/$d"; fi
  done
  snapshotted=1
  step setup_env setup_env
  step details regen
  cp "$saved/details_report.json" "$out/" 2> /dev/null || true
fi
play_args=""
if test "$trees" = "1"; then
  step prepare prepare
  step pick pick
  play_args="-Trees=leafy -TreeSwap=$(json swap_arg)"
  add_cook_dirs
  echo "- leafy mesh folders cooked for this package only: $(json cook_dirs)" >> "$report"
fi
if test "$interiors" = "1"; then
  if test "$snapshotted" != "1"; then   # the anchor goes into the city map: keep the map as found
    mkdir -p "$out/snapshot"
    clone_copy "$(cd -P "$content/Maps" && pwd)" "$out/snapshot/Maps"
    snap_dirs="Maps"; snapshotted=1
  fi
  mkdir -p "$out/interiors-snapshot"; : > "$out/interiors.created"
  for id in $(interior_ids); do
    for f in $(interior_files "$id"); do
      echo "$f" >> "$out/interiors.created"
      if test -e "$content/$f"; then mkdir -p "$(dirname "$out/interiors-snapshot/$f")"; clone_copy "$content/$f" "$out/interiors-snapshot/$f"; fi
    done
  done
  step interiors build_interiors
  # DirectoriesToAlwaysCook skips maps: a level opened only by name (LoadLevelInstance) is cooked
  # when it is listed in MapsToCook. Added for this package only, the ini is put back afterwards.
  test -f "$out/DefaultGame.ini.orig" || cp "$game_ini" "$out/DefaultGame.ini.orig"
  python3 - "$game_ini" $(for id in $(interior_ids); do echo "/Game/Interiors/L_Interior_$id"; done) <<'PY'
import sys
path, maps = sys.argv[1], sys.argv[2:]
s = open(path).read()
key = "[/Script/UnrealEd.ProjectPackagingSettings]\n"
assert key in s, "no packaging section in DefaultGame.ini"
open(path, "w").write(s.replace(key, key + "".join('+MapsToCook=(FilePath="%s")\n' % m for m in maps), 1))
PY
  echo "- interior maps cooked for this package only: $(interior_ids | sed 's#^#/Game/Interiors/L_Interior_#' | tr '\n' ' ')" >> "$report"
  echo "- interiors built into the package: $(interior_ids | tr '\n' ' ')(check: game log 'InteriorStreaming: load' then 'visible' with -Interiors=1)" >> "$report"
fi
step package package_cmd
put_back   # Content and the ini as found before anything else runs
grep -q "commit=$(git rev-parse --short HEAD)" "$build_dir/build-info.txt" 2> /dev/null \
  || { echo "- package: build-info.txt does not carry commit $(git rev-parse --short HEAD)" >> "$report"; exit 1; }
test "$trees" = "1" && cp "$out/trees_pick.json" "$build_dir/trees_pick.json"
python3 - "$build_dir/candidate.json" "$(git rev-parse --short HEAD)" "$details" "$trees" "$play_args" "$interiors" <<'PY'
import json, sys
json.dump({"commit": sys.argv[2], "details": sys.argv[3] == "1", "trees": sys.argv[4] == "1", "play_args": sys.argv[5],
           "interiors": sys.argv[6] == "1",
           "check": "game log: 'city details mode N: X actors shown' with X > 0 (DETAILS=1) and "
                    "'street trees: variant leafy, N of M trees leafy' with N > 0 (TREES=1)"},
          open(sys.argv[1], "w"), indent=1)
PY
echo "- play arguments for the benches: ${play_args:-none}" >> "$report"
echo "- build: $build_dir (candidate.json next to the app)" >> "$report"
