#!/usr/bin/env bash
# One Mac slot for the leafy street trees: what 10 and 112 leafy trees cost on M1,
# as they come from the pack, lightened, and as impostors, against the bare trees.
# Built on tools/perf/bench.sh (same route, same duration, load guard).
#
#   1. audit the Fab packs (triangles per LOD, Nanite, textures over 2K)
#                                         tools/assets/unreal/audit_meshes.py
#   2. stage a leafy copy of the trees on the city map (hidden by default)
#                                         tools/city/unreal/swap_street_trees.py
#   3. package (Shipping)                 tools/build/package-mac.sh
#   4. take the leafy copy off the map again (the next package is unchanged)
#   5. bench, one game argument set per case (UGTPStreetTreesSubsystem):
#        bare      -Trees=bare
#        light10   -Trees=light -TreeCount=10
#        light     -Trees=light            (all 112)
#        impostor  -Trees=impostor
#        leafy     -Trees=leafy            (the pack as it comes)
#      the 112-tree cases run only if light10 costs at most MAX_DELTA_10_MS over bare.
#
# Usage, from the root of the clone, nothing else running, after the coordinator's go:
#   caffeinate -dimsu tools/perf/tree-bench-mac.sh [version=perf-trees] [duration_s=90]
# Environment:
#   UE_ROOT (default /Users/Shared/Epic Games/UE_5.8), BUILDS_DIR (default <repo>/Builds)
#   GTP_TREE_MESH, GTP_SAPLING_MESH, GTP_TREE_PATTERN (see swap_street_trees.py;
#     default: the lightest beech or alder at least 8 m tall)
#   SCENARIO (default street-demo), CASES (default "bare light10 light impostor leafy")
#   MAX_DELTA_10_MS (default 2), SKIP_PACKAGE=1 reuses a build already staged,
#   KEEP_STAGED=1 leaves the leafy copy on the map
# Output: <BUILDS_DIR>/<version>/trees-<stamp>/TREES.md, mesh_audit.csv, one log per step.
# Expected length: audit + stage + unstage ~10 min, package 15-25 min, benches ~12 min.
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
version="${1:-perf-trees}"
duration="${2:-90}"
builds_dir="${BUILDS_DIR:-$repo_root/Builds}"
export BUILDS_DIR="$builds_dir"
build_dir="$builds_dir/$version"
stamp="$(date +%Y%m%d-%H%M%S)"
out="$build_dir/trees-$stamp"
scenario="${SCENARIO:-street-demo}"
cases="${CASES:-bare light10 light impostor leafy}"
max_delta_10="${MAX_DELTA_10_MS:-2}"
project="$repo_root/GranTheftPraline/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$ue_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
saved="$repo_root/GranTheftPraline/Saved/CityImport"
mkdir -p "$out"
report="$out/TREES.md"
cd "$repo_root" || exit 1

if pgrep -fl "UnrealEditor|GranTheftPraline" | grep -v UnrealEditorServices >/dev/null; then
  echo "tree-bench: an Unreal or game process is already running, stop it first" >&2
  exit 1
fi
test -x "$editor" || { echo "tree-bench: no Unreal Editor at $editor" >&2; exit 1; }

# The editor runs scripts from a path without spaces ("Claude work" has one).
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cp tools/assets/unreal/audit_meshes.py tools/city/unreal/swap_street_trees.py "$tmp/"

{
  echo "# Leafy street trees $version, $(date '+%Y-%m-%d %H:%M %Z'), commit $(git rev-parse --short HEAD)"
  echo
  echo "Profile m1, scenario $scenario, ${duration} s per run. Compare runs of this pass with each other only."
  echo
} > "$report"

step() {
  local name="$1"
  shift
  local start status
  start=$(date +%s)
  echo "=== $name"
  "$@" > "$out/$name.log" 2>&1
  status=$?
  echo "- $name: $( test $status -eq 0 && echo OK || echo "FAILED ($status)" ), $(( $(date +%s) - start )) s, log $name.log" >> "$report"
  return $status
}

run_py() {
  "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES -ExecutePythonScript="$tmp/$1"
}

trees() {
  GTP_TREE_MODE="$1" run_py swap_street_trees.py || return 1
  cp "$saved/trees_report.json" "$out/trees_$1.json"
  grep -q '"errors": \[\]' "$saved/trees_report.json"
}

