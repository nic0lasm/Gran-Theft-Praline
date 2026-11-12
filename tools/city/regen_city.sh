#!/usr/bin/env bash
# Regenerate the whole street map in one command: geo data, generation, carriageways,
# props and details, Rhone riverside, Unreal import with facades, props, details and
# materials, the parked car, then the traffic lanes. One Unreal process at a time:
# check that no other editor or game runs before starting (the script refuses).
#
# Usage: caffeinate -dimsu tools/city/regen_city.sh
#   PYTHON (default python3; a venv with numpy, scipy, shapely, PIL), UE_ROOT,
#   GTP_CITY_ZONE (presqu-ile), GTP_CITY_FOCUS (rue-de-la-republique),
#   GTP_CITY_EXTRA_BBOX (250,-60,700,220: quai du Rhone and pont Lafayette),
#   EDITOR_HEADLESS (default -nullrhi; empty for a windowed editor),
#   GTP_SHOP_GLASS (translucent | opaque | interior), GTP_CITY_LANDMARKS=0 (generic shells for the Opera
#   and the Hotel de Ville), SKIP_PLAY=1 to skip the screenshot run.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
py="${PYTHON:-python3}"
zone="${GTP_CITY_ZONE:-presqu-ile}"
focus="${GTP_CITY_FOCUS:-rue-de-la-republique}"
focus_name="${GTP_CITY_FOCUS_NAME:-Rue de la République}"
extra="${GTP_CITY_EXTRA_BBOX:-250,-60,700,220}"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
map="/Game/Maps/L_City_$(echo "$focus" | awk -F- '{for(i=1;i<=NF;i++) printf "%s", toupper(substr($i,1,1)) substr($i,2)}')"
t0=$(date +%s)
step() { echo "== $(date +%H:%M:%S) ($(( $(date +%s) - t0 )) s): $*"; }
# Editor launches without a window or GPU: they pass with the screen locked (11/11).
headless="${EDITOR_HEADLESS--nullrhi}"
# shellcheck source=unreal/editor_run.sh
. "$repo/tools/city/unreal/editor_run.sh"

if ps -axo pid=,command= | grep -E "UnrealEditor |UnrealEditor$|GranTheftPraline.app" | grep -vE "CrashReportClient|UnrealEditorServices|grep -E" > /dev/null; then
  echo "an Unreal editor or the game is running: one at a time" >&2
  exit 1
fi

step "geo data (roads with street names, buildings)"
"$py" "$repo/tools/city/build.py" --name "$zone" > /dev/null
step "landmarks (Opera, Hotel de Ville: LiDAR volumes instead of the facade kit; GTP_CITY_LANDMARKS=0 to skip)"
if test "${GTP_CITY_LANDMARKS:-1}" != "0"; then
  "$py" "$repo/tools/city/landmarks.py" --name "$zone" > /dev/null
else
  rm -f "$repo/assets/geo/processed/$zone/landmarks.json" "$repo/assets/geo/processed/$zone/gen/$focus/landmarks."*
fi
step "generation (focus street + $extra)"
"$py" "$repo/tools/city/generate.py" --name "$zone" --focus "$focus_name" --extra-bbox "$extra" > /dev/null
step "backdrop (low-definition volumes within 600 m, tools/city/backdrop.py)"
"$py" "$repo/tools/city/backdrop.py" --name "$zone" --focus "$focus" > /dev/null
test "${GTP_CITY_LANDMARKS:-1}" = "0" || "$py" "$repo/tools/city/landmarks.py" --name "$zone" --focus "$focus" > /dev/null
test -f "$repo/assets/geo/processed/$zone/terrain/terrain.glb" || "$py" "$repo/tools/city/terrain.py" --name "$zone" > /dev/null
step "signs, carriageways fitted to the kerbs, street furniture"
"$py" "$repo/tools/city/signs.py" --name "$zone" --focus "$focus" > /dev/null
"$py" "$repo/tools/city/carriageway.py" --name "$zone"
"$py" "$repo/tools/city/props.py" --name "$zone" > /dev/null

step "Unreal: import, facades, props, details, materials (run_city_import.sh)"
PATH="$(dirname "$(command -v "$py")"):$PATH" GTP_CITY_RIVERSIDE=1 GTP_CITY_ZONE="$zone" GTP_CITY_FOCUS="$focus" \
  SKIP_PLAY="${SKIP_PLAY:-0}" "$repo/tools/city/unreal/run_city_import.sh"

step "parked car"
# place_parked_car.py ends with quit_editor(), which makes the editor return 1 even when the
# car is placed and the map saved (measured 10/11): 0 and 1 pass, anything else stops. It writes
# no report: run_editor knows it is done from the gtp_run.py marker (GTP_PY_DONE).
rc=0
GTP_PARKED_MAP="$map" GTP_PY_SCRIPT="$repo/tools/vehicles/unreal/place_parked_car.py" \
  EDITOR_MAX_S=120 run_editor parked_car "$editor" "$project" $headless -unattended -nosplash -nosound -NSAppSleepDisabled YES \
  "-ExecutePythonScript=__import__('gtp_run').main()" || rc=$?
test "$rc" -le 1 || { echo "parked car: editor returned $rc" >&2; exit "$rc"; }

if test -f "$repo/tools/city/traffic.py"; then
  step "traffic lanes"
  "$py" "$repo/tools/city/traffic.py" --zone "$zone" --focus "$focus" --map "$(basename "$map")"
fi
step "done"
