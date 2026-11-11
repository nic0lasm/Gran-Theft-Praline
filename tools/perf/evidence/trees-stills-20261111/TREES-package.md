# Leafy street trees perf-trees, 2026-11-11 15:09 heure de Paris, commit f508036

Profile m1, scenario street-demo, 90 s per run, every case with: -Sky=lightfunction. Compare runs of this pass with each other only.

- map md5 at start: 1c92de0f813b89cb40aeb000cd92a1de
- prepare: OK, 36 s, ended 15:10, log prepare.log
- pick: OK, 26 s, ended 15:10, log pick.log
- trees swapped at run time: -TreeSwap=/Game/Environment/Props/Greenery/PlaneTree_Trunk/SM_PlaneTree_Trunk_A~/Game/BlackAlder/M1/SM_BlackAlder_Forest_06_M1+/Game/Environment/Props/Greenery/PlaneTree_YoungGuard/SM_PlaneTree_YoungGuard_A~/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1
- package: OK, 309 s, ended 15:16, log package.log
- packaged only (PACKAGE_ONLY=1); benches later with SKIP_BUILD=1
