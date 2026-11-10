# Free assets for Unreal: survey

*Version française : [free-assets.fr.md](free-assets.fr.md)*
*Surveyed on 2026-11-10, every page opened that day. Prices and licences change: re-read the listing at download time and log it in [`assets/SOURCES.md`](../../assets/SOURCES.md).*
*Complements the [sourcing survey](../sourcing/asset-sourcing.md) (tools, frameworks, geodata) and follows the [art bible](art-bible.md).*

Goal: stop generating (Tripo, 55 credits a model) what already exists for
free, starting with what the player sees on rue de la République (ground,
façades, street furniture, signs), then water, foliage and vehicles.

## 1. Summary

1. **Megascans is entirely free on Fab, with no end date** (announced on
   2026-11-05, every listing drops to USD 0 by 2026-11-12). Fab Standard
   licence: any engine, no attribution, no redistribution of the files on
   their own, so **out of Git** like all our assets. This is source no. 1 for
   ground, surfaces, decals and generic street furniture. It needs Nico's
   Epic account.
2. **Foliage: free Megascans trees, but no free plane tree found.** Free and
   checked: European Beech (17 trees), European Black Alder (22 trees),
   Megaplants Norway Maple (Forest and Saplings). The only real plane tree
   pack found is paid (Greenleaf Vision, 23 Platanus presets, USD 29.99,
   UE 5.8, experimental Nanite Foliage).
3. **Water: nothing to download.** Roadmap decision (2026-11-10): our own
   mesh with the `M_City_Water` material. The engine's Water plugin is ruled
   out (extra passes, over the M1 budget; simulation bake crashes on Mac,
   Metal bug reported with no fix).
4. **Without any account, threads can fetch Poly Haven (CC0) and ambientCG
   (CC0) on their own**: 521 models, 869 textures and 997 HDRIs on Poly
   Haven, 2,041 materials on ambientCG (counts read from their APIs on
   2026-11-10).
5. **Epic packs not to take now**: City Sample (free, updated for 5.8, but
   American, very heavy, Unreal-only licence), City Sample Vehicles (13 US
   cars), Electric Dreams (nature, needs DirectX 12), Paragon and Infinity
   Blade (fantasy), PCG Biome (experimental, no urban assets).
