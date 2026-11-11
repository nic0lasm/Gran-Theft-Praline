#!/usr/bin/env bash
# One Mac slot for the leafy street trees: what 10 and 112 leafy trees cost on M1,
# as they come from the pack, lightened, and as impostors, against the bare trees.
# Built on tools/perf/bench.sh (same route, same duration, load guard).
#
#   1. (AUDIT=1) audit the Fab packs (triangles per LOD, Nanite, textures over 2K)
#                                         tools/assets/unreal/audit_meshes.py
#   2. rebuild the leafy meshes for M1 (no Nanite, decreasing authored LODs, billboard
#      last) as copies under <pack>/M1/  tools/assets/unreal/prepare_trees_m1.py
#   3. pick the leafy meshes, map loaded read only (never saved)
#                                         tools/city/unreal/swap_street_trees.py
#   3. package (Shipping) with the leafy mesh folders cooked: added to
#      Config/DefaultGame.ini for the package only, the file is restored after
#                                         tools/build/package-mac.sh
#   4. bench, one game argument set per case (UGTPStreetTreesSubsystem swaps the
#      trees at run time with -TreeSwap, the map file never changes):
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
#   GTP_TREE_MESH, GTP_SAPLING_MESH, GTP_TREE_PATTERN (default /M1/, the rebuilt copies; see swap_street_trees.py;
#     default: the lightest beech or alder at least 8 m tall)
#   BASE_ARGS game arguments of every case (default -Sky=lightfunction, as the
#     "cumul-lf" reference of tools/perf/slot1-cumul-mac.sh)
#   SCENARIO (default street-demo), CASES (default "bare light10 light impostor leafy")
#   MAX_DELTA_10_MS (default 2), SKIP_PACKAGE=1 reuses a build packaged by this
#     script (the pick step still runs, it gives -TreeSwap)
#   MAP_MD5: md5 the city map must have before and after (default: none checked,
#     the md5 at start is compared with the md5 at the end anyway)
#   DRY_RUN=1 replaces Unreal, the package and the benches by stubs that write
#     fake outputs: checks the script end to end anywhere (Linux too), in seconds
#     (DRY_SLEEP stub length, EXPECTED_S forces every expected length: tests the watchdog)
#
# Slot rules: every step has an expected length; a step running past twice that
# length is killed, every Unreal and game process is stopped, the report says
# why and the script exits 3 ("Mac libre"). Every 10 minutes (HEARTBEAT_S) a
# "heartbeat" line with the step and its elapsed time goes to stdout and to
# <out>/heartbeat.txt, for the session that relays it. Times are Paris time.
# Output: <BUILDS_DIR>/<version>/trees-<stamp>/TREES.md, mesh_audit.csv, one log per step.
# Expected length: rebuild + pick ~12 min (audit +10 min), package 15-30 min, benches ~18 min (about 1 h at most).
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

dry="${DRY_RUN:-0}"
heartbeat_s="${HEARTBEAT_S:-600}"
paris() { TZ=Europe/Paris date '+%H:%M'; }

if test "$dry" != "1"; then
  if pgrep -fl "UnrealEditor|GranTheftPraline" | grep -v UnrealEditorServices >/dev/null; then
    echo "tree-bench: an Unreal or game process is already running, stop it first" >&2
    exit 1
  fi
  test -x "$editor" || { echo "tree-bench: no Unreal Editor at $editor" >&2; exit 1; }
fi

# Editor scripts run through Content/Python/gtp_run.py: the clone path has a space and
# nothing is copied outside the project.
test "$dry" = "1" || test -f "$repo_root/GranTheftPraline/Content/Python/gtp_run.py" \
  || { echo "tree-bench: Content/Python/gtp_run.py missing" >&2; exit 1; }

{
  echo "# Leafy street trees $version, $(TZ=Europe/Paris date '+%Y-%m-%d %H:%M') heure de Paris, commit $(git rev-parse --short HEAD)$( test "$dry" = 1 && echo ', DRY RUN (stubs, numbers are fake)')"
  echo
  echo "Profile m1, scenario $scenario, ${duration} s per run, every case with: ${BASE_ARGS--Sky=lightfunction}. Compare runs of this pass with each other only."
  echo
} > "$report"

