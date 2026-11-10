# Banc perf, passe 2 — 2026-11-10 / Perf bench, pass 2 — 2026-11-10

## Français

Banc : `tools/perf/bench.sh` (fec4161), builds Shipping, profil m1, 90 s par passage, capture caméra fixe à 30 s. Fichiers : `pass2/results/*.csv` (format bench), `pass2/calibration.md`, `pass2/stills/` (référence | variante | différence ×4).

### 1. Ombre des nuages : light function contre volumetric

Build `perf-sky/0.3` (feat/cheap-sky 54d2ad5), avant calibration. GPU p50/p95 en ms, % des images ≤ 33,3 ms :

| Ciel | Caméra fixe midi | Pluie |
| --- | --- | --- |
| volumetric | 27,1 / 37,3 (88 %) | 28,7 / 40,9 (86 %) |
| lightfunction | 18,8 / 29,1 (100 %) | 19,9 / 32,1 (97 %) |
| cheap | 17,9 / 28,8 (100 %) | 19,4 / 31,6 (99 %) |

Non calée, la light function change l'image : à midi la luminance moyenne passe de 86 à 113, sous la pluie de 81 à 65.

Calibration (build `perf-sky/0.3b` = feat/cheap-sky bc21aa6 + WriteStatusFile public + repli Swift sans ffmpeg, poussés en 3685728). `status.txt` confirme `cloud_shadow_light_function=loaded`. Couples retenus (gtp.CloudShadowLF.Mean / Strength) : **midi 0.38 / 0.2** (écart 0), **pluie 1.0 / 0.5** (écart +2). Par temps clair, Strength n'a aucun effet.

Bench calé :

| Ambiance | volumetric | lightfunction calée | Gain GPU p50 | Différence d'image |
| --- | --- | --- | --- | --- |
| midi (caméra fixe) | 25,3 / 35,6 (93 %) | 17,2 / 27,5 (100 %) | −8,1 ms | luminance 85,4 / 85,7, écart moyen 1,5/255 |
| pluie | 27,5 / 39,1 (87 %) | 17,4 / 30,1 (100 %) | −10,1 ms | 81,5 / 83,3, écart moyen 2,0/255 |

Verdict : à l'œil, on ne distingue pas les deux images, ni à midi ni sous la pluie. Il n'y a pas de tache d'ombre de nuage visible dans la référence volumetric non plus, dans cette vue à 13:30 : l'effet n'est donc pas encore vérifié par ciel couvert. **Candidats** pour les préréglages météo de DefaultGame.ini (feat/cheap-sky) : midi Mean 0.38 / Strength 0.2, pluie Mean 1.0 / Strength 0.5.

### 2. LOD des façades

Build `facade-lods/0.3-lods` (feat/facade-lods d41e16f, 4 niveaux de LOD, 0 erreur) contre la 0.3.0 publiée :

| Build | Caméra fixe | Parcours démo |
| --- | --- | --- |
| 0.3.0 | 28,7 / 39,8 (87 %) | 32,1 / 41,4 (67 %) |
| 0.3-lods | 25,3 / 35,7 (93 %) | 29,6 / 37,5 (88 %) |
| 0.3-lods (2e passage) | 25,1 / 35,3 (93 %) | 29,6 / 37,3 (88 %) |

Deux passages du même build : ±0,4 ms, différence d'image 0,8/255. Réserve : la 0.3.0 a une autre lumière (luminance 113 contre 119,5), donc l'écart ne vient pas uniquement des LOD. `pass2/results/reference-0.3-lods.csv` est la **référence provisoire** du chantier (`REFERENCE=` de bench.sh).

