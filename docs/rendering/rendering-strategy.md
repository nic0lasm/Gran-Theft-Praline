# Rendering strategy: photorealism in Gran Theft Praline

*French version: [rendering-strategy.fr.md](rendering-strategy.fr.md)*
*Decisions: [ADR-0001](../adr/adr-0001-rendering-pipeline.md), [ADR-0002](../adr/adr-0002-performance-budget.md)*
*Last verified: 2026-11-09, Unreal Engine 5.8.3*

## 1. Recommendation in short

1. Use the stock Unreal Engine 5.8 stack: **Lumen (software ray tracing on
   Mac), Nanite, Virtual Shadow Maps, Substrate, TSR**, physical sun and sky.
   No custom renderer.
2. Make photorealism come from **content quality**: de-lit, calibrated
   materials from our own photos of Lyon, a modular Nanite façade kit,
   dense street dressing, wear and wetness. This is where most of the
   difference between "UE5 demo" and "real street" lies.
3. **Do not ship raw city scans** (photomesh, Google 3D Tiles, Gaussian
   splats): they are references and blocking volumes.
4. Generative tools work **upstream** (concept, signage, decals, blockouts),
   never as final hero geometry.
5. Prove it on one **reference street scene** in the vertical slice district
   before producing anything at scale, with captured numbers and images.

## 2. What actually makes an image look real

In order of impact for a city at street level:

1. **Lighting coherence**: physical sun and sky intensities, correct exposure,
   bounce light (Lumen), soft contact shadows (VSM). A wrong exposure ruins
   good assets.
2. **Material calibration**: albedo without baked light, believable roughness
   variation, wetness. Most "CG look" comes from flat roughness and clean
   albedo.
3. **Geometric density and imperfection**: chamfered edges, cornices,
   window depth, cobbles with displacement, kerbs that are not perfectly
   straight (Nanite makes this affordable).
4. **Dressing and storytelling clutter**: signs, cables, bins, scooters,
   stains, posters, café terraces, leaves.
5. **Camera**: TSR stability, restrained post-process, natural lens.
6. **Life**: people, traffic, wind, particles, reflections in wet asphalt.

## 3. Renderer features: Mac versus PC

| Feature | Role | Apple Silicon (UE 5.8, Epic) | Windows + RTX-class GPU | Our use |
| ---- | ---- | ---- | ---- | ---- |
| Lumen, software RT | Dynamic GI and reflections | Supported on M1+ | Supported | **Baseline** |
| Lumen, hardware RT | Sharper reflections, better thin objects | Experimental, M2+ | Production | PC quality tier; Mac only if stable in test |
| Lumen Lite (Beta) | About 2× cheaper GI | Expected to work (to verify) | Supported | Performance mode, low scalability |
| Nanite | Unlimited static detail | Beta, M2+ (not M1) | Production | **All static opaque meshes** |
| Virtual Shadow Maps | Sharp, stable shadows | Supported with Nanite | Production | **Sun and key lights** |
| MegaLights | Hundreds of shadowed lights | Experimental, M2+ | Production | Night streets, if it passes on Mac |
| Substrate | Layered physical materials | Supported (on in project) | Production | **All materials** |
| TSR | Temporal upscaling and AA | Supported | Production | **Always on**; MetalFX evaluated later |
| Path tracer | Ground-truth reference renders | Not available | Available | PC only, for lighting reference stills |
| Volumetric clouds, fog | Weather, atmosphere | Supported | Supported | **Yes** |

Consequence: a recent Mac (M2 or newer, ideally Pro or Max tier with 32 GB or
more) can author and run the full look with software Lumen. A Windows PC with
an RTX-class GPU adds hardware ray tracing, the path tracer for reference and
the main future player platform. Whether to buy one is a decision for the
project lead, informed by the test scene numbers.

## 4. Asset sources

