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

echo "Packaging version $version ($config) into $out_dir"
echo "Engine: $ue_root"
echo "Full log: $log_file"

start_time="$(date +%s)"
if ! "$run_uat" BuildCookRun \
    -project="$project_file" \
    -platform=Mac \
    -clientconfig="$config" \
    -build -cook -stage -pak -iostore -package -archive \
    -archivedirectory="$staging_dir" \
    -compressed -prereqs -nop4 -utf8output -unattended -nocompileeditor \
    > "$log_file" 2>&1; then
  echo "BuildCookRun failed. Last errors:" >&2
  grep -E "Error|error:" "$log_file" | tail -n 30 >&2 || true
  die "see $log_file"
fi

built_app="$(find "$staging_dir" -maxdepth 3 -name '*.app' -type d | head -n 1)"
test -n "$built_app" || die "no .app found under $staging_dir"

rm -rf "$app_path"
mv "$built_app" "$app_path"
rm -rf "$staging_dir"

codesign --force --deep --sign - "$app_path" >> "$log_file" 2>&1 || die "ad hoc signing failed, see $log_file"
codesign --verify --deep "$app_path" || die "signature check failed"

elapsed=$(( $(date +%s) - start_time ))
size="$(du -sh "$app_path" | cut -f1)"
commit="$(git -C "$repo_root" rev-parse --short HEAD 2>/dev/null || echo unknown)"

cat > "$out_dir/build-info.txt" <<INFO
version=$version
configuration=$config
commit=$commit
built=$(date -u +%Y-%m-%dT%H:%M:%SZ)
duration_s=$elapsed
size=$size
INFO

echo "Done in ${elapsed}s: $app_path ($size, commit $commit)"
