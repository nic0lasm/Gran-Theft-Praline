#!/usr/bin/env bash
# Give the street trees their leaves back, measured: walk the street with the bare
# trees, swap them for a leafy tree, walk again, and put the bare trees back if the
# leafy run breaks the M1 budget. One Unreal process at a time; the city map must
# already be built (run_city_import.sh) and the tree pack imported from Fab.
#
# Usage: tools/city/unreal/run_tree_swap.sh
#   UE_ROOT (default /Users/Shared/Epic Games/UE_5.8), GTP_CITY_ZONE, GTP_CITY_FOCUS,
#   PROFILE (default m1), GTP_TREE_MESH, GTP_SAPLING_MESH, GTP_TREE_PATTERN
#   (see swap_street_trees.py), BUDGET_P95_MS (default 42, ADR-0002),
#   MAX_DELTA_MS (default 4: largest p95 increase accepted over the bare run).
#   DRY_RUN=1 only lists the tree meshes found and changes nothing.
# Results: GranTheftPraline/Saved/CityImport/trees/{bare,leafy}/ (play_report.json,
# shots), trees_report.json and verdict.txt.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
focus="${GTP_CITY_FOCUS:-rue-de-la-republique}"
map="/Game/Maps/L_City_$(echo "$focus" | awk -F- '{for(i=1;i<=NF;i++) printf "%s", toupper(substr($i,1,1)) substr($i,2)}')"
out="$repo/GranTheftPraline/Saved/CityImport"
budget="${BUDGET_P95_MS:-42}"
max_delta="${MAX_DELTA_MS:-4}"

test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }
if pgrep -f "UnrealEditor|GranTheftPraline.app" > /dev/null; then
  echo "another Unreal or game process is running; wait for the Mac to be free" >&2
  exit 1
fi

tmp="$(mktemp -d)"
cp "$repo"/tools/city/unreal/{swap_street_trees,shoot_city}.py "$tmp/"
export GTP_CITY_FOCUS="$focus" GTP_PROFILE="${PROFILE:-m1}"
cvars="$(python3 "$repo/tools/rendering/reference_scene/profiles.py" "${PROFILE:-m1}")"

swap() {
  GTP_TREE_MODE="$1" "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES \
    -ExecutePythonScript="$tmp/swap_street_trees.py"
  grep -q '"errors": \[\]' "$out/trees_report.json" || { cat "$out/trees_report.json"; exit 1; }
}

walk() {
  rm -rf "$out/shots" "$out/play_report.json"
  "$editor" "$project" "$map" -game -windowed -ResX=1920 -ResY=1080 -nosound -NoScreenMessages -unattended -NSAppSleepDisabled YES \
    "-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=True" \
    -ExecCmds="r.SetRes 1920x1080w, $cvars, py $tmp/shoot_city.py"
  test -f "$out/play_report.json" || { echo "walk wrote no play_report.json" >&2; exit 1; }
  rm -rf "$out/trees/$1"
  mkdir -p "$out/trees/$1"
  mv "$out/play_report.json" "$out/trees/$1/"
  test -d "$out/shots" && mv "$out/shots" "$out/trees/$1/"
}

p95() {
  python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["frame_ms_p95"])' "$out/trees/$1/play_report.json"
}

if test "${DRY_RUN:-0}" = "1"; then
  swap list
  cat "$out/trees_report.json"
  rm -rf "$tmp"
  exit 0
fi

walk bare
bare="$(p95 bare)"
echo "bare_p95_ms=$bare"
if python3 -c 'import sys; sys.exit(0 if float(sys.argv[1]) > float(sys.argv[2]) else 1)' "$bare" "$budget"; then
  echo "verdict=ABORT (bare map already above budget, trees not tried)" | tee "$out/trees/verdict.txt"
  rm -rf "$tmp"
  exit 2
fi
swap apply
cp "$out/trees_report.json" "$out/trees/trees_report.json"
walk leafy

leafy="$(p95 leafy)"
verdict="$(python3 -c 'import sys; b,l,bud,d=map(float,sys.argv[1:]); print("KEEP" if l<=bud and l-b<=d else "REVERT")' \
  "$bare" "$leafy" "$budget" "$max_delta")"
if test "$verdict" = "REVERT"; then
  swap revert
fi
{
  echo "verdict=$verdict"
  echo "bare_p95_ms=$bare"
  echo "leafy_p95_ms=$leafy"
  echo "budget_p95_ms=$budget"
  echo "max_delta_ms=$max_delta"
  echo "profile=${PROFILE:-m1}"
} | tee "$out/trees/verdict.txt"
rm -rf "$tmp"
