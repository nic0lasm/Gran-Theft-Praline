#!/usr/bin/env bash
# Perf slot 3 (day/night): validates the light function cloud shadow (-Sky=lightfunction,
# m1 default on feat/perf-cumul) in every weather against the volumetric shadow
# (-Sky=volumetric) of the SAME packaged build, and sweeps coverage, strength and mean
# where the presets are not calibrated yet. Nothing is compiled or packaged, the clone's
# files are not changed: only the build of slot 1 is launched.
#
# Run from the root of the GTAL-build clone, nothing else running (one command):
#   git fetch -q origin feat/sky-slot3 && mkdir -p Builds/sky-slot3 && git show origin/feat/sky-slot3:tools/environment/slot3-sky-mac.sh > Builds/sky-slot3/run.sh && bash Builds/sky-slot3/run.sh
# Environment:
#   BUILD      packaged build dir (default Builds/perf-cumul/0.3, from slot 1)
#   DRY_RUN=1  check the prerequisites and run 2 short stills only
#   MAP_MD5    expected md5 of the map (default 1c92de0f813b89cb40aeb000cd92a1de)
# Output: Builds/sky-slot3/<stamp>/REPORT.md (tables, verdicts, preset proposal),
#         index.html (stills side by side), heartbeat.txt, steps.log, one folder per still.
# Planned: about 35 stills of 45 s = 27 min, plus checks: 35 min. Each launch is killed
# at 2x its planned time (90 s); the whole pass stops at 70 min.
set -uo pipefail

ref_branch="origin/feat/sky-slot3"
repo="$(pwd)"
test -d "$repo/GranTheftPraline" || { echo "slot3: run from the root of the GTAL-build clone" >&2; exit 1; }
stamp="$(date +%Y%m%d-%H%M%S)"
dry="${DRY_RUN:-0}"
out="$repo/Builds/sky-slot3/$stamp"
test "$dry" = "1" && out="$repo/Builds/sky-slot3/dry-$stamp"
build="${BUILD:-$repo/Builds/perf-cumul/0.3}"
map="$repo/GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
want_md5="${MAP_MD5:-1c92de0f813b89cb40aeb000cd92a1de}"
mkdir -p "$out/tools"
report="$out/REPORT.md"
start_s="$(date +%s)"
budget_s=4200

log() { printf '%s %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$out/steps.log"; }
die() { log "ABORT: $*"; printf '\n**ABORTED:** %s\n' "$*" >> "$report"; exit 1; }
md5_of() { if command -v md5 > /dev/null; then md5 -q "$1"; else md5sum "$1" | cut -d' ' -f1; fi; }
# Time limit without coreutils `timeout` (macOS): perl alarm + exec of the game itself.
limit() { local s="$1"; shift; perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$s" "$@"; }

# Heartbeat every 10 minutes; it must not hold the caller's pipe.
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
  echo "# Slot 3 - light function cloud shadow in every weather ($stamp)"
  echo
  echo "Host $(hostname -s); $(uptime | sed 's/^ *//')"
  echo "Clone HEAD: $head_before"
  echo "Map md5: $md5_before (expected $want_md5: $( [ "$md5_before" = "$want_md5" ] && echo OK || echo DIFFERENT, the packaged build is measured anyway ))"
  echo "Build: $build"
  echo
  echo "Luminance = mean Rec. 601 luma 0-255 of the still at 20 s; ground = lower half;"
  echo "patch = std of 16x6 block means of the lower half (cloud spots), compared with the volumetric reference."
} > "$report"

log "prerequisites"
if pgrep -f "UnrealEditor( |$)|GranTheftPraline-Mac" > /dev/null; then die "an Unreal editor or the game is running"; fi
exe="$(find "$build/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x 2>/dev/null | head -n 1)"
test -n "$exe" || die "no packaged build at $build (slot 1 packages Builds/perf-cumul/0.3)"
git fetch -q origin feat/sky-slot3 2>/dev/null || log "fetch failed, using the local $ref_branch"
for f in tools/environment/still_stats.py tools/perf/summarize.py; do
  git show "$ref_branch:$f" > "$out/tools/$(basename "$f")" 2>/dev/null || die "cannot read $f from $ref_branch"
