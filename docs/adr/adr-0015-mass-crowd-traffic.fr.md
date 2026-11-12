# ADR-0015 : Foule et trafic sur Mass

*English version: [adr-0015-mass-crowd-traffic.md](adr-0015-mass-crowd-traffic.md)*

## Statut

Proposé le 2026-11-12. Décidé le soir même par le chef de projet (bascule sur Mass maintenant) ; cet ADR
l'enregistre et sera complété à deux par les fils Piétons et Trafic. Remplace les options « Mass plus
tard » des ADR-0003 et ADR-0005.

## Date

2026-11-12

## Décideurs

nic0lasm (chef de projet)

## Résumé

Piétons et trafic passent sur le framework Mass d'Unreal : des agents sur un ZoneGraph généré depuis nos
données, un niveau de détail qui transforme l'agent proche du joueur en vrai acteur (nos réactions, témoins,
ragdoll, vol de voiture) et les agents lointains en instances animées. Les systèmes actuels restent derrière
`-Pedestrians=legacy` et `-Traffic=legacy` jusqu'à ce que Mass mesure aussi bien.

## Contexte

La cible est une rue vivante de 150 piétons et 40 voitures et plus dans la tranche, hors de portée du pool
fixe de meshes squelettiques de l'ADR-0005 aux budgets M1 et PC. Les raisons du report de Mass (ZoneGraph dessiné à la
main dans la map, un binaire que plusieurs fils éditent) disparaissent si le ZoneGraph est généré par l'import
de la ville.

## Décision

1. **Plugins** du .uproject : MassGameplay, MassAI (MassCrowd, MassNavigation, MassLOD, MassRepresentation,
   MassAvoidance), ZoneGraph, ZoneGraphAnnotations, StateTree, GameplayStateTree, AnimToTexture. Smart Objects est déjà actif.
2. **ZoneGraph généré, jamais dessiné.** Un script Python d'éditeur, dans `run_city_import.sh`, crée les
   ZoneShapes depuis BD TOPO : trottoirs et passages depuis le graphe de `tools/city/pedestrians.py`, voies
   depuis `lanes.csv` ; tags Pedestrian, Crossing, Car. La map reste hors Git, recréée par l'import : personne ne l'édite à la main.
3. **Piétons.** Agents MassCrowd sur les voies piétonnes avec le LOD de Mass :
   - à moins de 40 m du joueur environ : un vrai acteur avec un personnage City Sample Crowds, nos réactions
     (panique, badauds, mains en l'air : `GTPWalkLogic.h`, `GTPThreat`), les témoins (`GTPWitnessLogic`) et le
     ragdoll, appelés depuis l'acteur ou des processeurs Mass ;
   - au-delà : des instances animées par texture (AnimToTexture) ;
   - la fuite passe par les annotations de perturbation du ZoneGraph, les arrêts aux vitrines et sur les bancs par les Smart Objects.
4. **Trafic** (fil Trafic) : MassTraffic copié depuis City Sample 5.8 dans `Plugins/MassTraffic`, ignoré par
   Git (le code Epic est interdit dans le dépôt public ; le README explique l'installation) ; voitures proches =
   acteurs Chaos (vol, collisions, poursuites), lointaines = instances ; notre code dépendant vit dans un
   module à part (GTPMass) pour que le reste compile sans.
5. **Porte de chaque étape** : CI verte, compilation PC, package PC qui tient 120 s sans crash. La parité est
   mesurée avant toute suppression de l'ancien code.

## Conséquences

- Les règles de réaction (hors moteur, testées en CI) sont conservées : elles deviennent la logique de
  l'acteur proche et des processeurs Mass ; le travail fait dessus n'est pas perdu.
- Les assets City Sample Crowds et MassTraffic sont du contenu Epic : hors Git, installés par script.
- La tranche peut glisser d'une à deux semaines (risque accepté par le chef de projet).
- Premier objectif mesurable : 150 piétons et plus dans la tranche, coût GPU mesuré sur le PC (semaine 1).

## Questions ouvertes (piétons)

- Seuils de LOD Mass entre acteur, ISM et instances AnimToTexture : à mesurer.
- Comment l'API de prêt (`AcquireFromPool`, commerces) s'articule avec les entités Mass : le pool d'acteurs proches reste.