A/B propre sur feat/facade-lods 3883236 (a : sans LOD ni atlas ; b : LOD sans atlas ; c : LOD + atlas d'enseignes) : **les trois builds se figent au chargement**, avec ou sans LOD. Aucune image ni ligne de frametimes en 152 s, et `-ExitAfter` jamais atteint. La 0.3-lods de d41e16f tournait au même banc. Le blocage vient donc de 3883236 ou de la réplique de la carte sur ce commit. Pas de mesure et pas de verdict sur le lettrage.

### Restes

- 3883236 : trouver pourquoi le jeu se fige (une build Development ou Test donnerait un log), puis refaire l'A/B a/b/c et la vérification du lettrage à 5 m.
- Répartition GPU par passe (build Test, `stat gpu` capturé dans l'image à 30 s, possible car la capture lit le back buffer) : pas faite.
- Ciel couvert (taches d'ombre au sol, lightfunction contre volumetric) : reporté.
- Banc nuages de la passe 1 (res256, res128, rays32) : coupé par l'arrêt de 18:20, pas rejoué.

## English

Bench: `tools/perf/bench.sh` (fec4161), Shipping builds, m1 profile, 90 s per run, fixed-camera still at 30 s. Files: `pass2/results/*.csv` (bench format), `pass2/calibration.md`, `pass2/stills/` (reference | variant | difference ×4).

### 1. Cloud shadow: light function vs volumetric

Build `perf-sky/0.3` (feat/cheap-sky 54d2ad5), before calibration. GPU p50/p95 ms, % of frames ≤ 33.3 ms: volumetric 27.1/37.3 (88 %) at noon and 28.7/40.9 (86 %) in rain; lightfunction 18.8/29.1 (100 %) and 19.9/32.1 (97 %); cheap 17.9/28.8 (100 %) and 19.4/31.6 (99 %). Uncalibrated, the light function changes the image (mean luma 86 → 113 at noon, 81 → 65 in rain).

Calibration (build `perf-sky/0.3b` = feat/cheap-sky bc21aa6 + WriteStatusFile made public + Swift fallback without ffmpeg, pushed as 3685728). `status.txt` confirms `cloud_shadow_light_function=loaded`. Picks (gtp.CloudShadowLF.Mean / Strength): **noon 0.38 / 0.2** (gap 0), **rain 1.0 / 0.5** (gap +2). Strength has no effect in clear weather.

Calibrated bench: noon (fixed camera) volumetric 25.3/35.6 (93 %) vs lightfunction 17.2/27.5 (100 %), −8.1 ms, luma 85.4 vs 85.7, mean difference 1.5/255. Rain 27.5/39.1 (87 %) vs 17.4/30.1 (100 %), −10.1 ms, 81.5 vs 83.3, 2.0/255.

Verdict: the two images cannot be told apart by eye, at noon or in rain. The volumetric reference shows no cloud shadow patches either in this 13:30 view, so overcast skies are still unchecked. **Candidates** for the weather presets in DefaultGame.ini (feat/cheap-sky): noon Mean 0.38 / Strength 0.2, rain Mean 1.0 / Strength 0.5.

### 2. Facade LODs

Build `facade-lods/0.3-lods` (feat/facade-lods d41e16f, 4 LOD levels, 0 errors) vs the released 0.3.0: fixed camera 25.3/35.7 (93 %) vs 28.7/39.8 (87 %); demo route 29.6/37.5 (88 %) vs 32.1/41.4 (67 %). Two runs of the same build agree within 0.4 ms and 0.8/255 image difference. Caveat: 0.3.0 has different lighting (luma 113 vs 119.5), so the gap is not only the LODs. `pass2/results/reference-0.3-lods.csv` is the project's **provisional reference** (`REFERENCE=` of bench.sh).

Clean A/B on feat/facade-lods 3883236 (a: no LODs, no atlas; b: LODs, no atlas; c: LODs + sign atlas): **all three builds hang while loading**, with or without LODs. No frame and no frame-time line in 152 s, and `-ExitAfter` is never reached. The d41e16f build ran fine on the same bench, so the hang comes from 3883236 or from replaying the map on it. No numbers and no lettering verdict.

### Left to do

- 3883236: find why the game hangs (a Development or Test build would give a log), then redo the a/b/c A/B and the lettering check at 5 m.
- Per-pass GPU split (Test build, `stat gpu` captured in the image at 30 s, which works because the capture reads the back buffer): not done.
- Overcast sky (cloud shadow patches on the ground, lightfunction vs volumetric): postponed.
- Pass 1 cloud bench (res256, res128, rays32): cut by the 18:20 stop, not replayed.
