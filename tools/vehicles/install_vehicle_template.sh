#!/usr/bin/env bash
# Copy the content of Epic's Vehicle project template (test car for version
# 0.3) into the project, at the same /Game paths. Epic content: kept out of
# Git (.gitignore), never overwrites an existing file.
#
# Usage: tools/vehicles/install_vehicle_template.sh [project_dir]
#   UE_ROOT can point to the engine folder (default: /Users/Shared/Epic Games/UE_5.8).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
project_dir="${1:-$repo/GranTheftPraline}"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"

template=""
for name in TP_VehicleAdvBP TP_VehicleBP TP_Vehicle; do
  if test -d "$ue_root/Templates/$name/Content"; then
    template="$ue_root/Templates/$name/Content"
    break
  fi
done
test -n "$template" || { echo "no Vehicle template under $ue_root/Templates" >&2; exit 1; }
echo "template: $template"

for folder in VehicleTemplate Vehicles; do
  if test -d "$template/$folder"; then
    mkdir -p "$project_dir/Content/$folder"
    # -n: never overwrite (our own Content/Vehicles/<Model>/ stays as is).
    cp -R -n "$template/$folder/." "$project_dir/Content/$folder/"
    echo "copied $folder"
  fi
done
echo "top-level template folders (only VehicleTemplate and Vehicles are copied):"
ls "$template"
