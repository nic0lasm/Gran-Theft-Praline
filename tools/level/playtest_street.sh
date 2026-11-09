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

# The "py" console command takes the rest of the line as the path, so run a
# copy from a folder without spaces. -ExecutePythonScript would quit at once.
tmp="$(mktemp -d)"
cp "$repo/tools/level/playtest_street.py" "$tmp/playtest_street.py"

# bAllowHighDPIInGameMode: otherwise 1920x1080 becomes 1440x1080 on a Retina Mac.
"$editor" "$project" /Game/Maps/L_RueRepublique -game -windowed -ResX=1920 -ResY=1080 \
  -nosound -NoScreenMessages -unattended -SkipTitle \
  "-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=True" \
  -ExecCmds="r.SetRes 1920x1080w, py $tmp/playtest_street.py"
rm -rf "$tmp"

test -f "$out/playtest.json" || { echo "no playtest report written" >&2; exit 1; }
cat "$out/playtest.json"
grep -q '"result": "PASS"' "$out/playtest.json"
