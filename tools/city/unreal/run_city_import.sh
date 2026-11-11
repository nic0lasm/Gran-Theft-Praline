#!/usr/bin/env bash
# Build the generated city level in the Unreal Editor, place the facade kit, then
# play it for screenshots and frame times. One Unreal process at a time.
#
# Inputs (regenerated, not committed): assets/geo/processed/<zone>/gen/<focus>/ and terrain/,
# made by tools/city/generate.py, terrain.py and facade_kit/assemble.py.
# Results: GranTheftPraline/Saved/CityImport/{report,facades_report,play_report}.json and shots/.
#
# Usage: tools/city/unreal/run_city_import.sh
#   UE_ROOT (default /Users/Shared/Epic Games/UE_5.8), GTP_CITY_ZONE, GTP_CITY_FOCUS,
#   GTP_CITY_LIGHT (Noon | GoldenHour | Rain | Night, reference scene lighting states),
#   PROFILE (rendering profile of tools/rendering/reference_scene/profiles.json, default m1).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
zone="${GTP_CITY_ZONE:-presqu-ile}"
focus="${GTP_CITY_FOCUS:-rue-de-la-republique}"
gen="$repo/assets/geo/processed/$zone/gen/$focus"
out="$repo/GranTheftPraline/Saved/CityImport"
map="/Game/Maps/L_City_$(echo "$focus" | awk -F- '{for(i=1;i<=NF;i++) printf "%s", toupper(substr($i,1,1)) substr($i,2)}')"

test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }
for f in "$gen/signs/signs.csv" "$gen/props_placement.csv" "$gen/shell.glb" "$gen/buildings_gen.csv" "$gen/facade_modules.csv" "$gen/report.json" \
         "$gen/kit/facade_kit.glb" "$gen/kit/kit_manifest.json" \
         "$repo/assets/geo/processed/$zone/terrain/terrain.glb"; do
  test -f "$f" || { echo "missing input: $f" >&2; exit 1; }
done
rm -rf "$out"
mkdir -p "$out"

# Editor scripts run through Content/Python/gtp_run.py: the project path has a space, which
# Unreal splits on, and nothing is copied outside the project.
py_dir="$repo/tools/city/unreal"
python3 "$repo/tools/city/ground_mask.py" --zone "$zone" --focus "$focus" > /dev/null
# Facade and street details table (tools/city/details.py), replayed on every run.
# GTP_CITY_DETAILS=0 leaves them out (M1 budget fallback).
if test "${GTP_CITY_DETAILS:-1}" = "0"; then
  rm -f "$gen/details_placement.csv"
else
  python3 "$repo/tools/city/details.py" --name "$zone" --focus "$focus" > /dev/null
fi
# Real shop logos live in the out-of-Git content plugin Plugins/CityBrands (art bible 3.5);
# place_facades.py uses them only when the plugin is mounted, so make sure it exists.
"$repo/tools/city/unreal/mount_citybrands.sh"
# Rhone riverside (water, quay parapets, pont Lafayette) when GTP_CITY_RIVERSIDE=1; import_city.py
# picks riverside.json up when it exists.
if [ "${GTP_CITY_RIVERSIDE:-0}" = "1" ]; then
  python3 "$repo/tools/city/riverside.py" --name "$zone" --focus "$focus" --bridge 41:Lafayette > /dev/null
fi
export GTP_CITY_ZONE="$zone" GTP_CITY_FOCUS="$focus" GTP_CITY_LIGHT="${GTP_CITY_LIGHT:-Noon}" GTP_PROFILE="${PROFILE:-m1}"
cvars="$(python3 "$repo/tools/rendering/reference_scene/profiles.py" "${PROFILE:-m1}")"

# FACADES_ONLY=1: replay only the facade kit (place_facades.py) and the materials on the
# existing map, ~4 min instead of ~10 (bench of facade LODs).
if [ "${FACADES_ONLY:-0}" != "1" ]; then
GTP_PY_SCRIPT="$py_dir/import_city.py" "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()"
test -f "$out/report.json" || { echo "import_city.py wrote no report" >&2; exit 1; }
grep -q '"errors": \[\]' "$out/report.json" || { cat "$out/report.json"; exit 1; }
fi

GTP_PY_SCRIPT="$py_dir/place_facades.py" "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()"
test -f "$out/facades_report.json" || { echo "place_facades.py wrote no report" >&2; exit 1; }
grep -q '"errors": \[\]' "$out/facades_report.json" || { cat "$out/facades_report.json"; exit 1; }

# Street furniture: assets not imported yet are skipped and listed in the report.
if [ "${FACADES_ONLY:-0}" != "1" ]; then
GTP_PY_SCRIPT="$py_dir/place_props.py" "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()"
test -f "$out/props_report.json" || { echo "place_props.py wrote no report" >&2; exit 1; }
fi

# Facade and street details (tools/city/details.py), when the table exists: own folder City/Details.
if [ "${FACADES_ONLY:-0}" != "1" ] && test -f "$gen/details_placement.csv"; then
  if [ "${GTP_DETAILS_MERGED:-0}" != "0" ]; then
    # one baked material per multi-slot detail mesh (merge_detail_materials.py), used by place_props.py
    GTP_PY_SCRIPT="$py_dir/merge_detail_materials.py" "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()"
    test -f "$out/merge_details_report.json" || { echo "merge_detail_materials.py wrote no report" >&2; exit 1; }
  fi
  GTP_PROPS_SET=details GTP_PY_SCRIPT="$py_dir/place_props.py" "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()"
  test -f "$out/details_report.json" || { echo "place_props.py (details) wrote no report" >&2; exit 1; }
  if [ "${GTP_DETAILS_MERGED:-0}" = "ab" ]; then
    # Bench A/B: the details stay as they are and the baked copy is placed as a second set
    # (City/Details_merged, tag GTP_Set_details_merged), hidden in game unless -CityDetailsMerged=1.
    cp "$gen/details_placement.csv" "$gen/details_merged_placement.csv"
    GTP_PROPS_SET=details_merged GTP_DETAILS_MERGED=1 GTP_PY_SCRIPT="$py_dir/place_props.py" "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()"
    test -f "$out/details_merged_report.json" || { echo "place_props.py (details_merged) wrote no report" >&2; exit 1; }
  else
    rm -f "$gen/details_merged_placement.csv"
  fi
fi

# Art bible ground and shell materials in place of the greybox one.
GTP_PY_SCRIPT="$py_dir/apply_city_materials.py" "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()"
grep -q '"errors": \[\]' "$out/materials_report.json" || { cat "$out/materials_report.json"; exit 1; }

if [ "${SKIP_PLAY:-0}" != "1" ]; then
GTP_PY_SCRIPT="$py_dir/shoot_city.py" "$editor" "$project" "$map" -game -windowed -ResX=1920 -ResY=1080 -nosound -NoScreenMessages -unattended -NSAppSleepDisabled YES \
  "-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=True" \
  ${GTP_PLAY_ARGS:-} -ExecCmds="r.SetRes 1920x1080w, $cvars, py __import__('gtp_run').main()"
fi

for f in report facades_report props_report; do if test -f "$out/$f.json"; then cat "$out/$f.json"; fi; done
if test -f "$out/play_report.json"; then cat "$out/play_report.json"; fi
