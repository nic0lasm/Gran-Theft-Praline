#!/usr/bin/env bash
# Package Gran Theft Praline for macOS into Builds/<version>/GranTheftPraline.app.
#
# Usage: tools/build/package-mac.sh [version] [configuration]
#   version        default: ProjectVersion from Config/DefaultGame.ini, as major.minor (0.1.0 -> 0.1)
#   configuration  Shipping (default), Development or Test
#
# Environment:
#   UE_ROOT        Unreal Engine install (default: /Users/Shared/Epic Games/UE_5.8)
#   BUILDS_DIR     output root (default: <repo>/Builds)
#   STALL_MINUTES  kill BuildCookRun when its log has not grown for this long
#                  (default 25; the cook can hang at the very end when the
#                  local Zen server stops)
#   MAX_MINUTES    hard limit for one BuildCookRun attempt (default 240)
#   RETRIES        new attempts after a stall or time-out (default 1; the cook
#                  is incremental, so a retry resumes most of the work)
#
# The result is signed ad hoc so it opens with a double-click on this Mac.
# Logs: Builds/<version>/package.log
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
project_dir="$repo_root/GranTheftPraline"
project_file="$project_dir/GranTheftPraline.uproject"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
run_uat="$ue_root/Engine/Build/BatchFiles/RunUAT.sh"

die() {
  echo "package-mac: $*" >&2
  exit 1
}

test "$(uname -s)" = "Darwin" || die "macOS only"
test -f "$project_file" || die "project not found: $project_file"
test -x "$run_uat" || die "RunUAT not found: $run_uat (set UE_ROOT)"

project_version="$(sed -n 's/^ProjectVersion=//p' "$project_dir/Config/DefaultGame.ini" | head -n 1 | tr -d '\r')"
default_version="$(printf '%s' "$project_version" | cut -d. -f1-2)"
version="${1:-$default_version}"
config="${2:-Shipping}"
test -n "$version" || die "no version given and no ProjectVersion in DefaultGame.ini"
case "$config" in
  Shipping|Development|Test) ;;
  *) die "unknown configuration '$config'" ;;
esac

builds_dir="${BUILDS_DIR:-$repo_root/Builds}"
out_dir="$builds_dir/$version"
staging_dir="$out_dir/.staging"
app_path="$out_dir/GranTheftPraline.app"
log_file="$out_dir/package.log"

mkdir -p "$out_dir"
rm -rf "$staging_dir"

stall_minutes="${STALL_MINUTES:-25}"
max_minutes="${MAX_MINUTES:-240}"
retries="${RETRIES:-1}"
bundle_id="$(sed -n 's/^BundleIdentifier=//p' "$project_dir/Config/DefaultEngine.ini" | head -n 1 | tr -d '\r')"

echo "Packaging version $version ($config) into $out_dir"
echo "Engine: $ue_root"
echo "Full log: $log_file (watchdog: ${stall_minutes} min without output, ${max_minutes} min max, ${retries} retry)"

# Kill a process and all its descendants (macOS has no setsid or timeout).
kill_tree() {
  local pid="$1" child
  for child in $(pgrep -P "$pid" 2>/dev/null); do
    kill_tree "$child"
  done
  kill -TERM "$pid" 2>/dev/null || true
}

file_size() {
  stat -f %z "$1" 2>/dev/null || echo 0
}

# One BuildCookRun attempt under a watchdog.
# Returns 0 on success, 1 on failure, 2 on a stall or time-out.
run_build_cook() {
  local attempt_log="$1" pid last_size size last_change now started
  "$run_uat" BuildCookRun \
    -project="$project_file" \
    -platform=Mac \
    -clientconfig="$config" \
    -build -cook -stage -pak -iostore -package -archive \
    -archivedirectory="$staging_dir" \
    -compressed -prereqs -nop4 -utf8output -unattended -nocompileeditor \
    > "$attempt_log" 2>&1 &
  pid=$!
  started="$(date +%s)"
  last_change="$started"
  last_size=0
  while kill -0 "$pid" 2>/dev/null; do
    sleep 30
    now="$(date +%s)"
    size="$(file_size "$attempt_log")"
    if test "$size" != "$last_size"; then
      last_size="$size"
      last_change="$now"
    fi
    if test $(( now - last_change )) -ge $(( stall_minutes * 60 )); then
      echo "package-mac: no output for ${stall_minutes} min, stopping BuildCookRun" | tee -a "$attempt_log" >&2
      kill_tree "$pid"
      wait "$pid" 2>/dev/null || true
      return 2
    fi
    if test $(( now - started )) -ge $(( max_minutes * 60 )); then
      echo "package-mac: BuildCookRun over ${max_minutes} min, stopping it" | tee -a "$attempt_log" >&2
      kill_tree "$pid"
      wait "$pid" 2>/dev/null || true
      return 2
    fi
  done
  if wait "$pid"; then
    return 0
  fi
  return 1
}

start_time="$(date +%s)"
: > "$log_file"
attempt=0
while :; do
  attempt=$(( attempt + 1 ))
  attempt_log="$out_dir/package-attempt$attempt.log"
  rm -rf "$staging_dir"
  status=0
  run_build_cook "$attempt_log" || status=$?
  cat "$attempt_log" >> "$log_file"
  rm -f "$attempt_log"
  if test "$status" -eq 0; then
    break
  fi
  if test "$status" -eq 2 && test "$attempt" -le "$retries"; then
    echo "Attempt $attempt stalled; retrying (the cook resumes from what is done)." >&2
    continue
  fi
  echo "BuildCookRun failed (attempt $attempt). Last errors:" >&2
  grep -E "Error|error:" "$log_file" | tail -n 30 >&2 || true
  die "see $log_file"
done

built_app="$(find "$staging_dir" -maxdepth 3 -name '*.app' -type d | head -n 1)"
test -n "$built_app" || die "no .app found under $staging_dir"

rm -rf "$app_path"
mv "$built_app" "$app_path"
rm -rf "$staging_dir"

# App identity and version. The engine takes them from the project settings;
# set them here as well so a stale Info.plist template can never ship the
# placeholder identifier.
plist="$app_path/Contents/Info.plist"
test -f "$plist" || die "no Info.plist in $app_path"
plist_set() {
  /usr/libexec/PlistBuddy -c "Set :$1 $2" "$plist" 2>/dev/null \
    || /usr/libexec/PlistBuddy -c "Add :$1 string $2" "$plist" \
    || die "cannot set $1 in Info.plist"
}
if test -n "$bundle_id"; then
  plist_set CFBundleIdentifier "$bundle_id"
fi
if test -n "$project_version"; then
  plist_set CFBundleShortVersionString "$project_version"
fi
shipped_id="$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$plist")"
case "$shipped_id" in
  *YourCompany*) die "placeholder bundle identifier still in Info.plist: $shipped_id" ;;
esac

codesign --force --deep --sign - "$app_path" >> "$log_file" 2>&1 || die "ad hoc signing failed, see $log_file"
codesign --verify --deep "$app_path" || die "signature check failed"

elapsed=$(( $(date +%s) - start_time ))
size="$(du -sh "$app_path" | cut -f1)"
commit="$(git -C "$repo_root" rev-parse --short HEAD 2>/dev/null || echo unknown)"

cat > "$out_dir/build-info.txt" <<INFO
version=$version
configuration=$config
commit=$commit
bundle_id=$shipped_id
attempts=$attempt
built=$(date -u +%Y-%m-%dT%H:%M:%SZ)
duration_s=$elapsed
size=$size
INFO

echo "Done in ${elapsed}s: $app_path ($size, commit $commit)"