done
python3 -c 'import sys' || die "python3 missing"
echo "Code of the tools: $(git rev-parse --short "$ref_branch")" >> "$report"

# One still: name, then game arguments. Prints "mean ground patch spots gpu_p50 gpu_p95";
# spots compares with the still in $flat_img (same view, no cloud shadow), nan without one.
flat_img=""
shot() {
  local name="$1"; shift
  local dir="$out/$name"
  mkdir -p "$dir"
  if test $(( $(date +%s) - start_s )) -gt "$budget_s"; then log "skip $name: pass time limit reached"; echo "nan nan nan nan nan nan"; return; fi
  limit 90 "$exe" -SkipTitle "$@" -EnvReport="$dir/status.txt" -FrameLogFile="$dir/frames.csv" \
    -CaptureVideo=1 -CaptureFps=1 -CaptureDelay=20 -CaptureDir="$dir" -ExitAfter=26 \
    > "$dir/stdout.log" 2>&1 < /dev/null || log "  $name: exit status $? (90 s limit = killed)"
  pkill -f "GranTheftPraline-Mac" 2>/dev/null || true
  local img stats gpu
  img="$(ls "$dir"/frame_*.jpg 2>/dev/null | tail -n 1 || true)"
  stats="$(python3 -I "$out/tools/still_stats.py" "${img:-none}" "${flat_img:-none}")"
  gpu="$(python3 -I "$out/tools/summarize.py" "$dir/frames.csv" s v 2>/dev/null | awk -F'|' 'NF>5{gsub(/ /,"",$4);gsub(/ /,"",$5);print $4, $5}')"
  echo "$stats ${gpu:-nan nan}"
}

table_head() {
  { echo; echo "## $1"; echo; echo "| Run | Sky | Arguments | Mean | Ground | Patch | Spots % | GPU p50 | GPU p95 | Light function | Gap |"; echo "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"; } >> "$report"
}
row() { # name sky args stats... ref_mean
  local name="$1" sky="$2" args="$3" mean="$4" ground="$5" patch="$6" spots="$7" p50="$8" p95="$9" ref="${10}"
  local state gap
  state="$(sed -n 's/^cloud_shadow_light_function=//p' "$out/$name/status.txt" 2>/dev/null || true)"
  gap="$(awk -v a="$mean" -v b="$ref" 'BEGIN{ if (a=="nan"||b=="nan") print "nan"; else printf "%+d", a-b }')"
  echo "| $name | $sky | \`$args\` | $mean | $ground | $patch | $spots | $p50 | $p95 | ${state:-?} | $gap |" >> "$report"
  printf '%s|%s|%s\n' "$name" "$gap" "$spots" >> "$out/rows.txt"
}

# weather label, then LF variants as "label:cvars" (empty cvars = the preset).
noon_args="-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0"
case_weather() {
  local weather="$1"; shift
  local args="$noon_args -Weather=$weather"
  table_head "$weather (13:30, 21 June)"
  log "== $weather flat (no cloud shadow)"
  flat_img=""
  local flat; flat=($(shot "$weather-flat" $args -Sky=volumetric -CloudShadows=0 -CloudShadowLF=0))
  row "$weather-flat" "no cloud shadow" "$args -CloudShadows=0" "${flat[@]}" "${flat[0]}"
  flat_img="$(ls "$out/$weather-flat"/frame_*.jpg 2>/dev/null | tail -n 1 || true)"
  log "== $weather reference (volumetric)"
  local ref; ref=($(shot "$weather-vol" $args -Sky=volumetric))
  row "$weather-vol" volumetric "$args" "${ref[@]}" "${ref[0]}"
  local ref_patch="${ref[3]}"
  for v in "$@"; do
    local label="${v%%:*}" cvars="${v#*:}" extra=""
    test -n "$cvars" && extra="-GfxCVars=$cvars"
    log "== $weather $label"
    # shellcheck disable=SC2086
    local s; s=($(shot "$weather-$label" $args -Sky=lightfunction $extra))
    row "$weather-$label" lightfunction "${extra:-preset}" "${s[@]}" "${ref[0]}"
  done
  echo "$weather|${ref[0]}|$ref_patch" >> "$out/refs.txt"
}

