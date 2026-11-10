#!/usr/bin/env bash
# Calibrates the light function cloud shadow (-Sky=lightfunction) against the volumetric
# cloud shadow by image luminance, on a packaged build, without touching the exposure.
# For each ambience: one still with -Sky=volumetric (reference), then one still per
# (Mean, Strength) pair through the gtp.CloudShadowLF.* cvars. The mean luminance of a still
# (0-255, ffmpeg) is compared with the reference; the closest pair is reported.
#
# Usage: tools/environment/calibrate-cloud-shadow-mac.sh <build_dir> [out_dir]
# Environment:
#   AMBIENCES  default "noon rain"
#   MEANS      default "0.5 0.65 0.8 1.0"     (gtp.CloudShadowLF.Mean)
#   STRENGTHS  default "0.2 0.5 0.75"         (gtp.CloudShadowLF.Strength)
#   TOLERANCE  accepted luminance gap, default 3
# About 26 s per still: 2 ambiences x (1 + 12) stills = 11 to 12 min.
# Output: <out_dir>/calibration.md (table and the pick per ambience), stills per run,
#         status.txt per run (what the game loaded: light function loaded or missing).
set -euo pipefail

build_dir="${1:?usage: calibrate-cloud-shadow-mac.sh <build_dir> [out_dir]}"
out="${2:-$build_dir/cloud-shadow-calibration/$(date +%Y%m%d-%H%M%S)}"
ambiences="${AMBIENCES:-noon rain}"
means="${MEANS:-0.5 0.65 0.8 1.0}"
strengths="${STRENGTHS:-0.2 0.5 0.75}"
tolerance="${TOLERANCE:-3}"
exe="$(find "$build_dir/GranTheftPraline.app/Contents/MacOS" -maxdepth 1 -type f -perm -u+x | head -n 1)"
test -n "$exe" || { echo "calibrate: no executable in $build_dir" >&2; exit 1; }
command -v ffmpeg > /dev/null || { echo "calibrate: ffmpeg missing" >&2; exit 1; }
if pgrep -fl "UnrealEditor|GranTheftPraline" > /dev/null; then
  echo "calibrate: an Unreal or game process is already running" >&2; exit 1
fi
mkdir -p "$out"

# Mean luma of the last still of a run, 0-255 (area downscale to one grey pixel).
luma() {
  local img
  img="$(ls "$1"/frame_*.jpg 2>/dev/null | tail -n 1 || true)"
  test -n "$img" || { echo "nan"; return; }
  ffmpeg -v error -i "$img" -vf "scale=1:1:flags=area,format=gray" -f rawvideo - | od -An -tu1 | tr -d ' \n'
}

shot() {  # name, args...
  local name="$1"; shift
  local dir="$out/$name"
  mkdir -p "$dir"
  caffeinate -dimsu "$exe" -SkipTitle "$@" -EnvReport="$dir/status.txt" \
    -CaptureVideo=1 -CaptureFps=1 -CaptureDelay=20 -CaptureDir="$dir" -ExitAfter=26 \
    > "$dir/stdout.log" 2>&1 || echo "  $name: exit status $?"
  luma "$dir"
}

table="$out/calibration.md"
{
  echo "# Cloud shadow calibration $(date +%Y-%m-%d\ %H:%M)"
  echo
  echo "Build: $build_dir ; tolerance ±$tolerance ; luminance = mean luma of the still, 0-255"
  echo
  echo "| Ambience | Sky | Mean | Strength | Luminance | Gap | Light function |"
  echo "| --- | --- | --- | --- | --- | --- | --- |"
} > "$table"
picks=""
for ambience in $ambiences; do
  echo "== $ambience: reference"
  ref="$(shot "$ambience-volumetric" -Ambience="$ambience" -Sky=volumetric)"
  echo "| $ambience | volumetric | | | $ref | 0 | off |" >> "$table"
  best=""; best_gap=9999
  for mean in $means; do
    for strength in $strengths; do
      name="$ambience-lf-m$mean-s$strength"
      echo "== $name"
      value="$(shot "$name" -Ambience="$ambience" -Sky=lightfunction \
        -GfxCVars="gtp.CloudShadowLF.Mean=$mean,gtp.CloudShadowLF.Strength=$strength")"
      state="$(sed -n 's/^cloud_shadow_light_function=//p' "$out/$name/status.txt" 2>/dev/null || true)"
      gap="$(awk -v a="$value" -v b="$ref" 'BEGIN{ if (a=="nan"||b=="nan") print "nan"; else printf "%+d", a-b }')"
      echo "| $ambience | lightfunction | $mean | $strength | $value | $gap | ${state:-no status} |" >> "$table"
      abs="$(awk -v g="$gap" 'BEGIN{ if (g=="nan") print 9999; else print (g<0?-g:g) }')"
      if test "$abs" -lt "$best_gap"; then best_gap="$abs"; best="Mean=$mean Strength=$strength"; fi
    done
  done
  verdict="outside ±$tolerance"
  test "$best_gap" -le "$tolerance" && verdict="within ±$tolerance"
  picks="$picks
- $ambience: $best (gap $best_gap, $verdict)"
done
{
  echo
  echo "## Pick (CloudShadowMean and CloudShadowStrength of the weather preset in DefaultGame.ini)"
  echo "$picks"
} >> "$table"
cat "$table"
