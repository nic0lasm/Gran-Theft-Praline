#!/usr/bin/env bash
# Night slot (day/night): cost of the night (street and shop lights, their shadows, car
# lights) against the day, and of dawn, golden hour and dusk, on a packaged build, with the
# night light budget varied by command line (-NightLights, -NightShadows, -NightDistance,
# -LitCars: in builds that contain feat/sky-slot3 3df9dcf; older builds ignore them and the
# report says "not in this build"). Nothing is compiled or packaged, the clone's files are not changed.
#
# Run from the root of the GTAL-build clone, nothing else running (one command):
#   git fetch -q origin feat/sky-slot3 && mkdir -p Builds/night-slot && git show origin/feat/sky-slot3:tools/environment/slot-night-mac.sh > Builds/night-slot/run.sh && bash Builds/night-slot/run.sh
# Environment:
#   BUILD      packaged build dir (default Builds/perf-cumul/0.3)
#   DRY_RUN=1  prerequisites and 2 short runs only
#   MODE       lights (default, the runs above, 14 min) or clouds (night clouds A/B,
#              -NightClouds=volumetric|dome|half at night, late dusk and on the night demo
#              route, 13 min; needs feat/sky-slot3 with -NightClouds in the build), or
#              compare (before/after images of the m1 night cloud default against
#              -NightClouds=full, golden hour to night and the night route, 13 min)
#   MAP_MD5    expected md5 of the map (default 1c92de0f813b89cb40aeb000cd92a1de)
# Output: Builds/night-slot/<stamp>/REPORT.md (GPU per run, night cost against noon,
#         verdict against the 3 ms budget), index.html (stills), heartbeat.txt, steps.log.
# Planned: 12 fixed runs of 50 s + 2 demo-route runs of 100 s = 14 min. Each launch is
# killed at 2x its planned time; the whole pass stops at 35 min.
set -uo pipefail

ref_branch="origin/feat/sky-slot3"
repo="$(pwd)"
test -d "$repo/GranTheftPraline" || { echo "night: run from the root of the GTAL-build clone" >&2; exit 1; }
stamp="$(date +%Y%m%d-%H%M%S)"
dry="${DRY_RUN:-0}"
mode="${MODE:-lights}"
out="$repo/Builds/night-slot/$stamp"
test "$mode" = "lights" || out="$repo/Builds/night-slot/$mode-$stamp"
test "$dry" = "1" && out="$repo/Builds/night-slot/dry-$stamp"
build="${BUILD:-$repo/Builds/perf-cumul/0.3}"
map="$repo/GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
want_md5="${MAP_MD5:-1c92de0f813b89cb40aeb000cd92a1de}"
mkdir -p "$out/tools"
report="$out/REPORT.md"
start_s="$(date +%s)"
budget_s=2100

