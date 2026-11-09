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
4. `render_stills.py` renders 3 frames per map with Movie Render Queue.

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
4. `render_stills.py` rend 3 images par niveau avec Movie Render Queue.

Les résultats vont dans `docs/rendering/test-results/<date>/`.
