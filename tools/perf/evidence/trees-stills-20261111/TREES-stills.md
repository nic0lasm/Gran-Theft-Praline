# Leafy street trees perf-trees, 2026-11-11 17:04 heure de Paris, commit f508036

Profile m1, scenario street-fixed, 35 s per run, every case with: -Sky=lightfunction. Compare runs of this pass with each other only.

- map md5 at start: 1c92de0f813b89cb40aeb000cd92a1de
- build reused (SKIP_BUILD=1): GTAL-build/Builds/perf-trees
- trees swapped at run time: -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1
- bench_bare: OK, 96 s, ended 17:06, log bench_bare.log
- bench_impostor: OK, 82 s, ended 17:07, log bench_impostor.log
- bench_leafy: OK, 83 s, ended 17:09, log bench_leafy.log
- map md5 at the end: 1c92de0f813b89cb40aeb000cd92a1de, unchanged
- DefaultGame.ini md5 at the end: cce554de9814113acf2961e38a99a9ad, unchanged (cook folders added for the package only, then restored)

## Frame p95 per case (112 trees on the street)

| Case | Game arguments | Frame p95 (ms) | vs bare (ms) | per leafy tree (ms) |
| ---- | ---- | ---- | ---- | ---- |
| bare | -Trees=bare | 25.8 |  |  |
| impostor | -Trees=impostor -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1 | 25.7 | -0.1 | -0.001 |
| leafy | -Trees=leafy -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1 | 26.2 | +0.4 | 0.004 |

## Tables of bench.sh

| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 12.8 | 25.5 | 12.9 | 25.8 | 100 % | 0 |
| street-fixed | base | 12.7 | 25.5 | 12.8 | 25.7 | 100 % | 0 |
| street-fixed | base | 13.1 | 26.1 | 13.2 | 26.2 | 100 % | 0 |

## Meshes picked

- W14-63: /Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1, 63 trees, 888 cm, Nanite False, triangles per LOD [5355, 1803, 913, 8]
- W14-64: /Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1, 49 trees, 376 cm, Nanite False, triangles per LOD [5678, 2577, 1251]
