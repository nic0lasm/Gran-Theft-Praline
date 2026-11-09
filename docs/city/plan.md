# Reproducing Lyon faithfully: sources and pipeline

*French version: [plan.fr.md](plan.fr.md)*
*Written: 2026-11-09. Inputs: [concept](../../design/gdd/game-concept.md), [roadmap](../../production/roadmap.md) (versions 0.7, 0.12, 0.14), [asset sourcing](../sourcing/asset-sourcing.md).*
*Code: [`tools/city/`](../../tools/city/). Data goes to `assets/geo/` and is regenerated, not versioned.*

## Decision in one paragraph

We **generate the city ourselves from open data**: footprints, heights, roads,
water, vegetation and shop positions come from IGN BD TOPO and OpenStreetMap,
become tables and GeoJSON in local metres, are imported into Unreal by an
editor script, then each building is built by a procedural façade kit (PCG)
and dressed with Tripo assets and real shop signs. We do **not** stream a
photogrammetry mesh (Google 3D Tiles / Cesium) as the game world: it cannot
be edited, has no interiors, cannot be collided or lit as game geometry, and
Google's terms forbid extracting it. Cesium for Unreal stays useful as a
free-to-look *reference layer* inside the editor to check our result.

## Sources compared

| Source | Gives | Licence | Verdict |
|---|---|---|---|
| IGN **BD TOPO V3** (WFS `data.geopf.fr`) | 2.6k building footprints with height, floors, uses, materials, altitudes in the test zone; roads with width, lanes, direction, speed; vegetation; water; structures; ERP (public venues) | Licence Ouverte Etalab 2.0 (credit IGN) | **Core.** Tested, no key needed |
| **OpenStreetMap** (standard API, tiled) | Shops, restaurants with `name`, `brand`, `brand:wikidata`, hours, addresses; trees; street lamps, benches, bins, bus stops, hydrants, traffic lights | ODbL 1.0 (credit, share-alike on the database) | **Core for shop signs and furniture.** Tested. Overpass is not reachable from our cloud, the API works for district-sized zones |
| IGN **LiDAR HD** (MNT / MNS) | Terrain and surface model, 0.5 m, roof shapes, tree heights | Licence Ouverte Etalab 2.0 | **In use** (step 2): terrain, eaves, roofs, courtyards. Tested, no key ([generation](generation.md)) |
| **Grand Lyon open data** (data.grandlyon.com: 3D model, imagery, trees, street furniture) | Street trees, 3D roof ridge lines, kerbs, bins, bike racks, shelters, benches; 2012 CityGML model | Licence Ouverte (benches and fountains: ODbL, from OSM) | **In use** (step 2): ridges, trees, furniture. CityGML 2012 not used ([generation](generation.md)) |
| **Google Photorealistic 3D Tiles** via Cesium for Unreal | Textured photogrammetry mesh | Google Maps Platform terms: online only, attribution, no extraction or offline use | **Reference only**, never shipped |
| Own **photogrammetry** (Apple Object Capture) | Façades and details of key landmarks | Ours | Later, for a handful of monuments (0.14+) |

## Pipeline

```
fetch.py  ->  assets/geo/raw/<zone>/        BD TOPO GeoJSON (Lambert-93) + OSM XML tiles
build.py  ->  assets/geo/processed/<zone>/  buildings.csv|geojson, shops.csv, trees.csv,
                                            furniture.csv, roads/vegetation/water.geojson,
                                            manifest.json
generate.py / terrain.py -> gen/, terrain/  roofs, courtyards, facade modules, greybox, heightmap
(editor)  ->  unreal/import_city.py         greybox level; facade kit / PCG reads facade_modules.csv
```

* Coordinates: Lambert-93 metres recentred on the zone, `x` east, `y` north.
  Unreal: `X = east`, `Y = south`, `Z = up`, in cm; `ue_x/ue_y/ue_z` columns
  are already converted (`Z` relative to the median ground altitude).
  Our projection matches the IGN's own EPSG:2154 output to under 5 cm.
* Heights: BD TOPO `hauteur`, else roof minus ground altitude, else floors × 3 m,
  else 15 m (`height_source` column). On the test zone 2 596 of 2 601 use the IGN height.
* Shops: every named OSM shop or amenity is matched to its building (inside the
  footprint, else nearest within 15 m): this is the per-façade sign list.

## Test zone: Presqu'île (bbox `45.7570,4.8250,45.7690,4.8420`)

Command: `python3 tools/city/fetch.py --name presqu-ile --bbox 45.7570,4.8250,45.7690,4.8420`
then `python3 tools/city/build.py --name presqu-ile`. About one minute, no key.
The box spans Bellecour to the Terreaux plus both rivers and part of the banks.

| Output | Count |
|---|---|
| Buildings | 2 601 |
| Shops / venues (named) | 2 692, of which 521 with a brand and 2 615 matched to a building |
| Trees | 1 466 |
| Street furniture | 3 604 |
| Road segments | 1 194 |
| Vegetation zones / water surfaces | 74 / 3 |

The three McDonald's of the zone are found with brand, hours and matched
building, including the one at the Bellecour end of rue de la République.

## Real shop signs

Decision (Nico, 2026-11-09): non-commercial use, so real signs and brands are
reproduced as faithfully as possible, not masked. `shops.csv` carries `name`,
`brand`, `brand_wikidata`, `cuisine`, `opening_hours` and the building id, so a
sign actor can pick the right logo and typeface per façade. Risk to keep in
mind (one sentence): logos are trademarks and the MIT licence does not cover
them, so the public repo should keep logo files out of Git (local storage like
the Tripo masters) and mention the non-commercial intent in the README.

## Licence handling

* IGN data: credit "IGN, BD TOPO" in `assets/SOURCES.md` and in the game credits.
* OSM data is ODbL: we do **not** commit the derived tables (only the code and the
  manifest); each contributor regenerates them. Credit "© OpenStreetMap contributors".
* Everything under `assets/geo/raw/` is already ignored by Git; `processed/` is
  regenerated and must not be committed except `manifest.json`.

## Fidelity limits and next steps

1. OSM coverage is uneven (only 19 % of venues carry a brand): we complete
   key shops by hand from street photos.
2. BD TOPO has no façade detail: window rhythm, balconies and ground floors
   come from the façade kit, fed by `usage_*`, `floors`, wall material and photos.
3. Done in step 2 ([generation](generation.md)): LiDAR HD terrain and roofs, façade layout,
   greybox import script. Still to do: bridges and quays (BD TOPO `construction_*`), road lanes and lights.
4. Tests: `python3 -I -m unittest discover -s tools/city/tests` (CI job "City tools tests").
