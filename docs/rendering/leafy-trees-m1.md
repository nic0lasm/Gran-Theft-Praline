# Leafy plane trees on M1: variants, estimated cost, one-command bench

Date: 2026-11-10. Free packs thread, performance push (no new assets; make what is already on the Mac affordable).

## Why the trees are bare today

The 112 street trees (W14-63 mature plane tree, W14-64 guarded young tree) are two instanced meshes (HISM) placed by `place_props.py`. Slot of 2026-11-10 16:31 UTC, editor `-game`, M1 profile, light map: bare trees p95 42.77 ms (p50 31.8, 10 hitches > 100 ms). The budget (p95 ≤ 42 ms, ADR-0002) is already full: leaves must be paid for by a saving elsewhere or cost almost nothing.

## What is on the Mac (free Fab packs, out of Git)

`.uasset` sizes in `Geometry/SimpleWind` (a rough stand-in for triangle counts; the real count comes from the bench's audit):

| Mesh | Size | Possible role |
| ---- | ---- | ---- |
| SM_EuropeanBeech_Field_01 | 227 MB | lone beech: too heavy for 112 |
| SM_EuropeanBeech_Forest_01..07 | 18 to 165 MB | too heavy |
| SM_EuropeanBeech_Forest_08 | 10 MB | mature candidate |
| SM_BlackAlder_Forest_06..09 | 1.8 to 2.9 MB | lightest mature candidates |
| SM_BlackAlder_Field_05 | 8.9 MB | lone mature candidate |
| SM_BlackAlder_Sapling_01..06 | 0.4 to 1.8 MB | W14-64 (young tree) |
| SM_EuropeanBeech_Sapling_01 | 5.3 MB | W14-64 |

The bench picks by itself (`GTP_TREE_MESH=auto`) the SimpleWind mature mesh with the fewest LOD 0 triangles among those at least 8 m tall, and the lightest young tree. The packs ship impostor materials (`Materials/Impostor`): the last LOD is a billboard. If the audit shows Nanite on, the light variant forces the fallback mesh, which is what a GPU without Nanite draws anyway.

## The three variants (one map, one package)

`swap_street_trees.py` (`stage` mode) adds a leafy copy of the 112 trees, same instances in the same order, without collision (the bare trunk keeps it). At start, `UGTPStreetTreesSubsystem` shows one or the other from the game arguments; without arguments the street stays as it is today.

| Argument | What changes |
| ---- | ---- |
| `-Trees=bare` | default, bare trees |
| `-Trees=leafy` | pack mesh as it comes (wind, shadows, every LOD) |
| `-Trees=light` | wind off beyond 30 m (`-TreeWindCm`), VSM pages not invalidated by wind (`ShadowCacheInvalidationBehavior=Static`), LOD 0 skipped (MinLOD 1), trees culled beyond 200 m (`-TreeCullCm`), non-Nanite fallback |
| `-Trees=impostor` | every tree on its last LOD (billboard), no wind: the cost floor |
| `-TreeCount=10` | only 10 leafy trees spread along the street, the others stay bare |
| `-TreeShadows=0` | leafy trees cast no shadow |

Wind (World Position Offset) is the first suspect: it moves vertices every frame, so it invalidates every tree's virtual shadow map pages every frame. Turning it off at distance and marking the shadow static removes most of that cost.

## Estimated cost for 112 trees (before measuring)

An estimate for the bench to confirm, from the push's measurements (wall details, shop windows, clouds) and the sizes above:

| Variant | Extra p95 (estimated) | Per tree |
| ---- | ---- | ---- |
| leafy (raw pack) | +6 to +12 ms | 0.05 to 0.1 ms |
| light | +2 to +4 ms | 0.02 to 0.035 ms |
| impostor | +0.3 to +1 ms | < 0.01 ms |

Proposed budget: 1.5 ms p95 for the 112 trees, i.e. 0.013 ms per tree. Only the impostor variant surely fits; the light variant fits if the cloud saving (cloud shadow 8.6 ms) is made. The intended compromise: light within 40 m, impostor beyond (`-TreeCullCm` and the transition LOD tuned after the first measurement).

## One-command bench (Mac, after the coordinator's go)

```
caffeinate -dimsu tools/perf/tree-bench-mac.sh perf-trees 90
```

Steps: audit of the packs (triangles per LOD, Nanite, textures > 2K); leafy copy staged on the map; Shipping package; copy taken off the map (the next package is unchanged); then `tools/perf/bench.sh` for bare, light10, light, impostor, leafy. Pilot's rule: 10 trees first; if light10 costs more than 2 ms (`MAX_DELTA_10_MS`) over bare, the 112-tree cases are skipped. Length: about 40 min (audit and map 10 min, package 15 to 25, benches 12). Result: `Builds/perf-trees/trees-<date>/TREES.md` (p95 per case, difference to bare, cost per tree, meshes picked) and `mesh_audit.csv`.

Requirements: in the clone that packages, the `Content/EuropeanBeech`, `Content/BlackAlder` and `Content/MSPresets` folders (links to GTAL in GTAL-build) and this branch's code compiled.

## Roadside Construction (same analysis)

`Content/Scene_RoadsideConstruction`, 4.5 GB, now out of Git: 70 meshes (2.7 GB) and 215 textures (2.05 GB, 28 over 20 MB, so 4K or 8K). A traffic cone weighs 75 MB, a concrete block 134 MB: these are Nanite scans of several million triangles. On M1, if Nanite does not run, the engine draws their fallback meshes, whose density is not controlled.

Before placing a single one in the street:

1. the bench's audit gives triangles and Nanite per mesh (`mesh_audit.csv`, same command);
2. for each object kept (cones, barriers, concrete blocks, bollards, cement bags, hoardings), set the Nanite fallback below 5,000 triangles or build 3 LODs (≈ 5,000 / 1,500 / 400 triangles), like the budgets in `tools/assets/manifest.py`;
3. cap textures at 2048 (the M1 profile loads no more);
4. place them as one HISM per type, shadows off beyond 30 m, culled beyond 80 m.

None of this is done while the push freezes new assets: it is the recipe for the day a thread needs it.
