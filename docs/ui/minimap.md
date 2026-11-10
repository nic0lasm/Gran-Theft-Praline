# Mini-map

*French version: [minimap.fr.md](minimap.fr.md)*

Roadmap: mini-map (0.5), full-screen map and GPS (0.10). Decision record: [ADR-0006](../adr/adr-0006-interface-settings-minimap.md).

## Look

A round radar at the bottom left (radius 120 px at 1080 lines, 36 px from the
corner): roads (major bright and wider, minor, paths green-grey, tunnels faint),
water in blue, building outlines faint, a ring, a north mark "N" on the ring, and the
player's arrow in the middle. It **turns with the camera**: up is where the player
looks, as in GTA. Zoom: near 120 m, medium 220 m (default), far 400 m of radius (far
leaves out buildings and paths). Switch on / off and zoom are in the
[settings](settings.md). It is hidden while a menu is open (the HUD is) and when the
map has no data.

Review images made from the real data, near / medium / far, facing north on the rue de
la République: `/mnt/project-files/ui/minimap-preview-*.png` (made by
`tools/ui/preview_minimap.py`, same maths as the game).

## Data

`tools/ui/minimap.py` turns the city tables (`roads.geojson`, `water.geojson`,
`buildings.geojson` of the zone: IGN BD TOPO, Licence Ouverte Etalab 2.0) into
`minimap.csv`: one row per polyline, `kind,width_m,points_ue_cm`, simplified to 1.5 m
and in Unreal centimetres like the traffic lanes. Presqu'île: 3 497 polylines,
295 KB. With `--map <Map>` it is copied to
`GranTheftPraline/Content/City/Minimap/<Map>.minimap.csv`, which is **not committed**
(derived city data, like the lanes) and is staged with the game
(`DirectoriesToAlwaysStageAsUFS`).

```
python3 tools/ui/minimap.py --map L_City_RueDeLaRepublique
```

The coordinates are the zone's local metres, the same origin as the traffic lanes and
the city import, so the mini-map and the world agree without a per-map offset. When the
map is extended (Rhône quay, Lafayette bridge) the zone tables already cover it; run
the tool again after a zone change.

## Code

- `GTPMinimapCore.h`: no Unreal types. Reading the CSV, the rotating view
  (`FMapView::ToScreen`), clipping segments to the disc, culling lines by bounding box.
  Tested by `tools/ui/tests/run_cpp_tests.sh`.
- `UGTPMinimapSubsystem` (`GTPMinimap.h`): world subsystem, loads the data when the map
  starts, draws on the canvas. `AGTPHUD::DrawHUD` calls `Draw()` once at its end: that
  is the only change in the HUD.
- Blips for other systems: `UGTPMinimapSubsystem::AddBlip(Actor, Colour, bClampToEdge)`
  and `RemoveBlip(Actor)`. Police cars, pedestrians or a destination register
  themselves; with `bClampToEdge` the dot stays on the ring when out of range.

## Place and cost

Bottom left is free: the driving panel and the contextual help are bottom right, the
subtitles bottom centre, the stars of the police system top right. Cost: one canvas line
item per visible segment (a few hundred at medium zoom), no texture, no render target.
The measured cost on the M1 Pro goes in `docs/ui/test-results/` (CPU and GPU ms with
the mini-map on and off, same route); the budget requested from the optimisation work
is 0.3 ms CPU and 0.2 ms GPU. If it does not fit, the fallback is one static
pre-rendered texture per map rotated by the HUD (one draw call).

## Next (roadmap 0.10)

Full-screen map with the same data and a larger view, waypoint set by the player, GPS
route drawn on the lanes (`lanes.csv` of the traffic), district names.
