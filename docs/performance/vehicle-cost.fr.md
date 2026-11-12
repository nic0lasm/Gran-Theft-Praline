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

## 4. Mode fantôme et réserve de voitures (mesurés le 10/11/2026)

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

Mesuré le 10/11/2026 (M1 Pro, profil m1, street-demo, 90 s, build 4c7906b,
map 799c8d5 avec LOD des façades). Avec
`-VehicleLod=1 -VehicleGhost=1 -TrafficPool=1`, 12 voitures donnent un temps
d'image p50/p95 de 28,9/38,8 ms, 86 % des images sous 33 ms et aucun
à-coup : autant que 3 voitures (28,9/38,2). Sans ces niveaux, 12 voitures
donnent 41,5/55,0.

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

## 6. Trafic lointain et budget CPU par voiture (mesuré le 11/11/2026)

Mesuré le 11/11/2026 sur M1 Pro : profil m1, parcours street-demo de 90 s,
build d0f7aea, chaque cas avec LOD, fantômes et réserve. Le parcours varie
d'environ 1,5 ms. Temps d'image p50/p95 en ms :

| Cas | Conduites + lointaines | p50 / p95 |
|---|---|---|
| d12 | 12 + 0 | 18,2 / 28,2 |
| d25 | 25 + 0 | 20,0 / 35,2 |
| d12-f13 | 12 + 13 | 17,9 / 28,3 |
| d40 | 39 + 0 | 20,1 / 34,3 |
| d12-f28 | 12 + 28 | 17,9 / 29,6 |
| d16-f24-p | 16 + 24, voitures garées | 18,0 / 28,2 |

Le trafic lointain coûte environ 0,1 ms de CPU, et de 0 à 1,3 ms au p95. Les
conducteurs PNJ coûtent 0,37 ms pour 39 voitures. Au-delà d'environ 16
voitures conduites, le p95 dépasse 33 ms, et ce coût vient du dessin des
voitures par le GPU. 40 véhicules perçus tiennent avec 16 voitures
conduites et 24 lointaines (p95 28,2 ms). Pour plus de voitures conduites,
il faut un dessin moins cher : des LOD plus tôt ou une distance fantôme plus
courte.

L'objectif est d'avoir 25 puis 40 véhicules en vue, sans 25 ou 40 voitures
Chaos avec conducteur. Au-delà de la distance fantôme, une voiture n'a pas
besoin d'être un acteur.

- `-FarTraffic=N` (ou `NumFarCars` sur le réseau de trafic) crée
  `AGTPFarTraffic`, soit N voitures de plus au-delà de la distance fantôme,
  sans physique et sans conducteur actif. L'interrupteur active aussi le
  mode fantôme. Deux façons de les dessiner :
  - marionnettes (par défaut) : des voitures des classes du trafic, créées
    une fois au départ. Chacune est gardée à son LOD le plus bas, sans
    ombre, sans mise à jour de pose et sans collision, cachée au-delà de
    150 m, et déplacée par `AGTPFarTraffic` ;
  - instances (`-FarInstances=1`) : des instances mobiles des maillages de
    voitures garées, un `InstancedStaticMeshComponent` par modèle, sans
    aucun acteur. Il faut pour cela des maillages `/Game/Vehicles/Parked`
    corrects ; sans eux, le jeu se replie sur les marionnettes.
- Une voiture lointaine suit les voies et prend une voie suivante au hasard
  en bout de voie. Elle garde 8 m avec la voiture de devant sur sa voie,
  voitures conduites comprises, et s'arrête aux feux rouges. Elle ne réserve
  pas les carrefours et ne sonde pas les obstacles : à cette distance,
  personne ne voit deux voitures se croiser. La logique est dans
  `GTPTrafficFar.h`, vérifiée par `tools/vehicles/far_test`.
- Passage de relais : une voiture lointaine qui arrive à moins de 90 m du
  joueur échange avec la voiture conduite la plus éloignée, qui est alors un
  fantôme. Avec les marionnettes, les deux voitures échangent leurs rôles :
  la marionnette devient une voiture conduite (un fantôme qui se réveille à
  70 m), et la voiture conduite devient la marionnette de cette voiture
  lointaine. C'est la même voiture qui reste en vue, couleur comprise. Avec
  les instances, la voiture conduite reprend la voie, la position et la
  vitesse de la voiture lointaine, et la voiture lointaine reprend
  l'ancienne place de la voiture conduite. Si aucune
  voiture conduite n'est libre, une voiture lointaine hors de vue qui arrive
  à moins de 50 m repart ailleurs. Celle qui est en vue continue et ne
  disparaît jamais. Hors de vue, une voiture lointaine au-delà de 250 m ou
  dans une impasse repart aussi ailleurs, entre 120 et 250 m.
- Trafic perçu = voitures conduites + voitures lointaines. Le palier C
  (25 véhicules) correspond à `-Traffic=12 -FarTraffic=13`, la cible
  (40 véhicules) à `-Traffic=12 -FarTraffic=28`.
