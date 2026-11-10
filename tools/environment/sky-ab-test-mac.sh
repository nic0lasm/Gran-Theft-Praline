#!/usr/bin/env bash
# One-command A/B test of the sky modes on a packaged build: frame times through the
# performance bench (tools/perf/bench.sh) and a still per mode and ambience, side by side
# in one HTML page.
#
# Usage: tools/environment/sky-ab-test-mac.sh <build_dir> [out_dir]
#   build_dir  folder holding GranTheftPraline.app
# Environment:
#   MODES      sky modes, default "volumetric lightfunction cheap"
#                volumetric    : volumetric clouds and their shadow map (0.3 look)
#                lightfunction : volumetric clouds, shadow from the M_CloudShadowLF light function
#                cheap         : 2D cloud dome and the light function shadow
#   SCENARIOS  bench scenarios, default "street-fixed rain"
#   DURATION   seconds per bench run, default 90
#   VARIANTS   passed to bench.sh (cvars of the optimisation work), default "base="
#   BENCH      path of bench.sh, default tools/perf/bench.sh
# Output: <out_dir>/index.html (stills side by side) and <out_dir>/<mode>/summary.md.
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:?usage: sky-ab-test-mac.sh <build_dir> [out_dir]}"
out="${2:-$build_dir/sky-ab/$(date +%Y%m%d-%H%M%S)}"
modes="${MODES:-volumetric lightfunction cheap}"
scenarios="${SCENARIOS:-street-fixed rain}"
bench="${BENCH:-$repo/tools/perf/bench.sh}"
test -x "$bench" || { echo "sky-ab: no bench at $bench (branch feat/perf-bench)" >&2; exit 1; }
app="$build_dir/GranTheftPraline.app"
exe="$(find "$app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
test -n "$exe" || { echo "sky-ab: no executable in $app" >&2; exit 1; }
mkdir -p "$out"

ambience_of() {
  case "$1" in
    rain) echo rain ;;
    night) echo night ;;
    *) echo noon ;;
  esac
}

for mode in $modes; do
  echo "== sky $mode: bench"
  EXTRA_ARGS="${EXTRA_ARGS:-} -Sky=$mode" SCENARIOS="$scenarios" "$bench" "$build_dir" "$out/$mode"
  for scenario in $scenarios; do
    ambience="$(ambience_of "$scenario")"
    still="$out/$mode/stills-$ambience"
    echo "== sky $mode: still $ambience"
    caffeinate -dimsu "$exe" -SkipTitle -Ambience="$ambience" -Sky="$mode" ${EXTRA_ARGS:-} \
      -CaptureVideo=1 -CaptureFps=1 -CaptureDelay=20 -CaptureDir="$still" -ExitAfter=26 \
      > "$still.log" 2>&1 || echo "  exit status $?"
  done
done

page="$out/index.html"
{
  echo '<!doctype html><meta charset="utf-8"><title>Sky A/B</title>'
  echo '<style>body{font:14px sans-serif;background:#111;color:#eee;margin:16px}img{width:100%}td{vertical-align:top;width:33%}pre{white-space:pre-wrap}</style>'
  echo "<h1>Sky A/B</h1><p>$build_dir</p>"
  for scenario in $scenarios; do
    ambience="$(ambience_of "$scenario")"
    echo "<h2>$ambience</h2><table><tr>"
    for mode in $modes; do
      img="$(ls "$out/$mode/stills-$ambience"/frame_*.jpg 2>/dev/null | tail -n 1 || true)"
      echo "<td><h3>$mode</h3>"
      test -n "$img" && echo "<img src=\"${img#"$out"/}\">" || echo "<p>no still</p>"
      echo "</td>"
    done
    echo "</tr></table>"
  done
  for mode in $modes; do
    echo "<h2>Frame times: $mode</h2><pre>"
    cat "$out/$mode/summary.md" 2>/dev/null || echo "no summary"
    echo "</pre>"
  done
} > "$page"
echo "sky-ab: $page"
