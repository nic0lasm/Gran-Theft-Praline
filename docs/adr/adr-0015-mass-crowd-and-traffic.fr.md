# ADR-0015 : Foule et trafic sur Mass

*Version anglaise : [adr-0015-mass-crowd-and-traffic.md](adr-0015-mass-crowd-and-traffic.md)*

## Statut

Accepté pour les piétons (2026-11-12). Corrigé pour le trafic le 2026-11-22 : le trafic par défaut
reste le trafic legacy, et MassTraffic est mesuré à chaque porte sans la décider.

## Date

2026-11-12, corrigé le 2026-11-22

## Contexte

La tranche demande une rue animée : plus de 150 personnes et 40 voitures autour du joueur, sur le
PC de référence, dans le budget d'image de l'ADR-0002. Nos premiers systèmes (ADR-0005 pour les
piétons, le trafic d'acteurs d'`AGTPTrafficNetwork`) coûtent un acteur par personne et par voiture.
Le framework Mass d'Unreal et le plugin MassTraffic de City Sample (gratuit, code Epic, jamais dans
Git) sont faits pour cette densité. Le 2026-11-12, Nico a choisi de basculer à la fois les piétons
et le trafic sur Mass, à la place de l'option « Mass plus tard » des ADR-0003 et ADR-0005.

## Décision

- **Piétons sur Mass par défaut.** Des agents MassCrowd sur le ZoneGraph généré depuis nos données,
  un vrai personnage près du joueur (réactions, témoins, ragdoll), des instances animées plus loin.
  `-Pedestrians=legacy` garde l'ancien système.
- **Trafic : legacy par défaut, MassTraffic derrière `-Traffic=mass`.** Les voies ZoneGraph, les
  spawners et le module `GTPMass` restent ; MassTraffic ne tourne que si on le demande.
- **Notre code sur Mass vit dans `GTPMass`**, pour que le jeu compile et tourne sans les plugins de
  City Sample.

## Correction du 2026-11-22 : ce que juge la porte

Les portes 52 à 57 ont jugé MassTraffic, alors que le jeu, sa passe `-GTARun` et ses vidéos roulent
avec le trafic legacy. MassTraffic n'a passé aucune porte complète pendant ce temps (voitures
couchées, puis files arrêtées derrière des piétons sur les passages, puis voitures proches sans corps
que le joueur traverse). Donc, à partir du candidat 60 :

1. La porte juge ce que voit le joueur. La passe default mesure le trafic legacy (voitures en
   mouvement, aucune couchée, le joueur ne les traverse pas : l'étape de choc de `-GTARun` tourne
   aussi sur des voitures legacy). C'est bloquant pour `next`, avec la vidéo de la porte.
2. MassTraffic sort du chemin critique du jalon A (voler une voiture et fuir la police). Ses passes
   (`mass`, `gtarun -Traffic=mass`) restent mesurées à chaque porte : voitures debout en mouvement
   de 120 à 300 s à 75,6 ou plus, au plus 5 % de couchées à moins de 100 m, voitures proches avec un
   corps, étape de choc. Ces critères sont informatifs. Ils redeviennent bloquants seulement pour
   basculer le trafic par défaut sur mass, quand une porte complète les tient.
3. Le seuil du trafic legacy est fixé à partir de la mesure de la porte 59, pas avant.

Les critères de chaque candidat sont dans `tools/perf/gate-spec.json`.

## Conséquences

- Le vol de voiture, la poursuite et leurs vidéos sont jugés sur le trafic que le joueur rencontre
  vraiment.
- Le travail sur MassTraffic continue, mais aucun nouveau lot MassTraffic n'est prévu avant la lecture
  de la mesure legacy de la porte 59.
- Les deux systèmes de trafic restent côte à côte jusqu'à ce que mass passe une porte complète ; le
  code legacy n'est pas retiré avant.

## Liens

ADR-0002 (budget de performance), ADR-0003 (déplacement), ADR-0005 (piétons),
[docs/gameplay/traffic-mass.fr.md](../gameplay/traffic-mass.fr.md),
[docs/gameplay/pedestrians-mass.fr.md](../gameplay/pedestrians-mass.fr.md).
