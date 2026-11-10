#!/usr/bin/env bash
# One-command performance bench: the judge every system is measured against.
#
# Runs fixed scenarios on a packaged build, one configuration (A/B) at a time,
# and writes one table: GPU p50/p95, frame p50/p95, share of frames <= 33.3 ms,
# hitches > 100 ms. Same route, same duration, same ambience for every run.
#
# Usage: tools/perf/bench.sh <build_dir> [out_dir]
#   build_dir  folder holding GranTheftPraline.app (e.g. Builds/perf-m1/0.3)
#
# Environment:
#   SCENARIOS  space-separated names, default "street-fixed street-demo"
#                street-fixed : player standing still, -Ambience=noon
#                street-demo  : -DemoRoute (walk, get in the car, drive)
#                night        : standing still, -Ambience=night
#                rain         : standing still, -Ambience=rain
#   DURATION   seconds per run, default 90 (use 330 for a release gate)
#   VARIANTS   ';'-separated "label=cvar=v,cvar=v" entries passed as
#              -GfxCVars, default "base=" (no override), e.g.
#              VARIANTS="base=;noclouds=r.VolumetricCloud=0;res45=r.ScreenPercentage=45"
#   EXTRA_ARGS extra game arguments for every run
#   MAX_LOAD   refuse to start while the 1-minute load average is above it
#              (default 4; Spotlight indexing after a reboot ruins numbers)
#
# Per-pass GPU timings (stat gpu) are not available in Shipping. For them,
# package the Test configuration (tools/build/package-mac.sh <v> Test) and
# run with PASS_PROFILE=1: the script then also saves the GPU stat CSV
# (CSV profiler) next to the frame times. This mode is unverified: check it
# once on the Mac and fix the flags in this file.
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
build_dir="${1:?usage: bench.sh <build_dir> [out_dir]}"
stamp="$(date +%Y%m%d-%H%M%S)"
out_dir="${2:-$build_dir/bench/$stamp}"
duration="${DURATION:-90}"
scenarios="${SCENARIOS:-street-fixed street-demo}"
variants="${VARIANTS:-base=}"
max_load="${MAX_LOAD:-4}"

die() { echo "bench: $*" >&2; exit 1; }

app="$build_dir/GranTheftPraline.app"
test -d "$app" || die "no build at $app"
exe="$(find "$app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
test -n "$exe" || die "no executable in $app"
if pgrep -fl "UnrealEditor|GranTheftPraline" | grep -v UnrealEditorServices >/dev/null; then
  die "an Unreal or game process is already running"
fi
mkdir -p "$out_dir"

wait_for_calm() {
  local tries=0 load
  while true; do
    load="$(sysctl -n vm.loadavg | tr -d '{}' | awk '{print $1}')"
    if awk -v l="$load" -v m="$max_load" 'BEGIN{exit !(l<=m)}'; then return 0; fi
    tries=$((tries + 1))
    test "$tries" -le 40 || die "load stays above $max_load (now $load), aborting"
    sleep 15
  done
}

scenario_args() {
  case "$1" in
    street-fixed) echo "-Ambience=noon" ;;
    street-demo)  echo "-Ambience=noon -DemoRoute" ;;
    night)        echo "-Ambience=night" ;;
    rain)         echo "-Ambience=rain" ;;
    *) die "unknown scenario $1" ;;
  esac
}

summary="$out_dir/summary.md"
{
  echo "# Bench $stamp"
  echo
  echo "Build: $build_dir ; ${duration} s per run ; load limit $max_load ; host $(hostname -s) ; uptime: $(uptime | sed 's/^ *//')"
  echo
  echo "| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |"
  echo "| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |"
} > "$summary"

IFS=';' read -r -a variant_list <<< "$variants"
for scenario in $scenarios; do
  # shellcheck disable=SC2207
  sargs=($(scenario_args "$scenario"))
  for v in "${variant_list[@]}"; do
    label="${v%%=*}"
    cvars="${v#*=}"
    vargs=()
    test -z "$cvars" || vargs+=("-GfxCVars=$cvars")
    run="$out_dir/${scenario}__${label}"
    mkdir -p "$run"
    wait_for_calm
    echo "== $scenario / $label (${duration}s)"
    extra=()
    if test "${PASS_PROFILE:-0}" = "1"; then
      extra+=(-csvGpuStats -CsvMetadata="scenario=$scenario,variant=$label" -csvCaptureFrames=1800)
    fi
    # shellcheck disable=SC2086
    caffeinate -dimsu "$exe" "${sargs[@]}" ${EXTRA_ARGS:-} ${vargs[@]+"${vargs[@]}"} ${extra[@]+"${extra[@]}"} \
      -SkipTitle -FrameLogFile="$run/frametimes.csv" -ExitAfter="$duration" > "$run/stdout.log" 2>&1 || echo "  exit status $?"
    if test -s "$run/frametimes.summary.txt"; then
      cp "$run/frametimes.summary.txt" "$run/summary.txt"
    fi
    python3 -I "$script_dir/summarize.py" "$run/frametimes.csv" "$scenario" "$label" >> "$summary" || echo "| $scenario | $label | no data | | | | | |" >> "$summary"
    sleep 5
  done
done

echo "Bench done: $summary"
cat "$summary"
