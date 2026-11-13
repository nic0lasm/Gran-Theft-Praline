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
2. `Traffic.uplugin` n'est pas activé par défaut : le `.uproject` le liste avec `"Optional": true`,
   il se charge là où le plugin est copié et il est ignoré sans erreur ailleurs (Mac, CI).
   RuleProcessor est activé par défaut.
3. Compiler la cible éditeur, lancer `place_zonegraph.py` puis `place_masstraffic.py`.
   Les configurations du trafic et les Blueprints de spawners de City Sample sont dans le Content
   du projet City Sample (/Game/AI/Traffic, /Game/AI/Traffic/Spawners, /Game/Vehicle/veh*) : ils
   sont copiés aux mêmes chemins /Game dans notre projet (hors Git) pour garder leurs références.
4. Lire le rapport : filtres de voies posés et spawners de City Sample placés (carrefours,
   voitures garées, voitures roulantes, lancés dans cet ordre).
5. Packager, lancer avec `-Traffic=mass`.

## Blueprints de voitures de City Sample

Ils dérivent de `ACitySampleMassVehicleBase`, une classe du module de jeu City Sample (pas du plugin
Traffic), que notre projet n'a pas. Lue d'après son interface seulement : un acteur avec un
composant d'agent Mass, recyclable par Mass, et une fonction appelable en Blueprint (flou des roues).
`AGTPMassVehicleBase` (module GTPMass) en est notre propre version, et une CoreRedirect dans
`Config/DefaultEngine.ini` y renvoie l'ancienne classe : les Blueprints se chargent tels quels.
Copier plutôt le module de jeu City Sample a été écarté : tout un module de jeu (mode de jeu,
joueur, HUD, foule) avec ses propres dépendances de plugins, pour une petite classe de base. Le
flou des roues n'est pas fait (roues nettes).

## Points ouverts

- Près du joueur, MassTraffic remplace ses entités par des acteurs physiques issus de ses propres
  Blueprints. Pour le vol, les accidents et la police, ces acteurs doivent être nos classes
  `AGTPVehicle` (véhicules Epic reparentés) : à régler dans la configuration de représentation une
  fois le plugin installé.
- Feux : MassTraffic a les siens ; les nôtres (`GTPTrafficLights`) restent pour le trafic historique.
