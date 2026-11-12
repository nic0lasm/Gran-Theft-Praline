# Piétons sur Mass : correspondance des comportements actuels

*English version: [pedestrians-mass.md](pedestrians-mass.md). Décision : [ADR-0015](../adr/adr-0015-mass-crowd-traffic.fr.md).*

Ce qui existe (`GTPWalkLogic.h`, hors moteur, testé par `tools/pedestrians/check_walk_logic.py`) et où chaque
partie vivra quand les piétons seront des agents Mass. Rien n'est encore construit : c'est le plan de la
première tranche Mass.

| Aujourd'hui | Sur Mass | Remarques |
| --- | --- | --- |
| Graphe de trottoirs (CSV BD TOPO) | Voies ZoneGraph générées depuis les mêmes données (tags Pedestrian, Crossing) | une source, deux sorties : le CSV reste pour l'ancien système et les tests |
| `GTPWalk::Step` (marche, pause, traversée) | Navigation MassCrowd sur les voies ; pauses par Smart Objects (vitrine, banc, téléphone) | l'attente au passage garde `GTPCrossingBlockedQuery` comme annotation ZoneGraph alimentée par les feux |
| `Flee`, `CatchPanic` (troupeau) | Annotation de perturbation ZoneGraph : un danger écrit une perturbation, les agents à portée passent en fuite et la transmettent une fois (mêmes rayon et rangs) | la règle (7 m, 65 %, 2 rangs) reste dans une fonction hors moteur commune aux deux |
| `Gawk`, `Freeze` (mains en l'air) | Tâches de StateTree « Regarder » et « Mains en l'air » sur l'acteur proche ; les agents lointains fuient ou s'arrêtent seulement | `GTPThreat` et `GTPWitnessLogic` sont appelés depuis l'acteur, jamais réécrits |
| 20 meshes squelettiques dessinés | Acteur proche (≈ 40 m, personnage City Sample Crowds) tiré d'un pool ; au-delà, instances AnimToTexture | le pool d'acteurs garde `AcquireFromPool` pour commerces et scènes |
| Plafond de ragdolls (3 sur m1) | Acteur proche seulement, même plafond partagé | les agents lointains ne font jamais de ragdoll |
| `-PedDemo=panic\|gawk\|shot` | Conservé : les événements déclenchent les mêmes chemins perturbation et bruit fort | le PC filme les trois mêmes scénarios sur les deux systèmes |

Première tranche (semaine 1) : agents MassCrowd sur les voies générées, échange vers l'acteur proche avec nos
réactions, 150 piétons et plus dans la tranche, coût GPU sur le PC, `-Pedestrians=legacy` pour comparer.

Ce qu'il faut des autres : le ZoneGraph généré (fil Trafic, avec voies Pedestrian et Crossing), les assets
City Sample Crowds (hors Git), et le PC pour compiler et mesurer.
