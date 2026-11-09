#!/usr/bin/env bash
# Measure the reference street scene on this Mac (ADR-0002 fidelity mode).
#
# For each lighting state: one warm-up run (shader compilation), then one
# measured run that plays the 60 s camera path with the CSV profiler.
# CSV files are copied to docs/rendering/test-results/<date>/csv/ and
# summarised by summarize_csv.py.
#
# Usage: tools/rendering/reference_scene/run_benchmark.sh [State ...]
#   UE_ROOT can point to the engine folder (default: /Users/Shared/Epic Games/UE_5.8).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
states=("$@")
test "${#states[@]}" -gt 0 || states=(Noon GoldenHour Rain Night)
out="$repo/docs/rendering/test-results/$(date +%Y-%m-%d)"
saved="$repo/GranTheftPraline/Saved/Profiling/CSV"
mkdir -p "$out/csv"

test -x "$editor" || { echo "Unreal Editor not found: $editor" >&2; exit 1; }

# ADR-0002 fidelity mode: 1920x1080 output, TSR at 67 %, Lumen at High (medium on our scale).
cvars="r.VSync 0, t.MaxFPS 0, r.ScreenPercentage 67, sg.ViewDistanceQuality 3, sg.ShadowQuality 2,"
cvars="$cvars sg.GlobalIlluminationQuality 2, sg.ReflectionQuality 2, sg.PostProcessQuality 3,"
cvars="$cvars sg.TextureQuality 2, sg.EffectsQuality 2, sg.FoliageQuality 2"

run() {
  local state="$1" frames="$2"
  "$editor" "$project" "/Game/ReferenceScene/Maps/ReferenceStreet_$state" -game -windowed \
    -ResX=1920 -ResY=1080 -nosound -NoScreenMessages -unattended \
    -ExecCmds="$cvars, csvprofile frames=$frames" -csvExitOnCompletion -csvMetadata="State=$state"
}

for state in "${states[@]}"; do
  echo "== $state: warm-up"
  run "$state" 300
  before="$(ls -1 "$saved" 2>/dev/null | wc -l)"
  echo "== $state: measured run"
  run "$state" 1800
  latest="$(ls -1t "$saved"/*.csv | head -n 1)"
  test "$(ls -1 "$saved" | wc -l)" -gt "$before" || { echo "no new CSV for $state" >&2; exit 1; }
  cp "$latest" "$out/csv/$state.csv"
done

python3 "$repo/tools/rendering/reference_scene/summarize_csv.py" "$out/csv" > "$out/benchmark.md"
cat "$out/benchmark.md"
