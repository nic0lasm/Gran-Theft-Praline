#!/usr/bin/env bash
# A/B slot "TSR and clouds in motion" on an existing Development build, no package.
# For every variant: a fixed-camera run (still at CAPTURE_AT s, frame times, GPU per pass)
# and a demo-route run (frame times, GPU per pass). Report with the stills side by side.
#
# Usage (root of GTAL-build, nothing else running), no checkout needed:
#   git fetch -q origin feat/perf-cumul-night && mkdir -p Builds/tsr-clouds \
#     && git show origin/feat/perf-cumul-night:tools/perf/slot-tsr-clouds-mac.sh > Builds/tsr-clouds/run.sh \
#     && caffeinate -dimsu bash Builds/tsr-clouds/run.sh
# Environment: DRY_RUN=1 (checks + base variant, short runs), BUILD (default
#   Builds/perf-cumul-test/0.3, a Development package with -CsvCopyTo, f1744dd or later),
#   VARIANTS (lines "label|cvar=v,cvar=v|extra args"), BRANCH (tools source, feat/perf-cumul-night)
# Output: Builds/tsr-clouds/<stamp>/REPORT.md, index.html, <variant>-fixed.jpg
# Plan: 11 variants x (45 s + 95 s) + launches = about 30 min; whole pass capped at 50 min,
#   every launch killed at its duration + 60 s.
set -euo pipefail
repo="$(pwd)"
test -d "$repo/GranTheftPraline" || { echo "run from the root of the GTAL-build clone" >&2; exit 2; }
branch="${BRANCH:-feat/perf-cumul-night}"
dry="${DRY_RUN:-0}"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$repo/Builds/tsr-clouds/$stamp"; test "$dry" = "1" && out="$repo/Builds/tsr-clouds/dry-$stamp"
build="${BUILD:-$repo/Builds/perf-cumul-test/0.3}"
map="$repo/GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
fixed_s=45; demo_s=95; at="${CAPTURE_AT:-30}"
test "$dry" = "1" && { fixed_s=36; demo_s=20; }
budget_s=3000
base_args="-Sky=lightfunction"
default_variants='base||
tsr-hist75|r.TSR.History.ScreenPercentage=75|
tsr-rejaa1|r.TSR.RejectionAntiAliasingQuality=1|
tsr-noflicker|r.TSR.ShadingRejection.Flickering=0|
tsr-aa-medium|sg.AntiAliasingQuality=1|
tsr-aa-medium-hist75|sg.AntiAliasingQuality=1,r.TSR.History.ScreenPercentage=75|
taa|r.AntiAliasingMethod=2|
cloud-rays128|r.VolumetricCloud.ViewRaySampleMaxCount=128|
cloud-rays64|r.VolumetricCloud.ViewRaySampleMaxCount=64|
cloud-rtmode2|r.VolumetricRenderTarget.Mode=2|
sky-cheap||-Sky=cheap'
variants="${VARIANTS:-$default_variants}"
test "$dry" = "1" && variants="base||"
mkdir -p "$out/tools"
report="$out/REPORT.md"
log() { printf '%s %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$out/steps.log" >&2; }
die() { log "ABORT: $*"; printf '\n**ABORTED:** %s\n' "$*" >> "$report"; exit 1; }
limit() { local s="$1"; shift; perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$s" "$@"; }
( while sleep 600; do printf "%s alive\n" "$(date +%H:%M:%S)" >> "$out/heartbeat.txt"; done ) > /dev/null 2>&1 &
hb=$!
cleanup() { pkill -P "$hb" 2>/dev/null || true; kill "$hb" 2>/dev/null || true; pkill -f "GranTheftPraline-Mac" 2>/dev/null || true; log "end"; }
trap cleanup EXIT

if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac|UnrealBuildTool|AutomationTool" > /dev/null; then die "Unreal, UBT/UAT or the game is running"; fi
test -f "$map" || die "map missing"
git fetch -q origin "$branch" || die "fetch $branch"
for f in gpu_passes.py summarize.py; do git show "origin/$branch:tools/perf/$f" > "$out/tools/$f" || die "tool $f"; done
exe="$(find "$build/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x 2>/dev/null | head -n 1)"
test -n "$exe" || die "no packaged build in $build"
{
  echo "# TSR and clouds A/B ($stamp)"
  echo
  echo "Build: <GTAL-build>/${build#$repo/} (commit $(cat "$build/.commit" 2>/dev/null || echo unknown)); tools: $(git rev-parse --short "origin/$branch")"
  echo "Map md5: $(md5 -q "$map" 2>/dev/null || echo missing); host $(hostname -s); $(uptime | sed 's/.*load/load/')"
  echo "Base arguments: $base_args ; fixed ${fixed_s} s (still at ${at} s), demo route ${demo_s} s; frame ms p50/p95, GPU per pass median ms"
  echo
  echo "| Variant | Fixed p50 | Fixed p95 | Demo p50 | Demo p95 | Demo ≤33 ms | Hitches | TSR | Clouds | Lumen | Basepass |"
  echo "| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |"
} > "$report"
html="$out/index.html"
echo "<html><body style='background:#111;color:#ddd;font-family:sans-serif'><h1>TSR and clouds A/B $stamp</h1>" > "$html"

start_s=$(date +%s)
# One run: dir, seconds, args... ; leaves frames.csv, profile.csv, game.log
run() {
  local dir="$1" secs="$2"; shift 2; mkdir -p "$dir"
  limit $((secs + 60)) "$exe" -SkipTitle $base_args "$@" -csvGpuStats -csvCaptureFrames=$((secs * 60)) \
    -FrameLogFile="$dir/frames.csv" -CsvCopyTo="$dir/profile.csv" -ExitAfter="$secs" -abslog="$dir/game.log" \
    > "$dir/stdout.log" 2>&1 < /dev/null || log "  $(basename "$dir"): exit $? (limit = killed)"
  pkill -f "GranTheftPraline-Mac" 2>/dev/null || true
  sleep 3
}
stat2() { python3 -I "$out/tools/summarize.py" "$1" s v 2>/dev/null | awk -F'|' 'NF>8{for(i=2;i<=NF;i++)gsub(/ /,"",$i); print $6, $7, $8, $9}'; }
pass() { # profile.csv, regex of GPU stat names -> summed median
  test -s "$1" || { echo nan; return; }
  python3 -I "$out/tools/gpu_passes.py" "$1" 300 2>/dev/null | awk -F'|' -v re="$2" 'NF>3{n=$2; gsub(/ /,"",n); if (n ~ re) {v=$3; gsub(/ /,"",v); s+=v; f=1}} END {if (f) printf "%.2f", s; else print "nan"}'
}
while IFS='|' read -r label cvars extra; do
  test -n "$label" || continue
  if test $(( $(date +%s) - start_s )) -gt "$budget_s"; then log "skip $label: pass time limit reached"; continue; fi
  vargs=(); test -z "$cvars" || vargs+=("-GfxCVars=$cvars")
  # shellcheck disable=SC2206
  test -z "$extra" || vargs+=($extra)
  log "variant $label: fixed ${fixed_s}s"
  run "$out/$label-fixed" "$fixed_s" ${vargs[@]+"${vargs[@]}"} -CaptureVideo=1 -CaptureFps=1 -CaptureDelay="$at" -CaptureDir="$out/$label-fixed/still"
  img="$(ls "$out/$label-fixed/still"/frame_*.jpg 2>/dev/null | head -n 1 || true)"
  if test -n "$img"; then mv "$img" "$out/$label-fixed.jpg"; rm -rf "$out/$label-fixed/still"; fi
  log "variant $label: demo ${demo_s}s"
  run "$out/$label-demo" "$demo_s" ${vargs[@]+"${vargs[@]}"} -DemoRoute
  read -r f50 f95 _ _ <<< "$(stat2 "$out/$label-fixed/frames.csv" || echo nan nan nan nan)"
  read -r d50 d95 dpct dhit <<< "$(stat2 "$out/$label-demo/frames.csv" || echo nan nan nan nan)"
  prof="$out/$label-demo/profile.csv"
  echo "| $label | ${f50:-nan} | ${f95:-nan} | ${d50:-nan} | ${d95:-nan} | ${dpct:-nan} | ${dhit:-nan} | $(pass "$prof" '^TSR') | $(pass "$prof" 'VolumetricCloud|VolCloud') | $(pass "$prof" '^Lumen') | $(pass "$prof" '^Basepass') |" >> "$report"
  printf "<div style='display:inline-block;margin:4px'><img src='%s-fixed.jpg' width='640'><br>%s : %s %s</div>\n" "$label" "$label" "${cvars:-}" "${extra:-}" >> "$html"
done <<< "$variants"
echo "</body></html>" >> "$html"
{ echo; echo "GPU columns: demo route, sum of the median of the matching passes. Full pass tables: <variant>-demo/profile.csv with gpu_passes.py."; echo "Duration: $(( ($(date +%s) - start_s) / 60 )) min"; } >> "$report"
test "$dry" = "1" && echo "DRY RUN (base only, short runs)" >> "$report"
log "done: $report"
cat "$report"
