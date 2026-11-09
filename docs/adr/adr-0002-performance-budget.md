# ADR-0002: Performance target and frame budget

*French version: [adr-0002-performance-budget.fr.md](adr-0002-performance-budget.fr.md)*

## Status

Proposed. The frame-rate target and the hardware question are decisions for
the project lead; this ADR records the recommendation until they are made.

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
Recommended: 30 fps "fidelity" mode at 1440p output on the development Mac,
with a 60 fps "performance" mode added later.

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

## Decision (recommended, pending the project lead)

### Target

| Mode | Frame rate | Output | Internal (TSR) | Hardware | Priority |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Fidelity | 30 fps locked (33.3 ms) | 2560×1440 | about 1706×960 (67 %) | Development Mac | P0 |
| Performance | 60 fps (16.7 ms) | 1920×1080 | about 1280×720 (67 %), Lumen Lite | Development Mac and Windows PC | P1 |

Hitches: no frame above 100 ms during traversal, shader compilation included
(PSO precaching on).

### GPU budget, fidelity mode (33.3 ms)

| Pass | Budget (ms) |
| ---- | ---- |
| Nanite geometry and base pass | 6.0 |
| Virtual Shadow Maps | 4.0 |
| Lumen GI and reflections | 8.0 |
| Direct lighting (incl. MegaLights) | 3.0 |
| Translucency, fog, clouds, particles, rain | 3.5 |
| Post-process and TSR | 3.0 |
| Characters and vehicles (skinned, hair, cloth) | 2.5 |
| Margin | 3.3 |

### Memory and content budgets (starting values, refined by measurement)

- Texture streaming pool: 3 GB; total GPU-visible memory used by the game: at
  most 60 % of the Mac's unified memory.
- Visible on screen at once: 40 animated characters, 25 moving vehicles,
  200 local lights (night).
- Nanite: no per-asset triangle limit; Nanite streaming pool sized from the
  test scene.

## Alternatives Considered

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
- If the development Mac cannot hold 33.3 ms in the test scene at Epic
  settings, the options are: lower internal resolution, Lumen Lite, or a
  Windows PC as the quality reference (decision for the project lead).

## Validation Criteria

The [reference street scene](../rendering/rendering-strategy.md#7-reference-street-scene-test-plan)
holds a 95th-percentile GPU time at or under 33.3 ms in all four lighting
states on the development Mac.

## Related

- [ADR-0001: Rendering pipeline](adr-0001-rendering-pipeline.md)
- [Rendering strategy](../rendering/rendering-strategy.md)
