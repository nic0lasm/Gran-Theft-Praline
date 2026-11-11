#!/usr/bin/env bash
# CPU part of the far-crowd mesh (Blender + Python, ~30 s), between the two Unreal steps:
#   1. Unreal:  export_ped_source.py  (-> <dir>/SKM.fbx, Anim.fbx)
#   2. this script: make_ped_far.sh <dir>
#   3. Unreal:  import_ped_far.py      (-> /Game/Characters/Far/SM_PedFar_A)
# Every step runs under tools/common/guard.py (limit = 2x planned). BLENDER=path to Blender.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
G=(python3 "$here/../common/guard.py")
dir="${1:?folder holding SKM.fbx and Anim.fbx}"
BL="${BLENDER:-$(command -v blender || echo /Applications/Blender.app/Contents/MacOS/Blender)}"
"${G[@]}" --name ped-pose --max 240 -- nice -n 10 "$BL" --background --python-exit-code 1 --python "$here/bake_ped_pose.py" -- --dir "$dir" --stage export --phase "${GTP_PED_PHASE:-0.25}" --tris "${GTP_PED_TRIS:-2500}"
"${G[@]}" --name ped-decimate --max 120 -- nice -n 10 python3 "$here/../vehicles/decimate_parked.py" "$dir" --tris "${GTP_PED_TRIS:-2500}"
"${G[@]}" --name ped-finish --max 240 -- nice -n 10 "$BL" --background --python-exit-code 1 --python "$here/bake_ped_pose.py" -- --dir "$dir" --stage finish
