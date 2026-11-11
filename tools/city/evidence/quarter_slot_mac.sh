#!/usr/bin/env bash
# First Mac measure of the streamed quarter (ADR-0010, option B): build the persistent level
# L_City_Quarter (bible lighting, terrain, one far proxy per 250 m cell) and the detailed
# streaming levels of the cells along the rue de la Republique, then play the demo route twice
# in the editor (-game): streaming on (UGTPCityStreaming loads the cells around the player)
# and proxies only (-CityCells=0). Frame times, the frames over 100 ms and the CityStreaming
# load / visible / unload lines go to QUARTER.md.
#
# Steps and expected times (a step over twice its time is stopped and the run ends with the cause):
#   compile   editor module with UGTPCityStreaming               10 min
#   cells     tools/city/cells.py, all 30 cells (needs numpy...)  10 min
#   proxies   tools/city/cell_proxies.py --check                   1 min
#   quarter   import_city + materials, GTP_CITY_LEVEL=quarter      5 min
#   per cell  import 5 min, facade kit and signs 10 min, materials 3 min
#   play on / play off   LAUNCH_S + 3 min each
# Default, 5 cells and LAUNCH_S=120: about 2 h 10 (guard limit about 4 h 20).
# Version 1 leaves out the street furniture and the details of the cells (props, details);
# the ground keeps the street map's material.
#
# Mac slot rules (Nico, 10/11): tried dry first (DRY_RUN=1, no Unreal), a time guard per step,
# a sign of life every 10 min (HEARTBEAT lines on stdout and in heartbeat.log), Paris times.
# The map and city content found at the start are restored at the end and checked by md5
# (KEEP=1 keeps the quarter for the optimisation bench instead).
#
# Usage, from the repository root: caffeinate -dimsu tools/city/evidence/quarter_slot_mac.sh
#   CELLS ("cell-p0000-p0000 cell-p0000-m0250 ..."; default the cells within 60 m of the street),
#   LAUNCH_S (default 120), PYTHON (venv with numpy, scipy, shapely, PIL), UE_ROOT,
#   BUILDS_DIR (default <repo>/Builds/quarter-slot), PROFILE (default m1), KEEP=1, DRY_RUN=1.
# Dry run outside the Mac: from a scratch copy of the repo, never commit what it writes.
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../.." && pwd)"
out="${BUILDS_DIR:-$repo/Builds/quarter-slot}"
dry="${DRY_RUN:-0}"
zone=presqu-ile
mkdir -p "$out"
report="$out/QUARTER.md"
paris() { TZ=Europe/Paris date +%H:%M; }
t0=$(date +%s)
echo "# Streamed quarter, started $(paris) Paris time, commit $(git -C "$repo" rev-parse --short HEAD)$(test "$dry" = 1 && echo ', DRY RUN')" > "$report"
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
rm -rf "$backup"; mkdir -p "$backup"
for k in "${kept[@]}"; do
  if test -e "$proj/$k"; then mkdir -p "$backup/$(dirname "$k")"; cp -Rp "$proj/$k" "$backup/$k"; fi
