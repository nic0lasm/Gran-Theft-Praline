# ADR-0002: Performance target and frame budget

*French version: [adr-0002-performance-budget.fr.md](adr-0002-performance-budget.fr.md)*

## Status

Accepted on 2026-11-09 by the project lead: 30 fps fidelity target with a
60 fps performance mode later, on the M1 Pro (no hardware purchase). The
numbers below are starting values, re-measured in the reference street
scene.

**Amended on 2026-11-11** (project lead's choice of a steady 40 fps): the M1
tier is capped at 40 fps and every merge to `next` goes through the gate below.
The amendment replaces the 30 fps fidelity target of the M1 tier; the rest of
this ADR is kept as the record of the starting values.

## Date

2026-11-09

## Last Verified

2026-11-11

## Decision Makers

nic0lasm (project lead)

## Summary

Sets the frame-rate and resolution target of Gran Theft Praline on the
reference hardware and splits the GPU frame into per-feature budgets, so that
every rendering choice of ADR-0001 is measured against a fixed number.
Two quality tiers (project lead's direction, 2026-11-09): content is built
for a photorealistic **Ultra** tier on capable hardware, and the M1 Pro runs a
reduced **M1** preset at 30 fps, 1080p. A 60 fps performance mode comes later.

## Engine Compatibility

| Field | Value |
| ---- | ---- |
| **Engine** | Unreal Engine 5.8 |
| **Domain** | Rendering / Core |
| **Layer** | Foundation |
| **Knowledge Risk** | HIGH (Apple Silicon numbers for UE 5.8 are not published; they come from our own measurements) |
| **References Consulted** | UE 5.8 release notes (Lumen Lite, MegaLights), Epic macOS requirements |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | All numbers below, measured in the reference street scene |

## ADR Dependencies

| Field | Value |
| ---- | ---- |
| **Depends On** | ADR-0001 |
| **Enables** | Art production budgets (triangles, texture memory, crowd and traffic density) |
| **Blocks** | Production of final-quality content |
| **Ordering Note** | The concept requires this ADR before production |

## Context

Photorealism and density compete for the same milliseconds. The benchmark
genre ships at 30 fps on consoles for its quality mode. Our first platform is
an Apple Silicon Mac, whose exact GPU tier sets the ceiling.

The development and reference machine is the current MacBook Pro (read with
`system_profiler` on 2026-11-09): **M1 Pro, 16-core GPU, 16 GB unified
memory**. The project lead decided on 2026-11-09 to stay on it, with no
hardware purchase. Per Epic, M1 chips do not run Nanite and have no hardware
ray tracing, and 16 GB is tight for the editor with a dense city. The budgets
below are set for this machine.

## Decision

### Quality tiers

The goal stays photorealism, even if the game ends up heavy in size and GPU
cost (project lead, 2026-11-09). Quality is therefore scaled per tier, never
lowered globally:

| Tier | Purpose | Hardware | Rendering | Target |
| ---- | ---- | ---- | ---- | ---- |
| **Ultra** | The photorealistic target; all content is authored for it | Apple M2 Max or newer, Windows with an RTX-class GPU | Nanite, Lumen high quality (hardware ray tracing where available), Virtual Shadow Maps, MegaLights, full-resolution textures | 30 fps at 1440p or more; measured once such hardware is available |
| **M1** | Playable development preset on the current MacBook Pro | M1 Pro, 16 GB | Nanite fallback / LODs, reduced Lumen (Lumen Lite or lower internal resolution, chosen from measurements), classic night lights, capped texture pool | 30 fps at 1080p, budget below |

Rules: assets and materials are never simplified at the source to fit the M1
tier; the M1 tier is produced by scalability settings, LODs and texture
streaming only. Client size is not a constraint on the Ultra tier; its size
budget is set later with the project lead.

### M1 tier target (M1 Pro, 16 GB)

| Mode | Frame rate | Output | Internal (TSR) | Lighting | Priority |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Fidelity | 30 fps locked (33.3 ms) | 1920×1080 | 960×540 (50 %) | Lumen Lite (GI and reflections at Medium), shadows at Medium: profile `m1`, chosen 2026-11-09 | P0 |
| Performance | 60 fps (16.7 ms) | 1920×1080 | about 1080×608 (56 %) | Lumen Lite | P1 |

Hitches: no frame above 100 ms during traversal, shader compilation included
(PSO precaching on).

### M1 tier GPU budget, fidelity mode (33.3 ms)

| Pass | Budget (ms) |
| ---- | ---- |
| Geometry (LOD meshes) and base pass | 7.0 |
| Shadows (VSM, or cascaded + distance field if cheaper) | 5.0 |
| Lumen GI and reflections | 8.0 |
| Direct lighting | 3.0 |
| Translucency, fog, clouds, particles, rain | 3.5 |
| Post-process and TSR | 2.5 |
| Characters and vehicles (skinned, hair, cloth) | 2.5 |
| Margin | 1.8 |

### Memory and content budgets (starting values, refined by measurement)

- Texture streaming pool: 1.5 GB; total GPU-visible memory used by the game:
  at most 9 GB of the 16 GB.
- Visible on screen at once: 20 animated characters, 15 moving vehicles,
  8 shadow-casting local lights and up to 100 unshadowed ones (night).
- Static meshes: LODs mandatory; LOD0 of a façade module at most 50k
  triangles; World Partition HLODs beyond about 150 m.

### M1 preset measurements (reference scene, 2026-11-09)

Greybox street, 1080p output, Mac unlocked and idle, profiles alternated state
by state. GPU time p50 / p95 in ms (raw files in
[test-results/2026-11-09](../rendering/test-results/2026-11-09/)):

| Profile | Noon | Golden hour | Rain | Night |
| ---- | ---- | ---- | ---- | ---- |
| `fidelity` (TSR 67 %, Lumen High) | 44.5 / 58.7 | 29.3 / 43.3 | 35.5 / 48.6 | 29.4 / 50.8 |
| `lumenlite` (TSR 67 %, Lumen Lite) | 31.3 / 42.0 | 27.5 / 41.6 | 32.8 / 45.8 | 26.6 / 47.6 |
| `res50` (TSR 50 %, Lumen High) | 28.5 / 39.2 | 24.4 / 38.4 | 29.3 / 42.8 | 22.3 / 44.0 |
| `m1` (TSR 50 %, Lumen Lite, shadows Medium) | 25.7 / 37.1 | 22.7 / 38.1 | 27.0 / 40.8 | 19.5 / 41.9 |

The project lead chose the `m1` combination on 2026-11-09. It holds the median
(19.5 to 27 ms) but its 95th percentile is still 4 to 9 ms over budget, with
one hitch at golden hour (131 ms) and one at night (198 ms).

**Provisional M1 target (chosen by the project lead on 2026-11-09):**
the `m1` preset is accepted for development at about 25 fps, GPU p95 at or
under 42 ms with no frame above 100 ms. 30 fps (33.3 ms) stays the M1 goal and
is to be reached by content optimisation (HLOD, shadow casters, light
complexity) by version 0.2; the Validation Criteria below are unchanged.

Hitch hypothesis: in both hitches the render thread and the GPU stall together
for one frame (render thread 106 and 198 ms) while the game thread stays under
5 ms and the number of loaded shaders does not change. This points to a
pipeline state (PSO) compiled at first use on Metal, or a one-off render target
or shadow page pool reallocation, rather than texture streaming. Check with
PSO precaching logs (`r.PSOPrecache.Validation`) when the street is dressed.

## Amendment of 2026-11-11: M1 tier capped at 40 fps, merge gate

### What changed

On the M1 Pro, the gap between median and 95th percentile came from frame
pacing, not from cost: an uncapped tier C run has a p50 of 13 to 18 ms and a
p95 of 24 to 33 ms, and the same world capped with `t.MaxFPS=40` shows every
frame at 25.0 ms. The project lead chose "40 fps, perfectly steady" on
2026-11-11. The `m1` profile now sets `t.MaxFPS=40` (`M1CVars` in
`Config/DefaultGame.ini`).

| Mode | Frame rate | Output | Internal (TSR) | Lighting |
| ---- | ---- | ---- | ---- | ---- |
| M1 (replaces "Fidelity 30 fps") | 40 fps capped, steady 25.0 ms | 1920×1080 | 960×540 (50 %) | Lumen Lite, shadows Medium, clouds at 64 view ray samples |

Hitch rule unchanged: no frame above 100 ms.

Tier C at 40 fps on `next` 3dcf33b (4 scenes × 120 s, 50 pedestrians, 25
driven and 24 far cars, police 3): frame p50 = p95 = 25.00 ms in every scene,
3 frames over 26 ms in 460 s, none over 100 ms; GPU p95 19.7 (fixed), 23.7
(route), 22.6 (night), 21.4 (rain) ms.

### Merge gate (every merge to `next`, no exception)

1. Green CI on the exact commit (Provenance check, tests outside Unreal).
2. Compile check on the Windows PC (MSVC, Win64 editor and game)
   of the same commit: the Mac's clang
   lets through narrowing and shadowing errors that MSVC refuses.
3. M1 Pro, packaged Shipping build, `m1` profile (cap from the profile, no
   `-GfxCVars`), tier C world, 4 scenes of 120 s (fixed camera,
   deterministic route, night, rain), no image capture during the
   measurement:
   - at least 99 % of frames at 25.0 ms ± 1 ms;
   - **no frame above 100 ms**;
   - GPU p95 at or under 24 ms (headroom under the 25 ms cap).
4. A branch that adds a system also gets an on/off A/B against its line of the
   per-system budget.

The cap makes frame time insensitive to the ~2 ms run-to-run variance of
tier C (random traffic and pedestrians on screen); GPU p95 is the headroom
gauge. Uncapped figures are still recorded for diagnosis but no longer gate a
merge.

## Alternatives Considered

### Buying a rendering reference machine (Windows RTX PC or M4/M5 Max Mac)
- **Pros**: Nanite, hardware ray tracing, MegaLights; much closer to the benchmark.
- **Cons**: 2,500 to 4,500 €.
- **Rejection Reason**: Declined by the project lead on 2026-11-09. Assets stay
  Nanite-ready so this can be revisited without rework.

### 60 fps as the only target
- **Pros**: Smoother driving and shooting.
- **Cons**: Halves the budget; on Apple Silicon it would force Lumen Lite or
  low internal resolution, losing much of the photorealism.
- **Rejection Reason**: Kept as a later performance mode.

### No fixed target until the vertical slice
- **Cons**: Art would be produced without a budget; the concept forbids it.

## Consequences

- Every content and feature review compares captured numbers with these
  budgets.
- If the M1 Pro cannot hold 33.3 ms in the test scene: lower the internal
  resolution, switch to Lumen Lite, or reduce density (decision for the
  project lead if it touches the visual target).
- The image will stay below the benchmark in geometric detail and reflection
  quality; this is accepted with the hardware decision.

## Validation Criteria

Since 2026-11-11: the merge gate above, on every merge to `next`.

Original criterion (2026-11-09): the [reference street scene](../rendering/rendering-strategy.md#7-reference-street-scene-test-plan)
holds a 95th-percentile GPU time at or under 33.3 ms in all four lighting
states on the M1 Pro.

## Related

- [ADR-0001: Rendering pipeline](adr-0001-rendering-pipeline.md)
- [Rendering strategy](../rendering/rendering-strategy.md)
