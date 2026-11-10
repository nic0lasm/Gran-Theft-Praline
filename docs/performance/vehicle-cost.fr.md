# Coût des voitures PNJ et de police, et comment dépasser 3 + 3

*English version: [vehicle-cost.md](vehicle-cost.md)*

État : analyse et une première étape à interrupteur (10/11/2026). Rien
n'est encore mesuré par composant : les chiffres marqués *estimation*
viennent des mesures connues et de la lecture du code ; c'est le banc qui
tranche.

## 1. Ce que coûte une voiture

Mesures connues : avant les optimisations de la 0.3, 6 voitures PNJ avec
conducteur ajoutaient environ 10 ms de GPU (environ 1,7 ms par voiture, fil
0.3, profil M1) ; le budget provisoire de l'ADR-0002 est 0,17 ms par
voiture ; la 0.3.0, tous systèmes actifs, a mesuré un p95 de 44,5 ms pour
une cible de 42 ms.

| Élément | Côté | Ce que c'est | Ce qui fait le coût | Déjà fait |
|---|---|---|---|---|
| Maillage de la caisse | GPU | 1,5 M triangles au LOD 0, skinné (os des roues) | triangles x skinning, matériaux Substrate | LOD lointain forcé au-delà de 25 m |
| Ombres | GPU | casteur mobile : ses pages de shadow map virtuelle sont redessinées à chaque image | nombre de pages touchées | pas d'ombre au-delà de 25 m ; lumières sans ombre |
| Lumières | GPU | feux avant, arrière, stop (3 à 5 par voiture), police : 2 de plus | pixels éclairés x lumières | sans ombre |
| Conducteur | GPU | personnage skinné au volant | skinning, ombre | caché, sans pose, sans ombre |
| Physique Chaos | CPU | 4 traces de roue, suspension, 3 sous-pas | par voiture et par image | rien |
| Décision IA | CPU | balayage d'obstacle, file le long des voies, feux, carrefours | par voiture et par décision | rien |
| Animation | CPU | blueprint d'animation des roues | par voiture et par image | rien |
| Dessin | CPU | 4 vitres + caisse + roues, appels de dessin | par voiture visible | non dessinée au-delà de 150 m |

Le GPU est ce qui limite le M1 (le temps d'image est limité par le GPU dans
toutes les mesures) : il passe d'abord ; le CPU compte à partir de 12
voitures.

## 2. Comment dépasser 3 voitures de trafic et 3 de police

| Idée | Côté | Gain attendu (estimation) | Risque | État |
|---|---|---|---|---|
| Décisions du conducteur selon la distance : chaque image sous 35 m, 10 Hz sous 90 m, 4 Hz au-delà | CPU | 60 à 80 % des balayages des voitures lointaines, environ 0,1 à 0,3 ms à 12 voitures | faible : les pédales gardent leur dernière valeur | fait, `-VehicleLod=1` |
| Pas de mise à jour de pose pour une voiture non dessinée | CPU | faible, 0,05 ms par voiture cachée | faible | fait, même interrupteur |
| Rampe de police éteinte au-delà de 60 m | GPU | environ 0,1 ms par voiture de police hors de portée | nul | fait, même interrupteur |
| Voitures lointaines « fantômes » cinématiques : physique coupée au-delà de 90 m ou hors de vue, la voiture glisse le long de sa voie | CPU | tout le coût Chaos des voitures lointaines, environ 0,2 ms par voiture | moyen : réactiver la physique près du joueur, files et carrefours à préserver | proposé, à tester après le banc |
| Maillage impostor (LOD 3, environ 2 k triangles) au-delà de 60 m | GPU | environ 0,3 ms par voiture lointaine visible | moyen : travail d'import (fil import) | proposé |
| Réutiliser les voitures plutôt que créer et détruire | CPU | supprime les à-coups de création | faible | proposé |
| Limiter les voitures vivantes par coût et non par nombre : une voiture lointaine hors de vue compte pour un tiers | les deux | permet 8 à 12 voitures vivantes pour le coût de 5 | faible | proposé |

