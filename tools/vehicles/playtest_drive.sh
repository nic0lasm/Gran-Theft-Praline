#!/usr/bin/env bash
# Play L_RueRepublique in -game mode with a test car parked in front of the
# player (-TestVehicle) and run the driving checks of playtest_drive.py.
#
# Two runs:
#   1. checks + video: the game films itself (-CaptureVideo, no screen
#      recording permission), frames turned into drive.mov;
#   2. bench: one minute at the wheel with the PROFILE console variables
#      (default m1), frame times in frametimes.csv + .summary.txt. Skipped
#      with BENCH=0.
#
# Results in GranTheftPraline/Saved/Vehicles/: playtest_drive.json, screenshots
# (parked, seated, driving, exited, damaged), drive.mov, frametimes-<profile>.*
#
# Usage: [CAR=/Game/...BP_X.BP_X_C] [PROFILE=m1] [BENCH=0|1] tools/vehicles/playtest_drive.sh
#   CAR defaults to the Clio V; UE_ROOT can point to the engine folder.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
out="$repo/GranTheftPraline/Saved/Vehicles"
car="${CAR:-/Game/Vehicles/Cars/Renault_ClioV/BP_Renault_ClioV.BP_Renault_ClioV_C}"
profile="${PROFILE:-m1}"

test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }
mkdir -p "$out"
rm -rf "$out/playtest_drive.json" "$out/frames"

# "py" takes the rest of the line as the path: run a copy from a folder without spaces.
tmp="$(mktemp -d)"
cp "$repo/tools/vehicles/playtest_drive.py" "$tmp/playtest_drive.py"

play() {
  "$editor" "$project" /Game/Maps/L_RueRepublique -game -windowed -ResX=1920 -ResY=1080 \
    -nosound -NoScreenMessages -unattended -SkipTitle -TestVehicle "-TestVehicleClass=$car" \
    "-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=True" "$@"
}

GTP_DRIVE_MODE=checks play -CaptureVideo=60 -CaptureFps=15 -CaptureDelay=6 -CaptureDir="$out/frames" \
  -ExecCmds="r.SetRes 1920x1080w, py $tmp/playtest_drive.py"
if swift "$repo/tools/build/frames-to-movie.swift" "$out/frames" 15 "$out/drive.mov"; then
  rm -rf "$out/frames"
fi

if test "${BENCH:-1}" != 0; then
  cvars="$(python3 "$repo/tools/rendering/reference_scene/profiles.py" "$profile")"
  GTP_DRIVE_MODE=bench play -FrameLogFile="$out/frametimes-$profile.csv" -FrameLogWarmup=12 \
    -ExecCmds="r.SetRes 1920x1080w, r.VSync 0, t.MaxFPS 0, $cvars, py $tmp/playtest_drive.py"
  cat "$out/frametimes-$profile.summary.txt" 2>/dev/null || echo "no frame time summary written" >&2
fi
rm -rf "$tmp"

test -f "$out/playtest_drive.json" || { echo "no playtest report written" >&2; exit 1; }
cat "$out/playtest_drive.json"
grep -q '"result": "PASS"' "$out/playtest_drive.json"
