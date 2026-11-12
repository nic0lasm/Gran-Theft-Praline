#!/usr/bin/env bash
# First Mac measure of the streamed quarter (ADR-0010, option B): build the persistent level
# L_City_Quarter (bible lighting, terrain, one far proxy per 250 m cell) and the detailed
# streaming levels of the cells along the rue de la Republique, package them (Development),
# then run the packaged demo route twice: streaming on (UGTPCityStreaming loads the cells
# around the player) and proxies only (-CityCells=0). Frame times, the frames over 100 ms and
# the CityStreaming load / visible / unload lines go to QUARTER.md.
#
# Steps and expected times (a step over twice its time is stopped and the run ends with the cause):
#   compile   editor module with UGTPCityStreaming               10 min
#   cells     tools/city/cells.py, all 30 cells (needs numpy...)  10 min
#   proxies   tools/city/cell_proxies.py --check                   1 min
#   quarter   import_city + materials, GTP_CITY_LEVEL=quarter      5 min
#   per cell  import 5 min, facade kit and signs 10 min, materials 3 min
#   package   Development, into <BUILDS_DIR>/quarter/             20 min
#   benches (5 by default)  LAUNCH_S + 2 min each
#   stills (STILLS, deliv and pct60 by default; STILLS=0 skips)  about 2 min each, after
#           every bench, never during one
# Modes: BUILD_ONLY=1 stops after the package (every editor step runs -nullrhi, so a locked
# Mac screen is fine): about 1 h 30 with 5 cells (65 min measured without furniture and details), guard limit about 3 h. SKIP_BUILD=1 runs the
# benches only, on the package a BUILD_ONLY run left in the same BUILDS_DIR, and stops at once
# when the screen is locked (CGSSessionScreenIsLocked); about 25 min with LAUNCH_S=120 (benches, then stills).
#   per cell (v2) furniture 2 min and street details 2 min (place_props.py, sets props and details)
# The benches (BENCHES) compare on one package: shippable cells (render defaults of
# UGTPCityRenderTuning), cells without those defaults, cells without furniture and details,
# proxies only. The ground keeps the street map's material.
#
# Mac slot rules (Nico, 10/11): tried dry first (DRY_RUN=1, no Unreal), a time guard per step,
# a sign of life every 10 min (HEARTBEAT lines on stdout and in heartbeat.log), Paris times.
# The map and city content found at the start are restored at the end and checked by md5
# (KEEP=1 keeps the quarter in the project instead; the package keeps it either way).
#
# Usage, from the repository root: caffeinate -dimsu tools/city/evidence/quarter_slot_mac.sh
#   CELLS ("cell-p0000-p0000 cell-p0000-m0250 ..."; default the cells within 60 m of the street),
#   LAUNCH_S (default 120), PYTHON (venv with numpy, scipy, shapely, PIL), UE_ROOT,
#   BUILDS_DIR (default <repo>/Builds/quarter-slot), PROFILE (default m1), KEEP=1, DRY_RUN=1,
#   BUILD_ONLY=1, SKIP_BUILD=1 (DRY_LOCKED=1 fakes a locked screen in a dry run).
# Dry run outside the Mac: from a scratch copy of the repo, never commit what it writes.
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../.." && pwd)"
out="${BUILDS_DIR:-$repo/Builds/quarter-slot}"
dry="${DRY_RUN:-0}"
zone=presqu-ile
mkdir -p "$out"
build_only="${BUILD_ONLY:-0}"
skip_build="${SKIP_BUILD:-0}"
report="$out/QUARTER$(test "$skip_build" = 1 && echo -bench || true).md"
paris() { TZ=Europe/Paris date +%H:%M; }
t0=$(date +%s)
screen_locked() {
  test "${DRY_LOCKED:-0}" = 1 && return 0
  ioreg -n Root -d1 -a 2> /dev/null | grep -A1 CGSSessionScreenIsLocked | grep -q true
}
echo "# Streamed quarter, started $(paris) Paris time, commit $(git -C "$repo" rev-parse --short HEAD)$(test "$dry" = 1 && echo ', DRY RUN')" > "$report"
if test "$skip_build" = 1 && screen_locked; then
  echo "- $(paris) screen locked (CGSSessionScreenIsLocked): no launch possible, Mac libre" | tee -a "$report"
  exit 4
