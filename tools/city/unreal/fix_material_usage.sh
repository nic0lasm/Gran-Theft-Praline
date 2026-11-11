#!/usr/bin/env bash
# Re-save the city materials with the usage flags the cooked game needs (Nanite, instanced
# static meshes), headless (-nullrhi, fine with the Mac screen locked), without touching any map.
# Expected 3 min, stopped at 10 (EDITOR_MAX_S). From the repository root:
#   caffeinate -dimsu tools/city/unreal/fix_material_usage.sh
# GTP_USAGE_ROOTS (default "/Game /CityBrands"). Expect a few minutes more on the whole project.
# Result: GranTheftPraline/Saved/CityImport/usage_report.json; exit 1 when it lists errors.
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../.." && pwd)"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
report="$repo/GranTheftPraline/Saved/CityImport/usage_report.json"
test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }
# shellcheck source=editor_run.sh
. "$repo/tools/city/unreal/editor_run.sh"
"$repo/tools/city/unreal/mount_citybrands.sh" > /dev/null
rm -f "$report"
GTP_PY_SCRIPT="$repo/tools/city/unreal/fix_material_usage.py" EDITOR_DONE_FILE="$report" EDITOR_MAX_S=600 \
  run_editor usage "$editor" "$repo/GranTheftPraline/GranTheftPraline.uproject" -nullrhi -unattended -nosplash -nosound \
  -NSAppSleepDisabled YES "-ExecutePythonScript=__import__('gtp_run').main()"
test -f "$report" || { echo "no usage report" >&2; exit 1; }
python3 -c "import json,sys; r=json.load(open(sys.argv[1])); print('materials %d, fixed %d, already %d, errors %d' % (r['materials'], len(r['fixed']), r['already'], len(r['errors']))); sys.exit(1 if r['errors'] else 0)" "$report"
