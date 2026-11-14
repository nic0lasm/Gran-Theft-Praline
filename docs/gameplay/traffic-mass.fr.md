# Trafic sur MassTraffic (City Sample)

État au 13/11/2026 : préparé, pas compilé, en attente de City Sample sur le PC. Décision :
ADR-0015 (piétons et trafic sur Mass). Anglais : [traffic-mass.md](traffic-mass.md).

## Ce qui est prêt

- **ZoneGraph de la carte** : `tools/city/zonegraph.py` et `tools/city/unreal/place_zonegraph.py`
  posent déjà nos voies (tag `Car`, profil à sens unique) et les polygones de carrefour (tags `Car`
  et `Intersection`), comme MassTraffic les attend.
- **Compilation** : le module `GTPMass` ne lie MassTraffic que si
  `Plugins/Traffic/Traffic.uplugin` (plugin « Traffic » de City Sample, module MassTraffic) existe (`WITH_GTP_MASSTRAFFIC`). Sans lui, le jeu
  compile comme avant.
- **Interrupteur** : `-Traffic=mass`. Le réseau historique (`AGTPTrafficNetwork`) garde alors les
  feux, les voitures garées à voler, les accidents et la police, mais ne conduit plus aucune
  voiture ; le sous-système `UGTPMassTrafficSubsystem` lance les générateurs Mass marqués
  `GTPMassTraffic`. Si le plugin n'est pas chargé, `-Traffic=mass` garde le trafic historique et
  l'écrit dans le journal.
