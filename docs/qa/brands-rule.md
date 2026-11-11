# Real-brands rule for shared builds

*French version: [brands-rule.fr.md](brands-rule.fr.md).*

The quarter is generated from open data, so its shops carry their **real
names** (OpenStreetMap). A shop's logo is trademark artwork, which is a
different matter. This rule applies to every build that leaves the owner's
machines, even privately.

## Rule

| Item | Public repository | Private test build (Nico and close people) | Public release |
| ---- | ---- | ---- | ---- |
| Real shop name as plain lettering (generated text, `T_Sign_*`) | Not committed (derived OSM table, ODbL) | Allowed | Allowed (facts, no artwork) |
| Logo artwork (`T_SignLogo_*`, `Plugins/CityBrands/`) | **Never** | **Off by default**; on only by an explicit decision of Nico for a named list of testers | **Never** |
| Brand colours | — | Allowed (a colour is not a logo) | Allowed |
| Real people, real car marques with their badges | Never | Never | Never |

Consequences:

1. The **default test build is the brand-free variant**: `Plugins/CityBrands`
   is not mounted, so signs show lettering only. This is what
   `tools/city/unreal/mount_citybrands.sh` leaves when it is not run.
2. A build with logos is produced only on request, marked `-logos` in its
   archive name (`GranTheftPraline-0.9.0-rc1-macos-logos.zip`), listed in
   `docs/releases/` with the people it was given to, and never uploaded to a
   public place.
3. `tools/qa/check_release_bundle.py`
   must find no `T_SignLogo_*` in a default archive.
4. Shop names stay factual. No sign text is invented to mock a real brand.

## Open decision

Nico chooses between: (1) lettering only in every shared build (recommended,
matches the rule above); (2) fictional names instead of real ones; (3) logos
for named testers. Until answered, option 1 applies.
