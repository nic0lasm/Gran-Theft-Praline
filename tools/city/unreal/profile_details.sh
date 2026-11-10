#!/usr/bin/env bash
# GPU cost of the street dressing by set and by catalogue group, on the current city map,
# without rebuilding it (tools/city/unreal/profile_details.py, about 4 minutes).
# Usage: tools/city/unreal/profile_details.sh   (UE_ROOT, GTP_CITY_ZONE, GTP_CITY_FOCUS, PROFILE=m1,
#   GTP_PLAY_ARGS: extra game arguments, e.g. -Sky=lightfunction)
# Result: GranTheftPraline/Saved/CityImport/details_profile.json. One Unreal at a time; run it
# under caffeinate -dimsu with the Mac unlocked.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
focus="${GTP_CITY_FOCUS:-rue-de-la-republique}"
map="/Game/Maps/L_City_$(echo "$focus" | awk -F- '{for(i=1;i<=NF;i++) printf "%s", toupper(substr($i,1,1)) substr($i,2)}')"
test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }
if pgrep -fl "UnrealEditor|GranTheftPraline.app" | grep -v UnrealEditorServices > /dev/null; then
  echo "profile_details: an Unreal editor or the game is running; one at a time" >&2
  exit 1
fi
tmp="$(mktemp -d)"
cp "$repo/tools/city/unreal/profile_details.py" "$tmp/"
export GTP_CITY_ZONE="${GTP_CITY_ZONE:-presqu-ile}" GTP_CITY_FOCUS="$focus" GTP_PROFILE="${PROFILE:-m1}"
cvars="$(python3 "$repo/tools/rendering/reference_scene/profiles.py" "${PROFILE:-m1}")"
"$editor" "$project" "$map" -game -windowed -ResX=1920 -ResY=1080 -nosound -NoScreenMessages -unattended -NSAppSleepDisabled YES \
  "-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=True" \
  ${GTP_PLAY_ARGS:-} -ExecCmds="r.SetRes 1920x1080w, $cvars, py $tmp/profile_details.py"
rm -rf "$tmp"
cat "$repo/GranTheftPraline/Saved/CityImport/details_profile.json"
