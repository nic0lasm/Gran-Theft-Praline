# Art bible: Gran Theft Praline

*French version: [art-bible.fr.md](art-bible.fr.md)*
*Version 1, 2026-11-09. Status: in force; numbers marked "starting" are reviewed after the 0.2 measurements.*
*Inputs: [concept](../../design/gdd/game-concept.md), [ADR-0001](../adr/adr-0001-rendering-pipeline.md),
[ADR-0002](../adr/adr-0002-performance-budget.md), [rendering strategy](../rendering/rendering-strategy.md),
[roadmap](../../production/roadmap.md), [asset catalog](../../production/asset-catalog.md),
[reference scene measurements](../rendering/test-results/2026-11-09/README.md).*

---

## 1. What this document is for

One set of rules for everything that ends up on screen: reference photos,
materials, Tripo models, façades, lighting, post-process. Every asset, thread
and visual review refers to it. When another document disagrees on an
artistic matter, the bible wins; the ADRs remain the reference for technical
choices and frame-time budgets.

**The intent in one sentence:** a capture of the game should be mistakable
for a photo taken in rue de la République on an ordinary day.

---

## 2. Visual pillars

| # | Pillar | What it means | What we reject |
| ---- | ---- | ---- | ---- |
| 1 | **Lyon, not a generic city** | Proportions, stone, render, ironwork, paving, shop signs and street furniture of the Presqu'île, surveyed on site | One-size-fits-all "European" façades, catalog furniture unrelated to Lyon |
| 2 | **Light first** | Physical sun and sky at 45.76° N, bounded exposure, contact shadows, Lumen bounce | Flat ambient light, nights that are too bright, pumping exposure |
| 3 | **Calibrated, worn materials** | Albedo without baked light, varying roughness, grime, water stains, edge wear | Clean, uniform materials, constant roughness, raw AI textures |
| 4 | **Real street density** | Cables, signs, bins, two-wheelers, terraces, posters, leaves, chewing gum | Empty, tidy streets, objects lined up with a ruler |
| 5 | **A natural game camera** | Restrained lens, stable TSR, moderate motion blur | Body-camera effects: distortion, heavy grain, chromatic aberration, shake, flares |

**Quality** reference: *Bodycam* (UE5) for scanned materials, Lumen lighting
and density near the camera. **Camera and gameplay** reference: the GTA
model, classic third person with optional first person (Nico's decision,
2026-11-09, ADR-0001). We take **none** of the body-camera effects.

---

## 3. Photographic direction: Lyon

### 3.1 What we reproduce

Vertical slice: north of the Presqu'île, from Bellecour to the Terreaux,
between the Saône and the Rhône. Everything visible in the street is
**reproduced faithfully from field photos**, real shop signs and brands
included (section 3.5).

| Element | Character to capture (to be confirmed on field photos) |
| ---- | ---- |
| 19th-century façades (rue de la République, rue Édouard-Herriot, quays) | 5 to 7-storey apartment buildings, light limestone ashlar (cream to pale ochre), grey hard limestone plinths and steps, continuous wrought-iron balconies on the 2nd and 5th floors, cornices, string courses, zinc and slate roofs with dormers |
| Rendered façades (side streets, towards the Terreaux) | Ochre, yellow, salmon pink and red-ochre render; louvred shutters; eaves; flaking paint in places |
| Ground floors | Continuous shop windows, wooden or aluminium shopfronts, metal shutters, awnings, projecting and fascia signs, terraces |
| Ground | Light slabs on the pedestrian axis, granite setts, stone kerbs, grey road asphalt, manhole covers, tactile strips; Bellecour's red ground |
| Vegetation | Plane trees on the quays and in Bellecour, planters, a few lime trees |
| Landmarks | Hôtel de Ville, Opéra, Bellecour and its equestrian statue, bridges over the Saône and the Rhône: modelled separately (Blender + photos), never generated whole by Tripo |
| Water | Saône and Rhône: green to grey water, matte reflections, landscaped banks |

Field photos (RAW, colour checker in frame, overcast weather for de-lit
textures) go to `assets/` with a row in `assets/SOURCES.md`. A game capture
that contradicts a field photo is wrong.

### 3.2 Light and reference times

