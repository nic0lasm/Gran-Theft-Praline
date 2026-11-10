# Content budgets for the M1 tier

*French version: [content-budgets.fr.md](content-budgets.fr.md)*

Every asset must arrive inside these limits; the M1 Pro has no Nanite, so
LODs, instancing and culling do the work. Draft values from the 2026-11-10
measurements, refined by the bench (`tools/perf/bench.sh`).

| Family | LOD0 triangles | LODs | Materials | Cull end | Shadows |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Facade module | ≤ 2 k (≤ 50 k hero) | 4 (100/50/15/5 %) | ≤ 2, atlas | 250 m (signs 150 m) | yes; none from far LOD |
| Large prop (> 3 m) | ≤ 30 k | 4 + impostor | ≤ 2 | 250 m | yes |
| Medium prop (1-3 m) | ≤ 15 k | 3 | ≤ 2 | 120 m | yes |
| Small prop (< 1 m) | ≤ 5 k | 3 | 1 | 80 m | none under 0.5 m |
| Street detail | ≤ 3 k | 2 | 1, shared atlas | 40-60 m | none |
| Pedestrian (full) | ≤ 40 k | 4 | ≤ 3 | full < 20 m, reduced 20-90 m, far crowd instanced | < 20 m |
| Vehicle | ≤ 80 k | 4 | ≤ 4 | 250 m | < 40 m |
| Tree | ≤ 60 k | 4 + billboard | ≤ 2 | 400 m | < 60 m |

Rules:

- Textures: 2K maximum on M1 (4K kept in source for the Ultra tier); the
  streaming pool is 1.5 GB.
- Opaque by default; translucency only with a measured budget (shop windows:
  ≤ 0.8 ms for the whole street, or the opaque "fake depth" fallback).
- One HISM per mesh and material, never thousands of single actors.
- A change that adds a system or content ships with a before/after bench run;
  `REFERENCE=<results.csv>` fails the run on a p95 regression above 2 ms or any
  new frame above 100 ms.