- **Générateurs** : `tools/city/unreal/place_masstraffic.py` copie les générateurs de carrefours
  et de voitures roulantes de City Sample, avec chaque configuration d'entité et donnée de
  `/Game/AI/Traffic` qu'ils atteignent, dans `/Game/AI/GTPTraffic` (hors Git, cela reste du contenu
  Epic), puis repointe chaque référence des copies par réflexion (`UGTPAssetReflection`) : vers notre
  copie, vers notre voiture `BP_GTP_<Nom>` pour le véhicule physique que MassTraffic fait apparaître
  près du joueur (`PhysicsVehicleTemplateActor`, d'après le numéro de la voiture de City Sample), ou
  vers rien pour ce qui casserait le cook (Blueprints d'affichage des voitures de City Sample,
  conducteurs). Les configurations d'entités de véhicules du générateur de voitures roulantes de
  City Sample sont copiées aussi, pour nos seules voitures (bus, camions et le reste écartés,
  proportions de City Sample gardées). Les Blueprints de générateurs de City Sample n'ont aucun
  générateur de données d'apparition (les leurs étaient sur les générateurs de la carte de City
  Sample, d'où aucune voiture Mass au candidat 32 malgré 5 635 voies dans la ZoneGraph) : chaque
  générateur posé reçoit celui de MassTraffic par `UGTPMassSpawnerSetup`, et les filtres de voies
  propres aux types de véhicules sont pointés sur nos tags. Il
  les pose (120 voitures roulantes par défaut, pas de lancement au démarrage) et écrit
  `Saved/CityImport/masstraffic_report.json` (`gtp_copy` : chaque changement, les risques de cook des
  copies et les chaînes qui les tirent). Le générateur de voitures garées de City Sample n'est pas
  repris : ses places appartiennent à sa propre ville ; nos voitures garées restent.
  `GTP_MASSTRAFFIC_SURVEY=1` ne change rien et décrit les générateurs et configurations de City
  Sample (propriétés, dépendances, risques de cook).

## Installation (PC)

1. City Sample (Fab, gratuit) téléchargé ; copier ses dossiers `Plugins/Traffic` et
   `Plugins/RuleProcessor`, tels quels, dans `GranTheftPraline/Plugins/` (ignorés par Git : code
   Epic, hors de notre licence MIT). Traffic demande aussi ChaosVehiclesPlugin, ZoneGraph, MassAI,
   MassCrowd, MassGameplay et AnimToTexture, déjà activés.
   Appliquer les correctifs listés dans le README de github.com/Myxcil/MassTraffic-Test (on lit,
   on ne copie rien).
   Puis `python tools/vehicles/patch_citysample_traffic.py <project>/GranTheftPraline` : il coupe le
   faux C4723 de MSVC (« division par 0 ») dans MassTrafficSubsystem.cpp, qui sinon fait échouer le
   package Development (idempotent, original gardé en .orig, rapport md5).
   Puis `python tools/vehicles/copy_citysample_vehicle_base.py --src <CitySample> --project
   <project>/GranTheftPraline` : les Blueprints de voitures de City Sample ont besoin de la classe
   ci-dessous.
   Le dossier `Content` du plugin vient avec : les matériaux des véhicules utilisent sa fonction
   `MF_UnpackTrafficVehicleInstanceCustomData` (signalée absente au candidat 32 : vérifier que
   `Plugins/Traffic/Content/MaterialFunctions` est là et que `Traffic.uplugin` porte
   `"CanContainContent": true`).
2. `Traffic.uplugin` n'est pas activé par défaut : le `.uproject` le liste avec `"Optional": true`,
   il se charge là où le plugin est copié et il est ignoré sans erreur ailleurs (Mac, CI).
   RuleProcessor est activé par défaut.
3. Compiler la cible éditeur, lancer `place_zonegraph.py` puis `place_masstraffic.py`. Nos tags
   ZoneGraph gardent des bits fixes (Car 1, Intersection 2, Pedestrian 3, Crossing 4) à chaque
   import, et les filtres de voies de MassTraffic sont pointés dessus en C++
   (`UGTPZoneGraphImport::ConfigureTrafficLaneFilters` : aucune des deux classes de réglages n'est
   exposée à Python en 5.8), par le module GTPMass à chaque démarrage du moteur, jeu et package
   compris. Aucun fichier de config n'est écrit ni nécessaire. La ligne de journal « traffic lane
   filters » liste les filtres posés ; `place_masstraffic.py` signale une erreur s'il n'y en a aucun.
   Le moteur ne construit les données ZoneGraph (les voies où roule MassTraffic) de lui-même que si
   « construire pendant l'édition » est activé, ce qui n'est pas le cas : `place_zonegraph.py`
   appelle donc `UGTPZoneGraphImport::BuildZoneGraphData`, qui ajoute un acteur ZoneGraphData à la
   carte s'il n'y en a pas et lance le constructeur du moteur (`FZoneGraphBuilder::BuildAll`, forcé ;
   la reconstruction du sous-système est protégée). Les voies sont sauvegardées avec la carte. Son
   rapport donne `build` : formes, voies, zones ; 0 voie est une erreur.
   Les configurations du trafic et les Blueprints de spawners de City Sample sont dans le Content
   du projet City Sample (/Game/AI/Traffic, /Game/AI/Traffic/Spawners, /Game/Vehicle/veh*) : ils
   sont copiés aux mêmes chemins /Game dans notre projet (hors Git) pour garder leurs références.
4. Lire le rapport : filtres de voies posés et spawners de City Sample placés (carrefours,
   voitures garées, voitures roulantes, lancés dans cet ordre).
5. Packager, lancer avec `-Traffic=mass`. Le journal prouve ce qui tourne : une ligne
   « MassTraffic count » toutes les 5 s pendant la première minute, puis toutes les 30 s, donne les
   données ZoneGraph de la carte (voies, zones : 0 voie veut dire ZoneGraph jamais construite, donc
   aucune voiture possible), les voitures que Mass dessine en instances et en acteurs, et la plus
   proche. `-MassTrafficView` pose le joueur à 12 m de la première voiture Mass vue, pour une
   capture.

## Blueprints de voitures de City Sample

Les Blueprints de voitures de City Sample (`BP_vehicle` et ses enfants, utilisés par les
configurations et les spawners de MassTraffic) dérivent de `ACitySampleMassVehicleBase`, une classe
du module de jeu `CitySample` de City Sample (`Source/CitySample/Vehicles/CitySampleMassVehicleBase.h`
et `.cpp`), pas du plugin Traffic, qui ne déclare aucune classe de véhicule, seulement deux
interfaces. Sans elle, ils ne se chargent pas.

`copy_citysample_vehicle_base.py` copie les deux fichiers d'Epic, tels quels, dans un plugin local
`Plugins/CitySampleVehicleBase` (hors Git) dont le module s'appelle aussi `CitySample` : la classe
garde son chemin `/Script/CitySample.CitySampleMassVehicleBase`, les Blueprints se chargent sans
redirection ni code à nous pour la classe. Le script n'écrit que le descripteur du plugin, les
règles du module et un fichier de module d'une ligne, et s'arrête (rien d'écrit) si les deux
fichiers incluent d'autres fichiers du module de jeu City Sample. Le plugin s'active là où il est
copié ; ailleurs (Mac, CI), rien ne change.

Écarté : copier tout le module de jeu City Sample (mode de jeu, joueur, HUD et le reste, pour une
classe) ; déclarer nos voitures comme types de véhicules MassTraffic (les configurations et les
spawners d'Epic pointent tous sur `BP_vehicle` ; il faudrait refaire leurs configurations
d'entités à la main dans l'éditeur).

## Candidat 32 sur le PC (14/11)

La ZoneGraph est construite (5 635 voies, 2 705 zones) et MassTraffic y fait rouler 120 voitures,
en éditeur, 6 min sans erreur fatale ni ensure. Ensuite :
- les carrefours cherchaient les données de suivi de MassCrowd sur leurs voies de passage piéton
  et ont écrit « ArePedestriansClearOfIntersection - Null crowd tracking data » 7 millions de fois :
  les filtres de voies piétonnes et de passages de MassTraffic ne retiennent plus aucune voie (tag
  `GTPNoLane`), les carrefours n'ont donc plus de passage à surveiller (`-TrafficCrosswalks=1` garde
  ceux de City Sample). Que les voitures attendent nos piétons aux passages demandera le suivi de
  voies de MassCrowd : plus tard, avec les piétons ;
- les configurations de véhicules de City Sample laissent `PhysicsVehicleTemplateActor` vide
  (« Vehicles will be forced to low simulation LOD ») : nos copies nomment notre voiture du même
  numéro, posée par son nom (un membre vide n'apparaît même pas dans le texte exporté, et le
  premier essai l'avait laissé vide) ;
- aucune voiture n'était dessinée à moins de 50 m du joueur (plus d'acteur d'affichage) : la
  visualisation des copies dessine aussi les voitures proches en instances ;
- les types de feux pointaient vers 5 maillages de lampadaires absents (risque de cook) : les
  références vers des paquets absents sont retirées comme les autres risques, et une copie qui garde
  ses propres risques (les types de feux, dont les maillages manquent) est écartée avec toutes les
  références vers elle. MassTraffic ne dessine de toute façon aucun feu (pas de
  TrafficLightInstanceData pour notre carte).

## Candidat 33 sur le PC (14/11)

Rapport de placement : plus aucun risque de cook dans les copies, la voiture physique posée dans
les 7 configurations, aucune propriété impossible à écrire. 355 s en `-Traffic=mass` : aucune erreur
fatale, erreur ni ensure, un seul warning MassTraffic (« No TrafficLightInstanceData asset
specified »). Mais les 120 voitures ne sont jamais venues à moins de 50 m du joueur (53 à 56 m de
t=10 s à 35 s) et aucune n'est devenue un acteur. Le générateur de voitures roulantes était parmi
les copies « à risque », et c'est lui qui est posé dans la carte. Le candidat 34 dit pourquoi
(ci-dessous).

## Candidat 34 : piétons vus par les voitures, trafic clairsemé ou figé

- La voie d'Epic pour que les voitures voient les gens est le trait « Traffic Obstacle »
  (`UMassTrafficObstacleTrait` : tag `FMassTrafficObstacleTag`, transform, vitesse, rayon d'agent).
  À chaque image, `UMassTrafficFindObstaclesProcessor` lie chaque obstacle à la voiture qui le suit
  sur les voies de circulation à moins de `ObstacleSearchRadius`, et
  `UMassTrafficUpdateDistanceToNearestObstacleProcessor` freine cette voiture d'après le temps de
  collision (jusqu'à 0,8 à 3 m de l'obstacle). Notre foule Mass (`GTPCrowd`, qui marche sur son
  propre graphe de trottoirs et non sur les voies de MassCrowd) porte maintenant transform, vitesse
  et rayon (40 cm) quand la carte a des voitures MassTraffic, et ne porte le tag que sur un passage :
  sur le trottoir une voiture qui passe à 2 m freinerait pour rien, chaque obstacle coûte une
  recherche de voies par image, et le générateur de voitures laisse 50 m libres autour de chaque
  obstacle (`ObstacleExclusionRadius` ; commentaire d'Epic : « This won't scale past very few
  obstacles »). Les gens sont posés hors des passages, les voitures créées au début ne sont donc pas
  écartées.
- `ObstacleSearchRadius` passe des 100 m d'Epic à 15 m, posé en mémoire avec les filtres de voies
  (`-TrafficObstacleRadius=<cm>`).
- L'autre voie de MassTraffic, aux feux : un carrefour attend `NumEntitiesOnLane = 0` sur ses voies de
  passage, d'après les données de suivi de MassCrowd (faites pour les voies qui portent le
  `CrossingTag` de MassCrowd), et ouvre ou ferme ces voies (`SetLaneState`). Il ne compte que les
  gens suivis par MassCrowd : pour notre foule, il faudrait appeler nous-mêmes l'API publique de
  MassCrowd (`OnEntityLaneChanged`, `AcquireWaitingSlot`, `GetLaneState`). Pas fait : les filtres de
  passages ne retiennent toujours aucune voie.
- Nouvelle ligne à chaque comptage, « MassTraffic cars » : les voitures simulées (entités Mass),
  combien roulent (plus de 1 m/s), leur vitesse moyenne, combien à moins de 100 et 200 m du joueur,
  la plus proche ; et une fois la longueur des voies de voitures et les voitures par km. Elle
  distingue un trafic clairsemé (120 voitures pour toute la carte) d'un trafic figé (voitures qui
  attendent à des carrefours qui ne s'ouvrent jamais).
- Placement : une copie n'est « à risque » que pour un risque trouvé. Un parcours coupé à sa limite
  de paquets (qui comptait aussi comme risque avant) est listé dans `unsafe_scan_cut` ; les chaînes
  de chaque copie à risque sont dans `unsafe_why` ; les références d'une copie à elle-même restent ;
  un générateur posé qui garde des risques est une erreur.
- Sur le PC la cause est apparue : notre générateur de voitures roulantes pointait encore, en dur,
  vers le générateur de carrefours de City Sample lui-même (et par lui vers les 5 lampadaires
  absents), une référence que le repointage du texte exporté n'a pas atteinte (sans doute le
  graphe du Blueprint ou son code compilé). Chaque copie passe d'abord par le remplacement de références du moteur
  (`UGTPAssetReflection::ReplacePackageReferences`, `FArchiveReplaceObjectRef` comme « Replace
  References », puis le Blueprint est recompilé) : toute référence à un paquet copié va vers notre
  copie. Rapport : `engine_replaced`, `engine_replace_errors`.

## Candidat 34 sur le PC : un trafic qui se fige

352 s sur le PC : aucune erreur, 769 voies de voitures (14,9 km), 120 voitures. Au début 99 roulent
(19 km/h) ; deux minutes plus tard, 12 (3 km/h). Cause la plus probable, lue dans les sources de
MassTraffic : une voie sans voie après elle est un cas que MassTraffic ne traite pas
(`UMassTrafficChooseNextLaneProcessor` : « Should never happen »), sa voiture s'y arrête pour de
bon, et notre zone a de telles impasses partout où une rue sort de la carte (traffic.py : « a lane
with no other way on is a dead end, where the game takes the car away », ce que fait le trafic
historique et pas MassTraffic). Le réseau de City Sample est fermé. Deux changements :
- `tools/city/zonegraph.py` ferme les impasses qu'il peut : là où une voie que rien n'alimente
  part à moins de 12 m dans l'autre sens (l'autre côté d'une rue à double sens), une spline de
  demi-tour (tag Car, 15 km/h) les relie, 3 m après le bout de la rue. Comptes `dead_ends_closed` et
  `dead_ends_left` (les rues à sens unique qui sortent de la carte restent des impasses) ;
- la ligne « MassTraffic cars » compte les voitures arrêtées sur une voie en impasse, et une fois
  les voies en impasse du réseau.

PC, deuxième passage : les copies n'ont plus aucun risque de cook (registre relu après les
sauvegardes). Mais zonegraph.py n'a fermé que 6 impasses sur 89, et 50 des 120 voitures ont fini
arrêtées sur les 83 restantes (775 voies de voitures dans la ZoneGraph). Ensuite :
- zonegraph.py relie d'abord une impasse à sa voie suivante de lanes.csv quand celle-ci part à
  moins de 10 m et que rien ne l'alimente (deux rues qui se rejoignent hors carrefour, voies décalées
  différemment) : les 6 derniers mètres de la voie sont redessinés pour finir dessus. Comptes
  `dead_ends_joined` et `dead_ends_left_with_next` ;