Four states are used for every review and measurement (the same states as
the reference scene). **Starting** exposure values in EV100, as the min and
max bounds of auto exposure; they fix the overexposed golden hour and the
too-bright night found on 2026-11-09.

| State | Date and time (Lyon) | Sun | Sky and atmosphere | Exposure EV100 (min – max) | Intent |
| ---- | ---- | ---- | ---- | ---- | ---- |
| **Noon** | June, 1:30 pm | ≈ 68° elevation, due south | Clear sky, slight heat haze | 13.5 – 15 | Crisp shadows, near-white stone, strong but readable contrast |
| **Golden hour** | October, 6 pm | ≈ 6°, west | Graded sky, haze over the Saône | 10 – 12.5 | East-facing façades lit orange, streets in bluish shade, no white sky |
| **Rain** | November, 11 am | hidden | Low overcast, rain, light fog | 10.5 – 12.5 | Wet ground and puddles, shop window reflections, desaturated colours |
| **Night** | January, 10 pm | below the horizon | Dark sky with a slight orange city glow | 2 – 6 | Deep blacks, pools of light from street lamps and windows, lit landmarks |

Rules:
- Physical units: sun in lux, local lights in candela or lumens, IES profiles
  for street lamps.
- Night: mostly warm white LED street lighting (≈ 3000 K), a few remaining
  orange sodium lamps to be checked on site; cooler shop windows; landmark
  lighting in the spirit of Lyon's Plan Lumière.
- Never an invisible "fill" light to lift shadows: if it is too dark, fix the
  exposure or add a real source.

### 3.3 Seasons

The vertical slice is set in **autumn** by default (yellowing plane tree
leaves, low light, frequent rain), which makes the most of wet surfaces and
golden light. The other seasons come with the day-night cycle and weather
(0.13):

| Season | Visible differences |
| ---- | ---- |
| Winter | Bare plane trees, morning fog on the rivers, white sky, festive lights, light frost (no snow in 1.0) |
| Spring | Light green foliage, showers, terraces reopening |
| Summer | Dense foliage, hard shadows, pale dust, full terraces, awnings out |
| Autumn | Leaves on the ground, puddles, low light, heavy skies |

### 3.4 Colour

- Natural palette taken from the photos: cream stone, blue-grey zinc, black
  to dark green ironwork, ochre render, neutral grey asphalt.
- No stylised grading (no tinted LUT, no teal-orange look). Only the white
  balance changes with the light.
- An asset's colour is judged under the neutral lighting of the review scene
  (section 10), not under golden hour light.

### 3.5 Shop signs, brands and text (Nico's decision, 2026-11-09)

The game is for **non-commercial** use: Nico asks for **every shop sign to
be reproduced faithfully**, real brands included (for example the fast-food
sign at the corner of rue de la République and place Bellecour, if the
field photos show it there). This rule replaces the ban on brands in earlier
documents for shop signs and city signage.

| Topic | Rule |
| ---- | ---- |
| Signs of shops, restaurants, banks, pharmacies | Reproduced exactly from field photos: logo, colours, typeface, position, size, night lighting |
| City signage (street name plates, transport, municipal furniture) | Reproduced exactly: real street names, real colours |
| Posters, menus, shop windows | Real or plausible content from photos; no incoherent invented text |
| Logo source | Rectified field photo, or a faithful vector redrawn by hand; never an AI-invented logo when the real one exists |
| Storage | Textures and materials of real signs go into a local content plugin **out of Git**, `Plugins/CityBrands/`, like Epic and Fab content: the public repo is MIT-licensed and cannot relicense trademarks that belong to their owners |
| Registry | Each sign has a row in `assets/SOURCES.md` (source photo, date, address) marked "real brand, non-commercial use" |
| Vehicles | Real models of the fleet on French roads, every make (project lead decision, 2026-11-09; see [vehicles](../gameplay/vehicles.md), section 10); logos and plates as decals, stored like shop signs |
| Unchanged | Real faces and people: forbidden. No asset, character or logo taken from an existing game |

One caution, stated once: non-commercial use lowers the risk but is not
permission from trademark owners. If the game is ever distributed publicly
(downloadable build, monetised video), the contents of `Plugins/CityBrands/`
can be swapped out in one go.