fi
if test "$dry" != 1 && pgrep -f "UnrealEditor |UnrealEditor$|GranTheftPraline.app|GranTheftPraline-Mac|UnrealPak|AutomationTool" > /dev/null; then
  echo "an Unreal editor, a package or the game is running: one at a time" >&2
  exit 1
fi
export PATH="$(dirname "$(command -v "${PYTHON:-python3}")"):$PATH"   # the city tools call python3
python3 -c "import numpy, scipy, shapely, PIL" 2> /dev/null || { echo "python3 needs numpy, scipy, shapely and PIL (PYTHON=<venv>/bin/python)" >&2; exit 1; }
gen="$repo/assets/geo/processed/$zone/gen"
camel() { echo "$1" | awk -F- '{for(i=1;i<=NF;i++) printf "%s", toupper(substr($i,1,1)) substr($i,2)}'; }
if test -z "${CELLS:-}"; then
  CELLS="$(python3 "$repo/tools/city/cell_proxies.py" --name "$zone" --near-street rue-de-la-republique --within 60)"
fi
echo "- cells: $CELLS" >> "$report"

# --- save the map and city content, restore on exit ------------------------------------
proj="$repo/GranTheftPraline"
kept=(Content/Maps/L_City_RueDeLaRepublique.umap Content/Maps/L_City_Quarter.umap Content/Environment/City
      Content/Environment/Facades/LyonKit Content/City Plugins/CityBrands/Content)
for c in $CELLS; do kept+=("Content/Maps/L_City_$(camel "$c").umap"); done
backup="$out/backup"
if command -v md5 > /dev/null; then md5r=(md5 -r); else md5r=(md5sum); fi
sums() { (cd "$1" && for k in "${kept[@]}"; do if test -e "$k"; then find "$k" -type f -print0; fi; done \
          | sort -z | xargs -0 -r "${md5r[@]}") ; }
if test "$skip_build" != 1; then
  rm -rf "$backup"; mkdir -p "$backup"
  for k in "${kept[@]}"; do
    if test -e "$proj/$k"; then mkdir -p "$backup/$(dirname "$k")"; cp -Rp "$proj/$k" "$backup/$k"; fi
  done
  sums "$proj" > "$out/before.md5"
fi
game_ini="$proj/Config/DefaultGame.ini"
ini_back() {   # the cook list is only widened for the package: DefaultGame.ini goes back as found
  if test -f "$out/DefaultGame.ini.orig"; then
    cp "$out/DefaultGame.ini.orig" "$game_ini" && rm -f "$out/DefaultGame.ini.orig"
    echo "- $(paris) DefaultGame.ini restored" >> "$report"
  fi
}
restore() {
  local rc=$?
  ini_back
  if test "$skip_build" = 1; then
    echo "- $(paris) benches only: the project content was not touched" >> "$report"
  elif test "${KEEP:-0}" = "1" && test "$rc" = 0; then
    echo "- $(paris) KEEP=1: the quarter stays in the project (L_City_Quarter and its cells); backup in $backup" >> "$report"
  else
    for k in "${kept[@]}"; do
      rm -rf "${proj:?}/$k"   # also what did not exist at the start
      if test -e "$backup/$k"; then mkdir -p "$proj/$(dirname "$k")"; cp -Rp "$backup/$k" "$proj/$k"; fi
    done
    sums "$proj" > "$out/after.md5"
    if cmp -s "$out/before.md5" "$out/after.md5"; then
      echo "- $(paris) restore: map and city content identical to the start (md5, $(wc -l < "$out/before.md5") files)" >> "$report"
    else
      echo "- $(paris) restore: MISMATCH, see before.md5 / after.md5" >> "$report"
    fi
  fi
  git -C "$repo" checkout -- assets/geo/processed 2> /dev/null || true
  echo "== $(paris) Mac libre: $(( ($(date +%s) - t0) / 60 )) min, exit $rc, report $report"
  cat "$report"
}
trap restore EXIT