- les impasses qui restent (rues à sens unique qui sortent de la carte) sont confiées à MassTraffic
  lui-même : chaque seconde, une voiture arrêtée au bout de l'une d'elles, à 80 m ou plus du joueur,
  reçoit `FMassTrafficRecyclableVehicleTag`, le tag que MassTraffic donne à une épave réparée, et son
  superviseur (`UMassTrafficOverseerProcessor`) la pose dans une place libre d'une des voies les moins
  chargées loin du joueur. Ses distances (`BusiestLaneDistanceToPlayerRange`,
  `LeastBusiestLaneDistanceToPlayerRange`) passent de 500 m, à peu près toute notre zone, à 150 m
  (`-TrafficTransferDistance=<m>`, `-DeadEndKeepNear=<m>`, `-NoDeadEndRecycle`). Journal : « waiting
  for recycling » et « recycled so far ».

PC, troisième passage (1b1f38f) : 43 impasses reliées, 6 fermées, 40 restantes ; 8 voitures
seulement arrêtées sur une voie en impasse. Mais tout le trafic se fige en deux minutes (110
voitures, 1 qui roule à 120 s), et les 10 voitures confiées au recyclage n'ont jamais été déplacées.
Lu dans les sources de MassTraffic :
- son choix de voie ne regarde pas plus loin, et `ShouldStopAtLaneExit` arrête une voiture pour de
  bon avant une voie suivante qui n'a pas de voie après elle (« NONEXT ») : les voitures ne
  s'arrêtent pas sur les voies en impasse mais sur la voie d'avant, et au milieu du carrefour quand
  l'impasse en part. Un carrefour sans feux (`UMassTrafficUpdateIntersectionsProcessor`, aucun des
  nôtres n'en a) ne change de phase qu'une fois ses voies qui se ferment vides : une voiture arrêtée
  dedans et le carrefour ne s'ouvre plus jamais, pour personne ; les files gagnent ensuite les
  carrefours suivants ;
