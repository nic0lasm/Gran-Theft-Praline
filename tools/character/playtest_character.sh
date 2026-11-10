#!/usr/bin/env bash
# Play L_RueRepublique in -game mode with the player character and run
# playtest_character.py: run, walk, sprint, jump, crouch, first person toggle,
# respawn in first person. Frame timings are recorded with -FrameLog.
#
# Results in .scratch/character/ (or $1): playtest.json, screenshots,
# frametimes.csv (+ .summary.txt) and game.log.
#
# Usage: tools/character/playtest_character.sh [output folder]
#   UE_ROOT can point to the engine folder (default: /Users/Shared/Epic Games/UE_5.8).
#   GTP_CAPTURE=<seconds> also films the run (UGTPFrameCapture, frames in
#   <out>/capture; tools/build/frames-to-movie.swift makes the video). Do not
#   use the frame times of a filmed run: reading frames back costs GPU time.
#   GTP_PROFILE=<name> applies a rendering profile of
#   tools/rendering/reference_scene/profiles.json (m1, fidelity...).
#   GTP_EXTRA_ARGS adds raw command line arguments (for example
#   -ini:Game:[/Script/GranTheftPraline.GTPFollowCameraComponent]:bEnabled=False
#   to measure with the sample camera).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
out="${1:-$repo/.scratch/character}"
# The editor runs from the engine binaries folder: make the output absolute.
case "$out" in /*) ;; *) out="$PWD/$out" ;; esac

test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }
mkdir -p "$out"
rm -f "$out/playtest.json"

# The "py" console command takes the rest of the line as the path, and the
# command line splits on spaces: run from a folder without spaces.
capture=()
if test -n "${GTP_CAPTURE:-}"; then
  capture=(-CaptureVideo="$GTP_CAPTURE" -CaptureFps=30 -CaptureDelay=12 -CaptureDir="$out/capture")
fi

profile_cmds=""
if test -n "${GTP_PROFILE:-}"; then
  profile_cmds="$(python3 - "$repo/tools/rendering/reference_scene/profiles.json" "$GTP_PROFILE" <<'PY'
import json, sys
profiles = json.load(open(sys.argv[1]))
def cvars(name):
    p = profiles[name]
    out = cvars(p["base"]) if "base" in p else {}
    out.update(p["cvars"])
    return out
print("".join("%s %s, " % kv for kv in cvars(sys.argv[2]).items()))
PY
)"
fi
extra=()
if test -n "${GTP_EXTRA_ARGS:-}"; then
  read -r -a extra <<< "$GTP_EXTRA_ARGS"
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cp "$repo/tools/character/playtest_character.py" "$tmp/playtest_character.py"

GTP_PLAYTEST_OUT="$out" "$editor" "$project" /Game/Maps/L_RueRepublique -game -windowed -ResX=1920 -ResY=1080 \
  -nosound -NoScreenMessages -unattended -SkipTitle -FrameLog -FrameLogWarmup=10 \
  -FrameLogFile="$tmp/frametimes.csv" -abslog="$tmp/game.log" ${capture[@]+"${capture[@]}"} ${extra[@]+"${extra[@]}"} \
  "-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=True" \
  -ExecCmds="r.SetRes 1920x1080w, ${profile_cmds}py $tmp/playtest_character.py" || true

for f in frametimes.csv frametimes.summary.txt game.log; do
  test -f "$tmp/$f" && cp "$tmp/$f" "$out/$f"
done
# The recorder may ignore -FrameLogFile when -FrameLog is also given and write
# to Saved/Profiling: take the newest log from there in that case.
if test ! -f "$out/frametimes.csv"; then
  latest="$(ls -t "$repo/GranTheftPraline/Saved/Profiling"/frametimes-*.csv 2>/dev/null | head -n 1 || true)"
  if test -n "$latest"; then
    cp "$latest" "$out/frametimes.csv"
    test -f "${latest%.csv}.summary.txt" && cp "${latest%.csv}.summary.txt" "$out/frametimes.summary.txt"
  fi
fi
test -f "$out/playtest.json" || { echo "no playtest report written" >&2; exit 1; }
cat "$out/playtest.json"
grep -q '"result": "PASS"' "$out/playtest.json"
