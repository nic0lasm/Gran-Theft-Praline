#!/usr/bin/env bash
# Mac slot 6 of the performance plan (ville v2), runnable by any session from the root of the
# GTAL-build clone, whatever branch it is on:
#   1. checks: no Unreal or game running, no tracked change, notes HEAD and the map md5;
#   2. checks out feat/city-perf (detached) and runs freeze_bisect_mac.sh dry, in a folder
#      whose name has a space (no Unreal, a few seconds);
#   3. runs it for real: A atlas off, B facades-only replay, C atlas on, D atlas on + merged
#      shells, each a Development package and a LAUNCH_S launch (time guard per step,
#      HEARTBEAT every 10 min, Paris times, map and city content restored by md5);
#   4. always (trap): puts back the branch or commit found at the start and checks the map md5.
# Expected 95 min with LAUNCH_S=90 (4 x (10 regen + 15 package + 3.5 launch)); a stuck step
# ends the run at twice its expected time.
#
# Run it from a copy under Builds/ (ignored by Git, untouched by the checkout), in one line:
#   mkdir -p Builds && git fetch -q origin feat/city-perf && git show origin/feat/city-perf:tools/city/evidence/city_slot6_mac.sh > Builds/city_slot6.sh && PYTHON="<venv>/bin/python" caffeinate -dimsu bash Builds/city_slot6.sh
#   STEPS (default ABCD), LAUNCH_S (default 90), REF (default origin/feat/city-perf),
#   DRY_ONLY=1 stops after the dry run.
#   BUILD_ONLY=1: regenerations and packages only (Mac screen locked is fine), builds kept in
#     "Builds/city slot6/freeze-<step>"; SKIP_BUILD=1 later: the launches only, on those builds,
#     stopping at once if the screen is locked. Expected: BUILD_ONLY ~85 min (4 x (10 + 11)),
#     SKIP_BUILD ~15 min with LAUNCH_S=90.
set -euo pipefail
root="$(pwd)"
test -f "$root/GranTheftPraline/GranTheftPraline.uproject" || { echo "run it from the root of GTAL-build" >&2; exit 1; }
paris() { TZ=Europe/Paris date +%H:%M; }
map="$root/GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
md5f() { if command -v md5 > /dev/null; then md5 -q "$1"; else md5sum "$1" | cut -d' ' -f1; fi; }
if pgrep -f "UnrealEditor |UnrealEditor$|GranTheftPraline.app|GranTheftPraline-Mac|UnrealPak|AutomationTool" > /dev/null; then
  echo "$(paris) an Unreal editor, a package or the game is running: one at a time" >&2
  exit 1
fi
if test "${SKIP_BUILD:-0}" = 1 && test "${DRY_LOCKED:-0}" != 1 && \
   ioreg -n Root -d1 -a 2> /dev/null | grep -A1 CGSSessionScreenIsLocked | grep -q true; then
  echo "$(paris) screen locked (CGSSessionScreenIsLocked): no launch possible, Mac libre" >&2
  exit 4
fi
test -z "$(git -C "$root" status --porcelain --untracked-files=no)" || { echo "tracked changes in GTAL-build: stop" >&2; exit 1; }
start_branch="$(git -C "$root" branch --show-current)"
start_head="$(git -C "$root" rev-parse HEAD)"
map_md5="$(test -f "$map" && md5f "$map" || echo none)"
echo "== $(paris) start: ${start_branch:-detached} $start_head, map md5 $map_md5"
back() {
  rc=$?
  git -C "$root" checkout -- assets/geo/processed 2> /dev/null || true
  git -C "$root" checkout -q "${start_branch:-$start_head}"
  now="$(test -f "$map" && md5f "$map" || echo none)"
  echo "== $(paris) GTAL-build back on ${start_branch:-$start_head} ($(git -C "$root" rev-parse --short HEAD)); map md5 $now ($(test "$now" = "$map_md5" && echo unchanged || echo CHANGED))"
  echo "== $(paris) Mac libre (exit $rc)"
}
trap back EXIT
git -C "$root" fetch -q origin feat/city-perf
git -C "$root" checkout -q --detach "${REF:-origin/feat/city-perf}"
mkdir -p "$root/Builds"
echo "== $(paris) on $(git -C "$root" log -1 --format='%h %s')"
BUILDS_DIR="$root/Builds/city slot6 dry" DRY_RUN=1 "$root/tools/city/evidence/freeze_bisect_mac.sh" "${STEPS:-ABCD}" \
  > "$root/Builds/city-slot6-dry.log" 2>&1
dry_ok="restore: map and city content identical"
test "${SKIP_BUILD:-0}" = 1 && dry_ok="launch D: exit 0"
grep -q "$dry_ok" "$root/Builds/city slot6 dry/FREEZE$(test "${SKIP_BUILD:-0}" = 1 && echo -bench || true).md" || {
  echo "dry run failed, see Builds/city-slot6-dry.log" >&2; exit 1; }
echo "== $(paris) dry run OK"
test "${DRY_ONLY:-0}" = "1" && exit 0
LAUNCH_S="${LAUNCH_S:-90}" BUILDS_DIR="$root/Builds/city slot6" \
  "$root/tools/city/evidence/freeze_bisect_mac.sh" "${STEPS:-ABCD}"
