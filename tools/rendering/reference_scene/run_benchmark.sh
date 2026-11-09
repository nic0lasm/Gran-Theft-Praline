#!/usr/bin/env bash
# Measure the reference street scene on this Mac.
#
# For each lighting state: one warm-up run (shader compilation), then one
# measured run that plays the 60 s camera path with the CSV profiler.
# CSV files are copied to docs/rendering/test-results/<date>/csv/ (or
# csv-<profile>/) and summarised by summarize_csv.py into benchmark.md (or
# benchmark-<profile>.md).
#
# Usage: [PROFILE=fidelity|lumenlite|res50|a,b,...] tools/rendering/reference_scene/run_benchmark.sh [State ...]
#   PROFILE picks the console variables from profiles.json (default: fidelity,
#   the ADR-0002 fidelity mode). A comma-separated list measures several
#   profiles in one pass, alternating them state by state so that they share
#   the same machine conditions.
#   The system load (uptime) just before and just after the pass is written at
#   the top of each summary.
#   UE_ROOT can point to the engine folder (default: /Users/Shared/Epic Games/UE_5.8).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
IFS=, read -r -a profiles <<< "${PROFILE:-fidelity}"
states=("$@")
test "${#states[@]}" -gt 0 || states=(Noon GoldenHour Rain Night)
out="$repo/docs/rendering/test-results/$(date +%Y-%m-%d)"
saved="$repo/GranTheftPraline/Saved/Profiling/CSV"

test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }

suffix() { test "$1" = fidelity && echo "" || echo "-$1"; }
# 1920x1080 output, profile console variables from profiles.json.
cvars_of() {
  local c
  c="$(python3 "$repo/tools/rendering/reference_scene/profiles.py" "$1")" || return 1
  echo "r.VSync 0, t.MaxFPS 0, $c"
}
for profile in "${profiles[@]}"; do
  cvars_of "$profile" > /dev/null || exit 1
  mkdir -p "$out/csv$(suffix "$profile")"
  echo "== profile $profile: $(cvars_of "$profile")"
done
load_before="$(uptime)"

# -benchmark -fps=30: fixed 1/30 s time step, so 1800 frames cover exactly the
# 60 s camera path whatever the frame rate; frame and GPU times stay real.
# bAllowHighDPIInGameMode: without it the game window is limited to the Retina
# screen size in points (1728x1117 on a 16" MacBook Pro) and 1920x1080 is
# silently reduced to 1440x1080.
# "csvprofile exitoncompletion" quits the game once the capture is written
# (UE 5.8 has no -csvExitOnCompletion command-line switch).
run() {
  local state="$1" frames="$2" profile="$3" cvars
  cvars="$(cvars_of "$profile")"
  "$editor" "$project" "/Game/ReferenceScene/Maps/ReferenceStreet_$state" -game -windowed \
    -ResX=1920 -ResY=1080 -nosound -NoScreenMessages -unattended -benchmark -fps=30 \
    "-ini:Engine:[/Script/Engine.UserInterfaceSettings]:bAllowHighDPIInGameMode=True" \
    -ExecCmds="r.SetRes 1920x1080w, $cvars, csvprofile exitoncompletion, csvprofile frames=$frames" \
    -csvMetadata="State=$state,Profile=$profile"
}

for state in "${states[@]}"; do
  for profile in "${profiles[@]}"; do
    echo "== $state / $profile: warm-up (full path, compiles shaders and PSOs)"
    run "$state" 1800 "$profile"
    before="$(ls -1 "$saved" 2>/dev/null | wc -l)"
    echo "== $state / $profile: measured run"
    run "$state" 1800 "$profile"
    latest="$(ls -1t "$saved"/*.csv | head -n 1)"
    test "$(ls -1 "$saved" | wc -l)" -gt "$before" || { echo "no new CSV for $state / $profile" >&2; exit 1; }
    cp "$latest" "$out/csv$(suffix "$profile")/$state.csv"
  done
done
load_after="$(uptime)"

for profile in "${profiles[@]}"; do
  summary="$out/benchmark$(suffix "$profile").md"
  {
    echo "Profile: $profile ($(cvars_of "$profile"))"
    echo "Profiles in this pass: ${profiles[*]}"
    echo "Load before: $load_before"
    echo "Load after: $load_after"
    echo
    python3 "$repo/tools/rendering/reference_scene/summarize_csv.py" "$out/csv$(suffix "$profile")"
  } > "$summary"
  cat "$summary"
done
