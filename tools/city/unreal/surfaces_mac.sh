#!/usr/bin/env bash
# Scanned street surfaces, on the Mac, headless (-nullrhi: fine with the screen locked), no map rebuilt:
#   1. import_surfaces.py   find the surfaces of tools/city/surfaces.json in the project, cap them
#                           at 2K, write Saved/CityImport/surfaces.json   (about 2 min, stopped at 8)
#   2. apply_city_materials.py  rebuild the city materials with the surface layers (gtp_surfaces.py)
#                           and assign them, as in run_city_import.sh   (about 3 min, stopped at 8)
# From the root of the clone whose Content is packaged:
#   caffeinate -dimsu tools/city/unreal/surfaces_mac.sh
# SURFACES_ONLY_IMPORT=1 stops after step 1 (to read the report first). GTP_SURFACES=0 at step 2
# rebuilds the procedural materials (the way back). Other knobs: GTP_SURFACE_ROOTS, GTP_SURFACE_MAX,
# GTP_SURFACES_DIR (loose exports, one folder per id), GTP_SURFACES_LITE=1 (colour maps only).
# Results: GranTheftPraline/Saved/CityImport/{surfaces,materials_report}.json, logs Saved/Logs/City_*.log.
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
out="$repo/GranTheftPraline/Saved/CityImport"
test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }
# shellcheck source=editor_run.sh
. "$repo/tools/city/unreal/editor_run.sh"
export GTP_NULLRHI=1
launch=(-nullrhi -unattended -nosplash -nosound -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()")

rm -f "$out/surfaces.json"
GTP_PY_SCRIPT="$repo/tools/city/unreal/import_surfaces.py" EDITOR_DONE_FILE="$out/surfaces.json" EDITOR_MAX_S=480 \
  run_editor surfaces "$editor" "$project" "${launch[@]}"
test -f "$out/surfaces.json" || { echo "no surfaces.json: see GranTheftPraline/Saved/Logs/City_surfaces.log" >&2; exit 1; }
python3 -c "import json,sys; r=json.load(open(sys.argv[1])); print('found', sorted(r['surfaces'])); print('missing', r['missing']); print('incomplete', r['incomplete']); print('re-saved', len(r['changed']), 'errors', r['errors'])" "$out/surfaces.json"
if [ "${SURFACES_ONLY_IMPORT:-0}" = "1" ]; then exit 0; fi

rm -f "$out/materials_report.json"
GTP_PY_SCRIPT="$repo/tools/city/unreal/apply_city_materials.py" EDITOR_DONE_FILE="$out/materials_report.json" EDITOR_MAX_S=480 \
  run_editor materials "$editor" "$project" "${launch[@]}"
test -f "$out/materials_report.json" || { echo "no materials_report.json" >&2; exit 1; }
python3 -c "import json,sys; r=json.load(open(sys.argv[1])); print('surfaces', r.get('surfaces')); print('errors', r['errors']); sys.exit(1 if r['errors'] else 0)" "$out/materials_report.json"
