#!/usr/bin/env bash
# Before/after evidence of a street dressing change, unattended, from two packaged builds.
# Runs tools/build/test-build-mac.sh (demo route, m1 profile, the game films itself) on the
# AFTER build unless it already has evidence, then writes side-by-side stills of the BEFORE
# and AFTER captures at the same times, plus both frame-time summaries, into one folder.
#
# Usage: tools/city/evidence/before_after.sh <before_builds_dir> <after_builds_dir> <version> [soak_s=180] [video_s=90]
#   <x_builds_dir>/<version>/GranTheftPraline.app, evidence in <x_builds_dir>/<version>/evidence/<stamp>/
#   BEFORE_EVIDENCE / AFTER_EVIDENCE: use these evidence folders instead of the latest ones.
#   RUN_AFTER=0: do not run the after build, take its latest evidence.
# Output: <after_builds_dir>/<version>/evidence/before-after-<stamp>/ (compare_t*.jpg, before.txt, after.txt).
# One Unreal or game process at a time: refuses to start if one is running. Long: run it under
# caffeinate -dimsu.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../../.." && pwd)"
before_dir="${1:?before builds dir}"
after_dir="${2:?after builds dir}"
version="${3:?version, e.g. 0.2}"
soak="${4:-180}"
video="${5:-90}"

if pgrep -fl "UnrealEditor|GranTheftPraline" | grep -v UnrealEditorServices >/dev/null; then
  echo "before_after: an Unreal editor or the game is running; one at a time" >&2
  exit 1
fi

latest() {  # latest evidence folder that holds a capture
  find "$1/$version/evidence" -mindepth 2 -maxdepth 2 -name capture.mov -print 2> /dev/null | sort | tail -n 1 | xargs -I{} dirname {}
}

if test "${RUN_AFTER:-1}" = "1" && test -z "${AFTER_EVIDENCE:-}"; then
  BUILDS_DIR="$after_dir" "$repo/tools/build/test-build-mac.sh" "$version" "$soak" "$video"
fi
before_ev="${BEFORE_EVIDENCE:-$(latest "$before_dir")}"
after_ev="${AFTER_EVIDENCE:-$(latest "$after_dir")}"
test -n "$before_ev" && test -f "$before_ev/capture.mov" || { echo "before_after: no before capture" >&2; exit 1; }
test -n "$after_ev" && test -f "$after_ev/capture.mov" || { echo "before_after: no after capture" >&2; exit 1; }

out="$after_dir/$version/evidence/before-after-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$out"
swift "$here/compare-stills.swift" "$before_ev/capture.mov" "$after_ev/capture.mov" "$out" 10 1280
for side in before after; do
  ev="$before_ev"
  test "$side" = "after" && ev="$after_ev"
  { echo "evidence: $(basename "$ev")"; cat "$ev"/frametimes.summary.txt 2> /dev/null || echo "no frame-time summary"; } > "$out/$side.txt"
done
echo "before_after: $out"
cat "$out/before.txt" "$out/after.txt"
