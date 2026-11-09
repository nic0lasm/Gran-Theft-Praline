#!/usr/bin/env bash
# Import one prepared car into the Unreal project, headless, and build its
# Blueprint (tools/vehicles/unreal/import_vehicle.py).
# Usage: tools/vehicles/import-vehicle-mac.sh [id]   default V-01 (Clio V)
# Input: assets/staging/vehicles/<id>/ from prepare_vehicle.py (out of Git).
# The Unreal editor must be CLOSED (one editor session at a time).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
id="${1:-V-01}"
ue="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
cmd="$ue/Engine/Binaries/Mac/UnrealEditor-Cmd"
staging="${GTP_VEHICLE_STAGING:-$repo/assets/staging/vehicles}"

test -x "$cmd" || { echo "UnrealEditor-Cmd not found at $cmd (set UE_ROOT)" >&2; exit 1; }
test -f "$staging/$id/report.json" || { echo "no prepared car in $staging/$id: run prepare_vehicle.py first" >&2; exit 1; }
if pgrep -x "UnrealEditor" >/dev/null || pgrep -x "UnrealEditor-Cmd" >/dev/null; then
  echo "An Unreal editor is already running: close it first." >&2
  exit 1
fi

log="$repo/GranTheftPraline/Saved/Vehicles/import-$id.log"
mkdir -p "$(dirname "$log")"
GTP_VEHICLE_ID="$id" GTP_VEHICLE_STAGING="$staging" \
  "$cmd" "$repo/GranTheftPraline/GranTheftPraline.uproject" \
  -run=pythonscript -script="$repo/tools/vehicles/unreal/import_vehicle.py" \
  -unattended -nosplash -stdout -FullStdOutLogOutput 2>&1 | tee "$log" | grep -E "\[import_vehicle\]|Error|Traceback" || true
grep -q "\[import_vehicle\] DONE" "$log" || { echo "import failed, see $log" >&2; exit 1; }
