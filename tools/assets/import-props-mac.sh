#!/usr/bin/env bash
# Import prepared props into the Unreal project, headless (commandlet, no editor GUI), on the Mac.
# Usage: tools/assets/import-props-mac.sh [wave] [ids...]   e.g. w1 W1-01 W1-07
# The Unreal editor must be CLOSED (one editor session at a time).
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
WAVE="${1:-w1}"
shift || true
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
STAGING="${GTP_STAGING:-$REPO/assets/staging/$WAVE}"

test -x "$CMD" || { echo "UnrealEditor-Cmd not found at $CMD (set UE_ROOT)" >&2; exit 1; }
test -d "$STAGING" || { echo "nothing prepared in $STAGING: run prepare_props.py first" >&2; exit 1; }
# -x: exact name, so the background UnrealEditorServices.app does not count
if pgrep -x "UnrealEditor" >/dev/null || pgrep -x "UnrealEditor-Cmd" >/dev/null; then
  echo "An Unreal editor is already running: close it first." >&2
  exit 1
fi

GTP_STAGING="$STAGING" GTP_WAVE="$WAVE" GTP_IDS="$*" \
  "$CMD" "$REPO/GranTheftPraline/GranTheftPraline.uproject" \
  -run=pythonscript -script="$REPO/tools/assets/unreal/import_props.py" \
  -unattended -nosplash -stdout -FullStdOutLogOutput