---

## 4. PBR materials

### 4.1 Principles

- **Substrate** (enabled in the project) for every material.
- **One master material per family**, driven by instances. No one-off
  material graphs in production.
- **De-lit, calibrated albedo**: no baked shadow, highlight or occlusion.
  Checked with a colour picker against the colour checker in the photos.
- **Roughness carries realism**: never a constant value; every material has
  variation (grime, wear, dust, water stains).
- **Global wetness**: `Wetness` and `Puddles` parameters in the
  `MPC_Weather` collection, read by every master (darker albedo, lower
  roughness, puddles in heightmap lows).

### 4.2 Value ranges (8-bit sRGB, reference colour checker)

| Material | Albedo | Metallic | Typical roughness |
| ---- | ---- | ---- | ---- |
| Light limestone (façade) | 150 – 210 | 0 | 0.65 – 0.9 |
| Coloured render | 110 – 200 | 0 | 0.7 – 0.95 |
| Dry asphalt | 40 – 70 | 0 | 0.75 – 0.95 |
| Granite setts | 70 – 130 | 0 | 0.55 – 0.85 |
| Painted wrought iron | 20 – 60 | 0 | 0.4 – 0.7 |
| Roof zinc | 120 – 160 | 0.8 – 1 | 0.4 – 0.6 |
| Stainless steel, raw aluminium | 180 – 230 | 1 | 0.2 – 0.5 |
| Shop window glass | Substrate glass, transmission | 0 | 0.0 – 0.1 (+ grime) |
| Vegetation | 40 – 140 | 0 | 0.5 – 0.8 |
| Car paint | Base coat + clear coat (Substrate) | per paint | clear coat 0.02 – 0.1 |

Hard limits: no non-metal albedo below 30 or above 240.

### 4.3 Master materials

| Master | Use |
| ---- | ---- |
| `M_Master_Surface` | Standard opaque: props, furniture, interiors |
| `M_Master_Facade` | Stone and render: tiling textures + trim sheet + vertex wear mask + wetness |
| `M_Master_Ground` | Ground: height blend of 2 to 3 layers, puddles, parallax (M1 tier) |
| `M_Master_Glass` | Shop windows, windows, bus shelters, with grime and reflections |
| `M_Master_CarPaint` | Car bodies: base, flakes, Substrate clear coat |
| `M_Master_Foliage` | Two-sided foliage, transmission |
| `M_Master_Decal` | Substrate decals (DBuffer) |
| `M_Master_Emissive` | Lit signs, screens, shop windows at night, in physical units (nits) |

### 4.4 Resolutions and texel density

Source textures are always made for the **Ultra** tier. The M1 tier scales
them down through streaming and texture LOD groups, never by shrinking the
source file.

| Category | Target density (Ultra) | Max source size | M1 tier (max loaded size, starting) |
| ---- | ---- | ---- | ---- |
| Within reach (ground floor, furniture, props, weapons) | 10.24 px/cm | 4K (8K for first-person weapons) | 2K |
| Façades above the ground floor | 5.12 px/cm | 4K tiling + 4K trim sheet | 2K |
| Ground (setts, slabs, asphalt) | 10.24 px/cm | 4K tiling (8K Megascans kept as master) | 2K |
| Roofs, distant objects | 2.56 px/cm | 2K | 1K |
| Decals (grime, cracks, markings) | per atlas | 4K atlas | 2K |
| Characters (MetaHuman) | MetaHuman standard | standard | low MetaHuman preset |
| Vehicles | 10.24 px/cm | 4K | 2K |

Streaming pool: **1.5 GB** on the M1 tier (ADR-0002); Ultra to be set on an
Ultra machine (starting at 4 GB). Standard maps per material: `_BC` (base
colour), `_N` (DirectX normals, as Unreal uses), `_ORM` (occlusion,
roughness, metallic), `_H` (height) for parallax or tessellation, `_M`
(masks), `_E` (emissive).

### 4.5 Trim sheets and façade kits

- A **façade kit** = modules per bay (ground floor, standard floor, balcony
  floor, attic, roof, corner) on a **50 cm** grid, with storey heights
  measured on site.
