#!/usr/bin/env bash
# Bisect the launch freeze of the 3883236 builds (game alive, no frame) between the code and
# the facades-only replays, unattended. Three builds, each followed by a 30 s launch:
#   A  full regeneration, sign atlas off   (code of the head, like 799c8d5 otherwise)
#   B  A then one FACADES_ONLY replay, atlas off   (the replay the bench used)
#   C  full regeneration, sign atlas on
# Read: A freezes -> the code; A runs and B freezes -> the replay; only C freezes -> the atlas.
# Development packages, so the game writes ~/Library/Logs/GranTheftPraline/GranTheftPraline.log.
#
# Mac slot rules (Nico, 10/11): tried dry first (DRY_RUN=1, no Unreal), a time guard per step
# (a step over twice its expected time is stopped and the run ends with its cause), a sign of
# life every 10 min (HEARTBEAT lines on stdout and in heartbeat.log), times in Paris time.
# The map and city content found at the start are restored at the end, checked by md5.
#
# Usage: caffeinate -dimsu tools/city/evidence/freeze_bisect_mac.sh [steps=ABC]
#   PYTHON (venv with numpy, scipy, shapely, PIL), BUILDS_DIR (default <repo>/Builds/freeze-bisect),
#   DRY_RUN=1 (every Unreal step replaced by a short sleep; DRY_SLOW=<step> makes that one hang).
# Dry run outside the Mac: run it from a scratch copy of the repo, never commit what it writes.
# Writes <BUILDS_DIR>/FREEZE.md. One Unreal or game process at a time.
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../.." && pwd)"
steps="${1:-ABC}"
out="${BUILDS_DIR:-$repo/Builds/freeze-bisect}"
dry="${DRY_RUN:-0}"
mkdir -p "$out"
report="$out/FREEZE.md"
paris() { TZ=Europe/Paris date +%H:%M; }
t0=$(date +%s)
echo "# Freeze bisect, started $(paris) Paris time, commit $(git -C "$repo" rev-parse --short HEAD)$(test "$dry" = 1 && echo ', DRY RUN')" > "$report"
if test "$dry" != 1 && pgrep -f "UnrealEditor |UnrealEditor$|GranTheftPraline.app" > /dev/null; then
  echo "an Unreal editor or the game is running: one at a time" >&2
  exit 1
fi

# --- save the map and city content, restore on exit ------------------------------------
proj="$repo/GranTheftPraline"
kept=(Content/Maps/L_City_RueDeLaRepublique.umap Content/Environment/City Content/Environment/Facades/LyonKit
      Content/City Plugins/CityBrands/Content)
backup="$out/backup"
if command -v md5 > /dev/null; then md5r=(md5 -r); else md5r=(md5sum); fi
sums() { (cd "$1" && for k in "${kept[@]}"; do if test -e "$k"; then find "$k" -type f -print0; fi; done \
          | sort -z | xargs -0 -r "${md5r[@]}") ; }
rm -rf "$backup"; mkdir -p "$backup"
for k in "${kept[@]}"; do
  if test -e "$proj/$k"; then mkdir -p "$backup/$(dirname "$k")"; cp -Rp "$proj/$k" "$backup/$k"; fi
done
sums "$proj" > "$out/before.md5"
restore() {
  for k in "${kept[@]}"; do
    rm -rf "${proj:?}/$k"   # also what did not exist at the start
    if test -e "$backup/$k"; then mkdir -p "$proj/$(dirname "$k")"; cp -Rp "$backup/$k" "$proj/$k"; fi
  done
  sums "$proj" > "$out/after.md5"
  if cmp -s "$out/before.md5" "$out/after.md5"; then
    echo "- $(paris) restore: map and city content identical to the start (md5, $(wc -l < "$out/before.md5") files)" >> "$report"
  else
    echo "- $(paris) restore: MISMATCH, see before.md5 / after.md5" >> "$report"
  fi
  git -C "$repo" checkout -- assets/geo/processed 2> /dev/null || true
  echo "== $(paris) Mac libre: $(( ($(date +%s) - t0) / 60 )) min, report $report"
  cat "$report"
}
trap restore EXIT

