# Generating the Presqu'île in 3D: LiDAR, Grand Lyon data and procedural façades

*French version: [generation.fr.md](generation.fr.md)*
*Written: 2026-11-09. Step 2 of the [city plan](plan.md). Code: [`tools/city/`](../../tools/city/).*
*Budgets and naming: [art bible](../art/art-bible.md). Everything generated stays out of Git and is regenerated.*

## Result in one paragraph

From open data only, with no key and no editor, the pipeline now rebuilds every
building along the rue de la République (Terreaux → Bellecour, 45 m on each side)
as a greybox that already has its real volume, its real roof, its courtyards,
storeys, bays, entrances, balconies and **real shop signs in the right bays**.
175 BD TOPO buildings become 281 volumes (courtyards split out), 13.2 k façade
modules, 215 signs including 106 brands, 288 k triangles, in 16 s. The roofs
match the 2021+ IGN LiDAR to **0.39 m median** (90 % of 0.5 m cells within
2.2 m). The same module list feeds the engine's façade kit later.

Previews are rendered by the pipeline itself (`render.py`, no engine) into
`assets/geo/processed/presqu-ile/gen/rue-de-la-republique/preview_*.png`.

## Sources added (all tested on 2026-11-09)

| Source | Access | Licence | Used for |
|---|---|---|---|
| IGN **LiDAR HD** MNT / MNS / MNH, 0.5 m | Geoplateforme WMS-Raster `data.geopf.fr/wms-r`, float32 grids, 40 s for 1.4 × 1.4 km | Licence Ouverte Etalab 2.0 | Terrain, ground height of each building, eaves, roof surface, courtyards, check of every roof |
| Grand Lyon **3D ridge lines** (2012) | WFS `download.data.grandlyon.com/wfs/grandlyon`, layer `fpc_fond_plan_communaut.fpclignefaitage` | Licence Ouverte | Ridge and hip lines of the roofs (kept only where the LiDAR still agrees) |
| Grand Lyon **street trees** | `abr_arbres_alignement.abrarbre` | Licence Ouverte | Species, height, trunk height, crown radius (2 153 trees, better than OSM) |
| Grand Lyon **kerbs, traffic islands, surveyed ground points** | `fpcbordchaussee`, `fpcilot`, `fpcpointcotesol` | Licence Ouverte | Pavement edges and ground checks (next step: roads and pavements) |
| Grand Lyon **bins, bike racks, shelters** | `gincorbeille`, `pvostationnementvelo`, `pvomobilierurbain` | Licence Ouverte | Furniture positions *with their model* (e.g. "Arceaux Wilmotte") |
| Grand Lyon **benches, ornamental fountains** | `adrbanc_latest`, `adrfontaineornem_latest` | ODbL (taken from OSM) | Furniture positions; stays out of Git like every OSM-derived table |
| Grand Lyon textured **CityGML model** | per-arrondissement zips, **2012** | Licence Ouverte | Not used: 14 years old; our façades come from the kit, roofs from the LiDAR |

Licences were read on the portal (dataset "Licence" field). Credits to add in
game: "IGN, LiDAR HD", "Métropole de Lyon".

## Pipeline

```
fetch.py     -> raw/<zone>/       + lidar_<mnt|mns|mnh>.f32, lidar.json, grandlyon_<layer>.geojson
build.py     -> processed/<zone>/ (unchanged: buildings, shops, trees, furniture, roads...)
generate.py  -> processed/<zone>/gen/<street>/  buildings_gen.csv, facade_modules.csv, greybox.glb, report.json
terrain.py   -> processed/<zone>/terrain/       heightmap.png (Landscape), terrain.glb (tiles), terrain.json
render.py    -> processed/<zone>/gen/<street>/preview_<view>.png
unreal/import_city.py (editor, headless) -> /Game/Maps/L_City_<Street>
unreal/run_city_import.sh (Mac) -> import_city.py, place_facades.py, then a -game run: shots + frame times
```

Commands (Python 3 + `pip install -r tools/city/requirements.txt`):

```
python3 tools/city/fetch.py --name presqu-ile --bbox 45.7570,4.8250,45.7690,4.8420
python3 tools/city/build.py --name presqu-ile
python3 tools/city/generate.py --name presqu-ile --focus "Rue de la République" --buffer 45
python3 tools/city/terrain.py --name presqu-ile
python3 tools/city/render.py --name presqu-ile --view street:176,-60,1.8,179,150,9
```

`--focus all` generates the whole zone (2 601 buildings).

## How a building is built