- le superviseur ne pose une voiture que sur une voie à au moins `MinTransferDistance` (500 m)
  d'elle : pour une voiture au milieu de notre zone, il n'y en a aucune.
Changements (C++ seulement, la carte reste telle quelle) :
- `UGTPMassTrafficSubsystem::CutDeadEndBranches`, une fois avant les générateurs : les voies de
  voitures dont tous les chemins finissent dans une impasse (les impasses, puis en remontant chaque
  voie dont toutes les suivantes sont de ce genre) sont retirées du choix des voitures dans les
  données de voies de MassTraffic lui-même (`FZoneGraphTrafficLaneData::NextLanes`) ; toutes les
  autres voies gardent un chemin. Dans une branche, les voies simples suivantes restent et les voies
  de carrefour partent, pour qu'une voiture née là s'arrête hors de tout carrefour ; les voies
  simples d'une branche sont fermées (`bIsOpen`), pour que le superviseur n'y remette jamais de voiture. Journal
  « MassTraffic dead ends: N car lane(s) of M only lead to dead ends » ; `-NoDeadEndCut` garde les
  liens ;
- le recyclage prend toute voiture d'une telle branche (celle qui roule dans un carrefour une fois
  sortie), et les voitures arrêtées au bout d'une voie sans suite, hors de la vue du joueur (dans le
  dos de la caméra dès 40 m, devant dès 120 m, `-DeadEndKeepNear=<m>`) ; une voiture sans le fragment
  de dégâts d'Epic (la requête du superviseur l'exige) en reçoit un ;
