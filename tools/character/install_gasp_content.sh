#!/usr/bin/env bash
# Copy the Game Animation Sample character into GranTheftPraline/Content.
#
# 1. Runs gasp_dependency_closure.py headless in the sample project (read only)
#    to list the packages the character, its camera and its input need.
# 2. Copies exactly those files, same /Game paths, into our Content folder.
# The copied folders are Epic content: they are listed in .gitignore and never
# committed (see docs/gameplay/player-character.md).
#
# Usage: tools/character/install_gasp_content.sh [path/to/GameAnimationSample]
#   default sample path: ../Samples/GameAnimationSample next to the repository.
#   UE_ROOT can point to the engine folder (default: /Users/Shared/Epic Games/UE_5.8).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
sample="${1:-$repo/../Samples/GameAnimationSample}"
sample="$(cd "$sample" && pwd)"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
cmd="$ue_root/Engine/Binaries/Mac/UnrealEditor-Cmd"
dest="$repo/GranTheftPraline/Content"

test -x "$cmd" || { echo "UnrealEditor-Cmd not found: $cmd" >&2; exit 1; }
test -f "$sample/GameAnimationSample.uproject" || { echo "sample project not found in $sample" >&2; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
closure="$work/closure.json"

GTP_CLOSURE_OUT="$closure" "$cmd" "$sample/GameAnimationSample.uproject" -run=pythonscript \
  -script="$repo/tools/character/gasp_dependency_closure.py" \
  -unattended -nosplash -nullrhi -nop4 > "$work/closure.log" 2>&1 || {
  echo "closure run failed, log:" >&2; cat "$work/closure.log" >&2; exit 1; }
test -f "$closure" || { echo "no closure written, log:" >&2; cat "$work/closure.log" >&2; exit 1; }

python3 -I -c 'import json,sys; print("\n".join(json.load(open(sys.argv[1]))["files"]))' "$closure" > "$work/files.txt"
count="$(wc -l < "$work/files.txt" | tr -d ' ')"
echo "copying $count files from $sample/Content"
rsync -a --files-from="$work/files.txt" "$sample/Content/" "$dest/"

echo "top-level folders copied (all must be ignored by Git):"
cut -d/ -f1 "$work/files.txt" | sort -u