# --- one guarded step -------------------------------------------------------------------
heartbeat() { echo "HEARTBEAT $(paris) Paris: $1, $(( ($(date +%s) - t0) / 60 )) min since start" | tee -a "$out/heartbeat.log"; }
step() {  # step <name> <expected_s> <log> <command...>
  local name="$1" expected="$2" log="$3"; shift 3
  if test "$dry" = 1; then   # minutes become seconds, so the guard is tried in a few seconds
    expected=$(( expected / 60 > 2 ? expected / 60 : 2 ))
    if test "${DRY_SLOW:-}" = "$name"; then set -- sleep $(( expected * 3 ))
    else set -- sh -c 'echo dry > "$1/Content/Maps/L_City_RueDeLaRepublique.umap"' sh "$proj"; fi
  fi
  echo "== $(paris) $name (expected $(( expected / 60 )) min, stopped at $(( 2 * expected / 60 )) min)"
  local s; s=$(date +%s)
  set -m; "$@" > "$out/$log" 2>&1 & local pid=$!; set +m
  local last_beat="$s"
  while kill -0 "$pid" 2> /dev/null; do
    sleep $(test "$dry" = 1 && echo 1 || echo 5)
    local now; now=$(date +%s)
    if (( now - s > 2 * expected )); then
      kill -TERM -- "-$pid" 2> /dev/null || kill -TERM "$pid" 2> /dev/null || true
      sleep 5; kill -KILL -- "-$pid" 2> /dev/null || true
      pkill -f "UnrealEditor |UnrealEditor$|GranTheftPraline.app|ShaderCompileWorker" 2> /dev/null || true
      echo "- $(paris) STOPPED: $name over twice its expected $(( expected / 60 )) min; last lines of $log:" >> "$report"
      tail -n 15 "$out/$log" | sed 's/^/    /' >> "$report"
      exit 3
    fi
    if (( now - last_beat >= 600 )); then heartbeat "$name running for $(( (now - s) / 60 )) min"; last_beat=$now; fi
  done
  local rc=0; wait "$pid" || rc=$?
  echo "- $(paris) $name: exit $rc in $(( ($(date +%s) - s) / 60 )) min ($log)" >> "$report"
  return "$rc"
}

export PATH="$(dirname "$(command -v "${PYTHON:-python3}")"):$PATH"   # run_city_import.sh calls python3
common=(GTP_SHOP_GLASS=opaque GTP_CITY_DETAILS=0 SKIP_PLAY=1)

launch() {  # launch <tag>: package Development, 30 s launch, verdict
  local v="freeze-$1"
  step "package $1" 900 "$v-package.log" env BUILDS_DIR="$out" "$repo/tools/build/package-mac.sh" "$v" Development
  step "launch $1" 150 "$v-test.log" env BUILDS_DIR="$out" "$repo/tools/build/test-build-mac.sh" "$v" 30 0 || true
  local verdict frames
  verdict="$(grep -m1 '^verdict=' "$out/$v-test.log" || echo "verdict=none")"
  frames="$(grep -m1 '^frame ' "$out/$v-test.log" || echo "no frame times")"
  cp ~/Library/Logs/GranTheftPraline/GranTheftPraline.log "$out/$v-game.log" 2> /dev/null || true
  echo "- **$1: $verdict**; $frames; game log $v-game.log" >> "$report"
  git -C "$repo" checkout -- assets/geo/processed 2> /dev/null || true
}

heartbeat "start, steps $steps"
case "$steps" in *A*|*B*)
  step "regen A" 600 "A-regen.log" env "${common[@]}" GTP_SIGN_ATLAS=0 "$repo/tools/city/regen_city.sh"
  case "$steps" in *A*) launch A ;; esac
  case "$steps" in *B*)
    step "replay B" 300 "B-replay.log" env "${common[@]}" GTP_SIGN_ATLAS=0 FACADES_ONLY=1 GTP_CITY_RIVERSIDE=0 \
      "$repo/tools/city/unreal/run_city_import.sh"
    launch B ;;
  esac ;;
esac
case "$steps" in *C*)
  step "regen C" 600 "C-regen.log" env "${common[@]}" GTP_SIGN_ATLAS=1 "$repo/tools/city/regen_city.sh"
  launch C ;;
esac
