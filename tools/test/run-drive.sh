#!/usr/bin/env bash
# Plays one named test drive in a packaged build and checks that it ran to the end.
#
# Usage: tools/test/run-drive.sh <build_dir> <drive> [rail|physics]
#   build_dir  folder holding GranTheftPraline.app (e.g. Builds/0.3)
#   drive      name of a file in GranTheftPraline/Content/Test/Drives (without .csv),
#              see tools/test/scenarios.json; the build must contain it (packaged
#              with the game), or set DRIVE_FILE to a CSV path without spaces
#   mode       rail (default: same camera path on every run, for measurements)
#              or physics (real driving, for gameplay tests)
#
# Environment:
#   GAME_ARGS   extra game arguments (split on spaces), e.g. "-Traffic=16 -Ambience=noon"
#   VIDEO       1: film the drive (10 fps) into capture.mov (never during a measurement)
#   OUT_DIR     default <build_dir>/drives/<drive>-<mode>-<timestamp>
#   DRY_RUN     1: print the command and checks, launch nothing
#
# Output: run.log (game stdout), result.txt (PASS/FAIL and why), positions.txt
# (the DemoPos lines: two runs compare with tools/perf/compare_demo_positions.py).
# Exit code 0 when the drive reached "TestDrive end" with no error line.
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
die() {
  echo "run-drive: $*" >&2
  exit 2
}
test $# -ge 2 || die "usage: run-drive.sh <build_dir> <drive> [rail|physics]"
build_dir="$1"
drive="$2"
mode="${3:-rail}"
case "$mode" in rail|physics) ;; *) die "mode must be rail or physics" ;; esac
app="$build_dir/GranTheftPraline.app"
csv="${DRIVE_FILE:-$repo_root/GranTheftPraline/Content/Test/Drives/$drive.csv}"
test -f "$csv" || die "no drive file $csv (tools/test/drive_route.py --scenario $drive --out ...)"
drive_arg="$drive"
if test -n "${DRIVE_FILE:-}"; then
  case "$DRIVE_FILE" in *" "*) die "DRIVE_FILE must not contain spaces (the game splits its arguments on them)" ;; esac
  drive_arg="$DRIVE_FILE"
fi
# Drive length (last t_s of the file); the guard allows load + entry + 3 s + drive + margin.
length="$(tail -n 1 "$csv" | cut -d, -f1 | cut -d. -f1)"
test -n "$length" || die "cannot read the drive length from $csv"
if test "$mode" = "physics"; then
  limit=$(( length * 3 + 120 ))
else
  limit=$(( length + 90 ))
fi
stamp="$(date +%Y%m%d-%H%M%S)"
out_dir="${OUT_DIR:-$build_dir/drives/$drive-$mode-$stamp}"
# shellcheck disable=SC2206
extra=(${GAME_ARGS:-})
args=(-SkipTitle -TestVehicle "-TestDrive=$drive_arg" "-TestDriveMode=$mode" -TestDriveQuit -DemoDeterministic=1 "-ExitAfter=$limit")
if test "${VIDEO:-0}" = "1"; then
  args+=(-CaptureVideo="$limit" -CaptureFps=10 -CaptureDelay=5 "-CaptureDir=$out_dir/frames")
fi
echo "run-drive: $drive ($mode), ${length} s of driving, guard ${limit} s, out $out_dir"
if test "${DRY_RUN:-0}" = "1"; then
  echo "run-drive: dry run: <exe> ${args[*]} ${extra[*]+${extra[*]}}"
  test -d "$app" || echo "run-drive: dry run: note: no app at $app"
  exit 0
fi
test -d "$app" || die "no build at $app"
exe="$(find "$app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
test -n "$exe" || die "no executable inside $app"
if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac" > /dev/null; then
  die "an Unreal editor or game is already running: one at a time"
fi
mkdir -p "$out_dir"
set +e
# perl alarm: macOS has no timeout(1); the game also quits itself at the end of the drive.
perl -e 'alarm shift; exec @ARGV' "$(( limit + 30 ))" "$exe" "${args[@]}" ${extra[@]+"${extra[@]}"} > "$out_dir/run.log" 2>&1
status=$?
set -e
grep -E "DemoPos .*phase=testdrive" "$out_dir/run.log" > "$out_dir/positions.txt" || true
result="PASS"
reason="drive reached its end"
if grep -q "TestDrive error" "$out_dir/run.log"; then
  result="FAIL"
  reason="$(grep -m 1 "TestDrive error" "$out_dir/run.log")"
elif ! grep -q "TestDrive end" "$out_dir/run.log"; then
  result="FAIL"
  reason="no 'TestDrive end' line (game exit $status; stuck, crash or guard)"
fi
if test "${VIDEO:-0}" = "1" && test -d "$out_dir/frames"; then
  swift "$repo_root/tools/build/frames-to-movie.swift" "$out_dir/frames" 10 "$out_dir/capture.mov" && rm -rf "$out_dir/frames"
fi
printf '%s: %s (%s, %s, exit %s)\n' "$result" "$reason" "$drive" "$mode" "$status" | tee "$out_dir/result.txt"
test "$result" = "PASS"
