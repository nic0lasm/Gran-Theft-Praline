#!/usr/bin/env bash
# Create the out-of-Git content plugin Plugins/CityBrands when it is missing, so that the
# editor mounts /CityBrands and place_facades.py imports the real shop logos there (art
# bible 3.5: trademarks never enter the MIT repository; .gitignore keeps the folder out).
# Idempotent; only writes the plugin descriptor and an empty Content folder.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../../.." && pwd)"
plugin="$repo/GranTheftPraline/Plugins/CityBrands"
mkdir -p "$plugin/Content"
if ! test -f "$plugin/CityBrands.uplugin"; then
  cat > "$plugin/CityBrands.uplugin" <<'JSON'
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0",
  "FriendlyName": "City Brands",
  "Description": "Real shop signs and logos of the city, local only (non-commercial use, never published).",
  "Category": "Content",
  "CanContainContent": true,
  "EnabledByDefault": true,
  "ExplicitlyLoaded": false
}
JSON
  echo "created $plugin/CityBrands.uplugin"
fi
git -C "$repo" check-ignore -q "$plugin/CityBrands.uplugin" || { echo "Plugins/CityBrands is not ignored by Git" >&2; exit 1; }