- Budget CPU : toutes les 5 s, le jeu écrit une ligne, Shipping compris :
  `traffic budget: D driven (G ghosts), P pooled, F far | drivers x ms/frame
  (y us per driver tick), network z ms/frame, far w ms/frame`.
- Une commande : `tools/perf/traffic-perceived-bench.sh <build_dir>`. Elle
  mesure 12 voitures conduites (la référence du palier B), puis 25 et 40
  voitures conduites, puis 12+13 et 12+28 avec le trafic lointain, et enfin
  16+24 avec les voitures garées. Elle écrit le tableau et les lignes de
  budget. Les voitures garées et `-FarInstances=1` ont besoin des maillages
  `/Game/Vehicles/Parked` dans le build ; les marionnettes, non.
- Un créneau Mac : depuis la racine du clone,
  `caffeinate -dimsu tools/perf/traffic-pass-mac.sh` package le jeu, fait
  un lancement de contrôle de 30 s, puis joue chaque cas. Chaque étape a un
  garde-temps : 50 min pour le package, 3 min pour le contrôle et
  2 × (durée + 60 s) par cas. Une étape qui dépasse sa limite est tuée et la
  passe s'arrête. Une ligne de signe de vie est écrite chaque minute.
  `DRY_RUN=1` vérifie les chemins et les garde-temps sans Unreal.

## 7. Coût de dessin des voitures conduites (balayé le 11/11/2026 : pas un levier)

Balayé le 11/11/2026 sur le candidat 09429f1 : M1, caméra fixe, 75 s par
cas, 16 voitures conduites et 24 lointaines, sans police. Les six cas
restent entre 13,5 et 14,4 ms au p50 et entre 25,2 et 27,0 ms au p95. Toutes
les images passent sous 33 ms, sans aucun à-coup. Ces écarts relèvent du
bruit : ni la distance fantôme (de 60 à 90 m) ni la distance du LOD complet
(de 8 à 20 m) ne sont des leviers. Les réglages par défaut (90 m et 20 m)
sont donc conservés. Le CPU du trafic reste à 0,3 ms ou moins.

Au-delà d'environ 16 voitures conduites, c'est le GPU qui limite. Deux
distances fixent ce que coûte le dessin d'une voiture conduite. Toutes deux
se règlent maintenant en ligne de commande, en mètres :

- `-GhostDistance=M` : au-delà de M, les voitures deviennent des fantômes,
  et elles se réveillent 20 m plus près. Les voitures lointaines passent le
  relais à la même distance. Par défaut : 90.
- `-LodNear=M` : le LOD complet et l'ombre ne s'appliquent que sous M ;
  au-delà, c'est `FarLOD` (2 par défaut, ou `-FarLOD=N`). Par défaut : 20.
- Une commande, sur un build existant :
  `caffeinate -dimsu tools/perf/traffic-lod-sweep-mac.sh <build_dir>`. Elle
  enchaîne 6 cas de 75 s avec 16 voitures conduites et 24 lointaines :
  g90-l20 (les réglages actuels), g75-l20, g60-l20, g90-l12, g90-l8 et
  g60-l8. La caméra est fixe par défaut ; avec `DETERMINISTIC=1`, le
  balayage suit le parcours de démo (`-DemoDeterministic=1`). Chaque cas a
  un garde-temps. Le balayage s'arrête si l'écran est verrouillé, et
  `DRY_RUN=1` le vérifie sans lancer le jeu.
- Depuis le candidat suivant, les réglages par défaut sont 16 voitures
  conduites et 24 lointaines. Les niveaux de détail, les fantômes et la
  réserve sont activés, les voitures garées restent coupées. `-Traffic=0`
  coupe aussi les voitures lointaines.

## 8. Leviers de dessin par voiture conduite (interrupteurs, pas encore mesurés)

Au-delà d'environ 16 voitures conduites, le p95 dépasse 33 ms sur le M1, et
c'est le GPU qui limite. Ces leviers réduisent ce que dessine chaque voiture
conduite. Tous sont coupés par défaut, donc le jeu ne change pas sans
interrupteur. Les distances sont en mètres, et une valeur négative veut dire
jamais.

- `-CarDriverDraw=M` : le corps du conducteur IA est caché au-delà de M.
- `-CarLightDistance=M` : les feux de la voiture sont éteints au-delà de M.
  Les voitures lointaines n'en ont pas.
- `-CarRenderStatic=M` : la carrosserie est dessinée en statique au-delà de M.
  Il n'y a plus de skinning ni de passe de vitesse, et les roues ne tournent
  plus à cette distance. Les voitures lointaines sont aussi statiques quand
  le levier est actif.
