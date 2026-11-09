#!/usr/bin/env bash
# Play L_RueRepublique in -game mode with a test car parked in front of the
# player (-TestVehicle) and run the driving checks of playtest_drive.py.
#
# Results: GranTheftPraline/Saved/Vehicles/playtest_drive.json and screenshots
# (parked, seated, driving, exited, damaged).
#
# Usage: tools/vehicles/playtest_drive.sh
#   UE_ROOT can point to the engine folder (default: /Users/Shared/Epic Games/UE_5.8).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
out="$repo/GranTheftPraline/Saved/Vehicles"

test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }
rm -f "$out/playtest_drive.json"

# "py" takes the rest of the line as the path: run a copy from a folder without spaces.
tmp="$(mktemp -d)"
cp "$repo/tools/vehicles/playtest_drive.py" "$tmp/playtest_drive.py"

"$editor" "$project" /Game/Maps/L_RueRepublique -game -windowed -ResX=1920 -ResY=1080 \
  -nosound -NoScreenMessages -unattended -SkipTitle -TestVehicle \
  "-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=True" \
  -ExecCmds="r.SetRes 1920x1080w, py $tmp/playtest_drive.py"
rm -rf "$tmp"

test -f "$out/playtest_drive.json" || { echo "no playtest report written" >&2; exit 1; }
cat "$out/playtest_drive.json"
grep -q '"result": "PASS"' "$out/playtest_drive.json"