Le mode fantôme est ce qui décide si 12 voitures sont atteignables sur le
M1 : c'est la suite après la première mesure.

## 3. L'interrupteur et le test en une commande

- `-VehicleLod=1` (ou `bVehicleLod=True` sous `[/Script/GranTheftPraline.GTPTrafficNetwork]`)
  active les trois premières lignes de la section 2. Désactivé par défaut
  tant que ce n'est pas mesuré. Distances : `LodNearDistance` 35 m,
  `LodMidDistance` 90 m, `LightBarDistance` 60 m.
- `tools/perf/vehicle-count-bench.sh <dossier_build>` lance le banc à 3, 6 et
  12 voitures, chacun interrupteur coupé puis activé (`COUNTS="3 6 12"`,
  `POLICE=1` ajoute une poursuite à 2 étoiles, `DURATION=90`) et écrit
  `vehicle-cost.md`. Il s'appuie sur `tools/perf/bench.sh` (branche
  feat/perf-bench) et demande le Mac.
- Lecture : le coût par voiture est la pente entre 3 et 12 voitures du GPU
  p50 et du p95 d'image ; l'interrupteur est gardé si le p95 ne se dégrade
  pas et que la pente baisse.

## 4. Mode fantôme et réserve de voitures (interrupteurs, pas encore mesurés)

- `-VehicleGhost=1` (ou `bVehicleGhost=True`) : au-delà de `GhostDistance`
  (90 m) du joueur, une voiture PNJ devient un fantôme. La physique et le
  véhicule Chaos sont coupés, et elle glisse le long de son chemin 20 fois
  par seconde, avec les mêmes arrêts qu'une voiture conduite : virages,
  voiture qui précède le long des voies, feux rouges, lignes d'arrêt des
  carrefours. Le sol est retrouvé quelques fois par seconde. Elle redevient
  conduite sous `GhostWakeDistance` (70 m), à la vitesse du fantôme. Pas de
  balayage d'obstacle pour un fantôme. La logique est dans
  `GTPTrafficGhost.h`, vérifiée par `tools/vehicles/ghost_test`.
- `-TrafficPool=1` (ou `bTrafficPool=True`) : une voiture retirée est cachée
  et gardée, puis réutilisée à la prochaine apparition, au lieu d'être
  détruite puis recréée.
- Les voitures de police sont pilotées par le directeur de la police, sans
  conducteur PNJ : elles ne deviennent jamais des fantômes.
- `tools/perf/vehicle-count-bench.sh` mesure maintenant trois niveaux par
  nombre de voitures : aucun, `-VehicleLod=1`, puis
  `-VehicleLod=1 -VehicleGhost=1 -TrafficPool=1`.

## 5. Voitures garées en instances (interrupteur, pas encore mesuré)

- `tools/city/parking.py --map <Map>` écrit `Content/City/Traffic/<Map>.parking.csv`.
  Il pose un emplacement tous les 5,6 m, là où la chaussée mesurée laisse
  2,1 m entre la voie extérieure et la bordure, à 15 m au moins des
  carrefours. 70 % des emplacements reçoivent une voiture, toujours les
  mêmes d'une exécution à l'autre. Cela fait environ 1 100 emplacements sur
  toute la zone de la presqu'île.
- `-ParkedCars=1` (ou `bParkedCars=True` sur le réseau de trafic) crée
  `AGTPParkedCars` : une instance hiérarchique de maillage statique par
  modèle (`SM_Parked_<Name>_A`, produit par
  `tools/vehicles/unreal/import_parked_static.py`). Chaque instance a sa
  couleur de carrosserie dans `PerInstanceCustomData[0..2]`. Pas d'ombre,
  disparition en fondu entre 120 et 150 m, collision en boîte simple. Pas de
  physique et pas de tick.
- Pas encore fait : remplacer une voiture garée par une vraie voiture quand
  le joueur s'en approche (pour la voler).
