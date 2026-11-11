# Leafy street trees perf-trees2, 2026-11-11 18:12 heure de Paris, commit 1a51b60

Profile m1, scenario street-demo, 90 s per run, every case with: -Sky=lightfunction. Compare runs of this pass with each other only.

- map md5 at start: 1c92de0f813b89cb40aeb000cd92a1de
- build reused (SKIP_BUILD=1): GTAL-build/Builds/perf-trees2
- trees swapped at run time: -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1
- bench_bare: OK, 113 s, ended 18:14, log bench_bare.log
- bench_light10: OK, 99 s, ended 18:16, log bench_light10.log
- bench_light: OK, 98 s, ended 18:17, log bench_light.log
- bench_light_SM_BlackAlder_Field_05_M1: OK, 117 s, ended 18:19, log bench_light_SM_BlackAlder_Field_05_M1.log
- bench_light_SM_EuropeanBeech_Forest_08_M1: OK, 99 s, ended 18:21, log bench_light_SM_EuropeanBeech_Forest_08_M1.log
- bench_leafy: OK, 98 s, ended 18:23, log bench_leafy.log
- map md5 at the end: 1c92de0f813b89cb40aeb000cd92a1de, unchanged
- DefaultGame.ini md5 at the end: cce554de9814113acf2961e38a99a9ad, unchanged (cook folders added for the package only, then restored)

## Frame p95 per case (112 trees on the street)

| Case | Game arguments | Frame p95 (ms) | vs bare (ms) | per leafy tree (ms) |
| ---- | ---- | ---- | ---- | ---- |
| bare | -Trees=bare | 27.5 |  |  |
| light10 | -Trees=light -TreeCount=10 -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1 | 28.3 | +0.8 | 0.080 |
| light | -Trees=light -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1 | 27.4 | -0.1 | -0.001 |
| light:SM_BlackAlder_Field_05_M1 | -Trees=light -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Field_05_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1 | 27.4 | -0.1 | -0.001 |
| light:SM_EuropeanBeech_Forest_08_M1 | -Trees=light -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/EuropeanBeech/M1/SM_EuropeanBeech_Forest_08_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1 | 27.7 | +0.2 | 0.002 |
| leafy | -Trees=leafy -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1 | 28.0 | +0.5 | 0.004 |

## Tables of bench.sh

| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-demo | base | 16.1 | 27.4 | 16.2 | 27.5 | 100 % | 0 |
| street-demo | base | 16.5 | 28.1 | 16.7 | 28.3 | 99 % | 0 |
| street-demo | base | 16.3 | 27.2 | 16.5 | 27.4 | 100 % | 0 |
| street-demo | base | 16.1 | 27.2 | 16.3 | 27.4 | 100 % | 0 |
| street-demo | base | 16.9 | 27.6 | 17.1 | 27.7 | 100 % | 0 |
| street-demo | base | 16.8 | 27.9 | 16.9 | 28.0 | 100 % | 0 |

## Meshes picked

- W14-63: /Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1, 63 trees, 888 cm, Nanite False, triangles per LOD [5355, 1803, 913, 8]
- W14-64: /Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1, 49 trees, 376 cm, Nanite False, triangles per LOD [5678, 2577, 1251]
