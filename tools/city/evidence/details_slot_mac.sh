#!/usr/bin/env bash
# One Mac slot to find what the street dressing costs on M1, unattended (about 35 minutes):
#   1. regenerate the map WITH the details and OPAQUE glass (GTP_SHOP_GLASS, default opaque), then
#      profile_details.sh: frame times with everything, without details, without furniture,
#      and without each heavy detail group;
#   2. regenerate WITHOUT details and with the INTERIOR-MAPPED glass (GTP_SLOT_GLASS, default interior),
#      with the street walk: against the opaque, detail-free release map, the gap is the glass alone.
# assets/geo/processed/<zone>/manifest.json is restored after each regeneration (build.py
# rewrites its furniture count). The map is left as in step 2; run regen_city.sh again with
# the chosen settings afterwards.
#
# Mac slot rules (project, 10/11): every step has a planned duration; a step that runs past
# twice that is stopped, Unreal is closed and the script exits with "MAC LIBRE" and the cause.
# A heartbeat line (Paris time, step, minutes) goes to stdout and heartbeat.log every
# HEARTBEAT_S seconds, for whoever runs the slot to relay. DRY_RUN=1 replays the whole slot
# off the Mac with stub steps (a few seconds each) and checks the inputs it would need.
#
# Usage: caffeinate -dimsu tools/city/evidence/details_slot_mac.sh
#   PYTHON (venv for the city tools), UE_ROOT, GTP_CITY_ZONE, GTP_CITY_FOCUS,
#   DRY_RUN=1, HEARTBEAT_S (default 600), PLAN_SCALE (multiplies the planned durations)
# Output: Saved/details-slot-<stamp>/ (details_profile.json, play_glass.json, logs, heartbeat.log).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../../.." && pwd)"
zone="${GTP_CITY_ZONE:-presqu-ile}"
dry="${DRY_RUN:-0}"
heartbeat_s="${HEARTBEAT_S:-600}"
plan_scale="${PLAN_SCALE:-1}"
manifest="$repo/assets/geo/processed/$zone/manifest.json"
saved="$repo/GranTheftPraline/Saved/CityImport"
out="$repo/GranTheftPraline/Saved/details-slot-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$out"

paris() { TZ=Europe/Paris date +%H:%M; }
say() { echo "[$(paris) Paris] $*" | tee -a "$out/heartbeat.log"; }

if [ "$dry" = "1" ]; then
  # Off-Mac rehearsal: same sequence, stub steps, inputs checked.
  for f in tools/city/regen_city.sh tools/city/unreal/profile_details.sh tools/city/unreal/profile_details.py \
           tools/city/details.py tools/city/unreal/place_props.py tools/city/unreal/run_city_import.sh; do
    test -f "$repo/$f" || { say "DRY RUN: missing $f"; exit 1; }
  done
  saved="$out/stub-saved"
  mkdir -p "$saved"
  test -f "$manifest" || { manifest="$out/stub-manifest.json"; echo '{}' > "$manifest"; }
fi
cp "$manifest" "$out/manifest.json.orig"
restore() { cp "$out/manifest.json.orig" "$manifest"; }

stop_unreal() {
  [ "$dry" = "1" ] && return 0
  pkill -TERM -f "UnrealEditor|GranTheftPraline.app" 2> /dev/null || true
  sleep 10
  pkill -KILL -f "UnrealEditor|GranTheftPraline.app" 2> /dev/null || true
}

kill_tree() {   # macOS has no setsid/timeout: walk the children by hand
  local p
  for p in $(pgrep -P "$1" 2> /dev/null); do kill_tree "$p"; done
  kill -TERM "$1" 2> /dev/null || true
}

current_pid=""
on_exit() {
  local rc=$?
  [ -n "$current_pid" ] && kill_tree "$current_pid"
  restore
  if [ "$rc" -ne 0 ]; then
    stop_unreal
    say "MAC LIBRE (slot stopped, exit $rc). Logs: $out"
  fi
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
}

if [ "$dry" = "1" ]; then
  # Stubs: a few seconds each, writing the files the real steps leave behind.
  regen() { sleep "${DRY_STEP_S:-3}"; echo '{"stub": true}' > "$saved/play_report.json"; echo '{}' > "$saved/details_report.json"; }
  profile() { sleep "${DRY_STEP_S:-3}"; echo '{"stub": true}' > "$saved/details_profile.json"; }
else
  regen() { "$repo/tools/city/regen_city.sh"; }
  profile() { "$repo/tools/city/unreal/profile_details.sh"; }
fi
export -f regen profile 2> /dev/null || true

say "details slot, $( [ "$dry" = "1" ] && echo "DRY RUN" || echo "Mac" ), output $out; planned about 35 min"
GTP_CITY_DETAILS=1 GTP_SHOP_GLASS="${GTP_SHOP_GLASS:-opaque}" SKIP_PLAY=1 \
  step "1/3 regenerate with details, ${GTP_SHOP_GLASS:-opaque} glass" 12 "$out/regen_details.log" regen
restore
step "2/3 profile the dressing" 6 "$out/profile.log" profile
cp "$saved/details_profile.json" "$out/"
cp "$saved/details_report.json" "$out/" 2> /dev/null || true
GTP_CITY_DETAILS=0 GTP_SHOP_GLASS="${GTP_SLOT_GLASS:-interior}" \
  step "3/3 regenerate without details, ${GTP_SLOT_GLASS:-interior} glass, street walk" 17 "$out/regen_glass.log" regen
restore
cp "$saved/play_report.json" "$out/play_glass.json"
say "MAC LIBRE (slot finished). Results: $out"
cat "$out/details_profile.json" "$out/play_glass.json"
