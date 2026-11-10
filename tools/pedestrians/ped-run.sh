#!/usr/bin/env bash
# Far-crowd mesh, one command on the Mac (about 7 min planned, 15 min ceiling):
#   tools/pedestrians/ped-run.sh [--dry-run] [<uproject>] [<staging dir>]
# Steps, each under tools/common/guard.py (limit = 2x planned), nothing starts while an Unreal or
# the game runs, no map is opened:
#   1 Unreal  export_ped_source.py   planned 120 s   max 240
#   2 Blender+Python make_ped_far.sh planned  60 s   (guards inside)
#   3 Unreal  import_ped_far.py      planned 120 s   max 240
# Heartbeat: a "[guard] ... alive" line every 60 s in the log; on failure it exits non-zero with the cause.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
DRY=0; [ "${1:-}" = "--dry-run" ] && { DRY=1; shift; }
uproject="${1:-$repo/GranTheftPraline/GranTheftPraline.uproject}"
out="${2:-$repo/assets/staging/ped}"
ue="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
cmd="$ue/Engine/Binaries/Mac/UnrealEditor-Cmd"
G=(python3 "$repo/tools/common/guard.py")
run() { if [ "$DRY" = 1 ]; then printf '+'; printf ' %q' "$@"; echo; else "$@"; fi; }
for f in "$cmd" "$uproject" "$here/export_ped_source.py" "$here/make_ped_far.sh" "$here/import_ped_far.py" "$repo/tools/common/guard.py"; do
  test -e "$f" || { echo "missing: $f" >&2; exit 1; }
done
if [ "$DRY" = 0 ] && pgrep -f "UnrealEditor|GranTheftPraline-Mac|Blender" >/dev/null; then
  echo "an Unreal, the game or Blender is running: stop." >&2; exit 3
fi
run "${G[@]}" --name sleep-selftest --max 5 -- sleep 1
run "${G[@]}" --name ped-export --max 240 -- env GTP_PED_OUT="$out" "$cmd" "$uproject" -run=pythonscript -script="$here/export_ped_source.py" -unattended -nosplash -stdout -FullStdOutLogOutput
run "$here/make_ped_far.sh" "$out"
run "${G[@]}" --name ped-import --max 240 -- env GTP_PED_OUT="$out" "$cmd" "$uproject" -run=pythonscript -script="$here/import_ped_far.py" -unattended -nosplash -stdout -FullStdOutLogOutput
echo "[ped-run] ${DRY:+dry-run }done"
