#!/usr/bin/env bash
# Play L_RueRepublique in -game mode and run the in-game collision checks.
#
# Results: GranTheftPraline/Saved/PlayableStreet/playtest.json and screenshots
# (spawn.png, facade.png, bounds.png).
#
# Usage: tools/level/playtest_street.sh
#   UE_ROOT can point to the engine folder (default: /Users/Shared/Epic Games/UE_5.8).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
out="$repo/GranTheftPraline/Saved/PlayableStreet"

test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }
rm -f "$out/playtest.json"

# The project path has a space: Content/Python/gtp_run.py runs the script in
# place (no temp copy). -ExecutePythonScript would quit at once, so "py" is used.
export GTP_PY_SCRIPT="$repo/tools/level/playtest_street.py"

# bAllowHighDPIInGameMode: otherwise 1920x1080 becomes 1440x1080 on a Retina Mac.
"$editor" "$project" /Game/Maps/L_RueRepublique -game -windowed -ResX=1920 -ResY=1080 \
  -nosound -NoScreenMessages -unattended -SkipTitle \
  "-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=True" \
  -ExecCmds="r.SetRes 1920x1080w, py __import__('gtp_run').main()"

test -f "$out/playtest.json" || { echo "no playtest report written" >&2; exit 1; }
cat "$out/playtest.json"
grep -q '"result": "PASS"' "$out/playtest.json"