log "control launch"
ctl=($(shot control $noon_args -Weather=Clear -Sky=lightfunction))
test -s "$out/control/frames.csv" || die "control launch produced no frame: the build freezes"
echo "Control: $(wc -l < "$out/control/frames.csv" | tr -d ' ') frames, light function $(sed -n 's/^cloud_shadow_light_function=//p' "$out/control/status.txt" 2>/dev/null || echo '?')" >> "$report"

C=gtp.CloudShadowLF
if test "$dry" = "1"; then
  case_weather Clear "preset:"
  printf "\nDRY RUN OK\n" >> "$report"
else
  # Clear: calibrated mean but no visible spots suspected (strength had no effect): more coverage.
  case_weather Clear "preset:" \
    "c60s50m55:$C.Coverage=0.6,$C.Strength=0.5,$C.Mean=0.55" "c60s50m65:$C.Coverage=0.6,$C.Strength=0.5,$C.Mean=0.65" \
    "c80s50m60:$C.Coverage=0.8,$C.Strength=0.5,$C.Mean=0.6" "c80s50m70:$C.Coverage=0.8,$C.Strength=0.5,$C.Mean=0.7" \
    "c60s70m70:$C.Coverage=0.6,$C.Strength=0.7,$C.Mean=0.7"
  case_weather Cloudy "preset:" \
    "s35m40:$C.Strength=0.35,$C.Mean=0.4" "s35m60:$C.Strength=0.35,$C.Mean=0.6" \
    "s60m55:$C.Strength=0.6,$C.Mean=0.55" "s60m75:$C.Strength=0.6,$C.Mean=0.75"
  case_weather Overcast "preset:" "m70:$C.Mean=0.7" "m85:$C.Mean=0.85" "m120:$C.Mean=1.2"
  case_weather Storm "preset:" "m70:$C.Mean=0.7" "m85:$C.Mean=0.85" "m120:$C.Mean=1.2"
  case_weather Rain "preset:"
  case_weather Fog "preset:" "m80:$C.Mean=0.8"
fi

# Verdict per weather: the run closest in mean, then whether its spots are visible.
{
  echo
  echo "## Verdict per weather (closest mean; spots visible = spots at least 70 % of the volumetric reference, both against the flat still)"
  echo
  echo "| Weather | Reference mean | Reference spots % | Best run | Gap | Spots % | Within ±3 | Spots visible |"
  echo "| --- | --- | --- | --- | --- | --- | --- | --- |"
  while IFS='|' read -r w rmean rpatch; do
    best="$(grep "^$w-" "$out/rows.txt" | grep -v -- "-vol|" | grep -v -- "-flat|" | awk -F'|' '$2!="nan"{g=$2+0; a=(g<0?-g:g); print a"|"$0}' | sort -t'|' -k1,1n | head -n 1)"
    if test -z "$best"; then echo "| $w | $rmean | $rpatch | none | | | no | ? |"; continue; fi
    IFS='|' read -r abs name gap patch <<< "$best"
    within="no"; test "$abs" -le 3 && within="yes"
    spots="$(awk -v p="$patch" -v r="$rpatch" 'BEGIN{ if (p=="nan"||r=="nan"||r==0) print "?"; else print (p>=0.7*r ? "yes" : "no") }')"
    echo "| $w | $rmean | $rpatch | $name | $gap | $patch | $within | $spots |"
  done < "$out/refs.txt"
} >> "$report"

# Side-by-side page.
{
  echo '<!doctype html><meta charset="utf-8"><title>Slot 3 sky</title>'
  echo '<style>body{font:13px sans-serif;background:#111;color:#eee;margin:16px}.r{display:flex;flex-wrap:wrap;gap:8px}figure{margin:0;width:32%}img{width:100%}</style>'
  echo "<h1>Slot 3 - $stamp</h1>"
  for w in $(cut -d'|' -f1 "$out/refs.txt"); do
    echo "<h2>$w</h2><div class=r>"
    for d in "$out/$w"-*; do
      img="$(ls "$d"/frame_*.jpg 2>/dev/null | tail -n 1 || true)"
      test -n "$img" && echo "<figure><img src=\"${img#"$out"/}\"><figcaption>$(basename "$d")</figcaption></figure>"
    done
    echo "</div>"
  done
} > "$out/index.html"
log "done"
cat "$report"
