# ADR-0001: Photorealistic rendering pipeline

*French version: [adr-0001-rendering-pipeline.fr.md](adr-0001-rendering-pipeline.fr.md)*

## Status

Proposed. Becomes Accepted once the reference street scene
([test plan](../rendering/rendering-strategy.md#7-reference-street-scene-test-plan))
passes on the development MacBook Pro (M1 Pro).

## Date

2026-11-09

## Last Verified

2026-11-09 (Unreal Engine 5.8.3, macOS requirements page and 5.8 release notes)

## Decision Makers

nic0lasm (project lead)

## Summary

How Gran Theft Praline reaches photorealism in Unreal Engine 5.8 on an Apple
Silicon Mac: which renderer features we use, how assets are sourced and built,
and where generative tools fit. The answer is the stock UE5 "next-gen" stack
(Lumen, Nanite, Virtual Shadow Maps, Substrate, TSR) with software ray tracing
as the Mac baseline, fed by a modular, scan-based Lyon façade kit rather than
by raw city scans.

## Engine Compatibility

| Field | Value |
| ---- | ---- |
| **Engine** | Unreal Engine 5.8 |
| **Domain** | Rendering |
| **Layer** | Foundation |
| **Knowledge Risk** | HIGH (5.8 is recent; Mac support for several features is Beta or Experimental) |
| **References Consulted** | UE 5.8 release notes; Epic "macOS development requirements" page; Epic tech blog "Bringing Unreal Engine on macOS up to feature parity with Windows" |
| **Post-Cutoff APIs Used** | MegaLights (production-ready in 5.8), Lumen Lite (Beta in 5.8) |
| **Verification Required** | LOD/fallback rendering of Nanite-enabled meshes, VSM cost, Lumen software and Lumen Lite on the M1 Pro, measured in the reference street scene |

## ADR Dependencies

| Field | Value |
| ---- | ---- |
| **Depends On** | None |
| **Enables** | ADR-0002 (performance target and budget), art bible, city reproduction study |
| **Blocks** | Final-quality art production for the vertical slice |
| **Ordering Note** | Greybox and gameplay prototyping can start before acceptance |

## Context

### Problem Statement

The concept targets visual parity with the best current open-world games on a
faithful Lyon. Photorealism in a real-time open world comes less from one
feature than from consistent physically based light, dense geometry, and
believable, varied, worn materials. We must pick a pipeline that a solo
developer with generative tools can feed, and that runs on Apple Silicon.

### Current State

`GranTheftPraline/Config/DefaultEngine.ini` already enables Lumen GI and
reflections, Virtual Shadow Maps, mesh distance fields, ray tracing support
and Substrate, with SM6 shaders on Metal. No content exists yet.

### Constraints

- Development and first target: macOS on Apple Silicon (Metal, SM6).
- Mac feature levels (Epic, UE 5.8): Lumen with software ray tracing on M1+;
  Nanite on M2+ (Beta); Lumen hardware ray tracing and MegaLights on M2+
  (Experimental); no path tracer on Mac.
- Development and reference machine (decided 2026-11-09): MacBook Pro
  M1 Pro, 16-core GPU, 16 GB unified memory, no hardware purchase. **It does
  not run Nanite** (M2+ only) nor hardware ray tracing: see "M1 Pro profile"
  below.
- No real brands or people; every asset logged in `assets/SOURCES.md`.
- Solo developer: the pipeline must favour reusable kits and procedural
  placement over unique hand-made buildings.

### Requirements

- Physically based sun, sky and weather for Lyon's latitude (45.76° N).
- Dense static geometry (façades, cornices, cobbles, street furniture).
- Wet surfaces, puddles, rain, fog, night street lighting.
- Characters at MetaHuman level, vehicles with layered car paint.
- Fits the frame budget set in ADR-0002.

## Decision

1. **Global illumination and reflections: Lumen**, software ray tracing as the
   baseline everywhere. Hardware ray tracing is an option enabled per
   platform once measured (Windows PC with an RTX-class GPU first, Mac only
   if the test scene proves it stable). Lumen Lite is the fallback for lower
   scalability levels.
2. **Geometry: Nanite-ready assets with classic LODs.** Opaque static meshes
   are authored high-poly with Nanite enabled **and** hand-checked LODs or
   tuned fallback meshes; the M1 Pro renders the LOD/fallback geometry, a
   future M2+ or RTX machine gets full Nanite detail without rework.
3. **Shadows: Virtual Shadow Maps** for the sun and key lights.
4. **Night lighting: classic lights** (MegaLights needs M2+): a few
   shadow-casting lights with tight radii near the camera, unshadowed fill
   and emissive shop windows for the rest.
5. **Materials: Substrate** (already on), with one master material per family
   (façade stone and render, metal, glass, car paint with clear coat, wet
   layer), driven by material instances and a global wetness parameter.
6. **Anti-aliasing and upscaling: TSR**, internal resolution set by ADR-0002.
   MetalFX upscaling is evaluated later as an alternative on Mac.
7. **Sky and weather: Sky Atmosphere, Volumetric Clouds, Exponential Height
   Fog** with a real sun position (SunPosition plugin) for Lyon. HDRIs are
   used only for look-development and asset validation, never as the game
   sky.
8. **Post-process: physically plausible camera**: manual exposure ranges in
   EV100, local exposure, filmic tonemapper, mild lens effects; no heavy
   grading, chromatic aberration or bloom. Colour reference is real
   photographs of the district taken by us.
9. **Asset strategy**: a modular Lyon façade kit (Blender, trim sheets and
   tiling materials made from our own photos of the Presqu'île), Megascans
   and other Fab assets for generic surfaces and props, MetaHuman for
   characters, fictional vehicles built in Blender. Open city meshes and
   Gaussian splats are reference only. Details in
   [rendering-strategy.md](../rendering/rendering-strategy.md).
10. **Generative tools** (Flux, GPT Image, Tripo) are used upstream: concept
    art, fictional signage, decals, texture bases and blockout props. They
    never ship as hero geometry or unprocessed PBR maps.

### Architecture Diagram

```
Photos of Lyon ──► de-lit textures / trim sheets ──┐
Fab / Megascans ──► ground, props, decals ─────────┤
Flux / GPT Image ─► signage, posters, concept ─────┤──► Substrate master materials
Tripo ───────────► blockout props ──► Blender ─────┤          │
Geodata (other study) ─► footprints, heights ──────┘          ▼
                                  Blender façade kit ──► Nanite meshes ──► PCG placement
                                                                               │
Sun/sky/clouds/fog + weather params ──► Lumen + VSM + MegaLights ──► TSR ──► post
```

### Implementation Guidelines

- Every opaque static mesh must be Nanite-enabled and have LODs or a tuned
  fallback mesh within the ADR-0002 triangle budget; translucent and skinned
  meshes must have LODs.
- Materials must use the master materials; no one-off material graphs in
  production content.
- Every texture must be de-lit and calibrated (albedo within physically
  plausible range); no baked shadows or highlights in base colour.
- Lighting units must be physical (lux for the sun, candela or lumens for
  local lights); auto-exposure must be bounded.
- Scalability presets (Low to Epic) must exist from the first test scene and
  be measured, not guessed.

## Visual reference: Bodycam (Reissad Studio, Steam)

The project lead's benchmark for photorealism (2026-11-09) is *Bodycam*, an
Unreal Engine 5 shooter by a small French studio. What makes it read as real
footage, and what we can take from it:

| Ingredient in Bodycam | Cost | Transposable to an open-world Lyon? |
| ---- | ---- | ---- |
| **Camera emulation**: wide lens with barrel distortion, strong motion blur, sensor noise, aggressive auto-exposure and blooming highlights, slight chromatic aberration, a shaky body-mounted camera. Our eyes accept it as real footage and it hides most CG tells | Very low (post-process) | **Not adopted**: we keep a classic game camera (decision below) |
| **Photogrammetry everywhere** (Megascans) with dense, worn surfaces | Medium (disk size, memory) | **Yes** at street level: Megascans is free on Fab, plus our own Lyon scans for façades |
| **Lumen** global illumination | High on GPU | **Yes**, same stack as ours |
| **Small closed maps**: all budget concentrated in a few hundred metres | n/a | **Partly**: an open world spreads that budget over 1.5 km² with traffic, crowds and streaming. We match the density near the camera and rely on HLODs further away |
| **Desktop GPUs** (minimum around an RTX 2070 or RX 5700) | n/a | Our **Ultra** tier targets this class of hardware or better |

**Conclusion: partially achievable, under conditions.** On the Ultra tier, a
first-person walk in a dressed street of the Presqu'île can reach Bodycam's
level, because its realism comes mostly from scanned materials, Lumen and
camera emulation, all of which we use. Matching it everywhere in an open
world with traffic, crowds and day-night cycles is harder and costs more
content and GPU time; it is a long-term target, not a vertical-slice promise.
On the **M1** tier the same scenes will show the lighting, materials and
camera style, but with less geometric detail and a softer image (lower
internal resolution), so they will look clearly below Bodycam.

**Decision (project lead, 2026-11-09): Bodycam is a reference for graphics
quality only, not for the camera.** The game keeps the genre's classic
third-person camera with an optional first-person view (see the game
concept). We take Bodycam's scanned materials, lighting and density, not its
body-camera effects; the post-process stays the restrained, natural lens of
Decision 8.

## M1 Pro profile (decided 2026-11-09)

The project lead chose to stay on the M1 Pro (16 GB) instead of buying a
rendering machine, while keeping photorealism as the goal even if the game is
heavy (2026-11-09). Content is therefore authored for the **Ultra** quality
tier (full pipeline above) and the M1 Pro runs the **M1** tier, a scalability
preset defined in ADR-0002. What the M1 tier changes:

| Feature | Full pipeline | On the M1 Pro |
| ---- | ---- | ---- |
| Geometry | Nanite, displacement | LODs or Nanite fallback meshes, World Partition HLODs; parallax occlusion mapping and decals for cobbles and wall relief |
| GI and reflections | Lumen SWRT, HWRT on capable GPUs | Lumen software at medium quality, Lumen Lite in performance mode; screen-space reflections where Lumen reflections cost too much |
| Shadows | Virtual Shadow Maps | VSM if within budget, otherwise cascaded shadow maps + distance field shadows (to measure) |
| Night lights | MegaLights | Classic lights (see Decision 4) |
| Memory | | 16 GB shared by macOS, editor and game: small World Partition cells, 1.5 GB texture pool, close other apps while working |

What it means for the image: lighting, materials, exposure and dressing
(sections 1, 2 and 4 of the [rendering strategy](../rendering/rendering-strategy.md))
are unaffected and carry most of the realism. Geometric micro-detail,
reflection sharpness and night light density will stay below the benchmark
until the hardware changes. Assets remain Nanite-ready so that upgrade
needs no rework.

## Alternatives Considered

### Alternative 1: Raw photogrammetry or open 3D city meshes as game geometry
- **Description**: Import the Grand Lyon 2023 photomesh or Google 3D Tiles and play inside it.
- **Pros**: Immediate likeness at a distance.
- **Cons**: Baked lighting and shadows, low resolution at street level, melted details, no collision or interiors, licence limits (Google is reference only).
- **Rejection Reason**: Looks worse than a modular kit up close and cannot be relit by Lumen. Kept as reference and blocking.

### Alternative 2: Gaussian splatting for the city
- **Description**: Capture streets as splats and render them with a plugin.
- **Pros**: Striking photorealism from the capture viewpoint.
- **Cons**: No native UE support (third-party plugins only), baked lighting, no collision, no destruction, heavy memory, poor on Metal.
- **Rejection Reason**: Incompatible with dynamic time of day and weather. May be revisited for distant skyline backdrops.

### Alternative 3: Baked lighting (lightmaps)
- **Description**: Static GI as in older open worlds.
- **Pros**: Cheap at runtime.
- **Cons**: No dynamic time of day, huge bake times and memory for a city.
- **Rejection Reason**: Conflicts with time-of-day and weather requirements.

### Alternative 4: Hardware ray tracing everywhere, PC-first
- **Description**: Target RTX-class PCs, HWRT Lumen and path-traced references.
- **Pros**: Best possible image.
- **Cons**: Experimental on Mac; would leave the development machine unable to show final quality.
- **Rejection Reason**: Kept as a later quality tier for Windows, not the baseline.

## Consequences

### Positive
- Stock, well-documented UE5 stack; no custom renderer work.
- Same content scales from Mac to PC by switching ray tracing mode and scalability.
- Modular kit makes 1.5 km² feasible for a solo developer.

### Negative
- Software Lumen is less precise (reflections, thin geometry) than HWRT.
- Mac performance is the main risk; final quality may need a PC to be shown at full resolution (see ADR-0002).
- Photo capture of façades takes field time in Lyon.

### Risks
- Mac features in Beta/Experimental may regress between hotfixes: re-run the test scene after each engine update.
- Megascans licence terms changed several times; check the Fab licence of each asset when downloaded and log it.

## Validation Criteria

The [reference street scene](../rendering/rendering-strategy.md#7-reference-street-scene-test-plan)
meets the budget of ADR-0002 in its four lighting states and the visual
checklist scores at least 4/5 against our reference photos.

## Related

- [Rendering strategy](../rendering/rendering-strategy.md)
- [ADR-0002: Performance target and budget](adr-0002-performance-budget.md)
- [Game concept](../../design/gdd/game-concept.md), section 8 "Graphics"