- Each stone family has **one 4K trim sheet** (cornices, string courses,
  sills, surrounds, mouldings) and **one 4K tiling texture** for flat
  surfaces, made from our photos.
- Variety comes from instances (tint, dirt, wetness) and decals, not new
  textures.
- Ironwork (balconies, railings): real meshes (Nanite), no opacity mask on
  the Ultra tier; a mask is allowed on the M1 tier's distant LODs.

### 4.6 Decals

- Always on top of a clean material: streaks under sills, grime at the foot
  of walls, chewing gum, oil stains, cracks, road markings, posters,
  graffiti surveyed on site.
- One 4K atlas per family, materials based on `M_Master_Decal` (Substrate
  DBuffer).
- Starting budget: 150 decals visible on screen on Ultra, 60 on M1, with a
  shorter fade distance on M1.

---

## 5. Budgets per category and tier

Reminder (ADR-0002): all content is made for **Ultra** (Nanite, high-quality
Lumen, full-resolution textures); the **M1** tier (30 fps at 1080p on the M1
Pro, no Nanite, no hardware RT) relies on LODs, streaming and settings,
never on simplified sources. **Starting** values, reviewed after the 0.2
measurements.

### 5.1 Geometry

| Category | Ultra: Nanite source mesh (triangles) | M1: max LOD0 (triangles) | M1 LODs | Collision |
| ---- | ---- | ---- | ---- | ---- |
| Small prop (bollard, bin, sign, < 1 m) | ≤ 300 k | 3 k | 3 LODs | simple box or capsule |
| Medium prop (bench, street lamp, table, 1 – 3 m) | ≤ 1 M | 10 k | 4 LODs | simple convex hulls |
| Large prop (bus shelter, kiosk, container) | ≤ 2 M | 30 k | 4 LODs | simple convex hulls |
| Façade module (one bay, one storey) | ≤ 3 M | 50 k (ADR-0002) | 4 LODs + HLOD | boxes, never the full geometry |
| Landmark | ≤ 5 M per piece | 150 k per piece | 5 LODs + HLOD | hand-simplified |
| Tree (plane tree) | Nanite foliage | 60 k | 4 LODs + impostor | trunk only |
| Vehicle (exterior) | ≤ 1.5 M | 120 k | 4 LODs | Chaos Vehicles (physics) |
| Character | MetaHuman | MetaHuman preset | MetaHuman LODs | capsule |
| First-person weapon | ≤ 500 k | 40 k | 3 LODs | none |
| Interior furniture | ≤ 500 k | 8 k | 3 LODs | simple convex hulls |

### 5.2 Texture memory per category (one unique asset)

| Category | Ultra (MB, compressed, mips included) | M1 (MB loaded) |
| ---- | ---- | ---- |
| Small prop | ≤ 22 (one 2K set) | ≤ 6 |
| Medium or large prop | ≤ 45 (one 4K set) | ≤ 12 |
| Façade family (tiling + trim + masks) | ≤ 120 | ≤ 30 |
| Vehicle | ≤ 90 | ≤ 25 |
| First-person weapon | ≤ 120 (8K) | ≤ 25 |

Raw Tripo masters (≈ 1 M vertices, 64 to 74 MB per GLB) are above these
numbers: they are cleaned up and resampled in Blender before import
(section 8).

### 5.3 On-screen density (ADR-0002 reminder, M1 tier)

20 animated characters, 15 moving vehicles, 8 shadow-casting local lights
and up to 100 unshadowed ones at night. The Ultra tier has no fixed ceiling
until an Ultra machine has been measured.

---

## 6. LODs, Nanite and HLOD

1. **Every opaque static mesh** is imported with Nanite enabled (Ultra tier)
   **and** a hand-checked LOD chain (M1 tier). The Nanite fallback mesh is
   tuned to stay within the M1 LOD0 of table 5.1.
2. About 50 % reduction per LOD; starting screen sizes 1.0 / 0.5 / 0.25 /
   0.1. No visible popping in the reference scene's 3 fixed shots.
3. M1 LOD sources: Unreal reduction for simple meshes; Tripo **Smart Mesh**
   light variant or Blender retopology for Tripo props; MetaHuman LODs for
   characters.