# Expected length of each step in seconds; past twice that, the step is stopped.
expected() {
  if test -n "${EXPECTED_S:-}"; then echo "$EXPECTED_S"; return; fi
  case "$1" in
    audit) echo 480 ;;
    pick) echo 420 ;;
    prepare) echo 600 ;;
    package) echo 1800 ;;
    bench_*) echo $(( duration + 120 )) ;;
    *) echo 600 ;;
  esac
}

stop_unreal() {
  test "$dry" = "1" && return 0
  pkill -f "MacOS/UnrealEditor( |$)" 2>/dev/null || true
  pkill -f "GranTheftPraline.app" 2>/dev/null || true
  pkill -f "UnrealBuildTool|ShaderCompileWorker" 2>/dev/null || true
}

step() {
  local name="$1"
  shift
  local start limit pid status=0 next_beat
  start=$(date +%s)
  limit=$(( 2 * $(expected "$name") ))
  next_beat=$(( start + heartbeat_s ))
  echo "=== $name ($(paris), stops after $(( limit / 60 )) min)"
  "$@" > "$out/$name.log" 2>&1 &
  pid=$!
  while kill -0 "$pid" 2>/dev/null; do
    sleep 2
    now=$(date +%s)
    if test $(( now - start )) -gt "$limit"; then
      kill "$pid" 2>/dev/null; sleep 2; kill -9 "$pid" 2>/dev/null
      stop_unreal
      {
        echo "- $name: STOPPED by the watchdog after $(( (now - start) / 60 )) min (expected $(( $(expected "$name") / 60 )) min), $(paris)"
        echo "- Mac libre: every Unreal process stopped."
      } >> "$report"
      echo "heartbeat $(paris) STOPPED $name, Mac libre" | tee -a "$out/heartbeat.txt"
      exit 3
    fi
    if test "$now" -ge "$next_beat"; then
      echo "heartbeat $(paris) step $name, $(( (now - start) / 60 )) min of at most $(( limit / 60 ))" | tee -a "$out/heartbeat.txt"
      next_beat=$(( now + heartbeat_s ))
    fi
  done
  wait "$pid" || status=$?
  echo "- $name: $( test $status -eq 0 && echo OK || echo "FAILED ($status)" ), $(( $(date +%s) - start )) s, ended $(paris), log $name.log" >> "$report"
  return $status
}

run_py() {
  GTP_PY_SCRIPT="$repo_root/$1" "$editor" "$project" -unattended -nosplash -nosound -NSAppSleepDisabled YES \
    "-ExecutePythonScript=__import__('gtp_run').main()"
}

if test "$dry" = "1"; then
  # Stubs: same files as the real tools, fake numbers.
  run_py() {
    sleep "${DRY_SLEEP:-1}"
    mkdir -p "$saved"
    case "$1" in
      *audit_meshes.py) echo '{"roots": {}, "meshes": []}' > "${GTP_AUDIT_OUT:-$saved/mesh_audit.json}" ;;
      *prepare_trees_m1.py) echo '{"errors": [], "meshes": [{"mesh": "/Game/BlackAlder/M1/SM_Stub_M1", "triangles": [5355, 1803, 913, 8]}]}' > "$saved/trees_m1_report.json" ;;
      *swap_street_trees.py) cat > "$saved/trees_report.json" <<JSON
{"mode": "$GTP_TREE_MODE", "errors": [], "swap_arg": "/Game/Environment/Props/W14-63/SM_Bare~/Game/BlackAlder/Geometry/SimpleWind/SM_Stub+/Game/Environment/Props/W14-64/SM_Young~/Game/BlackAlder/Geometry/SimpleWind/SM_StubSapling", "cook_dirs": ["/Game/BlackAlder/Geometry/SimpleWind"], "picked": {"W14-63": {"bare": "/Game/Environment/Props/W14-63/SM_Bare", "mesh": "/Game/BlackAlder/SM_Stub", "trees": 104, "mesh_info": {"height_cm": 1200, "nanite": false, "triangles": [9000, 3000, 8]}}, "W14-64": {"bare": "/Game/Environment/Props/W14-64/SM_Young", "mesh": "/Game/BlackAlder/SM_StubSapling", "trees": 8, "mesh_info": {"height_cm": 400, "nanite": false, "triangles": [2000, 600, 8]}}}}
JSON
      ;;
    esac
  }
  package_cmd() { sleep "${DRY_SLEEP:-1}"; grep -c BlackAlder "$game_ini"; mkdir -p "$build_dir/GranTheftPraline.app"; }
  bench_cmd() {
    sleep "${DRY_SLEEP:-1}"
    mkdir -p "$2"
    local p=40.0
    case "$EXTRA_ARGS" in *Trees=light*TreeCount=10*) p=40.6 ;; *Trees=light*) p=42.9 ;; *Trees=impostor*) p=40.4 ;; *Trees=leafy*) p=49.0 ;; esac
    printf '| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | %% <= 33.3 ms | Hitches > 100 ms |\n| ---- |\n| %s | base | 30.0 | 38.0 | 31.0 | %s | 70 %% | 1 |\n' "$SCENARIOS" "$p" > "$2/summary.md"
  }