- `MinTransferDistance` : 200 m (`-TrafficMinTransfer=<m>`) ;
- une ligne « MassTraffic stops » dit où sont les voitures arrêtées : dans une branche sans issue,
  dans un carrefour, pour un piéton ou le joueur, derrière une voiture, au bout de la voie (pas de
  voie suivante, carrefour fermé, carrefour ouvert sans place après lui, voie simple), autre.

PC, candidat 35 (22eb010) : 307 des 775 voies de voitures ne mènent qu'à des impasses (182 dans
des carrefours), 182 liens retirés, 54 voitures confiées au recyclage, dont 51 sans fragment de
dégâts, qui en ont reçu un (la requête du superviseur l'exige ; s'il les a ensuite déplacées, le
journal ne le dit pas encore). Plus aucune voiture arrêtée en impasse ni dans un carrefour,
mais à 300 s 11 voitures sur 120 roulent : sur les 109 arrêtées, 34 attendent devant une voie de
carrefour fermée et 72 derrière elles. 38 erreurs « Intersection N has side with no left most
intersection lane » : MassTraffic construit les côtés de ses carrefours à partir des voies suivantes
des voitures, donc une arrivée dont toutes les voies de carrefour ont été coupées devient un côté
vide (seule la recherche de feu le saute ; sa phase est vide et passe aussitôt). Lu dans
MassTraffic : `ShouldStopAtLaneExit` ne laisse une voiture entrer dans un carrefour que si la voie
d'après a la place pour elle et son écart (2 x rayon + jusqu'à 5 m, `MinimumDistanceToNextVehicleRange`),
et une voiture arrêtée à la ligne ne choisit plus jamais ; zonegraph.py coupe chaque voie 6 m avant
et après un carrefour et garde 2 m d'une voie courte, donc une voie après un carrefour peut ne
jamais avoir la place : un mur. Changements :
- une voie de carrefour vers une voie de moins de 10 m (voiture de 5 m, écart maximal d'Epic de 5 m,
  `-TrafficMinRoomAfterJunction=<m>`) compte comme une impasse dans `CutDeadEndBranches` ; le compte
  est dans la ligne « MassTraffic dead ends » ;
