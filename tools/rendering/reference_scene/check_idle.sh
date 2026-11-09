#!/usr/bin/env bash
# Check that this Mac is quiet enough for a benchmark pass.
#
# Samples the machine for about 2 minutes and prints "IDLE ..." (exit 0) or
# "BUSY <reasons>" (exit 1). Busy when any of these holds:
#   - 1-minute load average >= 2 at the start or at the end of the window;
#   - a process above 30 % CPU in at least 3 of 8 samples. WindowServer is
#     ignored (it stays above 30 % on this Mac even when idle); add other
#     always-on apps with IDLE_IGNORE, an extended regex matched against the
#     process name, e.g. IDLE_IGNORE='WindowServer|MyChatApp';
#   - mobileassetd or softwareupdated above 5 % CPU (system downloads);
#   - Safari or Terminal above 10 % CPU;
#   - any UnrealEditor (game or editor), UAT (AutomationTool, RunUAT),
#     UnrealBuildTool, ShaderCompileWorker, xcodebuild or clang running.
#     UnrealEditorServices (a background Epic service) is not counted.
#
# Usage: tools/rendering/reference_scene/check_idle.sh
#   Run it before run_benchmark.sh, for example:
#   until tools/rendering/reference_scene/check_idle.sh; do sleep 480; done
set -u

ignore="${IDLE_IGNORE:-WindowServer}"

samples=8
interval=15
reasons=""

load_now() { sysctl -n vm.loadavg | awk '{print $2}'; }
cpu_of() { ps -axo %cpu=,comm= | grep -i -- "$1" | grep -v grep | awk '{s+=$1} END{print s+0}'; }
above() { awk -v v="$1" -v t="$2" 'BEGIN{exit !(v>t)}'; }

# Engine and build tools, checked at every sample (they come and go).
build_procs() {
  ps -axo comm= | sed 's#.*/##' | grep -v '^UnrealEditorServices$' \
    | grep -E '^(UnrealEditor|ShaderCompileWorker|xcodebuild|clang)' | sort -u
  ps -axo args= | grep -E 'AutomationTool|RunUAT|UnrealBuildTool\.dll' | grep -v grep > /dev/null \
    && echo "UAT/UBT"
}
builds=""

load1="$(load_now)"
hot=""
for _ in $(seq "$samples"); do
  h="$(ps -axo %cpu=,comm= | awk '$1>30' | sed 's#.*/##' \
    | grep -v -E '^(ps|awk)$' | grep -v -E -- "$ignore" | sort -u | tr '\n' ';')"
  test -n "$h" && hot="$hot;$h"
  builds="$builds
$(build_procs)"
  sleep "$interval"
done
load2="$(load_now)"

builds="$(echo "$builds" | grep -v '^$' | sort -u | tr '\n' ' ')"
test -n "$builds" && reasons="$reasons build or engine running: $builds;"
above "$load1" 1.999 && reasons="$reasons load average $load1;"
above "$load2" 1.999 && reasons="$reasons load average $load2 at the end;"
persist="$(echo "$hot" | tr ';' '\n' | grep -v '^$' | sort | uniq -c \
  | awk -v n="$samples" '$1>=3{print $2"("$1"/"n")"}' | tr '\n' ' ')"
test -n "$persist" && reasons="$reasons above 30 % CPU: $persist;"
for p in mobileassetd softwareupdated; do
  c="$(cpu_of "$p")"; above "$c" 5 && reasons="$reasons $p ${c} %;"
done
for p in Safari Terminal; do
  c="$(cpu_of "$p")"; above "$c" 10 && reasons="$reasons $p ${c} %;"
done

if test -z "$reasons"; then
  echo "IDLE load average $load1 -> $load2"
  exit 0
fi
echo "BUSY$reasons"
exit 1