else
  package_cmd() { tools/build/package-mac.sh "$@"; }
  bench_cmd() { tools/perf/bench.sh "$@"; }
fi

map_file="$repo_root/GranTheftPraline/Content/Maps/L_City_RueDeLaRepublique.umap"
map_md5() { if test "$dry" = "1"; then echo dry; elif command -v md5 >/dev/null; then md5 -q "$map_file"; else md5sum "$map_file" | cut -d' ' -f1; fi; }
md5_start="$(map_md5)"
if test -n "${MAP_MD5:-}" && test "$md5_start" != "$MAP_MD5"; then
  echo "- map md5 $md5_start is not the expected $MAP_MD5: stopped before anything ran" >> "$report"
  echo "tree-bench: map md5 $md5_start, expected $MAP_MD5" >&2
  exit 1
fi
echo "- map md5 at start: $md5_start" >> "$report"
ini_md5() { if command -v md5 >/dev/null; then md5 -q "$repo_root/GranTheftPraline/Config/DefaultGame.ini"; else md5sum "$repo_root/GranTheftPraline/Config/DefaultGame.ini" | cut -d' ' -f1; fi; }
ini_md5_start="$(ini_md5)"

pick() {
  GTP_TREE_MODE=pick GTP_TREE_PATTERN="${GTP_TREE_PATTERN:-/M1/}" run_py tools/city/unreal/swap_street_trees.py || return 1
  cp "$saved/trees_report.json" "$out/trees_pick.json"
  grep -q '"errors": \[\]' "$saved/trees_report.json"
}
json() { python3 -c 'import json,sys; v=json.load(open(sys.argv[1]))[sys.argv[2]]; print(" ".join(v) if isinstance(v, list) else v)' "$out/trees_pick.json" "$1"; }

# The leafy meshes are referenced by no map: cook their folders, for this package only.
game_ini="$repo_root/GranTheftPraline/Config/DefaultGame.ini"
ini_backup="$out/DefaultGame.ini.orig"
restore_ini() { test -f "$ini_backup" && cp "$ini_backup" "$game_ini" && rm -f "$ini_backup"; return 0; }
trap 'restore_ini' EXIT
cook_package() {
  cp "$game_ini" "$ini_backup"
  local lines="" d
  for d in $(json cook_dirs); do lines="$lines+DirectoriesToAlwaysCook=(Path=\"$d\")\n"; done
  python3 - "$game_ini" "$lines" <<'PY'
import sys
path, lines = sys.argv[1], sys.argv[2].replace("\\n", "\n")
s = open(path).read()
key = "[/Script/UnrealEd.ProjectPackagingSettings]\n"
assert key in s, "no packaging section in DefaultGame.ini"
open(path, "w").write(s.replace(key, key + lines, 1))
PY
  package_cmd "$version"
  local status=$?
  restore_ini
  return $status
}

if test "${AUDIT:-0}" = "1"; then
  GTP_AUDIT_OUT="$out/mesh_audit.json" step audit run_py tools/assets/unreal/audit_meshes.py
