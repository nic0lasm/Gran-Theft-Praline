#!/usr/bin/env bash
# Audio steps of a Mac pass (ADR-0009), unattended. Two stages:
#
#   tools/audio/mac_audio_pass.sh prepare
#       before packaging, editor CLOSED: generates the banks (make_sfx.py, make_voices_ui.py, make_weapons.py) and
#       the surface grid (surface_map.py), the ambience bed loops
#       (make_ambiences.py) and the zone grid (zone_map.py), imports them headless
#       into Content/Sound/SFX and Content/Sound/Ambience, and reports whether the Game Animation Sample
#       already plays footsteps (Saved/gasp_foley.json).
#
#   tools/audio/mac_audio_pass.sh check [version]
#       after packaging (Builds/<version>/GranTheftPraline.app), no other game running:
#       1. filmed run with sound: demo route on foot then by car, a test siren
#          on the nearest AI car, foot calibration log; makes capture.mov
#          (frames-to-movie.swift) and capture-with-sound.mov (mux-audio.swift);
#       2. A/B soak: the same demo route with -Audio=0 and with audio;
#       3. analyze_audio_run.py: step rate, foot calibration, counters,
#          game thread cost against the 0.3 ms budget -> audio-report.md.
#
# Environment: UE_ROOT (default /Users/Shared/Epic Games/UE_5.8), PYTHON (needs
# numpy and pillow; default: the repo venv .scratch/venv, made if missing),
# GROUND_DIR (gen/<focus>/ground of the city pipeline), WATER_GEOJSON (BD TOPO water), GAME_ARGS (extra game
# arguments, e.g. "-PoliceMax=2"), AB_SECONDS (default 120), VIDEO_SECONDS (default 60).
# Output of check: Builds/<version>/evidence/audio-<timestamp>/
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
project="$repo/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
cmd="$ue_root/Engine/Binaries/Mac/UnrealEditor-Cmd"
stage="$repo/.scratch/audio"
focus="rue-de-la-republique"

die() {
  echo "mac_audio_pass: $*" >&2
  exit 1
}

python_with_numpy() {
  local py="${PYTHON:-$repo/.scratch/venv/bin/python3}"
  if ! test -x "$py"; then
    python3 -m venv "$repo/.scratch/venv"
    py="$repo/.scratch/venv/bin/python3"
  fi
  if ! "$py" -c "import numpy, PIL" 2>/dev/null; then
    "$py" -m pip install --quiet numpy pillow
  fi
  echo "$py"
}

editor_free() {
  if pgrep -x "UnrealEditor" >/dev/null || pgrep -x "UnrealEditor-Cmd" >/dev/null || pgrep -f "GranTheftPraline.app/Contents/MacOS" >/dev/null; then
    die "an Unreal editor or the game is running: one at a time"
  fi
}

run_commandlet() {
  # Paths with spaces cannot go through -script: copy the script to a plain folder.
  local script="$1"
  local tmp
  tmp="$(mktemp -d /tmp/gtp-audio.XXXXXX)"
  cp "$script" "$tmp/"
  "$cmd" "$project" -run=pythonscript -script="$tmp/$(basename "$script")" \
    -unattended -nosplash -nosound -stdout -FullStdOutLogOutput -NSAppSleepDisabled YES
  rm -rf "$tmp"
}