- une ligne « MassTraffic junction waits » dit pourquoi chaque voie de carrefour fermée le reste,
  dans l'ordre de MassTraffic : pas de voie après, voie d'après plus courte que la voiture ou pleine,
  voie dans aucune phase de son carrefour, carrefour qui attend qu'une voie de la phase d'avant se
  vide (une voiture dedans, comptée mais vide, réservée), attend son tour, pas vue prête ; et les
  voitures recyclées restées dans un carrefour.

PC, 6fbd155 : 468 des 775 voies de voitures finissent dans des branches coupées (133 voies de
carrefour vers une voie de moins de 10 m) : le réseau tombe à 40 %. À 300 s, 26 voitures sur 120
roulent, aucune à moins de 50 m du joueur ; des 94 arrêtées, 78 sont derrière une voiture et 14 à
une voie de carrefour fermée (10 avec une voie pleine après, 3 avec une voiture dans le carrefour,
1 qui attend son tour). Trois correctifs de suite ont traité des symptômes.

## Candidat 35 : des carrefours construits comme ceux d'Epic

Comparé à ce qu'Epic a construit pour City Sample, lu à trois endroits : le
`Config/DefaultPlugins.ini` de City Sample (réglages ZoneGraph et MassTraffic ; lu sur une copie
publiée du projet, à vérifier sur la copie du PC), le routage des polygones de ZoneGraph
(`ZoneShapeUtilities.cpp`, `TessellatePolygonShape`, `BuildLanesBetweenPoints`) et MassTraffic
(`ShouldStopAtLaneExit`, `MassTrafficIntersectionSpawnDataGenerator.cpp`). Ce qui diffère ici :
- **Routes.** Les routes de City Sample sont des splines à double sens de voies de 4 m
  (« Vehicle ↓↑ 400 » et plus larges, tags Vehicle + Capillary Road) d'un carrefour au suivant ; un
  polygone de carrefour a un point LaneProfile par route, avec tout le profil de cette route. Chez
  nous : une spline par voie (3,2 m), coupée partout où la BD TOPO coupe une rue (à chaque
  changement d'attribut), si bien qu'une voie après un carrefour peut être un bout de quelques
  mètres ; un point de polygone par bout de voie. Dans la table du quartier (1 043 voies), 196 voies
  font moins de 10 m, dont 122 des 604 voies qui sortent d'un carrefour.
- **Carrefours.** traffic.py donne un seul numéro de carrefour aux nœuds à moins de 18 m, mais les
  voies entre eux restent : 45 voies vont d'un carrefour au même carrefour, dans son polygone, et y
  ajoutent des points ; 21 voies de 18 à 22 m relient deux carrefours et gardent 6 à 10 m une fois
  coupées de 6 m à chaque bout. MassTraffic ne laisse entrer une voiture dans un carrefour que si la
  seule voie d'après a la place pour la voiture et son écart (2 x rayon + jusqu'à 5 m) : une telle
  voie est un mur.