1. **Footprint** (BD TOPO), split by the LiDAR: connected areas of at least 6 m²
   that sit at ground level are **open courtyards** (cut out); areas at least
   4 m below the main roof are **covered courtyards** (the shops' roofs over the
   ground floor, very common here) and come back as low flat "podium" parts.
   On the rue de la République: 3 118 m² of covered courtyards found.
2. **Ground height**: lowest LiDAR terrain point along the outline.
3. **Eaves**: low quantile of the LiDAR surface in a 0.4–1.6 m band inside the
   outline, capped so no roof is steeper than about 45° across the short side.
4. **Roof**: ridge lines clipped to the footprint and checked against the LiDAR
   (dropped if they disagree by more than 2.5 m: demolished or rebuilt roofs),
   then a constrained triangulation of outline + ridges: gables and hips come
   out of it. Flat roofs where the LiDAR is flat; otherwise a 3 m LiDAR grid.
5. **Edges**: each wall is a *street* façade (a road within its half-width +
   7 m in front), a *courtyard* façade, or a *party wall* (another building
   0.6 m behind it: no openings, as in reality).
6. **Typology** (rules, to be checked against photos):
   * `haussmann`: wall ≥ 14 m on a Second Empire street (République,
     Édouard-Herriot, Bourse, Jacobins, Cordeliers, Carnot...): ground floor
     4.2 m + entresol 2.6 m, storeys 3.3 m, 3.3 m bays, balconies on the 2nd
     and 5th floors (art bible), cream limestone, zinc roofs;
   * `ancien`: 3.8 m ground floor, 3 m storeys, 2.8 m bays, Lyon render
     colours (ochre, salmon, straw yellow, red ochre), tile roofs;
   * `moderne`: BD TOPO concrete walls; `monument`: churches (hand-made later).
7. **Bays and modules**: storeys are stretched to end exactly at the eaves;
   each street/courtyard wall is cut into whole bays. Kinds: `shop`,
   `shopfront_closed`, `entrance` (one per building, mid-façade), `gf_window`,
   `entresol_window`, `window`, `window_balcony`, `window_small` (attic storey),
   `court_window`, `monument_bay`, `blank` (walls under 1.8 m).
8. **Shop signs**: each OSM shop of the building goes to the nearest street
   bay (two bays when the façade has six or more). Its `name`, `brand` and
   `brand:wikidata` are copied onto the module: that is the key the sign actor
   uses to pick the real logo in `Plugins/CityBrands/` (kept out of Git).

## Outputs

* `facade_modules.csv`: one row per module: building, typology, side, street,
  kind, storey, bay, position (local metres and Unreal cm), Unreal yaw (facing
  out), width, height, colour, sign name/brand/Wikidata. This is the input of
  the façade kit (`Content/Environment/Facades/<Kit>/`, 50 cm grid) and of the
  PCG graph that will instance it.
* `greybox.glb`: one node per volume (`SM_Building_<id>`), pivot at the
  footprint centroid on the ground, vertex colours, glTF axes X east, Y up,
  Z south (same convention as the reference scene, already validated in UE).
* `buildings_gen.csv`: typology, levels, roof kind, heights, LiDAR checks,
  Unreal position and expected bounds of each mesh.
* `terrain/heightmap.png` 2017 × 2017, 0.70 m spacing, Landscape settings in
  `terrain.json` (scale 69.64 / 69.64 / 17.22, 1.3 mm height step).

## Measurements (rue de la République, 2026-11-09, cloud machine)

| Measure | Value |
|---|---|
| BD TOPO buildings / generated volumes | 175 / 281 (106 covered-courtyard podiums) |
| Roofs | 142 with LiDAR-checked ridges, 118 flat, 21 LiDAR grid |
| Roof vs LiDAR, 169 742 cells of 0.5 m | median 0.39 m, p90 2.23 m (dormers, chimneys, glass roofs not modelled) |
| BD TOPO height vs LiDAR (whole zone) | median 0.85 m, p90 3.8 m |
| Typologies | 70 haussmann, 211 ancien (podiums included) |
| Façade modules | 13 182, of which 215 signed shops (106 brands) |
| Greybox | 288 k triangles, 28 MB GLB (flat shaded, not indexed) |
| Run time | 16 s generation, 30 s terrain, about 1 min per preview |
| Whole zone (`--focus all`, 1.4 × 1.4 km) | 2 601 buildings → 3 617 volumes, 205 k modules, 2 486 signs (499 brands), 3.8 M triangles, roof vs LiDAR median 0.52 m / p90 3.2 m, 3 min 44 s |