GTP_AUDIT_OUT="$out/mesh_audit.json" step audit run_py audit_meshes.py
if test "${SKIP_PACKAGE:-0}" != "1"; then
  step stage trees stage || { echo "- staging failed, see trees_stage.json" >> "$report"; exit 1; }
  if ! step package tools/build/package-mac.sh "$version"; then
    grep -hE ": (fatal )?error:|Error: " "$out/package.log" | sort -u | head -n 80 > "$out/compile-errors.txt"
    echo "- package failed: errors in compile-errors.txt" >> "$report"
    test "${KEEP_STAGED:-0}" = "1" || step unstage trees unstage
    exit 1
  fi
  test "${KEEP_STAGED:-0}" = "1" || step unstage trees unstage
fi
test -d "$build_dir/GranTheftPraline.app" || { echo "- no build at $build_dir" >> "$report"; exit 1; }

case_args() {
  case "$1" in
    bare)     echo "-Trees=bare" ;;
    light10)  echo "-Trees=light -TreeCount=10" ;;
    light)    echo "-Trees=light" ;;
    impostor) echo "-Trees=impostor" ;;
    leafy)    echo "-Trees=leafy" ;;
    *) echo "unknown" ;;
  esac
}

# frame p95 of a case, from bench.sh's table (column 7)
p95() { grep '^| ' "$out/$1/summary.md" 2>/dev/null | grep -v Scenario | head -n 1 | awk -F'|' '{gsub(/ /,"",$7); print $7}'; }

for c in $cases; do
  if test "$c" != "bare" && test "$c" != "light10" && test -n "${skip_112:-}"; then
    echo "- $c: skipped (light10 too expensive)" >> "$report"
    continue
  fi
  EXTRA_ARGS="$(case_args "$c")" SCENARIOS="$scenario" DURATION="$duration" \
    step "bench_$c" tools/perf/bench.sh "$build_dir" "$out/$c"
  if test "$c" = "light10"; then
    b="$(p95 bare)"; l="$(p95 light10)"
    if test -n "$b" && test -n "$l" && python3 -c 'import sys; sys.exit(0 if float(sys.argv[2]) - float(sys.argv[1]) > float(sys.argv[3]) else 1)' "$b" "$l" "$max_delta_10"; then
      skip_112=1
    fi
  fi
done

# One table: frame p95 per case, its difference to bare and per leafy tree.
trees_total="$(python3 -c 'import json,sys; print(sum(v["trees"] for v in json.load(open(sys.argv[1]))["staged"].values()))' "$out/trees_stage.json" 2>/dev/null || echo 112)"
{
  echo
  echo "## Frame p95 per case ($trees_total trees on the street)"
  echo
  echo "| Case | Game arguments | Frame p95 (ms) | vs bare (ms) | per leafy tree (ms) |"
  echo "| ---- | ---- | ---- | ---- | ---- |"
  b="$(p95 bare)"
  for c in $cases; do
    v="$(p95 "$c")"
    n="$trees_total"; test "$c" = "light10" && n=10
    python3 - "$c" "$(case_args "$c")" "${v:-}" "${b:-}" "$n" <<'PY'
import sys
c, args, v, b, n = sys.argv[1:]
if not v:
    print("| %s | %s | no data | | |" % (c, args)); raise SystemExit
d = float(v) - float(b) if b else float("nan")
per = d / int(n) if c != "bare" and b else float("nan")
print("| %s | %s | %.1f | %s | %s |" % (c, args, float(v), "" if c == "bare" else "%+.1f" % d,
                                       "" if c == "bare" else "%.3f" % per))
PY
  done
  echo
  echo "## Tables of bench.sh"
  echo
  for c in $cases; do grep '^|' "$out/$c/summary.md" 2>/dev/null; done | awk 'NR==1 || !/Scenario/'
  echo
  echo "## Meshes picked"
  echo
  python3 -c '
import json,sys
for cid, v in json.load(open(sys.argv[1]))["staged"].items():
    i = v["mesh_info"]
    print("- %s: %s, %d trees, %d cm, Nanite %s, triangles per LOD %s" % (cid, v["mesh"], v["trees"], i["height_cm"], i["nanite"], i["triangles"]))
' "$out/trees_stage.json" 2>/dev/null || echo "(see trees_stage.json)"
} >> "$report"
echo "Done: $report"
cat "$report"
