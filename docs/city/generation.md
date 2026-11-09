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

The greybox is a layout check, not an asset: final façades are kit modules
(≤ 50 k triangles each, ADR-0002), instanced; the module list is what carries over.

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