The greybox is a layout check, not an asset: final façades are kit modules
(≤ 50 k triangles each, ADR-0002), instanced; the module list is what carries over.

## Facade kit v0 and placement (added 2026-11-09)

Code: [`tools/city/facade_kit/`](../../tools/city/facade_kit/) (Blender 4.2+, app or the `bpy`
module) and [`tools/city/unreal/place_facades.py`](../../tools/city/unreal/place_facades.py).

* **18 modules**, one bay × one storey, built procedurally in Blender from the
  art bible (section 3.1): *Haussmann* (ashlar, grey plinth, entresol with
  rustication, French windows with stone surrounds, entablatures and
  balconettes, continuous balconies on consoles, carriage doors with
  fanlights, moulded cornice with modillions, aluminium shopfronts with a
  fascia) and *Lyonnais* (coloured render, pierre dorée surrounds and sills,
  louvred shutters, barred ground-floor windows, wooden shopfronts, genoise).
  6.5 k triangles for the whole kit (largest module 1 k): far below the
  50 k M1 budget per module; the detail will come from the trim sheets and
  Megascans/photo textures (`MI_*` slot names already follow the bible).
* `assemble.py` exports `facade_kit.glb` + `kit_manifest.json` and assembles
  the street from `facade_modules.csv` with the same placement rule as
  Unreal: module at its position, front turned outward, width and height
  stretched to the bay, cornice/genoise on top of every street bay, **raised
  letters with the shop's real name** on every shop fascia. Cycles renders of
  the rue de la République: 3 895 module instances and 62 signs within
  160 m, about 2 min per 1600 × 900 still on 4 cloud cores.
* `place_facades.py` (editor, after `import_city.py`): imports the kit into
  `Content/Environment/Facades/LyonKit/`, checks every module's size and which
  side is the outside (from the balcony bounds) before placing, then one
  actor per module type with a hierarchical instanced static mesh component
  (13 k instances for the street), the building's render colour in
  per-instance custom data, and the signs: the real logo when
  `/CityBrands/Logos/T_Brand_<Wikidata QID>` exists (kept out of Git),
  otherwise a text actor with the real name. `import_city.py` now places the
  **shells** (roofs, party walls) so the kit is not doubled.
* Not done yet: a PCG graph is not needed for this placement (the module
  list is already computed); it will be worth it for streaming (PCG
  partitioning) and for runtime variation, after the first editor test.
  Next: trim sheets and textures from Megascans/photos, `M_Master_Facade`
  reading the per-instance colour, corner modules, dormers and the Haussmann
  mansard.

## What the player sees first: shopfronts and real signs (added 2026-11-09)

Priority set by measuring what faces the street on the rue de la République:
2 801 street modules; at eye level, 672 ground-floor bays without a known shop
and 185 bays of real shops, then the Haussmann windows (2 698) and balconies
(1 846). So this pass works on those, not on the kit as a whole.

* **Shopfronts** (`kit.py`): the Haussmann shop bay now has three designs
  (aluminium frame, painted-wood heritage shopfront in front of the piers,
  frameless glass with a steel rim) and a lowered metal shutter; every
  shopfront has a lit room behind see-through glass (back wall, floor,
  ceiling, two display tables), so windows read as shops and not as black
  mirrors. The fascia is 60 cm high (was 35). Ground-floor bays with no known
  shop use the same designs with a plain fascia; one building in eight has its
  shutters down. Lyon render shops got the lit room too.
* **Window surrounds**: three Haussmann designs (entablature, triangular
  pediment, lintel cornice on consoles with a keystone), for plain and
  balcony storeys. Lyon render windows (the second most frequent street
  module): shutters folded back, a wrought-iron guard without shutters, or one
  shutter closed.
* **Variants are per building** (`kit_layout.module_for`, stable hash of the
  building id): a frontage reads as one design, neighbours differ, and
  Blender and Unreal pick the same one.
