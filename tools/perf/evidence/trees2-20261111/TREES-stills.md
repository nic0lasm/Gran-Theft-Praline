# Leafy street trees perf-trees2, 2026-11-11 19:50 heure de Paris, commit 1a51b60

Profile m1, scenario street-fixed, 40 s per run, every case with: -Sky=lightfunction. Compare runs of this pass with each other only.

- map md5 at start: 1c92de0f813b89cb40aeb000cd92a1de
- build reused (SKIP_BUILD=1): GTAL-build/Builds/perf-trees2
- trees swapped at run time: -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1
- bench_bare: OK, 90 s, ended 19:52, log bench_bare.log
- bench_light: OK, 87 s, ended 19:53, log bench_light.log
- bench_light_SM_BlackAlder_Field_05_M1: OK, 86 s, ended 19:55, log bench_light_SM_BlackAlder_Field_05_M1.log
- bench_light_SM_EuropeanBeech_Forest_08_M1: OK, 86 s, ended 19:56, log bench_light_SM_EuropeanBeech_Forest_08_M1.log
- map md5 at the end: 1c92de0f813b89cb40aeb000cd92a1de, unchanged
- DefaultGame.ini md5 at the end: cce554de9814113acf2961e38a99a9ad, unchanged (cook folders added for the package only, then restored)

## Frame p95 per case (112 trees on the street)

| Case | Game arguments | Frame p95 (ms) | vs bare (ms) | per leafy tree (ms) |
| ---- | ---- | ---- | ---- | ---- |
| bare | -Trees=bare | 25.6 |  |  |
| light | -Trees=light -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1 | 25.4 | -0.2 | -0.002 |
| light:SM_BlackAlder_Field_05_M1 | -Trees=light -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Field_05_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1 | 26.3 | +0.7 | 0.006 |
| light:SM_EuropeanBeech_Forest_08_M1 | -Trees=light -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/EuropeanBeech/M1/SM_EuropeanBeech_Forest_08_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1 | 25.6 | +0.0 | 0.000 |

## Tables of bench.sh

| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 12.8 | 25.5 | 12.9 | 25.6 | 100 % | 0 |
| street-fixed | base | 12.7 | 25.2 | 12.8 | 25.4 | 100 % | 0 |
| street-fixed | base | 13.3 | 26.0 | 13.5 | 26.3 | 100 % | 0 |
| street-fixed | base | 12.8 | 25.4 | 12.9 | 25.6 | 100 % | 0 |

## Meshes picked

- W14-63: /Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1, 63 trees, 888 cm, Nanite False, triangles per LOD [5355, 1803, 913, 8]
- W14-64: /Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1, 49 trees, 376 cm, Nanite False, triangles per LOD [5678, 2577, 1251]
