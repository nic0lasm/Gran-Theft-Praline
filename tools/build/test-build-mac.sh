#!/usr/bin/env bash
# Soak test of a packaged build. Two runs, both straight into gameplay:
#   1. video run: the game films itself (-CaptureVideo), then the frames are
#      turned into a movie by frames-to-movie.swift (no screen-recording
#      permission needed);
#   2. soak run: frame times recorded for the whole duration, then the game
#      must quit cleanly with no crash report.
#
# Usage: tools/build/test-build-mac.sh [version] [duration_s] [video_s]
#   version     default: ProjectVersion from Config/DefaultGame.ini, as major.minor
#   duration_s  default 600 (10 minutes)
#   video_s     default 120; 0 skips the video run
#
# Environment:
#   GAME_ARGS   extra game arguments for every run, e.g. "-GfxProfile=ultra"
#               (the default profile is m1, see GTPGraphicsProfile.h)
#   DEMO_ROUTE  1 (default): the video and soak runs play the demo route
#               (look around, walk, get in the nearest parked car, drive, get
#               out; see GTPDemoRoute.h); 0: the player stands still. Add
#               -TestVehicle to GAME_ARGS on a map with no parked car.
#
# Output: Builds/<version>/evidence/<timestamp>/
#   title.jpg, capture.mov, frametimes.csv, frametimes.summary.txt, result.txt
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
project_version="$(sed -n 's/^ProjectVersion=//p' "$repo_root/GranTheftPraline/Config/DefaultGame.ini" | head -n 1 | tr -d '\r')"
version="${1:-$(printf '%s' "$project_version" | cut -d. -f1-2)}"
# Word splitting is wanted here: GAME_ARGS holds several arguments.
# shellcheck disable=SC2206
game_args=(${GAME_ARGS:-})
if test "${DEMO_ROUTE:-1}" = "1"; then
  game_args+=(-DemoRoute)
fi
duration="${2:-600}"
video="${3:-120}"
builds_dir="${BUILDS_DIR:-$repo_root/Builds}"
app_path="$builds_dir/$version/GranTheftPraline.app"

die() {
  echo "test-build: $*" >&2
  exit 1
}

test -d "$app_path" || die "no build at $app_path (run tools/build/package-mac.sh first)"
exe="$(find "$app_path/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
test -n "$exe" || die "no executable inside $app_path"

stamp="$(date +%Y%m%d-%H%M%S)"
out_dir="$builds_dir/$version/evidence/$stamp"
mkdir -p "$out_dir"
touch "$out_dir/.start"
csv="$out_dir/frametimes.csv"

echo "Running $(basename "$exe") for ${duration}s, evidence in $out_dir"

# Title screen still: launch without -SkipTitle and keep one frame.
title_status="failed"
set +e
"$exe" ${game_args[@]+"${game_args[@]}"} -CaptureVideo=1 -CaptureFps=1 -CaptureDelay=10 -CaptureDir="$out_dir/title" -ExitAfter=16 \
  > "$out_dir/title-run.log" 2>&1
set -e
if test -f "$out_dir/title/frame_00000.jpg"; then
  mv "$out_dir/title/frame_00000.jpg" "$out_dir/title.jpg"
  rm -rf "$out_dir/title"
  title_status="ok"
fi

video_status="skipped"
if test "$video" -gt 0; then
  fps=10
  echo "Video run: ${video}s at ${fps} fps"
  set +e
  "$exe" ${game_args[@]+"${game_args[@]}"} -SkipTitle -CaptureVideo="$video" -CaptureFps="$fps" -CaptureDelay=5 \
    -CaptureDir="$out_dir/frames" -ExitAfter=$(( video + 15 )) > "$out_dir/video-run.log" 2>&1
  set -e
  if swift "$script_dir/frames-to-movie.swift" "$out_dir/frames" "$fps" "$out_dir/capture.mov"; then
    video_status="ok"
    rm -rf "$out_dir/frames"
  else
    video_status="failed (frames kept in $out_dir/frames)"
  fi
fi

echo "Soak run: ${duration}s"
start="$(date +%s)"
set +e
"$exe" ${game_args[@]+"${game_args[@]}"} -SkipTitle -FrameLogFile="$csv" -ExitAfter="$duration" > "$out_dir/stdout.log" 2>&1
status=$?
set -e
elapsed=$(( $(date +%s) - start ))

crash_reports="$(find "$HOME/Library/Logs/DiagnosticReports" -newer "$out_dir/.start" -iname '*GranTheftPraline*' 2>/dev/null | wc -l | tr -d ' ')"

verdict="PASS"
test "$status" -eq 0 || verdict="FAIL"
test "$crash_reports" -eq 0 || verdict="FAIL"
test -s "$csv" || verdict="FAIL"
test "$elapsed" -ge "$duration" || verdict="FAIL"

{
  echo "verdict=$verdict"
  echo "exit_status=$status"
  echo "elapsed_s=$elapsed"
  echo "requested_s=$duration"
  echo "crash_reports=$crash_reports"
  echo "title=$title_status"
  echo "video=$video_status"
  echo "game_args=${game_args[*]+${game_args[*]}}"
  cat "$builds_dir/$version/build-info.txt" 2>/dev/null || true
} > "$out_dir/result.txt"

cat "$out_dir/result.txt"
test -f "${csv%.csv}.summary.txt" && cat "${csv%.csv}.summary.txt"
test "$verdict" = "PASS"
