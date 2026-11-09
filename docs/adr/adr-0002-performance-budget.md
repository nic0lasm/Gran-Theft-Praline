# ADR-0002: Performance target and frame budget

*French version: [adr-0002-performance-budget.fr.md](adr-0002-performance-budget.fr.md)*

## Status

Accepted on 2026-11-09 by the project lead: 30 fps fidelity target with a
60 fps performance mode later, on the M1 Pro (no hardware purchase). The
numbers below are starting values, re-measured in the reference street
scene.

## Date

2026-11-09

## Last Verified

2026-11-09

## Decision Makers

nic0lasm (project lead)

## Summary

Sets the frame-rate and resolution target of Gran Theft Praline on the
reference hardware and splits the GPU frame into per-feature budgets, so that
every rendering choice of ADR-0001 is measured against a fixed number.
Recommended: 30 fps "fidelity" mode at 1080p output on the M1 Pro, with a
60 fps "performance" mode added later.

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

### Target (M1 Pro, 16 GB)

| Mode | Frame rate | Output | Internal (TSR) | Lighting | Priority |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Fidelity | 30 fps locked (33.3 ms) | 1920×1080 | about 1280×720 (67 %) | Lumen software, medium quality | P0 |
| Performance | 60 fps (16.7 ms) | 1920×1080 | about 1080×608 (56 %) | Lumen Lite | P1 |

Hitches: no frame above 100 ms during traversal, shader compilation included
(PSO precaching on).

### GPU budget, fidelity mode (33.3 ms)

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

The [reference street scene](../rendering/rendering-strategy.md#7-reference-street-scene-test-plan)
holds a 95th-percentile GPU time at or under 33.3 ms in all four lighting
states on the M1 Pro.

## Related

- [ADR-0001: Rendering pipeline](adr-0001-rendering-pipeline.md)
- [Rendering strategy](../rendering/rendering-strategy.md)
