#!/usr/bin/env bash
# Soak test of a packaged build: launches it straight into gameplay, records
# frame times for the whole run, optionally films the first minutes, and
# checks that it quits cleanly (no crash).
#
# Usage: tools/build/test-build-mac.sh [version] [duration_s] [video_s]
#   version     default 0.1
#   duration_s  default 600 (10 minutes)
#   video_s     default 120; 0 disables the screen recording
#               (screen recording needs the Screen Recording permission
#               for the app running this script)
#
# Output: Builds/<version>/evidence/<timestamp>/
#   frametimes.csv, frametimes.summary.txt, capture.mov, result.txt
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
version="${1:-0.1}"
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
csv="$out_dir/frametimes.csv"

echo "Running $(basename "$exe") for ${duration}s, evidence in $out_dir"

recorder_pid=""
if test "$video" -gt 0; then
  # Give the game time to open its window before filming.
  ( sleep 15 && screencapture -v -x -V "$video" "$out_dir/capture.mov" ) &
  recorder_pid=$!
fi

start="$(date +%s)"
set +e
"$exe" -SkipTitle -FrameLogFile="$csv" -ExitAfter="$duration" > "$out_dir/stdout.log" 2>&1
status=$?
set -e
elapsed=$(( $(date +%s) - start ))

if test -n "$recorder_pid"; then
  wait "$recorder_pid" || echo "screen recording failed (permission?)" >&2
fi

crash_reports="$(find "$HOME/Library/Logs/DiagnosticReports" -newer "$out_dir/stdout.log" -iname '*GranTheftPraline*' 2>/dev/null | wc -l | tr -d ' ')"

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
  cat "$builds_dir/$version/build-info.txt" 2>/dev/null || true
} > "$out_dir/result.txt"

cat "$out_dir/result.txt"
test -f "${csv%.csv}.summary.txt" && cat "${csv%.csv}.summary.txt"
test "$verdict" = "PASS"