done
sums "$proj" > "$out/before.md5"
restore() {
  local rc=$?
  if test "${KEEP:-0}" = "1" && test "$rc" = 0; then
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
# Editor Python runs in place through Content/Python/gtp_run.py (no temporary copy).
ue_py() {  # ue_py <script under tools/city/unreal> <report json to check>
  rm -f "$saved/$2"
  GTP_PY_SCRIPT="$repo/tools/city/unreal/$1.py" "$editor" "$project" -unattended -nosplash -nosound \
    -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()" || true
  test -f "$saved/$2" || { echo "$1.py wrote no $2" >&2; return 1; }
  cat "$saved/$2"
  python3 -c "import json,sys; e=json.load(open(sys.argv[1]))['errors']; sys.exit(1 if e else 0)" "$saved/$2"
}
export -f ue_py
export repo editor project saved
play() {  # play <tag> <extra args>: demo route on L_City_Quarter, frame times to frames-<tag>.csv
  local tag="$1"; shift
  local cvars; cvars="$(python3 "$repo/tools/rendering/reference_scene/profiles.py" "${PROFILE:-m1}")"
  rm -f "$proj/Saved/Logs/GranTheftPraline.log"
  "$editor" "$project" /Game/Maps/L_City_Quarter -game -windowed -ResX=1728 -ResY=1117 -nosound -unattended \
    -NSAppSleepDisabled YES -SkipTitle -DemoRoute -TestVehicle -DemoDriveTime=$(( ${LAUNCH_S:-120} - 40 )) \
    "-FrameLogFile=$out/frames-$tag.csv" -FrameLogWarmup=10 "-ExitAfter=${LAUNCH_S:-120}" "$@" \
    -ExecCmds="r.SetRes 1728x1117w, $cvars" || true
  cp "$proj/Saved/Logs/GranTheftPraline.log" "$out/game-$tag.log" 2> /dev/null || true
  test -f "$out/frames-$tag.csv"
}
summary() {  # summary <tag>
  python3 - "$out/frames-$1.csv" "$out/game-$1.log" "$1" >> "$report" <<'PY' || echo "- $1: no frame times" >> "$report"
import csv, sys
rows = list(csv.DictReader(open(sys.argv[1])))
ms = sorted(float(r["frame_ms"]) for r in rows)
q = lambda p: ms[min(len(ms) - 1, int(p * len(ms)))]  # noqa: E731
hitches = [r for r in rows if float(r["frame_ms"]) > 100.0]
try:
    log = [l.strip() for l in open(sys.argv[2], errors="replace") if "CityStreaming" in l]
except OSError:
    log = []
print("- **%s**: %d frames, p50 %.1f ms, p95 %.1f ms, p99 %.1f ms, max %.1f ms, %.1f %% <= 33.3 ms, %d frames > 100 ms%s"
      % (sys.argv[3], len(ms), q(0.5), q(0.95), q(0.99), ms[-1], 100.0 * sum(m <= 33.3 for m in ms) / len(ms),
         len(hitches), (" at " + ", ".join("%.1f s" % float(r["time_s"]) for r in hitches[:8])) if hitches else ""))
for l in log[:40]:
    print("    " + l[-140:])
PY
}

heartbeat "start, cells $CELLS"
step compile 600 compile.log "$ue_root/Engine/Build/BatchFiles/Mac/Build.sh" GranTheftPralineEditor Mac Development \
  "-project=$project" -WaitMutex
step cells 600 cells.log python3 "$repo/tools/city/cells.py" --name "$zone" --jobs 6
step proxies 60 proxies.log python3 "$repo/tools/city/cell_proxies.py" --name "$zone" --check
"$repo/tools/city/unreal/mount_citybrands.sh" > /dev/null
step "quarter import" 300 quarter-import.log env GTP_CITY_ZONE="$zone" GTP_CITY_FOCUS=quarter GTP_CITY_LEVEL=quarter \
  bash -c 'ue_py import_city report.json'
step "quarter materials" 180 quarter-materials.log env GTP_CITY_ZONE="$zone" GTP_CITY_FOCUS=quarter GTP_CITY_LEVEL=quarter \
  bash -c 'ue_py apply_city_materials materials_report.json'
for c in $CELLS; do
  heartbeat "cell $c"
  cellenv=(GTP_CITY_ZONE="$zone" GTP_CITY_FOCUS="$c" GTP_CITY_LEVEL=cell GTP_KIT_FOCUS=rue-de-la-republique
           GTP_KIT_REUSE=1 GTP_SHOP_GLASS=opaque GTP_PROFILE="${PROFILE:-m1}")
  step "$c import" 300 "$c-import.log" env "${cellenv[@]}" bash -c 'ue_py import_city report.json'
  step "$c facades" 600 "$c-facades.log" env "${cellenv[@]}" bash -c 'ue_py place_facades facades_report.json'
  step "$c materials" 180 "$c-materials.log" env "${cellenv[@]}" bash -c 'ue_py apply_city_materials materials_report.json'
done
step "play streaming" $(( ${LAUNCH_S:-120} + 180 )) play-on.log play on -CityCells=1
summary on
step "play proxies only" $(( ${LAUNCH_S:-120} + 180 )) play-off.log play off -CityCells=0
summary off
echo "- $(paris) done in $(( ($(date +%s) - t0) / 60 )) min" >> "$report"