* **Signs** (`brands.py`, `signs.py`): for every distinct brand, Wikidata gives
  the label, official colour and logo file; the logo is fetched from Commons
  (960 px thumbnails, spaced requests: Commons throttles scripts). `signs.py`
  draws one 2048 × 464 texture per shop with its real name (brand name when
  the brand has a Wikidata id, else the shop's own name), background from the
  brand's colour, else the logo's dominant colour, else a palette by trade;
  lettering white or near-black, whichever contrasts more (WCAG ratio checked,
  4.3 minimum). When a logo exists, a second texture shows it on the fascia.
  ATMs, parcel lockers, artworks and car parks get no fascia (13 of 215).
* **Placement**: a sign panel (`SM_Sign_Panel`) on the fascia of every bay of
  the shop, at the height and depth of the variant placed there. In Unreal,
  `place_facades.py` builds `M_Sign_Fascia` and one material instance per sign;
  lettering textures go under `/Game/Environment/City/Signs/`, logo textures
  only under `/CityBrands/Signs/` when the out-of-Git plugin is mounted
  (trademark artwork never enters Git).

Commands: `python3 tools/city/brands.py --name presqu-ile --focus rue-de-la-republique`,
then `python3 tools/city/signs.py --name presqu-ile`, then `assemble.py` (add
`--logos` for local renders with the logos).

## Morning test map and street furniture (added 2026-11-09)

* `import_city.py` now gives the city map the **art-bible lighting** of the
  reference scene (`GTP_CITY_LIGHT` = Noon by default, GoldenHour, Rain or
  Night: the reference map is copied and everything but its lights, sky, fog
  and post-process is removed), **complex collision** on buildings and terrain
  (the pawn walks on the LiDAR ground and is stopped by the walls), a kill-Z,
  and the **PlayerStart at the Bellecour end** of the rue de la République,
  8 m into the street, looking up it.
* `props.py` places the wave-1 street furniture at **surveyed positions only**:
  Grand Lyon litter bins (transparent-bag hoops where the register says so),
  bike-rack hoops (one per arceau, built racks only), bus shelters and tree
  grates; OSM benches, street lamps, hydrants, post boxes, drinking fountains,
  bike-share terminals and glass bells; and café/restaurant terraces in front
  of the 16 places on the street (tables, chairs, parasol, chalkboard). 732
  props for the street, plus 362 wave-14 details on the same real anchors
  (plane-tree trunks on the Grand Lyon plane trees, pharmacy crosses,
  mannequins in clothes shops, bay laurels at café doors, terrace heaters and
  planter dividers, intercoms and doorstep stones at entrances, parcel
  lockers), found in the editor by their CatalogId tag once imported: 1 094
  rows in all. `unreal/place_props.py` instances them (one
  hierarchical instanced component per asset, collision on), drops each one on
  the ground by a trace, and lists the assets not imported yet.
* `unreal/run_city_import.sh` chains import, facades, props, then walks the
  street in `-game` with the m1 rendering profile at 5 m/s from Bellecour to the
  far end, recording every frame time (p50, p95, p99, hitches over 100 ms) and
  five screenshots.

## Ground and shells: art bible materials (added 2026-11-10)

`tools/city/ground_mask.py` draws a ground mask from the BD TOPO road centre lines
(2048 × 8192, ~15 cm per texel, not committed): asphalt carriageway (real width), granite
kerbs, granite setts on streets closed to cars (zero speed: rue de la République, paths,
steps), dashed centre lines on two-way roads, limestone slabs everywhere else.
`tools/city/unreal/apply_city_materials.py` creates `M_City_Ground` (mask read at world
coordinates, procedural slab and sett joints and tone variation, a single texture) and
`M_City_Shell` (building colour written by the generator in the vertex colours, stone courses
on walls, smoother roofs), assigns them to the terrain tiles and shells, and checks the facade
instances of the map. It replaces `M_CityGreybox` without rebuilding the map.

Weathering, all in shader arithmetic (no texture, cheap on the M1 profile): the facade kit
(`M_City_Kit`) and the shell walls get a slow tint drift across the block, a darker plinth
over the first 1-4 m above the building's street level (`place_facades.py` writes it in
per-instance custom data 3) and rain streaks under each cornice and sill, scaled per slot by
`Weathering` (stone 1, wood 0.5, glass 0.1-0.15, shop interiors 0). The asphalt gets patches
(repairs, wheel paths). The mask is drawn on square texels and resampled, so streets keep their
width whatever the texture aspect.

### Carriageways fitted to the kerbs and one-command regeneration

The BD TOPO centre lines sit 0.8 m off the middle of the carriageway (2.2 m at the 90th
percentile) and cars on lanes drawn from them rode on the kerbs of the LiDAR terrain.
`tools/city/carriageway.py` fits each car road to the Grand Lyon kerb lines (rays every 2 m,
smoothed; 79 % of samples find a real kerb) and writes `processed/<zone>/carriageway.geojson`:
one polygon, a corrected centre line and half widths per road. `ground_mask.py` draws the
asphalt and kerbs from it; `traffic.py` can place its lanes on it.

