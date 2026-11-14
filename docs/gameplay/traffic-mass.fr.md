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
  numéro ;
- aucune voiture n'était dessinée à moins de 50 m du joueur (plus d'acteur d'affichage) : la
  visualisation des copies dessine aussi les voitures proches en instances ;
- les types de feux pointaient vers 5 maillages de lampadaires absents (risque de cook) : les
  références vers des paquets absents sont désormais retirées comme les autres risques. MassTraffic
  ne dessine de toute façon aucun feu (pas de TrafficLightInstanceData pour notre carte).

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
