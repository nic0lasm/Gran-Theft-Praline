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
- **Générateurs** : `tools/city/unreal/place_masstraffic.py` lit les réglages du plugin (les
  filtres de voies sont pointés sur nos tags), trouve ses configurations d'entités, pose un
  générateur de voitures roulantes et un de voitures garées (120 et 200 par défaut, pas de
  lancement au démarrage) et écrit `Saved/CityImport/masstraffic_report.json`.

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

## Points ouverts

- Près du joueur, MassTraffic remplace ses entités par des acteurs physiques issus de ses propres
  Blueprints. Pour le vol, les accidents et la police, ces acteurs doivent être nos classes
  `AGTPVehicle` (véhicules Epic reparentés) : à régler dans la configuration de représentation une
  fois le plugin installé.
- Feux : MassTraffic a les siens ; les nôtres (`GTPTrafficLights`) restent pour le trafic historique.