- **Tags et filtres.** City Sample : TrafficLaneFilter Vehicle (pas Pedestrian),
  IntersectionLaneFilter Vehicle + Intersection + City (pas Pedestrian, Freeway, Trunk Road,
  Crosswalk), TrunkLaneFilter « Trunk Road » (ses voies d'autoroute seulement),
  LaneChangingLaneFilter Vehicle (pas Pedestrian, Intersection, Crosswalk). Chez nous, toute voie de
  voiture était une voie « trunk » (chaque côté de carrefour compte alors comme venant de
  l'autoroute) et les changements de voie étaient permis dans les carrefours.
- **Comme chez Epic, gardé :** nos points de polygone sont par voie, mais ZoneGraph relie un point à
  tous les autres dans LaneConnectionAngle (120 degrés par défaut, que nous gardons ; City Sample
  met 180 avec un point par route) : avec des points par voie, 180 ajouterait des demi-tours, 120
  les écarte.
- **Pas fait ici :** les feux (Small City a des feux à 106 de ses 172 carrefours, d'après un export
  publié de son réseau ; les nôtres sont tous des stops), les splines de route à double sens
  (changements de voie), les tags de densité.
Changements :
- `tools/city/zonegraph.py` enchaîne une voie qui ne continue que dans une voie que rien d'autre
  n'alimente (`lanes_chained`), fond en un seul les carrefours qu'une voie de moins de 22 m
  (6 m + 10 m + 6 m) relie, tant que leurs nœuds tiennent dans 45 m (`junctions_merged`), et retire
  une voie courte d'un carrefour vers le même (`junction_lanes_dropped`) ; `short_after_junction`
  compte les voies qui sortent encore d'un carrefour avec moins de 10 m. Sur la table du quartier :
  687 voies au lieu de 1 043 (38,3 km au lieu de 38,7), 173 polygones au lieu de 186, voies de moins
  de 10 m à la sortie d'un carrefour 10 au lieu de 122 (les 10 au bord de la carte, sans voie après) ;
- `ConfigureTrafficLaneFilters` : aucune voie « trunk » (GTPNoLane), pas de changement de voie dans
  un carrefour.

## Points ouverts

- Près du joueur, MassTraffic peut confier une voiture à son véhicule physique : nos copies
  désignent nos voitures (`BP_GTP_*`, volables), mais MassTraffic conduit un tel acteur par son
  interface `IMassTrafficVehicleControlInterface` (`SetVehicleInputs`), qu'`AGTPVehicle`
  n'implémente pas encore. À vérifier sur le PC, puis à ajouter (une sous-classe dans GTPMass, qui
  peut lier MassTraffic).
- `MassTrafficSubsystem::ClearAllTrafficLanes()` doit être appelée quand le dépeuplement d'un
  générateur est fini (README de Myxcil/MassTraffic-Test) : pas fait, on ne dépeuple pas encore.
- Feux : MassTraffic cadence les siens mais n'en dessine aucun ; les nôtres (`GTPTrafficLights`) ne
  sont pas synchronisés avec eux.
- Le joueur à pied n'est pas un obstacle de trafic (City Sample donne au joueur un agent Mass).
- Passages aux feux (données de suivi de MassCrowd, voir le candidat 34) : pas fait.