fi
prepare() {
  run_py tools/assets/unreal/prepare_trees_m1.py || return 1
  cp "$saved/trees_m1_report.json" "$out/trees_m1.json"
  grep -q '"errors": \[\]' "$saved/trees_m1_report.json"
}
step prepare prepare || { echo "- M1 rebuild of the tree meshes failed, see trees_m1.json" >> "$report"; exit 1; }
step pick pick || { echo "- pick failed, see trees_pick.json" >> "$report"; exit 1; }
swap_arg="$(json swap_arg)"
echo "- trees swapped at run time: -TreeSwap=$swap_arg" >> "$report"
if test "${SKIP_PACKAGE:-0}" != "1"; then
  if ! step package cook_package; then
    grep -hE ": (fatal )?error:|Error: " "$out/package.log" | sort -u | head -n 80 > "$out/compile-errors.txt"
    echo "- package failed: errors in compile-errors.txt" >> "$report"
    exit 1
  fi
fi
test -d "$build_dir/GranTheftPraline.app" || { echo "- no build at $build_dir" >> "$report"; exit 1; }

case_args() {
  case "$1" in
    bare)     echo "-Trees=bare" ;;
    light10)  echo "-Trees=light -TreeCount=10 -TreeSwap=$swap_arg" ;;
    light)    echo "-Trees=light -TreeSwap=$swap_arg" ;;
    impostor) echo "-Trees=impostor -TreeSwap=$swap_arg" ;;
    leafy)    echo "-Trees=leafy -TreeSwap=$swap_arg" ;;
    *) echo "unknown" ;;
  esac
}

# frame p95 of a case, from bench.sh's table (column 7)
p95() { grep '^| ' "$out/$1/summary.md" 2>/dev/null | grep -v -e Scenario -e '| -' | head -n 1 | awk -F'|' '{gsub(/ /,"",$7); print $7}'; }

for c in $cases; do
  if test "$c" != "bare" && test "$c" != "light10" && test -n "${skip_112:-}"; then
    echo "- $c: skipped (light10 too expensive)" >> "$report"
    continue
  fi
  EXTRA_ARGS="${BASE_ARGS--Sky=lightfunction} $(case_args "$c")" SCENARIOS="$scenario" DURATION="$duration" \
    step "bench_$c" bench_cmd "$build_dir" "$out/$c"
  if test "$c" = "light10"; then
    b="$(p95 bare)"; l="$(p95 light10)"
    if test -n "$b" && test -n "$l" && python3 -c 'import sys; sys.exit(0 if float(sys.argv[2]) - float(sys.argv[1]) > float(sys.argv[3]) else 1)' "$b" "$l" "$max_delta_10"; then
      skip_112=1
    fi
  fi
done

md5_end="$(map_md5)"
if test "$md5_end" = "$md5_start"; then
  echo "- map md5 at the end: $md5_end, unchanged" >> "$report"
else
  echo "- WARNING map md5 changed: $md5_start at start, $md5_end at the end" >> "$report"
fi

if test "$(ini_md5)" = "$ini_md5_start"; then
  echo "- DefaultGame.ini md5 at the end: $ini_md5_start, unchanged (cook folders added for the package only, then restored)" >> "$report"
else
  echo "- WARNING DefaultGame.ini md5 changed: $ini_md5_start at start, $(ini_md5) at the end" >> "$report"
fi

# One table: frame p95 per case, its difference to bare and per leafy tree.
trees_total="$(python3 -c 'import json,sys; print(sum(v["trees"] for v in json.load(open(sys.argv[1]))["picked"].values()))' "$out/trees_pick.json" 2>/dev/null || echo 112)"
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
  for c in $cases; do grep '^|' "$out/$c/summary.md" 2>/dev/null; done | awk '/Scenario/ { if (h++) next } /----/ { if (s++) next } { print }'
  echo
  echo "## Meshes picked"
  echo
  python3 -c '
import json,sys
for cid, v in json.load(open(sys.argv[1]))["picked"].items():
    i = v["mesh_info"]
    print("- %s: %s, %d trees, %d cm, Nanite %s, triangles per LOD %s" % (cid, v["mesh"], v["trees"], i["height_cm"], i["nanite"], i["triangles"]))
'  "$out/trees_pick.json" 2>/dev/null || echo "(see trees_pick.json)"
} >> "$report"
echo "Done: $report"
cat "$report"