`tools/city/regen_city.sh` regenerates everything in one command (geo data, generation with
the Rhone box, signs, carriageways, props, details, riverside, Unreal import, facades, props,
details, materials, parked car, traffic lanes): `caffeinate -dimsu tools/city/regen_city.sh`.

## Rhone quay and pont Lafayette (added 2026-11-10, for 0.3)

The generated footprint grows east for the 0.3 driving loop:
`generate.py --extra-bbox 250,-60,700,220` adds the buildings of quai Jules-Courmont, the bridge
head and a bit of the left bank (233 BD TOPO buildings, 378 parts, instead of 175 / 281).
`tools/city/riverside.py` builds, as greybox with vertex colours, the Rhone water (a plane at the
LiDAR water surface, no collision), the pont Lafayette deck (Grand Lyon footprint, gid 41,
~20 m × 190 m, parapets; the footprint stops over the lower quays, so each end is stretched
along the bridge axis until it meets the street within 12 cm and settles on the median street
height, measured step at most 7 cm, collision on) and a quay parapet along the water
(519 m in the box, cut under the bridge). `import_city.py` places them when present (folder
City/Riverside); `apply_city_materials.py` gives them `M_City_Shell` and `M_City_Water`.
The river surface is our own mesh with `M_City_Water` (small moving waves, opaque), not the
engine Water plugin: it needs a landscape and extra passes the M1 budget cannot afford.
Falling in: a river bed with collision 2.5 m under the surface (nothing falls through the
world), 25 m trigger boxes tagged `GTP_RiverRecovery` over the water and quay exits at street
level every 25 m, tagged `GTP_RiverExit` (target points facing the land, never on the lower
quays): the game swims or fishes out the player and the cars from them.
`build.py` now keeps the street names of the current BD TOPO, `nom_collaboratif_*` and
`nom_voie_ban_*` (the old `nom_1_*` fields are gone).

## Facade and street details: second dressing pass (added 2026-11-10)

The in-game audit of 0.2 and 0.3 showed a street that reads as a scale model: of about 344
imported props, only 24 types were placed. `tools/city/details.py` adds the small details, always
anchored on something real:

* **Facades** (`facade_modules.csv`): one full-height downpipe per building on the party-wall side;
  cellar grilles under Haussmann ground-floor windows; meter cabinet, house-number plaque and a
  keystone mascaron over carriage doors (positions computed from the kit); extended awnings over
  cafés, restaurants and food shops, retracted on every other shop elsewhere, stretched to the bay
  width; ATMs on banks; ashtrays at bar and tobacconist doors; doormats; window boxes on Lyonnais
  sills, the odd satellite dish and air conditioner on older buildings; a display shelf against the back
  of every open shop window of the street (nothing to see behind the glass otherwise).
* **Corners**: street-name plates on every corner building, on the corner side.
* **OpenStreetMap**: bollards, pedestrian lights on both kerbs of signalled crossings (real BD TOPO
  carriageway width), vehicle lights on the right-hand kerb, signs by French code (B1 no entry, AB3a
  give way, B54 pedestrian zone...) moved onto the Grand Lyon kerb line, street cabinets,
  advertising columns.
* **Trees, benches and bins**: fallen plane leaves around every other tree (autumn, the bible's
  season), litter and newspapers by some bins, pigeons near benches.
* **Roofs**: chimney stacks on party walls, at ridge height.

Which bay gets a detail is a stable hash of building and bay: every run gives the same street. The
table has the columns of `props_placement.csv` plus `snap` (`keep` keeps the height of wall and
roof items), `scale_x/y/z` and `collision` (none on small items). It is capped at 3,000 instances
(budget of the optimisation pass, 1 ms GPU on m1), by visibility priority; about 2,100 for rue de la
République. `run_city_import.sh` replays `details.py` and places it with `GTP_PROPS_SET=details`
in the City/Details folder.

Real shop logos did not show: the out-of-Git plugin `Plugins/CityBrands` did not exist on the Mac
(`citybrands_mounted: false`). `tools/city/unreal/mount_citybrands.sh`, run by
`run_city_import.sh`, creates the empty content plugin when missing and checks Git ignores it.

**Blade signs** (`place_facades.py`): every other shop gets, on its first bay, a sign
perpendicular to the facade above the fascia (1.1 m, same texture as the fascia, two
back-to-back panels, no shadow).

**Before/after evidence**: `tools/city/evidence/before_after.sh <before builds> <after builds> <version>`
runs the after build on the demo route (m1 profile, the game films itself), then writes
side-by-side stills at the same times (`compare-stills.swift`, AVFoundation, no ffmpeg) and
both frame-time summaries.

