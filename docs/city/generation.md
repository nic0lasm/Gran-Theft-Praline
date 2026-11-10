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