# --- one guarded step -------------------------------------------------------------------
heartbeat() { echo "HEARTBEAT $(paris) Paris: $1, $(( ($(date +%s) - t0) / 60 )) min since start" | tee -a "$out/heartbeat.log"; }
step() {  # step <name> <expected_s> <log> <command...>
  local name="$1" expected="$2" log="$3"; shift 3
  if test "$dry" = 1; then   # minutes become seconds, so the guard is tried in a few seconds
    expected=$(( expected / 60 > 2 ? expected / 60 : 2 ))
    if test "${DRY_SLOW:-}" = "$name"; then set -- sleep $(( expected * 3 ))
    elif test "$skip_build" = 1; then set -- true
    else set -- sh -c 'mkdir -p "$1/Content/Maps" && echo dry > "$1/Content/Maps/L_City_Quarter.umap"' sh "$proj"; fi
  fi
  echo "== $(paris) $name (expected $(( expected / 60 )) min, stopped at $(( 2 * expected / 60 )) min)"
  local s; s=$(date +%s)
  set -m; "$@" > "$out/$log" 2>&1 & local pid=$!; set +m
  local last_beat="$s"
  while kill -0 "$pid" 2> /dev/null; do
    sleep $(test "$dry" = 1 && echo 1 || echo 5)
    local now; now=$(date +%s)
    if (( now - s > 2 * expected )); then
      kill -TERM -- "-$pid" 2> /dev/null || kill -TERM "$pid" 2> /dev/null || true
      sleep 5; kill -KILL -- "-$pid" 2> /dev/null || true
      pkill -f "UnrealEditor |UnrealEditor$|GranTheftPraline.app|ShaderCompileWorker" 2> /dev/null || true
      echo "- $(paris) STOPPED: $name over twice its expected $(( expected / 60 )) min; last lines of $log:" >> "$report"
      tail -n 15 "$out/$log" | sed 's/^/    /' >> "$report"
      exit 3
    fi
    if (( now - last_beat >= 600 )); then heartbeat "$name running for $(( (now - s) / 60 )) min"; last_beat=$now; fi
  done
  local rc=0; wait "$pid" || rc=$?
  echo "- $(paris) $name: exit $rc in $(( ($(date +%s) - s) / 60 )) min ($log)" >> "$report"
  if test "$rc" != 0; then tail -n 15 "$out/$log" | sed 's/^/    /' >> "$report"; fi
  return "$rc"
}

ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
project="$proj/GranTheftPraline.uproject"
saved="$proj/Saved/CityImport"
# Editor Python runs in place through Content/Python/gtp_run.py (no temporary copy), headless
# (-nullrhi) through run_editor (tools/city/unreal/editor_run.sh: absolute log, done marker,
# SIGKILL once the report is written since the editor hangs on exit, time-stamped lines).
ue_py() {  # ue_py <script under tools/city/unreal> <report json to check> <limit s>
  rm -f "$saved/$2"
  # shellcheck source=../unreal/editor_run.sh
  . "$repo/tools/city/unreal/editor_run.sh"
  GTP_PY_SCRIPT="$repo/tools/city/unreal/$1.py" EDITOR_DONE_FILE="$saved/$2" EDITOR_MAX_S="${3:-600}" \
    run_editor "quarter_$1" "$editor" "$project" -nullrhi -unattended -nosplash -nosound \
    -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()" || true
  test -f "$saved/$2" || { echo "$1.py wrote no $2" >&2; return 1; }
  cat "$saved/$2"
  python3 -c "import json,sys; e=json.load(open(sys.argv[1]))['errors']; sys.exit(1 if e else 0)" "$saved/$2"
}
export -f ue_py
export repo editor project saved
bench() {  # bench <tag> <game args...>: the packaged demo route on L_City_Quarter
  local tag="$1"; shift
  local before; before="$(date +%s)"
  local glog=~/Library/Logs/GranTheftPraline/GranTheftPraline.log
  rm -f "$glog" 2> /dev/null || true
  local mark="$out/.bench-$tag"; touch "$mark"
  set -m
  BUILDS_DIR="$out" GAME_ARGS="/Game/Maps/L_City_Quarter -TestVehicle -DemoDriveTime=$(( ${LAUNCH_S:-120} - 40 )) $*" \
    "$repo/tools/build/test-build-mac.sh" quarter "${LAUNCH_S:-120}" 0 > "$out/test-$tag.log" 2>&1 &
  local tpid=$!
  set +m
  # Fail at once when the map does not open (not cooked, missing) instead of timing an empty world.
  while kill -0 "$tpid" 2> /dev/null; do
    sleep 2
    # the game's own log, and the stdout of this bench's runs only (newer than the mark)
    if grep -qs "Failed to enter\|SkipPackage: /Game/Maps/L_City_Quarter" "$glog" || \
       find "$out/quarter/evidence" -name stdout.log -newer "$mark" \
         -exec grep -qs "Failed to enter\|SkipPackage: /Game/Maps/L_City_Quarter" {} \; -print 2> /dev/null | grep -q .; then
      kill -TERM -- "-$tpid" 2> /dev/null || kill -TERM "$tpid" 2> /dev/null || true
      pkill -f "GranTheftPraline.app/Contents/MacOS" 2> /dev/null || true
      echo "- $(paris) bench $tag: L_City_Quarter did not open (Failed to enter / SkipPackage); stopped" >> "$report"
      { grep -hs "Failed to enter\|SkipPackage" "$glog"
        find "$out/quarter/evidence" -name stdout.log -newer "$mark" -exec grep -hs "Failed to enter\|SkipPackage" {} \; 2> /dev/null
      } | head -n 3 | sed 's/^/    /' >> "$report"
      return 2
    fi
  done
  wait "$tpid" || true
  local csv; csv="$(ls -t "$out"/quarter/evidence/*/frametimes.csv 2> /dev/null | head -n 1)"
  test -n "$csv" && test "$(_mtime_q "$csv")" -ge "$before" || { echo "no frame times for $tag" >&2; return 1; }
  cp "$csv" "$out/frames-$tag.csv"
  { cat ~/Library/Logs/GranTheftPraline/GranTheftPraline.log "$(dirname "$csv")/stdout.log"; } > "$out/game-$tag.log" 2> /dev/null || true
}
_mtime_q() { stat -c %Y "$1" 2> /dev/null || stat -f %m "$1"; }
summary() {  # summary <tag>
  python3 - "$out/frames-$1.csv" "$out/game-$1.log" "$1" >> "$report" <<'PY' || echo "- $1: no frame times" >> "$report"
import csv, sys
rows = list(csv.DictReader(open(sys.argv[1])))
ms = sorted(float(r["frame_ms"]) for r in rows)
q = lambda p: ms[min(len(ms) - 1, int(p * len(ms)))]  # noqa: E731
hitches = [r for r in rows if float(r["frame_ms"]) > 100.0]
try:
    log = [l.strip() for l in open(sys.argv[2], errors="replace") if "CityStreaming" in l or "CityRenderTuning" in l]
except OSError:
    log = []
print("- **%s**: %d frames, p50 %.1f ms, p95 %.1f ms, p99 %.1f ms, max %.1f ms, %.1f %% <= 33.3 ms, %d frames > 100 ms%s"
      % (sys.argv[3], len(ms), q(0.5), q(0.95), q(0.99), ms[-1], 100.0 * sum(m <= 33.3 for m in ms) / len(ms),
         len(hitches), (" at " + ", ".join("%.1f s" % float(r["time_s"]) for r in hitches[:8])) if hitches else ""))
gpu = [float(r["gpu_ms"]) for r in rows if r.get("gpu_ms") not in (None, "")]
if gpu:
    g = sorted(gpu)
    gq = lambda p: g[min(len(g) - 1, int(p * len(g)))]  # noqa: E731
    print("    GPU p50 %.2f / p95 %.2f / p99 %.2f ms" % (gq(0.5), gq(0.95), gq(0.99)))
    # Where the p95 comes from: GPU p95 per 10 s of the run (cell loads show up as windows)
    win = {}
    for r in rows:
        if r.get("gpu_ms") not in (None, ""):
            win.setdefault(int(float(r["time_s"]) // 10), []).append(float(r["gpu_ms"]))
    cells = []
    for k in sorted(win):
        w = sorted(win[k])
        cells.append("%d-%ds %.1f/%.1f" % (k * 10, k * 10 + 10, w[len(w) // 2], w[min(len(w) - 1, int(0.95 * len(w)))]))
    print("    GPU p50/p95 per 10 s: " + ", ".join(cells))
for l in log[:40]:
    print("    " + (l if "CityRenderTuning" in l else l[-140:]))
PY
}

heartbeat "start, cells $CELLS"
if test "$skip_build" != 1; then
  step compile 600 compile.log "$ue_root/Engine/Build/BatchFiles/Mac/Build.sh" GranTheftPralineEditor Mac Development \
    "-project=$project" -WaitMutex
  step cells 600 cells.log python3 "$repo/tools/city/cells.py" --name "$zone" --jobs 6
  step proxies 60 proxies.log python3 "$repo/tools/city/cell_proxies.py" --name "$zone" --check
  "$repo/tools/city/unreal/mount_citybrands.sh" > /dev/null
  step "quarter import" 300 quarter-import.log env GTP_CITY_ZONE="$zone" GTP_CITY_FOCUS=quarter GTP_CITY_LEVEL=quarter \
    bash -c 'ue_py import_city report.json 600'
  step "quarter materials" 180 quarter-materials.log env GTP_CITY_ZONE="$zone" GTP_CITY_FOCUS=quarter GTP_CITY_LEVEL=quarter \
    bash -c 'ue_py apply_city_materials materials_report.json 360'
  for c in $CELLS; do
    heartbeat "cell $c"
    cellenv=(GTP_CITY_ZONE="$zone" GTP_CITY_FOCUS="$c" GTP_CITY_LEVEL=cell GTP_KIT_FOCUS=rue-de-la-republique
             GTP_KIT_REUSE=1 GTP_SHOP_GLASS=opaque GTP_PROFILE="${PROFILE:-m1}")
    step "$c import" 300 "$c-import.log" env "${cellenv[@]}" bash -c 'ue_py import_city report.json 600'
    step "$c facades" 600 "$c-facades.log" env "${cellenv[@]}" bash -c 'ue_py place_facades facades_report.json 1200'
    step "$c materials" 180 "$c-materials.log" env "${cellenv[@]}" bash -c 'ue_py apply_city_materials materials_report.json 360'
    # surveyed street furniture and the street details of the cell (props.py / details.py --cell),
    # placed in the cell level; a table with no row is skipped
    for pset in props details; do
      table="$repo/assets/geo/processed/$zone/gen/$c/${pset}_placement.csv"
      if test "$(wc -l < "$table" 2> /dev/null || echo 0)" -gt 1; then
        step "$c $pset" 120 "$c-$pset.log" env "${cellenv[@]}" GTP_PROPS_SET="$pset" \
          bash -c "ue_py place_props ${pset}_report.json 300"
      fi
    done
  done
  # No cooked map references the quarter: list it and its cells in the cook for this package only.
  cook_maps="/Game/Maps/L_City_Quarter /Game/Maps/L_City_RueDeLaRepublique"
  for c in $CELLS; do cook_maps="$cook_maps /Game/Maps/L_City_$(camel "$c")"; done
  cp "$game_ini" "$out/DefaultGame.ini.orig"
  python3 - "$game_ini" $cook_maps <<'PY'
import sys
path, maps = sys.argv[1], sys.argv[2:]
s = open(path).read()
key = "[/Script/UnrealEd.ProjectPackagingSettings]\n"
assert key in s, "no packaging section in DefaultGame.ini"
open(path, "w").write(s.replace(key, key + "".join('+MapsToCook=(FilePath="%s")\n' % m for m in maps), 1))
PY
  echo "- maps cooked for this package only: $cook_maps" >> "$report"
  step package 1200 package.log env BUILDS_DIR="$out" "$repo/tools/build/package-mac.sh" quarter Development
  ini_back
  echo "- package: $out/quarter" >> "$report"
fi
if test "$build_only" = 1; then
  echo "- $(paris) BUILD_ONLY: built in $(( ($(date +%s) - t0) / 60 )) min; benches with SKIP_BUILD=1 when the screen is unlocked" >> "$report"
  exit 0
fi
if test "$dry" != 1 && ! test -d "$out/quarter"; then
  echo "- no package in $out/quarter (run BUILD_ONLY=1 first)" >> "$report"
  exit 1
fi
# A game launched right after the previous one closed can segfault at start (exit 139 at t = 0,
# no log; Mac 11/11, intermittent): 20 s between launches, and one retry after a failed launch.
launch() {  # launch <step args...>
  if test "$dry" != 1; then sleep 20; fi
  step "$@" && return 0
  echo "- $(paris) $1 failed: one retry in 20 s" >> "$report"
  if test "$dry" != 1; then sleep 20; fi
  step "$@"
}
# BENCHES: "<tag> <game args>;..." on the same package. Default: the shippable cells three times
# (run-to-run spread of the GPU p95, about 2 ms on 12/11), 60 % furniture and detail distances,
# proxies only. Each summary gives the GPU p95 per 10 s, to find what makes the p95.
benches="${BENCHES:-deliv -CityCells=1 -CityTuneReport=1;deliv2 -CityCells=1 -CityTuneReport=1;deliv3 -CityCells=1 -CityTuneReport=1;pct60 -CityCells=1 -CityCellDetailPct=60 -CityTuneReport=1;off -CityCells=0}"
IFS=';' read -r -a bench_list <<< "$benches"
for run in "${bench_list[@]}"; do
  set -- $run
  if screen_locked; then echo "- $(paris) screen locked before bench $1: skipped" >> "$report"; continue; fi
  launch "bench $1" $(( ${LAUNCH_S:-120} + 120 )) "bench-$1.log" bench "$@"
  summary "$1"
done
# --- stills, separate pass, never during a measure (project rule) -------------------------------
# STILLS: "<tag> <game args>;..." Same deterministic demo route for each, one still every 10 s
# from the 5th second, so stills-<tag>/frame_0000N.jpg show the same shot in every variant:
# what 60 % distances would cost in image (deliv against pct60), kept for Nico.
stills() {  # stills <tag> <game args...>
  local tag="$1"; shift
  local exe; exe="$(find "$out/quarter/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
  test -n "$exe" || { echo "no executable in $out/quarter" >&2; return 1; }
  mkdir -p "$out/stills-$tag"
  "$exe" /Game/Maps/L_City_Quarter -SkipTitle -TestVehicle -DemoRoute -DemoDeterministic=1 \
    -DemoDriveTime=$(( ${STILLS_S:-75} - 30 )) "$@" -CaptureVideo=$(( ${STILLS_S:-75} - 5 )) -CaptureFps=0.1 \
    -CaptureDelay=5 -CaptureDir="$out/stills-$tag" -ExitAfter=$(( ${STILLS_S:-75} + 10 )) || true
  ls "$out/stills-$tag"/*.jpg > /dev/null 2>&1
}
if test "${STILLS:-}" != "0"; then
  stills_list="${STILLS:-deliv -CityCells=1;pct60 -CityCells=1 -CityCellDetailPct=60}"
  IFS=';' read -r -a still_runs <<< "$stills_list"
  for run in "${still_runs[@]}"; do
    set -- $run
    if screen_locked; then echo "- $(paris) screen locked before stills $1: skipped" >> "$report"; continue; fi
    launch "stills $1" $(( ${STILLS_S:-75} + 60 )) "stills-$1.log" stills "$@"
    echo "- stills $1: $(ls "$out/stills-$1" 2> /dev/null | grep -c jpg || true) images in $out/stills-$1" >> "$report"
  done
fi
echo "- $(paris) done in $(( ($(date +%s) - t0) / 60 )) min" >> "$report"