| Content | Primary source | Secondary | Not used in game |
| ---- | ---- | ---- | ---- |
| Façades (Presqu'île 19th-century stone and render buildings) | Modular kit in Blender: bays, cornices, balconies, shutters, shopfronts; trim sheets and tiling materials **made from our own photos** of the district | Fab façade kits, reworked to Lyon proportions | Grand Lyon photomesh, Google 3D Tiles (reference and blocking only) |
| Ground: cobbles, asphalt, pavements, kerbs | Megascans / Fab surfaces + our photo-scanned Lyon pavements (RealityScan) | Procedural Substrate layers for wear and wetness | |
| Street furniture, props | Fab and Megascans, then made fictional (no brands) | Tripo blockouts finished in Blender | |
| Landmarks (Hôtel de Ville, Opéra, fountains, statues) | Hand-modelled in Blender from photos and plans; photogrammetry of details | | |
| Vegetation (plane trees on the quays, Bellecour) | Fab / Megascans plants, Nanite foliage | | |
| Characters | MetaHuman (in-editor creator, supported on macOS in 5.8), fictional faces only | | Scans of real people |
| Vehicles | Fictional designs: Tripo or concept blockout, hard-surface in Blender, Chaos Vehicles, Substrate car paint | Fab vehicle bases, debadged and redesigned | Real makes, logos, plates |
| Sky and lighting | Sky Atmosphere, Volumetric Clouds, height fog, real sun position | HDRIs (Poly Haven, CC0) for look-dev only | HDRI as game sky |
| Signs, posters, menus | Flux / GPT Image, then cleaned in an image editor | | Real brands |

**Photo capture in Lyon** (field days by the project lead or anyone on
site): overcast days for de-lit textures, a colour checker card in frame,
RAW photos, 60 to 80 % overlap for photogrammetry of details. Output goes to
`assets/` with a row in `assets/SOURCES.md`.

**Licences to check per asset**: Fab Standard Licence (usable in any engine),
older Megascans downloads under the Epic Content Licence (Unreal only),
Poly Haven CC0, Grand Lyon open data under Licence Ouverte 2.0 with the
mandatory credit, IGN under Licence Ouverte. Megascans pricing and terms have
changed several times since 2024: read the licence on Fab when downloading.

## 5. Where generative tools fit

| Tool | Good for | Not for |
| ---- | ---- | ---- |
| Flux 3 Max, GPT Image | Mood boards and key frames of target shots; fictional shop signs, logos, posters, menus, graffiti; decal and grunge bases; texture ideas | Final PBR maps straight from the model (no reliable de-lighting, tiling or roughness); building façades |
| Tripo | Fast blockout of props and vehicle shapes; background clutter after cleanup | Hero assets, buildings, anything near the camera without retopology, UVs and proper materials in Blender |
| Blender | Façade kit, landmarks, vehicles, cleanup of every generated mesh | |

Every generated asset is logged in `assets/SOURCES.md` (tool, prompt, date).

## 6. Light, sky and weather

- Sun position from date, time and Lyon's coordinates (SunPosition plugin).
- Sky Atmosphere + Volumetric Clouds + Exponential Height Fog, driven by one
  weather controller: cloud cover, rain intensity, wetness, puddle level,
  fog density, wind.
- Wetness is a global material parameter blended in the master materials
  (darker albedo, lower roughness, puddles in heightmap lows).
- Night: street lights with measured photometric profiles (IES), shop window
  emissive light, MegaLights if validated.
- Exposure: bounded auto-exposure in EV100; daylight around EV 13 to 15,
  night street around EV 3 to 6.

## 7. Reference street scene: test plan

### Location

A segment of about 150 m of **rue de la République** in the northern
Presqu'île: wide street, continuous 19th-century façades, shopfronts, paving
and street furniture, both pedestrian and vehicle space nearby. It sits in the
vertical slice district.

### Content

| Item | Quantity |
| ---- | ---- |
| Façade kit modules | 8 to 12, forming both sides of the street |
| Ground materials | 4 (paving, asphalt, kerb, cobbles) with wetness |
| Props | about 50 unique, 300 instances (bins, benches, bollards, signs, scooters, terrace chairs) |
| Street lights | 20, IES profiles |
| Vehicles | 3 fictional, parked and one driven |
| Characters | 10 MetaHumans walking |
| Trees | 6 plane trees |

### Lighting states

1. Clear noon in June.
2. Golden hour in October.
3. Overcast with rain and wet ground.
4. Night with street and shop lighting.

### Measurement protocol

- A Sequencer camera path of 60 seconds at street level plus 3 fixed shots,
  identical for every run.
- Run in a packaged Development build (not in the editor) at 2560×1440,
  Epic scalability, TSR at 67 %.
- Capture with the CSV profiler (`csvprofile start` and `csvprofile stop`)
  and Unreal Insights for one run per state; `stat gpu` screenshots for the
  fixed shots.
- Images: Movie Render Queue stills of the 3 fixed shots per lighting state,
  placed side by side with our photos of the same spots.
- Repeat on any Windows PC available, with hardware ray tracing on and off.

### Pass criteria

| Metric | Threshold |
| ---- | ---- |
| GPU frame time, 95th percentile, every state | ≤ 33.3 ms (ADR-0002) |
| Longest frame during the path | ≤ 100 ms |
| GPU-visible memory | ≤ 60 % of unified memory |
| Visual checklist (exposure, materials, shadows, reflections, wetness, density), scored 1 to 5 against the photos | Average ≥ 4, no item below 3 |
| Stability | No crash or visible artefact (Nanite holes, Lumen light leaks, TSR ghosting) in the 3 fixed shots |

Results are written in `docs/rendering/test-results/` with the captures and
numbers, before anything is merged into the integration branch.

### Steps

1. Greybox the street from the geodata (footprints and heights) and set the
   lighting rig and post-process.
2. Field photos of the segment and material calibration.
3. Façade kit v1 and ground materials.
4. Dressing, vehicles, characters, weather controller.
5. Measure, compare with the photos, tune, re-measure.

## 8. Cost

| Item | Cost |
| ---- | ---- |
| Unreal Engine, MetaHuman, RealityScan, Blender | Free (Unreal royalties only above USD 1 million revenue) |
| Fab and Megascans assets | Free to paid per asset; budget to be set when needed |
| Poly Haven HDRIs and textures | Free (CC0) |
| Colour checker card for photo capture | About 30 to 100 € |
| Windows PC with RTX-class GPU (optional) | About 2,000 to 3,500 € depending on GPU tier; decision deferred to the test scene numbers |
| Time | The façade kit and the test scene are the largest effort: several weeks for one person |

## 9. Sources

- Epic, UE 5.8 release notes: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes
- Epic, macOS development requirements: https://dev.epicgames.com/documentation/unreal-engine/macos-development-requirements-for-unreal-engine
- Epic tech blog, Unreal Engine on macOS feature parity: https://www.unrealengine.com/en-US/tech-blog/bringing-unreal-engine-on-macos-up-to-feature-parity-with-windowsprogress-report
- MetaHuman 5.8 release notes: https://dev.epicgames.com/documentation/metahuman/metahuman-5-8-release-notes-in-unreal-engine
- MetaHuman licence: https://www.metahuman.com/license
- Grand Lyon textured 3D models (data.gouv.fr): https://www.data.gouv.fr/datasets/611621d83eed0bb631f343d0
- Epic forum, Gaussian splatting support: https://forums.unrealengine.com/t/official-support-for-gaussian-splat-rendering/1767323