## Landmarks and cell weights (added 2026-11-10)

`tools/city/landmarks.py` replaces the generic facade kit with a measured volume for the
monuments people recognise: the Opera (glass vault) and the Hotel de Ville (pavilions, belfry,
courtyards). OpenStreetMap footprint (Wikidata id), courtyards removed where the LiDAR sees the
ground, walls on the true outline, roof = LiDAR MNS heightfield joined to the wall tops.
Opera: 2,072 m², 45 m, 4,261 triangles; Hotel de Ville: 3,764 m² with 1,604 m² of courtyards
removed, 52 m, 7,856 triangles. The 6 BD TOPO buildings covered leave `generate.py`
(`left_to_landmarks` in the report) and `import_city.py` places `gen/<focus>/landmarks.glb`
with collision. `GTP_CITY_LANDMARKS=0` goes back to the generic shells.

Place Bellecour and place des Jacobins: data only (outline, surface, monuments). The Louis XIV
statue and the Jacobins and Bartholdi fountains have no volume: the LiDAR MNS equals the
terrain on those squares (above-ground filtered), they will need models. Known limit.

`tools/city/cell_budget.py` estimates each cell's weight (facade instances, signs, furniture)
calibrated on the measured street (21,299 + 3,400 instances = 14 ms). Worst cell
p0000-m0500: 8.4 ms; worst 2 × 2 block: 26.1 ms; 3 × 3: 50.4 ms. Upper bounds (no cull
distance, no occlusion): 2 × 2 exceeds the 14 ms of ADR-0010, hence kit variety at constant
cost and the 250 m cull to be measured.

