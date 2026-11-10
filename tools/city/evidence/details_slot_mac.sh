#!/usr/bin/env bash
# One Mac slot to find what the street dressing costs on M1, unattended (about 35 minutes):
#   1. regenerate the map WITH the details and OPAQUE shop glass (no play run), then
#      profile_details.sh: frame times with everything, without details, without furniture,
#      and without each heavy detail group;
#   2. regenerate WITHOUT details and with TRANSLUCENT glass, with the usual street walk:
#      compared with tonight's opaque, detail-free release map, the gap is the glass alone.
# assets/geo/processed/<zone>/manifest.json is restored after each regeneration (build.py
# rewrites its furniture count). The map is left as in step 2; run regen_city.sh again with
# the chosen settings afterwards.
#
# Usage: caffeinate -dimsu tools/city/evidence/details_slot_mac.sh
#   PYTHON (venv for the city tools), UE_ROOT, GTP_CITY_ZONE, GTP_CITY_FOCUS
# Output: Saved/CityImport/details-slot-<stamp>/ (details_profile.json, play_glass.json, log).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../../.." && pwd)"
zone="${GTP_CITY_ZONE:-presqu-ile}"
manifest="$repo/assets/geo/processed/$zone/manifest.json"
saved="$repo/GranTheftPraline/Saved/CityImport"
out="$repo/GranTheftPraline/Saved/details-slot-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$out"
cp "$manifest" "$out/manifest.json.orig"
restore() { cp "$out/manifest.json.orig" "$manifest"; }
trap restore EXIT

echo "== 1/3 regenerate with details, opaque glass"
GTP_CITY_DETAILS=1 GTP_SHOP_GLASS=opaque SKIP_PLAY=1 "$repo/tools/city/regen_city.sh" > "$out/regen_details.log" 2>&1
restore
echo "== 2/3 profile the dressing"
"$repo/tools/city/unreal/profile_details.sh" > "$out/profile.log" 2>&1
cp "$saved/details_profile.json" "$out/"
cp "$saved/details_report.json" "$out/" 2> /dev/null || true
echo "== 3/3 regenerate without details, translucent glass, street walk"
GTP_CITY_DETAILS=0 GTP_SHOP_GLASS=translucent "$repo/tools/city/regen_city.sh" > "$out/regen_glass.log" 2>&1
restore
cp "$saved/play_report.json" "$out/play_glass.json"
echo "details slot: $out"
cat "$out/details_profile.json" "$out/play_glass.json"