6. **Proposed rule**: before any new Tripo generation, search Megascans
   first, then Poly Haven and ambientCG. Tripo stays for what is specific to
   Lyon (bollards, TCL shelters, Vélo'v, kiosks, city benches) and that no
   library has.

## 2. Packs

M1 cost = estimate for the M1 profile (target p95 ≤ 42 ms, ADR-0002), to be
measured before anything lands on next.

### 2.1 Fab and Epic (Epic account required)

| Pack | Link | Licence | Size | UE 5.8 / Mac | Covers | Estimated M1 cost | Verdict |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Quixel Megascans (whole library) | [Quixel seller on Fab](https://www.fab.com/sellers/Quixel%20Megascans), [news](https://www.cgchannel.com/2026/11/epic-has-made-megascans-free-again-for-good-this-time/) | Fab Standard: any engine, no attribution, no redistribution of files alone; "usage with AI: yes" | Per asset (tens to hundreds of MB) | Yes, through the editor's Fab plugin | Cobbles, asphalt, stone, plaster, decals (cracks, markings, covers), generic furniture and litter, plants | Low with Nanite and textures capped at 2K on M1; watch decal overdraw | **Take now** |
| European Beech (Megascans Trees) | [fab.com/s/5db64af5b801](https://fab.com/s/5db64af5b801) | Free, Megascans | Not shown | "Unreal Engine", version not stated | 17 trees (8 forest, 1 field, 4 saplings, 4 seedlings), wind and seasons, LODs | Medium: masked foliage, VSM shadows | **Take** (parks, quays) |
| European Black Alder (Megascans Trees) | [fab.com/s/8f6f8b0282d1](https://fab.com/s/8f6f8b0282d1) | Free, Megascans | Not shown | Same | 22 trees, wind and seasons, LODs | Medium | **Take** (river banks) |
| Megaplants: Norway Maple Forest and Saplings | [Forest](https://www.fab.com/listings/e789ad04-cf79-4973-adb0-85c794d77144), [Saplings](https://www.fab.com/listings/e79a64ce-2291-4505-ba8e-d2fdf20791e8) | Free, Megaplants | Not shown | Needs the PVE plugin and "Nanite Foliage (Experimental)" | Norway maple, editable presets: a leafy street tree until we have plane trees | Unknown, to measure: experimental Nanite Foliage | **Take and test** |
| PVE Broadleaf Park Tree Presets (Greenleaf Vision) | [Fab listing](https://www.fab.com/listings/e141228c-0db1-4560-9dbe-48067e8ba2d5) | **Paid USD 29.99** | 7 GB | UE 5.8 required, experimental PVE + Nanite Foliage | 23 plane trees + 19 lindens, leafless winter variants, wind, seasons | Unknown, to measure | **Purchase to decide** after the Megaplants test |
| Water plugin (engine) | [Epic docs](https://dev.epicgames.com/documentation/unreal-engine/water-system-in-unreal-engine) | Engine | 0 (installed) | Plugin listed as experimental; simulation bake crashes on Mac ([thread](https://forums.unrealengine.com/t/ue-v-5-6-1-water-simulation-bake-crash-on-macos-15-6-1-metal-apple-m3-ultra-28-core-cpu-60-core-gpu-32-core/2656076)); river vanishes in LiveSim on 5.8.3 ([thread](https://forums.unrealengine.com/t/wateradvanced-shallowwaterrivercomponent-no-valid-sinks-warning-and-river-disappears-in-5-8-3/2840450)) | Rhône and Saône (river spline, waves, buoyancy, swimming later) | Medium to high (Single Layer Water, extra passes) | **Ruled out** (roadmap 2026-11-10): own mesh + `M_City_Water` |
| Procedural Vegetation Editor (engine) | [Epic docs](https://dev.epicgames.com/documentation/unreal-engine/procedural-vegetation-editor-in-unreal-engine) | Engine | 0 | Experimental; PVE 5.7 files do not open in 5.8 | Grow our own plane trees (Grower node); Nanite Foliage output + wind | Unknown | **Test** alongside Megaplants |
| City Sample (5.8) | [fab.com/s/5e8f5eda64d8](https://fab.com/s/5e8f5eda64d8) | Free, Unreal-only content | Very heavy (several tens of GB) | Updated for 5.8 (PCG building graphs) | PCG city study; American style | High | **Later**, study only |
| City Sample Vehicles | [fab.com/s/62cf08ebaf2e](https://fab.com/s/62cf08ebaf2e) | Free, Unreal-only | Not shown | Unreal | 13 drivable American vehicles | Medium | **No**: our fleet is French |
| Electric Dreams | [Epic docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/electric-dreams-environment-in-unreal-engine) | Free, Unreal | Heavy | Needs DirectX 12, 32 GB RAM: not for our Mac | Nature, cliffs, creek | High | **No** |
| PCG Biome Core and Sample | [Epic docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-overview-guide-in-unreal-engine) | Engine | 0 | Experimental | Placement tool, no urban assets | Low | **No** for now |
| Paragon, Infinity Blade | [Epic free content](https://dev.epicgames.com/documentation/en-us/unreal-engine/free-epic-games-content-for-unreal-engine) | Free, Unreal | Heavy | Varies | Fantasy and sci-fi | — | **No**: off style |
| Limited-time free | [fab.com/limited-time-free](https://www.fab.com/limited-time-free) | Fab | — | — | Until 17/11: Underground Tunnel, SciFi Beast, platformer kit | — | **Nothing useful** this cycle; check every two weeks |

Paid packs seen along the way, for the record: Paris Dauphine Street
(USD 249.99, UE 5.4, full Parisian avenue), Street Prop Pack Nanite
(USD 39.99, 42 generic street items), Torino Street Props (USD 3.99, no
Unreal format).

### 2.2 Open sources (no account, threads download on their own)

| Source | Link | Licence | Checked content | Covers | Estimated M1 cost | Verdict |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Poly Haven | [polyhaven.com](https://polyhaven.com), [licence](https://polyhaven.com/license) | CC0, no attribution, redistribution allowed | 521 models, 869 textures, 997 HDRIs. Useful examples: `street_lamp_01/02`, `metal_trash_can`, `water_manhole_cover`, `concrete_road_barrier(_02)`, `modular_street_seating`, `outdoor_table_chair_set_01`, `bar_chair_round_01`, `planter_box_01..03`, `potted_plant_01/02/04`, `trashbag`, `cardboard_box_01`, `plastic_crate_02/03`; textures `cobblestone_*` (32), `asphalt_*` (22), plasters (73), `granite_tile_*` | Café terraces, litter, potted plants, look-dev HDRIs | Low (light meshes, go through our `prepare_props.py`) | **Take** |
| ambientCG | [ambientcg.com](https://ambientcg.com), [licence](https://docs.ambientcg.com/license/) | CC0, no attribution | 2,041 materials, including 151 asphalts, 39 plasters, 31 façades, a Paving Stones category | Ground and plaster variants | None (textures) | **Take** as needed |
| Sketchfab | [sketchfab.com](https://sketchfab.com) | Per model (CC0, CC BY with credit, CC BY-NC…) | Quality varies widely | Items found nowhere else | Varies | **Case by case**, free account needed; CC0 and CC BY only |
| Quaternius | [quaternius.com](https://quaternius.com) | Not checked | Stylised / low-poly packs | — | — | **No**: off photoreal style |

## 3. What Nico clicks (Epic account), step by step

One session of about 15 minutes on the Mac. Everything is free, no card.

1. Open [fab.com](https://www.fab.com) and sign in with the Epic account.
2. Open each listing below and click **"Add to My Library"** (nothing is
   downloaded yet):
   1. [Megaplants: Norway Maple Forest](https://www.fab.com/listings/e789ad04-cf79-4973-adb0-85c794d77144)
   2. [Megaplants: Norway Maple Saplings](https://www.fab.com/listings/e79a64ce-2291-4505-ba8e-d2fdf20791e8)
   3. [European Beech](https://fab.com/s/5db64af5b801)
   4. [European Black Alder](https://fab.com/s/8f6f8b0282d1)
3. Megascans for the street: on the [Quixel seller page](https://www.fab.com/sellers/Quixel%20Megascans),
   search and add to the library what comes up for these words (3 to 5
   results per word, those that look like the Presqu'île): `granite
   cobblestone`, `granite curb`, `asphalt`, `road marking`, `manhole`,
   `drain`, `crack decal`, `limestone`, `plaster`, `cigarette`, `leaves
   decal`, `traffic cone`, `road barrier`, `trash bag`, `cardboard`. The
   remaining listings become free by 12/11: if a price still shows, retry
   on 13/11.
4. Say "done" in the thread: a thread then imports from the Fab library into
   the project (editor Fab window, "Add to project"), with the editor and the
   Epic session open on the Mac, out of Git, one line per asset in
   `SOURCES.md`.

Nothing to click for water (own mesh) or for Poly Haven / ambientCG.

## 4. What threads do on their own

- Fetch the Poly Haven and ambientCG models and textures above by script,
  run them through `tools/assets` (normalisation, triangle caps, M1 LODs),
  out of Git.
- Once the trees are in the project: give leaves back to the street's 112
  trees with `tools/city/unreal/run_tree_swap.sh` (street walk with bare
  then leafy trees, automatic return to the bare trees if the M1 p95 goes
  above 42 ms or rises by more than 4 ms). Megaplants are PVE presets that
  must be turned into a mesh first; the "Field" beech is the first try. If it holds the budget, offer Nico the
  Greenleaf plane trees (USD 29.99) on a card.
- Cross the remaining Tripo catalogue with Megascans and Poly Haven and drop
  what already exists for free.

## 5. Licence reminders

- Fab Standard (Megascans): use in a public non-commercial project allowed,
  no attribution, **never asset files in the public repo**. "UE-only"
  content (City Sample): only inside Unreal.
- CC0 (Poly Haven, ambientCG): no constraint; still kept out of Git for size.
- CC BY (Sketchfab): credit required in `SOURCES.md` and in the game credits.
- Any real brand or face stays under the art bible (section 3.5) and the
  working rules.

## 6. Sources

- Megascans free: [CG Channel, 2026-11-06](https://www.cgchannel.com/2026/11/epic-has-made-megascans-free-again-for-good-this-time/), [Creative Bloq](https://creativebloq.com/news/the-worlds-biggest-library-of-scanned-assets-for-3d-artists-is-now-free-and-it-seems-that-this-time-its-for-good)
- Fab licence: [fab.com/eula](https://www.fab.com/eula)
- Fab listings opened on 2026-11-10: see links in table 2.1
- Water plugin: [docs](https://dev.epicgames.com/documentation/unreal-engine/water-system-in-unreal-engine), forum threads linked in the table
- PVE: [docs](https://dev.epicgames.com/documentation/unreal-engine/procedural-vegetation-editor-in-unreal-engine), [Greenleaf announcement](https://forums.unrealengine.com/t/greenleaf-vision-pve-procedural-vegetation-broadleaf-park-tree-presets/2740042)
- Poly Haven: [licence](https://polyhaven.com/license), API `api.polyhaven.com/assets`
- ambientCG: [licence](https://docs.ambientcg.com/license/), API `ambientcg.com/api/v2/full_json`