4. **Translucent and skinned meshes**: always LODs (no Nanite).
5. **World Partition HLODs** beyond about 150 m; impostors for distant trees.
6. Sett and wall relief uses Nanite tessellation on Ultra and parallax (`_H`)
   on M1.

---

## 7. Naming and Content/ folders

### 7.1 Prefixes

| Prefix | Type | Prefix | Type |
| ---- | ---- | ---- | ---- |
| `SM_` | Static mesh | `SK_` | Skeletal mesh |
| `M_` | Master material | `MI_` | Material instance |
| `MF_` | Material function | `MPC_` | Material parameter collection |
| `T_` | Texture | `BP_` | Blueprint |
| `L_` | Level (e.g. `L_RueRepublique`) | `LI_` | Level Instance |
| `PCG_` | PCG graph | `NS_` | Niagara system |
| `DA_` | Data Asset | `DT_` | Data table |
| `IA_`, `IMC_` | Input (already in place) | `ABP_`, `AM_` | Anim Blueprint, montage |
| `PP_` | Post-process volume or settings | `IES_` | Photometric profile |

Pattern: `Prefix_Family_Name_Variant`, in English, PascalCase, no spaces or
accents. Examples: `SM_Bench_WoodCastIron_A`, `MI_Facade_LimestoneCream_Dirty`,
`T_Facade_LimestoneCream_BC`, `SM_Facade_Rep_Bay3_Floor2`.
Texture suffixes: `_BC`, `_N`, `_ORM`, `_H`, `_M`, `_E`.

The Tripo catalog code (`W1-01`…) is not part of the name: it is recorded in
`assets/SOURCES.md` and in the asset's metadata (`CatalogId` tag).

### 7.2 Folders

One folder per asset (mesh, instances and textures together), grouped by
family rather than by Tripo wave:

```
Content/
  Maps/                         L_RueRepublique, future levels
  Characters/Player/            (in place) GASP children, camera
  Characters/NPC/
  Input/                        (in place) IA_*, IMC_*
  Materials/Masters/            M_Master_*
  Materials/Functions/          MF_*
  Materials/Parameters/         MPC_Weather, MPC_TimeOfDay
  Environment/Facades/<Kit>/    modules, trim sheets, instances
  Environment/Ground/<Material>/
  Environment/Props/<Family>/<Asset>/       e.g. Props/StreetFurniture/Bench_WoodCastIron/
  Environment/Vegetation/<Species>/
  Environment/Landmarks/<Landmark>/
  Environment/Decals/<Family>/
  Interiors/<ShopType>/
  Vehicles/<Vehicle>/
  Weapons/<Weapon>/
  Lighting/IES/   Lighting/Sky/
  PostProcess/
  FX/   UI/   Audio/
Plugins/ (out of Git, local)
  GameAnimationSample/          Epic content (already in place)
  FabContent/                   Megascans and other Fab assets
  CityBrands/                   real shop signs and brands (section 3.5)
```

This layout replaces the catalog's `Content/Props/<wave>/`.

---

## 8. Tripo charter

It applies to every wave of the [catalog](../../production/asset-catalog.md).

### 8.1 What Tripo does and does not do

| Yes | No |
| ---- | ---- |
| Street furniture, props, carried objects, interior furniture and products, façade ornaments, car body drafts | Façades and buildings, ground, characters, whole landmarks, final vehicles without Blender rework, real logos and signs (field photos, section 3.5) |

A Tripo model may go **close to the camera** if it passes review (section 10)
after Blender cleanup; otherwise it stays in the background.

### 8.2 Prompt template

Prompts **in English**, one entity per generation, in this order:

```
<precise object>, <materials and colours>, <condition: wear, dirt, rust>,
<context: street furniture of Lyon, France, 2020s>,
photorealistic, real-world proportions, single object, isolated,
standing upright on flat ground, front view facing camera,
no base plate, no ground, no background, no people,
even neutral lighting, no baked shadows, PBR materials
```

W1-01 example:
`public bench with wooden slats and anthracite cast-iron legs and armrests, weathered wood, slightly dusty, street furniture of Lyon, France, 2020s, photorealistic, real-world proportions, single object, isolated, standing upright on flat ground, front view facing camera, no base plate, no ground, no background, no people, even neutral lighting, no baked shadows, PBR materials`

