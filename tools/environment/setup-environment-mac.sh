#!/usr/bin/env bash
# Prepare the day-night and weather content on the Mac (one Unreal process at a time):
#   1. night lights file from the city pipeline outputs (build_night_lights.py);
#   2. weather assets and wet street materials in the editor (unreal/setup_environment.py).
# Run after tools/city/unreal/run_city_import.sh: a regenerated material is patched again.
#
# Usage: tools/environment/setup-environment-mac.sh
#   UE_ROOT (default /Users/Shared/Epic Games/UE_5.8), GTP_CITY_ZONE (default presqu-ile),
#   GTP_CITY_FOCUS (default: every focus under assets/geo/processed/<zone>/gen),
#   GTP_ENV_WET=off to leave the street materials untouched.
# Result: GranTheftPraline/Content/Environment/{Data,Weather}/ (not in Git) and
#         GranTheftPraline/Saved/Environment/setup_report.json
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
zone="${GTP_CITY_ZONE:-presqu-ile}"
gen_root="$repo/assets/geo/processed/$zone/gen"

test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }
# Same test as the perf slots: the editor or the game, never the UnrealEditorServices helper
# (a "pgrep | grep -v" pipeline can match its own grep).
if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac" > /dev/null; then
  echo "an Unreal process is already running, wait for it to finish" >&2
  exit 1
fi

gens=()
if test -n "${GTP_CITY_FOCUS:-}"; then
  gens+=("$gen_root/$GTP_CITY_FOCUS")
else
  for d in "$gen_root"/*/; do
    test -f "$d/facade_modules.csv" && gens+=("${d%/}")
  done
fi
test "${#gens[@]}" -gt 0 || { echo "no city outputs under $gen_root" >&2; exit 1; }
python3 "$repo/tools/environment/build_night_lights.py" "${gens[@]}"

# The project path has a space and Unreal splits -ExecutePythonScript on
# spaces: Content/Python/gtp_run.py runs the script named by GTP_PY_SCRIPT, nothing is
# copied or written outside the project.
test -f "$repo/GranTheftPraline/Content/Python/gtp_run.py" || { echo "Content/Python/gtp_run.py missing (fix/py-launcher-no-tmp)" >&2; exit 1; }
GTP_PY_SCRIPT="$repo/tools/environment/unreal/setup_environment.py" \
  "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES \
  "-ExecutePythonScript=__import__('gtp_run').main()"

result="$repo/GranTheftPraline/Saved/Environment/setup_report.json"
test -f "$result" || { echo "setup_environment.py wrote no report" >&2; exit 1; }
cat "$result"
grep -q '"errors": \[\]' "$result"