- `-CarNearLOD=N` : LOD N sous la distance de plein détail, au lieu du LOD 0.
- `-CarShadows=0` : aucune voiture ne projette d'ombre, même de près.
- Une commande sur un build existant :
  `caffeinate -dimsu tools/perf/car-draw-bench-mac.sh <build_dir>`. Elle lance
  11 cas de 75 s avec une caméra fixe et 24 voitures lointaines, en
  26 minutes environ :
  - base et « cheap » (tous les leviers sauf les ombres) à 16, 20 et
    24 voitures conduites ;
  - chaque levier seul à 24.

  Elle a le même garde-temps, le même test d'écran verrouillé, le même signe
  de vie et le même `DRY_RUN=1` que le balayage.
- `-FarLean=0` : les marionnettes lointaines restent entières. Par défaut,
  elles sont allégées : pas de vitres translucides, pas de tick du son moteur
  ni de tick d'acteur. Les vitres sont suspectées de causer le coût du trafic
  lointain la nuit (+1,4 ms p50, +3,4 ms p95) et sous la pluie (+2,9 ms p50,
  +5,8 ms p95), que les scènes de jour ne montrent pas. Une vitre translucide
  est éclairée par chaque réverbère et embrumée sous la pluie. À mesurer de
  nuit et sous la pluie, avec et sans l'interrupteur.
- Mesuré le 11/11/2026 (M1, 24 voitures conduites et 24 lointaines, caméra
  fixe, passes uniques de 75 s, environ 2 ms de bruit), GPU p50/p95 en ms :
  - base : 15,0/26,9 ;
  - tous les leviers (« cheap ») : 12,2/24,6 ;
  - statique au-delà de 30 m seul : 12,3/24,9 ;
  - LOD proche 1 : 12,8/25,4 ;
  - feux : 13,1/26,4 ;
  - conducteur et ombres : dans le bruit.

  Depuis, le défaut est `StaticRenderDistance=3000` (statique au-delà de
  30 m), et `-CarRenderStatic=-1` le coupe. Les marionnettes allégées restent
  coupées par défaut tant que Nico ne les a pas validées (`-FarLean=1`).
- Un levier n'entre dans les défauts que si le p95 à 20 ou 24 voitures
  conduites reste sous 33 ms, sans aucune image au-delà de 100 ms.

## 9. Conducteurs assis allégés (interrupteur actif par défaut)

Chaque voiture de trafic, lointaine ou de police installe un personnage
joueur complet au volant (BP_PlayerCharacter, le pion du mode de jeu).
Chacun fait tourner sa caméra, son bras à ressort, son directeur de caméra et
son mouvement. Sur le monde C, cela fait environ 44 personnages, et le profil
CPU Windows montre 3 à 4 ms par image dans la logique de caméra, plus
environ 3 ms dans le mouvement. `SeatOccupant` allège désormais un conducteur
assis qui n'est pas le joueur : plus de tick d'acteur ni de contrôleur, plus
de tick de composant hormis les maillages, caméras et bras à ressort
désactivés, mouvement None. Sortir de la voiture (`ReleaseCharacter`) rétablit
exactement ce qui a été coupé. `-DriverLite=0` garde les conducteurs entiers.

## 10. Trafic sur tout le quartier streamé (11/11/2026)

Le réseau du quartier compte 1 043 voies sur environ 1,3 × 1,5 km, contre
quelques dizaines dans la rue. Avant, chaque apparition tirait une voie au
hasard parmi toutes, si bien que la plupart des essais tombaient loin de
l'anneau d'apparition. `AGTPTrafficNetwork::LanesInRing` ne garde maintenant
que les voies dont la boîte englobante touche l'anneau, et les voitures
conduites comme lointaines tirent dans cette liste.

Mesuré hors Unreal sur le lanes.csv du quartier (400 positions du joueur,
20 essais chacune), la part des essais qui tombent dans l'anneau est passée :
- pour les voitures conduites (40 à 250 m) : de 8,2 % à 83,9 % ;
- pour les voitures lointaines (120 à 250 m) : de 6,5 % à 75,5 %.

Le quartier compte 124 carrefours à feux et 471 têtes de feu. Une tête dont la
cellule streamée n'était pas chargée au départ trouve son sol quand elle
arrive à distance de dessin. Elle reste cachée jusque-là, au lieu de flotter
ou d'être enterrée.

## 11. Lumières des voitures de jour (12/11/2026, à mesurer)

Le diagnostic du PC comptait jusqu'à 44 lumières de voiture allumées en plein jour sur street-demo. Le code n'en allume aucune de jour : les phares de l'environnement ne s'allument que la nuit, sur les `LitCars` voitures les plus proches. Ces lumières viennent du blueprint de la voiture, allumées en permanence sur chaque voiture du trafic.

Correctif : `MakeCheap` éteint désormais les lumières du blueprint sur les voitures du trafic, les marionnettes et la police (le joueur garde les siennes). La nuit, les phares restent ceux de l'environnement, plafonnés à `LitCars`. `-CarBPLights=1` rend l'ancien comportement pour l'A/B. Aucun effet d'épave n'existe en C++ : une épave ne dessine que ce que le blueprint ajoute dans `OnWrecked`.