Rules:
- Reuse the notes and dimensions of the catalog row; for typical Lyon
  objects, give a reference image (field photo first, otherwise Flux or GPT
  Image).
- Text and logos: ask for `no text`; real lettering is added afterwards as a
  decal from a photo.
- Studio settings: maximum quality, PBR textures at the highest resolution,
  master generated without a polygon limit; the light variant goes through
  Smart Mesh.
- Several variants per object, keep the best; the others are not exported.

### 8.3 Scale, pivot and orientation (Blender cleanup)

| Rule | Value |
| ---- | ---- |
| Units | Metres in Blender (unit scale 1.0); centimetres in Unreal after import |
| Scale | Catalog dimensions (Size column) within ± 2 %; Tripo does not guarantee scale, so it is always set in Blender |
| Pivot | On the ground, at the centre of the footprint. Wall-mounted objects (bracket lamp, air conditioner, ATM, plate): at the centre of the back face, on the wall plane |
| Orientation in Unreal | Front facing **+X**, up +Z, so that manual and PCG placement rotate correctly (to be checked on the first wave 1 import) |
| Transforms | Applied (rotation 0, scale 1) before export |
| Geometry | No internal faces or floating parts; real openings (grilles, perforations) as geometry for Ultra |
| UVs | Channel 0 for textures; textures rebaked at the section 4.4 density |
| Materials | Replaced by an instance of the matching master; Tripo textures are a starting point, recalibrated (de-lit albedo, varied roughness) |
| Outputs | Master GLB: `assets/tripo/<wave>/<id>.glb` (out of Git). Unreal: `Content/Environment/Props/<Family>/<Asset>/` |

---

## 9. Post-process

Goal: a restrained photographic lens, not a body camera.

| Setting | Starting value | Note |
| ---- | ---- | ---- |
| Anti-aliasing | TSR always on | Internal resolution set by ADR-0002 (67 % on M1) |
| Exposure | Auto, histogram, section 3.2 bounds per state | Slow adaptation (≈ 1 s to bright, 3 s to dark) |
| Local exposure | On, moderate | Avoids crushed shadows without flattening |
| Tonemapper | Unreal's default filmic | No stylised LUT |
| White balance | Per state (warmer at golden hour) | Never a uniform tint (the sepia night of 2026-11-09 is a defect) |
| Bloom | Low (intensity ≈ 0.3) | Only on real light sources |
| Motion blur | Moderate (amount ≈ 0.3, max 2 %) | Stronger when driving fast |
| Vignette | Light (≈ 0.2) | |
| Depth of field | Off in gameplay | Cutscenes and photo mode only |
| Grain | Off | Trial at a very low dose (≤ 0.1) only if captures show colour banding |
| Chromatic aberration, distortion, lens flares, lens dirt | Off | Body-camera effects excluded (Nico's decision) |
| Fog | Exponential Height Fog + volumetric | Denser in the morning near the rivers |

Values live in `Content/PostProcess/` (one `PP_` per state and per tier);
they are measured in the reference scene after each change.

---

## 10. Visual review and validation

No asset reaches `next` without a review with a capture:

1. **Review scene**: the asset under neutral lighting (overcast sky, fixed
   EV) next to a grey chart and a virtual colour checker.
2. **Checks**: scale (1.80 m mannequin next to it), pivot, orientation,
   albedo within range, varied roughness, no baked shadow, LODs without
   popping, collision, cost within section 5 budgets.
3. **In context**: capture in rue de la République under the 4 states, next
   to a field photo of the same kind of object or the same spot.
4. **Grid** (rendering strategy, section 7): exposure, materials, shadows,
   reflections, wetness, density scored 1 to 5 against the photos; average
   ≥ 4, no item below 3.
5. **Registry**: row in `assets/SOURCES.md`, status in the catalog.

---

## 11. Changes

- Revised after the 0.2 measurements (choice of M1 tier levers) and after
  the first batch of wave 1 is integrated.
- Any rule that touches the visual intent (pillars, shop signs, camera) is
  changed by Nico's decision; starting numbers are updated from
  measurements, with the date.