log() { printf '%s %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$out/steps.log"; }
die() { log "ABORT: $*"; printf '\n**ABORTED:** %s\n' "$*" >> "$report"; exit 1; }
md5_of() { if command -v md5 > /dev/null; then md5 -q "$1"; else md5sum "$1" | cut -d' ' -f1; fi; }
limit() { local s="$1"; shift; perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$s" "$@"; }

( while sleep 600; do printf '%s alive\n' "$(date +%H:%M:%S)" >> "$out/heartbeat.txt"; done ) > /dev/null 2>&1 < /dev/null &
hb=$!
head_before="$(git rev-parse HEAD 2>/dev/null || echo none)"
status_before="$(git status --porcelain --untracked-files=no 2>/dev/null | md5_of /dev/stdin 2>/dev/null || echo none)"
md5_before="$(md5_of "$map" 2>/dev/null || echo missing)"
cleanup() {
  kill "$hb" 2>/dev/null || true
  pkill -f "GranTheftPraline-Mac" 2>/dev/null || true
  local md5_after head_after status_after
  md5_after="$(md5_of "$map" 2>/dev/null || echo missing)"
  head_after="$(git rev-parse HEAD 2>/dev/null || echo none)"
  status_after="$(git status --porcelain --untracked-files=no 2>/dev/null | md5_of /dev/stdin 2>/dev/null || echo none)"
  {
    echo
    echo "## Clone after the pass"
    echo "- Map md5 before $md5_before, after $md5_after: $( [ "$md5_before" = "$md5_after" ] && echo unchanged || echo CHANGED )"
    echo "- HEAD before $head_before, after $head_after: $( [ "$head_before" = "$head_after" ] && echo unchanged || echo CHANGED )"
    echo "- Tracked files: $( [ "$status_before" = "$status_after" ] && echo unchanged || echo CHANGED )"
    echo "- Duration: $(( ($(date +%s) - start_s) / 60 )) min"
  } >> "$report"
  log "end, report: $report"
}
trap cleanup EXIT

{
  echo "# Night slot ($mode) - cost of the night ($stamp)"
  echo
  echo "Host $(hostname -s); $(uptime | sed 's/^ *//')"
  echo "Clone HEAD: $head_before"
  echo "Map md5: $md5_before (expected $want_md5: $( [ "$md5_before" = "$want_md5" ] && echo OK || echo DIFFERENT, the packaged build is measured anyway ))"
  echo "Build: $build"
  echo
  echo "GPU = p50 / p95 in ms over the whole run (frame log of the game); fixed camera unless 'demo'."
  echo "Mean = mean luma 0-255 of the still. Budget = what the game applied (status file after 12 s)."
} > "$report"

log "prerequisites"
if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac" > /dev/null; then die "an Unreal editor or the game is running"; fi
exe="$(find "$build/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x 2>/dev/null | head -n 1)"
test -n "$exe" || die "no packaged build at $build"
git fetch -q origin feat/sky-slot3 2>/dev/null || log "fetch failed, using the local $ref_branch"
for f in tools/environment/still_stats.py tools/perf/summarize.py; do
  git show "$ref_branch:$f" > "$out/tools/$(basename "$f")" 2>/dev/null || die "cannot read $f from $ref_branch"
done
echo "Code of the tools: $(git rev-parse --short "$ref_branch")" >> "$report"

# run name seconds args... : one launch, still at 25 s, frame log of the whole run.
run() {
  local name="$1" secs="$2"; shift 2
  local dir="$out/$name"
  mkdir -p "$dir"
  if test $(( $(date +%s) - start_s )) -gt "$budget_s"; then log "skip $name: pass time limit reached"; return; fi
  log "== $name ($secs s)"
  limit $(( (secs + 20) * 2 )) "$exe" -SkipTitle "$@" -EnvReport="$dir/status.txt" -FrameLogFile="$dir/frames.csv" \
    -CaptureVideo=1 -CaptureFps=1 -CaptureDelay=25 -CaptureDir="$dir" -ExitAfter="$secs" \
    > "$dir/stdout.log" 2>&1 < /dev/null || log "  $name: exit status $? (limit = killed)"
  pkill -f "GranTheftPraline-Mac" 2>/dev/null || true
  local img stats gpu budget lights
  img="$(ls "$dir"/frame_*.jpg 2>/dev/null | tail -n 1 || true)"
  stats="$(python3 -I "$out/tools/still_stats.py" "${img:-none}" | cut -d' ' -f1)"
  gpu="$(python3 -I "$out/tools/summarize.py" "$dir/frames.csv" s v 2>/dev/null | awk -F'|' 'NF>5{gsub(/ /,"",$4);gsub(/ /,"",$5);print $4" "$5}')"
  budget="$(sed -n 's/^night_budget=//p' "$dir/status.txt" 2>/dev/null)"
  test "$mode" = "clouds" && budget="$(sed -n 's/^night_clouds=//p' "$dir/status.txt" 2>/dev/null)"
  lights="$(sed -n 's/^status=//p' "$dir/status.txt" 2>/dev/null | grep -o 'lights [0-9]*/[0-9]* on, [0-9]* shadowed' | head -n 1)"
  local args="$*"
  set -- ${gpu:-nan nan}
  echo "| $name | \`$args\` | $1 | $2 | ${stats:-nan} | ${budget:-not in this build} | ${lights:-?} |" >> "$out/rows.md"
  printf '%s %s %s\n' "$name" "$1" "$2" >> "$out/gpu.txt"
}

dawn="-Date=2026-10-15 -TimeOfDay=7.75 -TimeScale=0 -Weather=Clear"
if test "$dry" = "1"; then
  run noon 20 -Ambience=noon
  run night 20 -Ambience=night
elif test "$mode" = "compare"; then
  late="-Date=2026-10-15 -TimeOfDay=19.6 -TimeScale=0 -Weather=Cloudy"
  for pair in "golden|-Ambience=golden" "dusk|-Ambience=dusk" "late-dusk|$late" "night|-Ambience=night"; do
    label="${pair%%|*}"; args="${pair#*|}"
    run "$label-full" 50 $args -NightClouds=full
    run "$label-default" 50 $args
  done
  run night-demo-full 100 -Ambience=night -DemoRoute -NightClouds=full
  run night-demo-default 100 -Ambience=night -DemoRoute
elif test "$mode" = "clouds"; then
  late="-Date=2026-10-15 -TimeOfDay=19.6 -TimeScale=0 -Weather=Cloudy"
  run noon 50 -Ambience=noon
  run night 50 -Ambience=night -NightClouds=volumetric
  run night-dome 50 -Ambience=night -NightClouds=dome
  run night-half 50 -Ambience=night -NightClouds=half
  run late-dusk 50 $late -NightClouds=volumetric
  run late-dusk-dome 50 $late -NightClouds=dome
  run night-demo 100 -Ambience=night -DemoRoute -NightClouds=volumetric
  run night-demo-dome 100 -Ambience=night -DemoRoute -NightClouds=dome
  run night-demo-half 100 -Ambience=night -DemoRoute -NightClouds=half
else
  run noon 50 -Ambience=noon
  run dawn 50 $dawn
  run golden 50 -Ambience=golden
  run dusk 50 -Ambience=dusk
  run night 50 -Ambience=night
  run night-nolights 50 -Ambience=night -NightLights=0 -LitCars=0
  run night-shadows2 50 -Ambience=night -NightShadows=2
  run night-shadows0 50 -Ambience=night -NightShadows=0
  run night-lights32 50 -Ambience=night -NightLights=32
  run night-dist6000 50 -Ambience=night -NightDistance=6000
  run night-nocars 50 -Ambience=night -LitCars=0
  run night-rain 50 -Ambience=night-rain
  run night-demo 100 -Ambience=night -DemoRoute
  run night-demo-shadows2 100 -Ambience=night -DemoRoute -NightShadows=2
fi

{
  echo
  echo "## Runs"
  echo
  echo "| Run | Arguments | GPU p50 | GPU p95 | Mean | Budget applied | Lights |"
  echo "| --- | --- | --- | --- | --- | --- | --- |"
  cat "$out/rows.md" 2>/dev/null
  echo
  if test "$mode" = "compare"; then
    echo "## Gain of the default against -NightClouds=full (same ambience)"
    echo
    echo "| Pair | GPU p50 full -> default | GPU p95 full -> default |"
    echo "| --- | --- | --- |"
    awk '{ n=$1; if (n ~ /-full$/) { k=substr(n,1,length(n)-5); f50[k]=$2; f95[k]=$3 }
           else if (n ~ /-default$/) { k=substr(n,1,length(n)-8); d50[k]=$2; d95[k]=$3; order[++c]=k } }
         END { for (i=1;i<=c;i++) { k=order[i];
           if (f50[k]=="nan"||d50[k]=="nan"||f50[k]=="") print "| " k " | nan | nan |";
           else printf "| %s | %.1f -> %.1f (%+.1f) | %.1f -> %.1f (%+.1f) |\n", k, f50[k], d50[k], d50[k]-f50[k], f95[k], d95[k], d95[k]-f95[k] } }' "$out/gpu.txt" 2>/dev/null
    echo
  fi
  echo "## Cost against noon (fixed camera), budget: night <= 3 ms GPU over the day"
  echo
  echo "| Run | GPU p50 - noon | GPU p95 - noon | Within 3 ms (p50) |"
  echo "| --- | --- | --- | --- |"
  awk 'NR==FNR { if ($1=="noon") { n50=$2; n95=$3 }; next }
       $1!="noon" && $1 !~ /demo/ {
         if ($2=="nan" || n50=="") { print "| " $1 " | nan | nan | ? |"; next }
         d50=$2-n50; d95=$3-n95; printf "| %s | %+.1f | %+.1f | %s |\n", $1, d50, d95, (d50<=3 ? "yes" : "no") }' \
    "$out/gpu.txt" "$out/gpu.txt" 2>/dev/null
} >> "$report"

{
  echo '<!doctype html><meta charset="utf-8"><title>Night slot</title>'
  echo '<style>body{font:13px sans-serif;background:#111;color:#eee;margin:16px}.r{display:flex;flex-wrap:wrap;gap:8px}figure{margin:0;width:32%}img{width:100%}</style>'
  echo "<h1>Night slot - $stamp</h1><div class=r>"
  for d in "$out"/*/; do
    img="$(ls "$d"frame_*.jpg 2>/dev/null | tail -n 1 || true)"
    test -n "$img" && echo "<figure><img src=\"${img#"$out"/}\"><figcaption>$(basename "$d")</figcaption></figure>"
  done
  echo "</div>"
} > "$out/index.html"
test "$dry" = "1" && printf '\nDRY RUN OK\n' >> "$report"
log "done"
cat "$report"
