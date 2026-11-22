# Documentation

*English version: [README.md](README.md). Retour au [README du projet](../README.fr.md).*

Chaque document existe en français (`nom.fr.md`) et en anglais (`nom.md`).
Les documents de la partie *Historique* décrivent des plans antérieurs
(versions 0.4 à 0.9, versions Mac, chaîne Tripo) ; ils sont gardés pour
mémoire et ne guident plus le travail.

## Pour commencer

| Document | À quoi il répond |
| ---- | ---- |
| [Concept du jeu](../design/gdd/game-concept.fr.md) | Ce qu'est le jeu, pour qui, et ce qu'il n'est pas |
| [Feuille de route](../production/roadmap.fr.md) | La tranche, ses trois jalons et les dates |
| [Tableau de bord](../production/roadmap-status.fr.md) | Ce qui est prouvé aujourd'hui, porte par porte |
| [Décisions d'architecture](adr/) | Pourquoi chaque grand choix technique a été fait |

## Décisions d'architecture (ADR)

| ADR | Sujet |
| ---- | ---- |
| [0001](adr/adr-0001-rendering-pipeline.fr.md) | Pipeline de rendu photoréaliste |
| [0002](adr/adr-0002-performance-budget.fr.md) | Cible de performance et budget d'image |
| [0003](adr/adr-0003-movement.fr.md) | Déplacement : à pied, en voiture, conducteurs PNJ |
| [0004](adr/adr-0004-wanted-police.fr.md) | Niveau de recherche et poursuite de police |
| [0005](adr/adr-0005-pedestrians.fr.md) | Piétons |
| [0006](adr/adr-0006-ragdoll-and-impacts.fr.md) | Ragdoll et réactions physiques |
| [0007](adr/adr-0007-interface-settings-minimap.fr.md) | Textes, réglages et mini-carte |
| [0008](adr/adr-0008-day-night-weather.fr.md) | Cycle jour/nuit et météo pilotés par code |
| [0009](adr/adr-0009-audio-mix.fr.md) | Audio du jeu |
| [0010](adr/adr-0010-city-cells-streaming.fr.md) | Charger le quartier par cellules |
| [0011](adr/adr-0011-gameplay-framework.fr.md) | Framework gameplay du combat |
| [0012](adr/adr-0012-interiors.fr.md) | Intérieurs sans chargement |
| [0014](adr/adr-0014-missions-framework.fr.md) | Cadre de missions, données en tables |
| [0015](adr/adr-0015-mass-crowd-and-traffic.fr.md) | Foule et trafic sur Mass ; trafic legacy par défaut |

## La ville

| Document | Sujet |
| ---- | ---- |
| [Reproduire Lyon fidèlement](city/plan.fr.md) | Sources de données et chaîne de traitement |
| [Générer la Presqu'île en 3D](city/generation.fr.md) | LiDAR, données Grand Lyon, façades procédurales |
| [Bible artistique](art/art-bible.fr.md) | L'image visée |
| [Stratégie de rendu](rendering/rendering-strategy.fr.md) | Photoréalisme : ce qui est utilisé et pourquoi |
| [Cycle jour/nuit et météo](rendering/day-night-weather.fr.md) | Soleil, ciel, pluie |
| [Assets gratuits](art/free-assets.fr.md), [recensement des sources](sourcing/asset-sourcing.fr.md) | Ce qui existe gratuitement avant de fabriquer quoi que ce soit |

## Le jeu

| Document | Sujet |
| ---- | ---- |
| [Personnage jouable](gameplay/player-character.fr.md) | Nicolito et le Game Animation Sample |
| [Sensation à pied](gameplay/on-foot-feel.fr.md) | Caméra, aides, ambiance |
| [Véhicules conduisibles](gameplay/vehicles.fr.md) | Voler et conduire |
| [Trafic sur MassTraffic](gameplay/traffic-mass.fr.md), [réactions du trafic](gameplay/traffic-reactions.fr.md) | Le trafic de City Sample à Lyon |
| [Piétons](gameplay/pedestrians.fr.md), [piétons sur Mass](gameplay/pedestrians-mass.fr.md) | La foule et ses réactions |
| [Recherche et police](gameplay/wanted.fr.md) | Témoins, étoiles, poursuite |
| [Combat et armes](gameplay/combat.fr.md), [réactions aux coups](gameplay/hit-reactions.fr.md), [réaction à la menace](gameplay/threat-response.fr.md) | Braquages et tir |
| [Ragdoll](gameplay/ragdoll.fr.md), [mobilier cassable](gameplay/breakable-street-furniture.fr.md) | Réactions physiques |
| [Sources d'animation](animation/sources.fr.md), [animations de combat](animation/combat-sources.fr.md), [poses de voiture](animation/car-poses.fr.md) | D'où viennent les animations |
| [Trajets de test](testing/test-drives.fr.md) | Trajets scriptés utilisés par les tests |

## Performance et tests

| Document | Sujet |
| ---- | ---- |
| [Coût des véhicules](performance/vehicle-cost.fr.md), [des piétons](gameplay/pedestrians-cost.fr.md), [du ragdoll](gameplay/ragdoll-cost.fr.md) | Ce que coûte chaque système par image |
| [QA et sortie](qa/README.fr.md) | Portes, listes de contrôle, modèle de bogue |
| [Git LFS](project/git-lfs.fr.md) | Quels fichiers passent par LFS |

## Historique

| Document | Sujet |
| ---- | ---- |
| [Notes de la version 0.3.0](releases/0.3.fr.md) | La dernière version publique, macOS |
| [Build macOS en un clic](build/one-click-build.fr.md) | Comment les versions Mac étaient packagées |
| [Inventaire des systèmes](../production/systems-inventory.fr.md) | La revue du 10 novembre, sur la feuille de route v3 |
| [Plan vers la 0.9](../production/test-phase-plan.fr.md), [protocole de test 0.9](qa/test-protocol.fr.md) | Une build de test repoussée par le recentrage du 12 novembre |
| [Missions](gameplay/missions.fr.md), [argent](gameplay/money.fr.md), [intérieur de référence](gameplay/reference-interior.fr.md), [audio](audio/audio-design.fr.md), [interface](ui/) | Systèmes gelés jusqu'à la fin de la tranche |
| [Chaîne d'import Tripo](assets/import-pipeline.fr.md), [catalogue d'assets](../production/asset-catalog.fr.md) | Assets générés, remplacés par les contenus gratuits d'Epic |
| [Résultats des tests de rendu](rendering/test-results/) | Captures Mac des 9 et 10 novembre |
