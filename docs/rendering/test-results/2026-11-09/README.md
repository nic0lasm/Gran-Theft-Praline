# Scène témoin, greybox v0 — mesures du 2026-11-09 / Reference scene, greybox v0 — measurements of 2026-11-09

Plan de test : [rendering-strategy.md, section 7](../../rendering-strategy.md#7-reference-street-scene-test-plan).
Outils : [tools/rendering/reference_scene](../../../../tools/rendering/reference_scene/README.md).

## FR

### Résumé

Greybox v0 de la rue de la République (334 bâtiments BD TOPO, chaussées, sol), sans
habillage, sans personnages ni véhicules. **Les quatre états dépassent le budget
GPU de 33,3 ms au 95e centile** (43 à 51 ms). Aucune image ne dépasse 100 ms.
Le rendu est limité par le GPU (thread de jeu ≈ 1,1 ms).

| État | Images | Image p50 (ms) | Image p95 (ms) | GPU p50 (ms) | GPU p95 (ms) | Image max (ms) | Images > 100 ms | GPU ≤ 33,3 ms | Résultat |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Midi | 1770 | 34,8 | 44,5 | 34,1 | 43,7 | 89,2 | 0 | 38 % des images | échec |
| Heure dorée | 1770 | 32,2 | 44,1 | 31,4 | 43,0 | 96,3 | 0 | 75 % | échec |
| Pluie | 1770 | 38,3 | 49,7 | 37,7 | 48,7 | 98,0 | 0 | 1 % | échec |
| Nuit | 1770 | 33,4 | 51,7 | 33,0 | 50,7 | 77,7 | 0 | 54 % | échec |

Tableau brut produit par `summarize_csv.py` : [pass1/benchmark.md](pass1/benchmark.md). CSV : [pass1/csv/](pass1/csv/).
Les 30 premières images (1 s après le chargement) sont exclues. Mémoire libre
du système pendant la mesure : 4,0 à 5,4 Go sur 16 Go.

### Machine et moteur

- MacBook Pro 16 pouces (MacBookPro18,1), Apple M1 Pro, CPU 10 cœurs (8P + 2E), GPU 16 cœurs, 16 Go, macOS 27.0.1 (26A434), sur secteur.
- Écran intégré Retina 3456×2234 (1728×1117 points).
- Unreal Engine **5.8.4** (CL 58902052, `++UE5+Release-5.8`), installé dans `/Users/Shared/Epic Games/UE_5.8`.
- RHI Metal SM5. Ray tracing matériel indisponible sur M1 Pro (`Ray tracing is disabled: not supported by current RHI`) : Lumen tourne en logiciel.
- Applications ouvertes pendant la mesure : Finder et une application de bureau légère. L'éditeur Unreal était fermé. Un processus système `BackgroundShortcutRunner` (Raccourcis) utilisait ~57 % d'un cœur au début.

### Réglages

- Binaire `UnrealEditor -game` en Development, **pas une build empaquetée** comme le prévoit le protocole : les chiffres incluent le surcoût de l'éditeur en mode jeu.
- Sortie 1920×1080 en fenêtre, `r.ScreenPercentage 67` (rendu interne ≈ 1286×724), TSR (`r.AntiAliasingMethod 4`), VSync coupé, `t.MaxFPS 0`.
- `sg.ViewDistanceQuality 3`, `sg.ShadowQuality 2`, `sg.GlobalIlluminationQuality 2`, `sg.ReflectionQuality 2`, `sg.PostProcessQuality 3`, `sg.TextureQuality 2`, `sg.EffectsQuality 2`, `sg.FoliageQuality 2`.
- Lumen GI et réflexions, ombres virtuelles, Substrate, Nanite actif mais greybox sans maillage Nanite.
- `-benchmark -fps=30` : pas de temps fixe, 1800 images = exactement les 60 s du trajet caméra. Un passage de chauffe complet (1800 images) précède chaque passage mesuré.
- Soleil (azimut, hauteur) : midi 181,8°, 67,7° ; heure dorée 251,3°, 6,1° ; pluie 157,6°, 24,7° ; nuit sous l'horizon (lune + 15 lampadaires, dont un tiers avec ombres).

Vérifications : la rue est bien orientée nord-sud ; le trajet caméra suit l'axe de la rue
(100 % des points sur la chaussée dans le glTF, 0 dans un bâtiment) ; le soleil de midi
vient du sud. L'import glTF d'UE 5.8 donne X = est, Y = sud, Z = haut, conforme au fichier `.meta.json`.

### Images

Movie Render Queue, 1920×1080, 120 images de chauffe moteur, images 300, 900 et 1500 du trajet :

| État | Plan 1 (image 300) | Plan 2 (image 900) | Plan 3 (image 1500) |
| ---- | ---- | ---- | ---- |
| Midi | [Noon_shot1](stills/Noon_shot1_frame0300.png) | [Noon_shot2](stills/Noon_shot2_frame0900.png) | [Noon_shot3](stills/Noon_shot3_frame1500.png) |
| Heure dorée | [GoldenHour_shot1](stills/GoldenHour_shot1_frame0300.png) | [GoldenHour_shot2](stills/GoldenHour_shot2_frame0900.png) | [GoldenHour_shot3](stills/GoldenHour_shot3_frame1500.png) |
| Pluie | [Rain_shot1](stills/Rain_shot1_frame0300.png) | [Rain_shot2](stills/Rain_shot2_frame0900.png) | [Rain_shot3](stills/Rain_shot3_frame1500.png) |
| Nuit | [Night_shot1](stills/Night_shot1_frame0300.png) | [Night_shot2](stills/Night_shot2_frame0900.png) | [Night_shot3](stills/Night_shot3_frame1500.png) |

Observations visuelles (à reprendre dans la passe d'éclairage, sans effet sur la mesure ci-dessus) :

- Heure dorée surexposée (ciel et façades délavés) : la plage d'exposition `EV 7–14` est à remonter.
- Nuit beaucoup trop claire, teinte sépia uniforme : l'exposition automatique descend jusqu'à `EV -2` et éclaircit la scène comme en plein jour ; plage à resserrer (vers `EV 2–6`).
- Pluie : sol mouillé crédible (reflets Lumen des façades).

### Problèmes rencontrés et corrections des scripts

`build_reference_scene.py`
- `volumetric_fog` n'existe pas en 5.8 : propriété renommée `enable_volumetric_fog`.
- Clés de la séquence placées en ticks alors que `add_key` attend des images à la cadence d'affichage par défaut : les clés tombaient très loin hors de la plage. Corrigé (clé à l'image `i`).
- Auto-lecture : `auto_play` n'est pas une propriété de `LevelSequenceActor` ; réglée directement via `playback_settings`.
- Mobilité des lumières via `set_mobility()` au lieu de `set_editor_property("mobility")`.
- Profondeur de champ désactivée sur la caméra (mise au point manuelle par défaut à 1 km, qui floutait les façades proches).
- Nom de fichier MRQ : `{sequence_name}` et `{level_name}` sont vides en rendu PIE 5.8 ; nom fixé par plan.
- Avertissement non bloquant : `add_spawnable_from_class` est déprécié (à remplacer par le sous-système Level Sequence Editor).

`run_benchmark.sh`
- `-csvExitOnCompletion` n'existe pas en 5.8 : remplacé par la commande `csvprofile exitoncompletion` avant `csvprofile frames=N`.
- Fenêtre réduite à 1440×1080 sur écran Retina (taille d'écran en points) : ajout de `bAllowHighDPIInGameMode=True` et `r.SetRes 1920x1080w`, vérifié par capture (1920×1080).
- `-benchmark -fps=30` pour que la capture couvre exactement le trajet de 60 s (à ~30 i/s réelles, 1800 images auraient dépassé la fin de la séquence).
- Chauffe portée de 300 à 1800 images pour compiler les shaders et PSO de tout le trajet.

`summarize_csv.py`
- Le CSV 5.8 répète la ligne d'en-tête avant les métadonnées et les lignes sans événement ont une cellule de moins : la lecture s'arrêtait après 17 images. Corrigé.

`render_stills.py`
- `-ExecutePythonScript` quitte l'éditeur dès la fin du script, avant le rendu MRQ : lancer avec `-ExecCmds="py <chemin sans espace>"`.
- Sortie dans `docs/rendering/test-results/<date>/stills/`, dossier créé d'avance.

### Variantes Lumen Lite et rendu à 50 % (passage 1, non comparable)

Mesurées après le profil de référence, avec le même protocole (`PROFILE=lumenlite` puis
`PROFILE=res50`). Profils définis dans `tools/rendering/reference_scene/profiles.json`.

**Ces chiffres ne permettent pas de comparer les profils.** Pendant ces mesures, la machine
s'est chargée : téléchargement système `mobileassetd` à ~80 % d'un cœur, Safari, Moniteur
d'activité et Terminal ouverts. Une mesure témoin du profil de référence à midi, refaite
juste après, donne **64,1 ms** de GPU p95 au lieu de 43,7 ms. Lumen Lite paraît plus lent que la
référence, mais l'écart s'explique par cette charge.

GPU p50 / p95 (ms) :

| État | Lumen Lite (GI et réflexions en Medium, TSR 67 %) | Rendu à 50 % (Lumen High) |
| ---- | ---- | ---- |
| Midi | 44,5 / 58,1 (12 images > 100 ms) | 29,6 / 38,7 |
| Heure dorée | 39,5 / 54,0 (4 images > 100 ms) | 26,3 / 38,0 |
| Pluie | 48,1 / 62,9 (4 images > 100 ms) | 31,6 / 42,6 (1 image > 100 ms) |
| Nuit | 31,6 / 50,9 | 26,0 / 44,1 |

Tableaux bruts : [pass1/benchmark-lumenlite.md](pass1/benchmark-lumenlite.md),
[pass1/benchmark-res50.md](pass1/benchmark-res50.md). CSV : `pass1/csv-lumenlite/`, `pass1/csv-res50/`.

**À faire : passage 2 alterné.** `run_benchmark.sh` accepte maintenant plusieurs profils
(`PROFILE=fidelity,lumenlite,res50`) et les alterne état par état, en notant la charge avant
et après. `check_idle.sh` vérifie d'abord que la machine est au repos. La mesure n'a pas été
lancée : aucune fenêtre de repos entre 17:25 et 17:45 (charge moyenne de 4 à 5,5, Safari
actif), puis compilation et empaquetage du projet en cours sur le Mac. Ses résultats
iront dans `csv/`, `csv-lumenlite/`, `csv-res50/` et `benchmark*.md` à la racine de ce dossier.

Images de comparaison, midi, plan 2 (image 900), rendues avec les cvars de chaque profil :
[référence](stills/Noon_shot2_frame0900_fidelity.png),
[Lumen Lite](stills/Noon_shot2_frame0900_lumenlite.png),
[rendu à 50 %](stills/Noon_shot2_frame0900_res50.png).
Sur ce greybox sans texture, les écarts sont faibles. Lumen Lite assombrit un peu plus le
pied des façades et y ajoute du bruit. Le rendu à 50 % rend les façades plus granuleuses. Il
faudra de vraies matières pour juger.

### Écarts par rapport au protocole et suites

- Mesure dans l'éditeur en mode jeu et non dans une build empaquetée ; pas encore de capture Unreal Insights ni de `stat gpu` sur les plans fixes ; mémoire GPU non relevée.
- Le greybox ne contient qu'une fraction du contenu prévu (ni façades détaillées, ni accessoires, véhicules, personnages, arbres) : le coût réel sera plus élevé. Avec 43 à 51 ms au p95 sur le seul greybox, il faut trancher avant de poursuivre : baisser la qualité Lumen (Lumen Lite), viser une résolution interne plus basse, ou revoir le budget.

## EN

### Summary

Greybox v0 of rue de la République (334 BD TOPO buildings, roads, ground), no
dressing, characters or vehicles. **All four states exceed the 33.3 ms GPU
budget at the 95th percentile** (43 to 51 ms). No frame goes above 100 ms.
Rendering is GPU-bound (game thread ≈ 1.1 ms).

| State | Frames | Frame p50 (ms) | Frame p95 (ms) | GPU p50 (ms) | GPU p95 (ms) | Max frame (ms) | Frames > 100 ms | GPU ≤ 33.3 ms | Result |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Noon | 1770 | 34.8 | 44.5 | 34.1 | 43.7 | 89.2 | 0 | 38 % of frames | fail |
| GoldenHour | 1770 | 32.2 | 44.1 | 31.4 | 43.0 | 96.3 | 0 | 75 % | fail |
| Rain | 1770 | 38.3 | 49.7 | 37.7 | 48.7 | 98.0 | 0 | 1 % | fail |
| Night | 1770 | 33.4 | 51.7 | 33.0 | 50.7 | 77.7 | 0 | 54 % | fail |

Raw `summarize_csv.py` output: [pass1/benchmark.md](pass1/benchmark.md). CSV: [pass1/csv/](pass1/csv/). The
first 30 frames (1 s after load) are skipped. Free system memory during the runs:
4.0 to 5.4 GB out of 16 GB.

### Machine and engine

- 16-inch MacBook Pro (MacBookPro18,1), Apple M1 Pro, 10-core CPU (8P + 2E), 16-core GPU, 16 GB, macOS 27.0.1 (26A434), on AC power.
- Built-in Retina display 3456×2234 (1728×1117 points).
- Unreal Engine **5.8.4** (CL 58902052, `++UE5+Release-5.8`) in `/Users/Shared/Epic Games/UE_5.8`.
- Metal SM5 RHI. Hardware ray tracing is not available on the M1 Pro, so Lumen runs in software.
- Apps open during the runs: Finder and one light desktop app. The Unreal Editor was closed. A system `BackgroundShortcutRunner` process (Shortcuts) used ~57 % of one core at the start.

### Settings

- `UnrealEditor -game` Development binary, **not a packaged build** as the protocol asks: the numbers include the editor's overhead in game mode.
- 1920×1080 windowed output, `r.ScreenPercentage 67` (≈ 1286×724 internal), TSR, VSync off, `t.MaxFPS 0`.
- Scalability: view distance 3, shadows 2, GI 2, reflections 2, post-process 3, textures 2, effects 2, foliage 2.
- Lumen GI and reflections, virtual shadow maps, Substrate. Nanite is on, but the greybox has no Nanite meshes.
- `-benchmark -fps=30`: fixed time step, so 1800 frames cover the 60 s camera path exactly. A full 1800-frame warm-up run precedes each measured run.
- Sun (azimuth, elevation): noon 181.8°, 67.7°; golden hour 251.3°, 6.1°; rain 157.6°, 24.7°; night below the horizon (moon plus 15 street lights, a third of them casting shadows).

Checks: the street runs north-south, the camera path follows the street axis (all
points on the road in the glTF, none inside a building), and the noon sun comes from
the south. The UE 5.8 glTF import gives X = east, Y = south, Z = up, as the `.meta.json` assumes.

### Images

Movie Render Queue, 1920×1080, 120 engine warm-up frames, frames 300, 900 and 1500
of the path. The files are listed in the table in the French section, in `stills/`.

Visual notes (for the lighting pass; they do not affect the measurement):

- Golden hour is overexposed: raise the `EV 7–14` range.
- Night is far too bright with a uniform sepia tint: auto exposure goes down to `EV -2`; narrow the range (around `EV 2–6`).
- Rain: wet ground is believable (Lumen reflections of the façades).

### Issues found and script fixes

- `build_reference_scene.py`: `enable_volumetric_fog` (5.8 name); sequence keys were placed in ticks while `add_key` expects display-rate frames; auto-play set through `playback_settings`; `set_mobility()`; camera depth of field disabled (the default manual focus at 1 km blurred nearby façades); still file names set per shot. `add_spawnable_from_class` is deprecated (warning only).
- `run_benchmark.sh`: there is no `-csvExitOnCompletion` switch in 5.8, so it now runs `csvprofile exitoncompletion`; the window was cut to 1440×1080 on Retina, fixed with `bAllowHighDPIInGameMode=True` and `r.SetRes 1920x1080w` (checked by screenshot); `-benchmark -fps=30` makes the capture match the 60 s path; the warm-up now covers the full path.
- `summarize_csv.py`: the 5.8 CSV repeats the header before the metadata, and rows without an event are one cell short; parsing stopped after 17 frames. Fixed.
- `render_stills.py`: `-ExecutePythonScript` quits the editor before MRQ renders, so it is now started with `-ExecCmds="py <path without spaces>"`. It writes to `docs/rendering/test-results/<date>/stills/`.

### Lumen Lite and 50 % resolution variants (pass 1, not comparable)

Measured after the reference profile with the same protocol (`PROFILE=lumenlite`, then
`PROFILE=res50`). The profiles are defined in `tools/rendering/reference_scene/profiles.json`.

**These numbers cannot be used to compare profiles.** The machine became loaded during
these runs: a `mobileassetd` system download at ~80 % of one core, and Safari, Activity
Monitor and Terminal open. A control run of the reference profile at noon, made right
after, gave **64.1 ms** GPU p95 instead of 43.7 ms. Lumen Lite looks slower than the
reference, but that load explains the gap.

GPU p50 / p95 (ms):

| State | Lumen Lite (GI and reflections at Medium, TSR 67 %) | 50 % resolution (Lumen High) |
| ---- | ---- | ---- |
| Noon | 44.5 / 58.1 (12 frames > 100 ms) | 29.6 / 38.7 |
| GoldenHour | 39.5 / 54.0 (4 frames > 100 ms) | 26.3 / 38.0 |
| Rain | 48.1 / 62.9 (4 frames > 100 ms) | 31.6 / 42.6 (1 frame > 100 ms) |
| Night | 31.6 / 50.9 | 26.0 / 44.1 |

Raw tables: [pass1/benchmark-lumenlite.md](pass1/benchmark-lumenlite.md),
[pass1/benchmark-res50.md](pass1/benchmark-res50.md).

**To do: interleaved pass 2.** `run_benchmark.sh` now takes several profiles
(`PROFILE=fidelity,lumenlite,res50`) and alternates them state by state, recording the load
before and after. `check_idle.sh` first checks that the machine is idle. The pass has not been
run: there was no idle window between 17:25 and 17:45 (load average 4 to 5.5, Safari busy),
then the project was being compiled and packaged on the Mac. Its results will go to `csv/`,
`csv-lumenlite/`, `csv-res50/` and `benchmark*.md` at the root of this folder.

Comparison stills, noon, shot 2 (frame 900), rendered with each profile's cvars:
`stills/Noon_shot2_frame0900_{fidelity,lumenlite,res50}.png`. On this untextured greybox the
differences are small. Lumen Lite darkens the base of the façades a little more and adds
noise there. The 50 % resolution makes the façades grainier. Real materials are needed to
judge.

### Deviations from the protocol and next steps

- Measured in the editor's game mode rather than in a packaged build. There is no Unreal Insights capture, no `stat gpu` on the fixed shots, and no GPU memory figure yet.
- The greybox holds only a fraction of the planned content. With 43 to 51 ms at p95 on the greybox alone, a decision is needed before going further: lower Lumen quality (Lumen Lite), a lower internal resolution, or a revised budget.