Mac slot in one command: `caffeinate -dimsu tools/city/city_slot_mac.sh <before_builds> 0.3`
(regeneration, package, filmed 60 s M1 test, the realism thread's before/after).

**GPU cost of the dressing**: `tools/city/unreal/profile_details.sh` measures, in `-game` and
without rebuilding the map, the frame time at four fixed points of the street with everything
visible, then hiding the details, the furniture, or one detail group at a time (actor tags
`GTP_Set_*` and `GTP_Cat_*` set by `place_props.py`). Result: `Saved/CityImport/details_profile.json`.
Wall-flush details, shelves behind the glass and small items no longer cast shadows (`shadow`
column); only awnings and chimneys keep theirs.

**Trees in leaf**: `GTP_CITY_LEAFY=1` (or `props.py --leafy`) swaps the bare plane trunks for
the free Megascans beech (`SM_EuropeanBeech_Field_01`, `_Sapling_01` for young trees) on the same
Grand Lyon anchors. Off by default until its M1 cost is measured.

**Fake-depth shop windows** (default, `GTP_SHOP_GLASS=interior`): `M_City_ShopInterior` is opaque
glass that draws a lit shop by interior mapping (one ray-box test per pixel in the module frame:
shelved back wall in varied colours, floor, bright ceiling, side walls), with no translucency
and no overdraw; reflections and Fresnel on top, emissive scaled by `EmissiveScale` at night.
`translucent` (real glass with W14-32 shelves) and `opaque` stay available. Details draw within
20 to 40 m (`cull_m` column, 60 m for chimneys and lights).
Details without a shadow (small or flush on a wall) also stay out of the Lumen distance fields,
the ray-tracing scene and decals: nothing visible changes, and software Lumen has fewer mesh
distance fields to trace (`GTP_DETAILS_LEAN=0` turns it off for a comparison).

**Per-group switches** (perf passes, all empty by default = the full dressing), read by
`tools/city/details.py` and keyed by catalogue id or family:
`GTP_DETAILS_ONLY=W14-42,Shopfront` keeps these groups only, `GTP_DETAILS_DROP=W14-03` removes
groups, `GTP_DETAILS_THIN=W14-42:0.5` keeps a stable share of a group (the same instances on
every run), `GTP_DETAILS_CULL=W14-42:25,*:0.8` sets a draw distance in metres, or scales them all.
`details_report.json` lists what was applied. **Material fusion**: with `GTP_DETAILS_MERGED=1`,
`run_city_import.sh` first runs `tools/city/unreal/merge_detail_materials.py`, which bakes every
multi-slot detail mesh into `SM_<Asset>_Merged` (one atlas material, 512 px by default,
`GTP_MERGE_TEXTURE`; source LOD chain rebuilt), and `place_props.py` uses those meshes: without
Nanite, every material slot is one more draw per pass. It writes new assets in Content, so it
stays off until a slot approved for it.

### Facade kit LODs (performance, 2026-11-10)

The 21,242 modules of the street map were 10.2 M triangles at LOD0 out to 250 m, drawn again by
the virtual shadow maps, with no Nanite on M1: the first cause of the 44.5 ms packaged p95.
`place_facades.py` now builds four LODs per module (100 %, 50 % under 0.3 screen size, 15 % under
0.1, 5 % under 0.035, about 10, 30 and 85 m); nothing visible is cut. `GTP_KIT_LODS=0` for the
before, `GTP_FACADE_CULL="court=120"` to try a distance on the bench. One-command bench:
`caffeinate -dimsu tools/city/facade_lods_mac.sh` (replays only the facades and materials,
`FACADES_ONLY=1`, then packages into Builds/facade-lods/).

### Shop sign atlas (performance, 2026-11-10)

Each sign was its own actor with its own material and texture: about 600 draw calls for 341
fascia panels and the blade signs. `tools/city/sign_atlas.py` packs the textures into 4096 × 4096
pages (64 signs per page, 1008 × 228 px each, edges repeated against mip bleeding): 5 lettering
pages and 1 logo page (out of Git, CityBrands plugin). `place_facades.py` places one instanced
component per page (plus a shadowless one for the blade signs) with `M_Sign_Atlas`, each sign's
cell in its instance data. `GTP_SIGN_ATLAS=0` goes back to separate actors (the bench's before).

### Shells merged per cell (performance, 2026-11-10)

`generate.py` also writes `shell_cells.glb`: the shells merged per 250 m cell (ADR-0010 grid),
376 shells → 8 meshes on the street map. `GTP_SHELL_MERGE=1` makes `import_city.py` place them
instead of the separate shells (complex collision kept). Off by default.

## Streamed quarter: far proxies, cell levels, streaming (added 2026-11-11, ADR-0010)

First implementation of ADR-0010 option B, prepared and tested off the Mac; not measured yet.

- `tools/city/cell_proxies.py` merges, for each cell, every building shell and landmark
  volume of `gen/<cell>/` into one far proxy (`gen/quarter/cell_proxies.glb` and `.json`:
  level, cell box in Unreal cm, triangles). Quarter of 30 cells: 29 proxies (one cell is
  empty), 29 draws, 242 k triangles; `--check` fails above 100 draws or 400 k triangles.
  `--near-street rue-de-la-republique --within 60` lists the cells a drive along the street
  streams in: p0000-m0250, p0000-p0000, p0000-p0250, p0000-p0500, p0000-m0500.
- `import_city.py` takes `GTP_CITY_LEVEL`: `quarter` builds the persistent `L_City_Quarter`
  (bible lighting, the terrain reused, the proxies without collision, shadow or distance
  field, tagged `GTP_CellProxy`, level and `bounds:`), `cell` builds a blank streaming level
  `L_City_Cell<X><Y>` with only the cell's buildings and landmarks. `place_facades.py` reuses
  the street kit (`GTP_KIT_FOCUS`, `GTP_KIT_REUSE=1`) so the kit and its materials are not
  re-imported per cell; `apply_city_materials.py` only gives the cell's shells the existing
  shell material.
- `UGTPCityStreaming` (C++) reads the tagged proxies at begin play, plans with
  `GTPCityStreamingCore.h` (load within 60 m of a cell, keep until 90 m, at most 4 cells,
  one request per update, nearest first), loads with `ULevelStreamingDynamic`, hides a proxy
  once its level is visible and shows it before release. `-CityCells=0`, `-CityCellLoad=`,
  `-CityCellUnload=`, `-CityCellMax=`. Inert on maps without proxies. The rules are checked
  by `tools/city/tests/city_streaming_test.cpp` (CI).
- Mac slot: `tools/city/evidence/quarter_slot_mac.sh` (compile, cells, proxies, quarter,
  5 cells, then the demo route with streaming on and proxies only; time guard per step,
  heartbeat, content restored by md5 unless `KEEP=1`). About 2 h 10.
- Whole quarter (2026-11-11, 0.9): `cells.py` now also places the surveyed street furniture per cell (`props.py --cell`, 20 460 objects) and, for a full run, writes the proxies, the lanes of the quarter (`traffic.py --focus quarter`: 1 043 lanes, 45.5 km) and the budget per cell with a memory estimate (`cell_budget.py`: worst 2 x 2 + shared about 446 MB for 1.5 GB). `tools/city/quarter_build.py` builds every level headless on the Mac or the Windows PC (plain Python, time limit per step, forced stop of a hung editor, `--dry-run`).
- Levers on a package, without regenerating (`UGTPCityRenderTuning`): `-CityCull=<family>=<m>,...`
  sets the end cull distance per tag family, on the cells' instanced components (`ground`, `upper`,
  `eave`, `court`, `sign`, `props_<size>`, `details_<size>`) and on the persistent level
  (`proxy`, `terrain`; a map built before those tags is recognised by mesh folder);
  `-CityProxies=0` never draws the far proxies; `-CityPropShadowBelow=<m>`; `-CityTuneReport=1`
  only logs. One line per level: `CityRenderTuning: <level> <n> components changed; <family>=<changed>/<found> ...`.
- Cells v2 (2026-11-11, 0.9): `cells.py` also writes the street details of each cell along the
  rue de la Republique (`details.py --focus <cell> --cell=x0,y0,x1,y1`: the street zone clipped to
  the cell box, so nothing twice at a seam; 1 909 in the five street cells, about the street
  map's 1 890). `quarter_slot_mac.sh` and `quarter_build.py` place the furniture and the details
  in each cell level (`place_props.py`, sets `props` and `details`). Budget compensation, on by
  default in the cell levels only (`UGTPCityRenderTuning`): courtyard facades and shop signs end
  at 60 m and 45 m, furniture and detail distances at 60 % (`-CityCellDetailPct=`, 10 to 100), no shadow under 1 m;
  `-CityCellDefaults=0` turns it off, `-CityCull` wins over it, a distance of 0 hides a family.
- Not yet: furniture and details in the cells away from the rue de la Republique, a quarter-wide ground mask, the cell
  levels in the cook list for a package.

## Map data for the NPC AI (added 2026-11-12)

`tools/city/npc_data.py --out <dir> [--osm-extra <tiles>]` writes, for the République slice (Bellecour -> Jacobins) and in the game frame (Unreal cm), the data the NPC AI needs: OSM sidewalks and pedestrian areas, crossings with their control, driving lanes with direction and the signals on them, every OSM carriageway with its one-way flag, stops, shop doors on the generated facade with opening hours, and a survey of the traboules and building passages of the centre. Outputs are not committed (OSM tables); the README written with them lists sources and known gaps.

## Limits and next steps

1. **Editor test**: `tools/city/unreal/import_city.py` imports the greybox and
   terrain, checks every mesh against its expected bounds before placing
   anything, and builds `/Game/Maps/L_City_RueDeLaRepublique`. Not run yet:
   the editor is busy (rendering, then Tripo import); it needs one slot.
2. Typology rules are heuristics; the Opéra, Hôtel de Ville, Palais de la
   Bourse and churches need hand-made landmarks (0.14+).
3. Façade kit: build the modules of the two main families (haussmann limestone,
   Lyon render) in Blender, then a PCG graph reading `facade_modules.csv`.
4. Roads, pavements and kerbs from Grand Lyon kerb lines; furniture from the
   Grand Lyon tables (with real models) instead of OSM where both exist.
5. Some 2012 ridge lines that the LiDAR still accepts produce steep gables on
   a few narrow buildings; to check against street photos.

## Scanned surfaces (street realism)

`tools/city/surfaces.json` lists the scanned surfaces of the street: Megascans, free on Fab, 2K, kept out of Git. It covers light granite slabs, limestone ashlar, render, kerb, zinc, slate and asphalt, then setts, sidewalks, tiles, grates and decals.

`python tools/city/surfaces_build.py [--ab]` (Windows PC or Mac, no bash; `--ab` also shoots the street without then with the surfaces into `Saved/CityImport/surfaces_ab/`) or `tools/city/unreal/surfaces_mac.sh` runs two headless launches:
1. `import_surfaces.py` finds each surface in the project, either by a folder named after its id or by its Megascans name. It caps the surface at 2K and writes `Saved/CityImport/surfaces.json`.
2. `apply_city_materials.py` then rebuilds the city materials. Its hooks in `gtp_surfaces.py` layer the textures into:
   - the ground, one layer per mask channel, with markings staying procedural;
   - the facade stone and render, as a detail that keeps each building's hue;
   - the roofs.

Without the manifest, or with `GTP_SURFACES=0`, the materials stay procedural.

M1 budget: the ground uses at most 15 texture fetches (grime included, as a texture rather than decal actors), the kit 4 and the shells 6, all through shared samplers. `GTP_SURFACES_LITE=1` keeps only the colour maps.
