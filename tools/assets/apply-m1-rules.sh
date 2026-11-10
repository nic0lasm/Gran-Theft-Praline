#!/usr/bin/env bash
# Apply the M1 import rules (tighter M1 LOD0 caps by size, textures 512/1K/2K, no 4K) to the
# whole prop catalogue, in one command, on the Mac.
#   tools/assets/apply-m1-rules.sh prepare   # CPU only, no Unreal: re-prepares every wave (nice 10)
#   tools/assets/apply-m1-rules.sh import    # Unreal commandlet, one pass per wave: needs "Mac a toi"
#   tools/assets/apply-m1-rules.sh all       # prepare then import
# Assets whose settings are unchanged are skipped by prepare_props.py; changed rules change the
# settings key, so every asset is redone once. Hand-tuned assets (explicit m1_tris / texture_cap
# in the spec) keep their values. Glass ids still need GTP_GLASS_IDS (see import_props.py).
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
MODE="${1:-prepare}"
G="python3 $REPO/tools/common/guard.py"
# Planned: prepare ~3 min per asset; import ~70 s per asset + 90 s start. Limits are 2x that.
N_IDS=$(echo ${GTP_IDS:-x x x x x x x x x x x x x x x x x x x x} | wc -w)
WAVES="${GTP_WAVES:-w1 w6 w8 w13 w14}"
# First wave of the rollout: only the props placed in the street. GTP_IDS="W1-01 W1-07 ..." limits
# both prepare and import to those ids (ids of other waves are ignored by prepare_props.py).
IDS="${GTP_IDS:-}"
LOG="${GTP_LOG_DIR:-$REPO/assets/.m1-rules-logs}"
mkdir -p "$LOG"

manifest_for() {
  case "$1" in w1) echo wave-01;; w2) echo wave-02;; w6) echo wave-06;; w8) echo wave-08;; w13) echo wave-13;; w14) echo wave-14;; esac
}

prepare_all() {
  for w in $WAVES; do
    echo "[m1-rules] prepare $w"
    $G --name "prepare-$w" --max $((N_IDS * 360 + 120)) -- nice -n 10 python3 "$REPO/tools/assets/prepare_props.py" \
      --manifest "$REPO/assets/manifests/$(manifest_for "$w").json" \
      --masters "$REPO/assets/tripo/$w" --staging "$REPO/assets/staging/$w" ${IDS:+--ids $IDS} \
      2>&1 | tee "$LOG/prepare-$w.log"
  done
}

import_all() {
  if pgrep -x "UnrealEditor" >/dev/null || pgrep -x "UnrealEditor-Cmd" >/dev/null || pgrep -f "GranTheftPraline-Mac" >/dev/null; then
    echo "[m1-rules] an Unreal editor or the game is running: stop." >&2
    return 3
  fi
  for w in $WAVES; do
    echo "[m1-rules] import $w"
    $G --name "import-$w" --max $((N_IDS * 140 + 180)) -- caffeinate -dimsu nice -n 10 "$REPO/tools/assets/import-props-mac.sh" "$w" $IDS 2>&1 | tee "$LOG/import-$w.log"
  done
}

case "$MODE" in
  prepare) prepare_all;;
  import) import_all;;
  all) prepare_all; import_all;;
  *) echo "usage: $0 prepare|import|all" >&2; exit 2;;
esac
echo "[m1-rules] done: $MODE"
