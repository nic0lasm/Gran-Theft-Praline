# Reference street scene / Scène témoin

*Test plan: [docs/rendering/rendering-strategy.md, section 7](../../../docs/rendering/rendering-strategy.md#7-reference-street-scene-test-plan).*

## EN

Greybox v0 of rue de la République (northern Presqu'île), used to measure the
lighting stack of ADR-0001 against the ADR-0002 budget on the M1 Pro.

1. `tools/geo-pipeline/extract_bbox.py --name rue-republique --bbox 45.7605,4.8335,45.7640,4.8380 --street "Rue de la République"`
   downloads IGN BD TOPO buildings and roads and writes the greybox `.glb` and
   its `.meta.json` (street axis) to `assets/geo/processed/rue-republique/`.
2. `build_reference_scene.py` (Unreal Editor Python) imports the greybox,
   creates the materials, the 60 s camera path, the still-shot configs and
   four maps: `ReferenceStreet_Noon`, `_GoldenHour`, `_Rain`, `_Night`.
3. `run_benchmark.sh` plays the camera path in `-game` mode with the CSV
   profiler (warm-up run, then measured run) for each map, and
   `summarize_csv.py` checks the results against the budget.
4. `render_stills.py` renders 3 frames per map with Movie Render Queue. Start
   it with `-ExecCmds="py <path>/render_stills.py"`, not `-ExecutePythonScript`
   (which quits before the render), from a path without spaces.

Notes for UE 5.8 on Mac (checked on 5.8.4, 2026-11-09):

- `run_benchmark.sh` uses `-benchmark -fps=30` (fixed time step: the 1800
  measured frames are exactly the 60 s path) and `csvprofile exitoncompletion`
  (there is no `-csvExitOnCompletion` switch).
- It enables `bAllowHighDPIInGameMode`: otherwise the game window is capped at
  the screen size in points and 1920×1080 becomes 1440×1080 on a Retina Mac.
- Close the Unreal Editor before measuring.

### Profiles and interleaved pass

`profiles.json` defines the rendering profiles, read by `run_benchmark.sh`
(through `profiles.py`) and by `render_stills.py`:

| Profile | Settings |
| ---- | ---- |
| `fidelity` (default) | ADR-0002 fidelity mode: TSR at 67 %, GI and reflections at High (`sg.GlobalIlluminationQuality 2`, `sg.ReflectionQuality 2`) |
| `lumenlite` | GI at Medium (Lumen irradiance volume, "Lumen Lite") and reflections at Medium (SSR, no Lumen reflections), TSR at 67 % |
| `res50` | `fidelity` with TSR at 50 % |
| `m1` | Candidate M1 preset: TSR at 50 %, GI and reflections at Medium (Lumen Lite), shadows at Medium |

- One profile: `PROFILE=res50 tools/rendering/reference_scene/run_benchmark.sh`.
  CSV files go to `csv/` for `fidelity` and `csv-<profile>/` otherwise; the
  summary to `benchmark.md` or `benchmark-<profile>.md`.
- Several profiles in one pass, to compare them under the same machine
  conditions: `PROFILE=fidelity,lumenlite,res50 tools/rendering/reference_scene/run_benchmark.sh`.
  For each state the profiles run one after the other (warm-up, then
  measured run), so background load affects them alike. About 30 min for 3
  profiles and 4 states.
- Each summary starts with the load average (`uptime`) just before and just
  after the pass.
- A comparison still for one profile:
  `-ExecCmds="py <path>/render_stills.py --profile lumenlite --states Noon --shots 2"`
  writes `Noon_shot2_frame0900_lumenlite.png`.

### Before measuring: idle machine

`check_idle.sh` samples the Mac for 2 minutes and prints `IDLE` (exit 0) or
`BUSY` with the reasons (exit 1). It requires a load average below 2, no
process above 30 % CPU (apart from WindowServer and the names in
`IDLE_IGNORE`, an extended regex, e.g. `IDLE_IGNORE='WindowServer|MyApp'`), no
system download (`mobileassetd`, `softwareupdated`), Safari and Terminal
nearly idle, and no UnrealEditor, UAT, UnrealBuildTool, ShaderCompileWorker,
xcodebuild or clang running. Waiting for an idle window, with a check every
10 minutes:

```bash
until tools/rendering/reference_scene/check_idle.sh; do sleep 480; done && PROFILE=fidelity,lumenlite,res50 tools/rendering/reference_scene/run_benchmark.sh
```

On 2026-11-09 a background load (system download, Safari) moved the same
`fidelity` noon measurement from 43.7 to 64.1 ms GPU p95: compare profiles
only within one interleaved pass.

Results go to `docs/rendering/test-results/<date>/`.

## FR

Greybox v0 de la rue de la République (nord de la Presqu'île), pour mesurer
la pile d'éclairage de l'ADR-0001 face au budget de l'ADR-0002 sur le M1 Pro.

1. `extract_bbox.py` télécharge les bâtiments et routes BD TOPO de l'IGN et
   écrit le greybox `.glb` et son `.meta.json` (axe de la rue).
2. `build_reference_scene.py` (Python de l'éditeur Unreal) importe le
   greybox, crée les matériaux, le trajet caméra de 60 s, les réglages des
   plans fixes et quatre niveaux : midi, heure dorée, pluie, nuit.
3. `run_benchmark.sh` joue le trajet caméra en mode `-game` avec le
   profileur CSV (passage de chauffe puis passage mesuré) pour chaque niveau,
   et `summarize_csv.py` compare au budget.
4. `render_stills.py` rend 3 images par niveau avec Movie Render Queue. Le
   lancer avec `-ExecCmds="py <chemin>/render_stills.py"` et non
   `-ExecutePythonScript` (qui quitte avant le rendu), depuis un chemin sans espace.

Notes pour UE 5.8 sur Mac (vérifiées sur 5.8.4, le 2026-11-09) :

- `run_benchmark.sh` utilise `-benchmark -fps=30` (pas de temps fixe : les 1800
  images mesurées couvrent exactement le trajet de 60 s) et
  `csvprofile exitoncompletion` (l'option `-csvExitOnCompletion` n'existe pas).
- Il active `bAllowHighDPIInGameMode` : sinon la fenêtre de jeu est limitée à la
  taille de l'écran en points et 1920×1080 devient 1440×1080 sur un Mac Retina.
- Fermer l'éditeur Unreal avant la mesure.

### Profils et passage alterné

`profiles.json` définit les profils de rendu, lus par `run_benchmark.sh` (via
`profiles.py`) et par `render_stills.py` :

| Profil | Réglages |
| ---- | ---- |
| `fidelity` (par défaut) | Mode fidélité de l'ADR-0002 : TSR à 67 %, GI et réflexions en High (`sg.GlobalIlluminationQuality 2`, `sg.ReflectionQuality 2`) |
| `lumenlite` | GI en Medium (volume d'irradiance Lumen, « Lumen Lite ») et réflexions en Medium (SSR, sans réflexions Lumen), TSR à 67 % |
| `res50` | `fidelity` avec TSR à 50 % |
| `m1` | Préréglage M1 candidat : TSR à 50 %, GI et réflexions en Medium (Lumen Lite), ombres en Medium |

- Un profil : `PROFILE=res50 tools/rendering/reference_scene/run_benchmark.sh`.
  Les CSV vont dans `csv/` pour `fidelity` et dans `csv-<profil>/` sinon ; le
  résumé dans `benchmark.md` ou `benchmark-<profil>.md`.
- Plusieurs profils en un seul passage, pour les comparer dans les mêmes
  conditions : `PROFILE=fidelity,lumenlite,res50 tools/rendering/reference_scene/run_benchmark.sh`.
  Pour chaque état, les profils passent l'un après l'autre (chauffe puis
  passage mesuré) : une charge parasite les touche de la même façon. Environ
  30 min pour 3 profils et 4 états.
- Chaque résumé commence par la charge moyenne (`uptime`) juste avant et juste
  après le passage.
- Image de comparaison pour un profil :
  `-ExecCmds="py <chemin>/render_stills.py --profile lumenlite --states Noon --shots 2"`
  écrit `Noon_shot2_frame0900_lumenlite.png`.

### Avant la mesure : machine au repos

`check_idle.sh` observe le Mac pendant 2 minutes et affiche `IDLE` (code 0) ou
`BUSY` avec les raisons (code 1). Il exige une charge moyenne sous 2, aucun
processus au-dessus de 30 % de CPU (hors WindowServer et les noms de
`IDLE_IGNORE`, une expression régulière étendue, par exemple
`IDLE_IGNORE='WindowServer|MonApp'`), aucun téléchargement système (`mobileassetd`, `softwareupdated`),
Safari et Terminal presque inactifs, et aucun UnrealEditor, UAT,
UnrealBuildTool, ShaderCompileWorker, xcodebuild ou clang en cours. Pour
attendre une fenêtre de repos avec un contrôle toutes les 10 minutes :

```bash
until tools/rendering/reference_scene/check_idle.sh; do sleep 480; done && PROFILE=fidelity,lumenlite,res50 tools/rendering/reference_scene/run_benchmark.sh
```

Le 2026-11-09, une charge parasite (téléchargement système, Safari) a fait
passer la même mesure `fidelity` à midi de 43,7 à 64,1 ms GPU p95 : ne
comparer les profils qu'à l'intérieur d'un même passage alterné.

Les résultats vont dans `docs/rendering/test-results/<date>/`.