prepare() {
  test -x "$cmd" || die "UnrealEditor-Cmd not found at $cmd (set UE_ROOT)"
  editor_free
  local py
  py="$(python_with_numpy)"

  echo "== banks"
  rm -rf "$stage/sfx"
  "$py" -I "$repo/tools/audio/make_sfx.py" "$stage/sfx"
  "$py" -I "$repo/tools/audio/make_voices_ui.py" "$stage/sfx"
  "$py" -I "$repo/tools/audio/make_weapons.py" "$stage/sfx"

  echo "== surface grid"
  local ground="${GROUND_DIR:-$repo/assets/geo/processed/presqu-ile/gen/$focus/ground}"
  if ! test -f "$ground/T_GroundMask.png"; then
    echo "no ground mask in $ground: generating it (tools/city/ground_mask.py)"
    "$py" "$repo/tools/city/ground_mask.py" --focus "$focus"
  fi
  "$py" -I "$repo/tools/audio/surface_map.py" --ground "$ground" --focus "$focus"

  echo "== ambience bed loops and zone grid"
  rm -rf "$stage/ambience"
  "$py" -I "$repo/tools/audio/make_ambiences.py" "$stage/ambience"
  local water="${WATER_GEOJSON:-$repo/assets/geo/processed/presqu-ile/water.geojson}"
  if test -f "$water"; then
    "$py" -I "$repo/tools/audio/zone_map.py" --ground "$ground" --water "$water" --focus "$focus"
  else
    echo "no $water: no zone grid, the street loop plays everywhere"
  fi

  echo "== import into /Game/Sound/SFX and /Game/Sound/Ambience"
  GTP_AUDIO_SRC="$stage/sfx" run_commandlet "$repo/tools/audio/unreal/import_sounds.py" | tee "$stage/import.log"
  grep -q "\[Audio\] imported" "$stage/import.log" || die "import did not finish (see $stage/import.log)"
  GTP_AUDIO_SRC="$stage/ambience" GTP_AUDIO_ROOT="/Game/Sound/Ambience" run_commandlet "$repo/tools/audio/unreal/import_sounds.py" | tee "$stage/import-ambience.log"
  grep -q "\[Audio\] imported" "$stage/import-ambience.log" || die "ambience import did not finish (see $stage/import-ambience.log)"

  echo "== GASP foley"
  GTP_AUDIO_REPORT="$stage/gasp_foley.json" run_commandlet "$repo/tools/audio/unreal/report_gasp_foley.py" | tee "$stage/foley.log"
  grep "\[Audio\] GASP foley" "$stage/foley.log" || die "foley report missing (see $stage/foley.log)"
  echo "prepare: done. Banks, grid and report in $stage; package next."
}

check() {
  local version="${1:-$(sed -n 's/^ProjectVersion=//p' "$repo/GranTheftPraline/Config/DefaultGame.ini" | head -n 1 | tr -d '\r' | cut -d. -f1-2)}"
  local app="$repo/Builds/$version/GranTheftPraline.app"
  test -d "$app" || die "no build at $app"
  local exe
  exe="$(find "$app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
  test -n "$exe" || die "no executable in $app"
  editor_free
  local py
  py="$(python_with_numpy)"
  local out="$repo/Builds/$version/evidence/audio-$(date +%Y%m%d-%H%M%S)"
  mkdir -p "$out"
  local video="${VIDEO_SECONDS:-60}"
  local ab="${AB_SECONDS:-120}"
  # shellcheck disable=SC2206
  local extra=(${GAME_ARGS:-})

  echo "== filmed run with sound (${video}s)"
  set +e
  "$exe" ${extra[@]+"${extra[@]}"} -SkipTitle -DemoRoute -TestSiren -DebugFeet -AudioStats \
    -CaptureVideo="$video" -CaptureFps=10 -CaptureDelay=5 -CaptureDir="$out/frames" \
    -RecordAudio -RecordAudioDir="$out" -ExitAfter=$(( video + 15 )) > "$out/video-run.log" 2>&1
  set -e
  if swift "$repo/tools/build/frames-to-movie.swift" "$out/frames" 10 "$out/capture.mov"; then
    rm -rf "$out/frames"
    if test -f "$out/game_audio.wav"; then
      swift "$repo/tools/audio/mux-audio.swift" "$out/capture.mov" "$out/game_audio.wav" "$out/capture-with-sound.mov" \
        || echo "mux failed: video and sound kept apart in $out"
    else
      echo "no game_audio.wav: see $out/video-run.log"
    fi
  else
    echo "frames-to-movie failed: frames kept in $out/frames"
  fi

  echo "== A/B soak, ${ab}s each"
  set +e
  "$exe" ${extra[@]+"${extra[@]}"} -SkipTitle -DemoRoute -Audio=0 -FrameLogFile="$out/frametimes-audio-off.csv" \
    -ExitAfter="$ab" > "$out/soak-audio-off.log" 2>&1
  "$exe" ${extra[@]+"${extra[@]}"} -SkipTitle -DemoRoute -AudioStats -FrameLogFile="$out/frametimes-audio-on.csv" \
    -ExitAfter="$ab" > "$out/soak-audio-on.log" 2>&1
  set -e

  echo "== report"
  "$py" -I "$repo/tools/audio/analyze_audio_run.py" "$out"
}

case "${1:-}" in
  prepare) prepare ;;
  check) shift; check "$@" ;;
  *) die "usage: mac_audio_pass.sh prepare | check [version]" ;;
esac
