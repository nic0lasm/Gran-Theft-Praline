# Réaction à la menace : personnes braquées (braquages de la 0.7)

*Version anglaise : [threat-response.md](threat-response.md)* · ADR-0006 · interrupteur : `-ThreatResponse=1` (coupé par défaut)

Une seule peur dans le jeu. Le combat dit au monde qui vise qui ; chaque personne décide quoi
faire. Le script de braquage de la boutique (feat/reference-interior) et les piétons lisent
cette peur ; ils ne calculent pas la leur.

Code : `GTPThreatLogic.h` (règles, sans types Unreal, vérifiées par
`python3 tools/ragdoll/check_impact_logic.py`), `GTPThreat.h` (`UGTPThreatSubsystem`,
`UGTPThreatResponseComponent`).

## États

Calme, Alarmé, MainsEnLair, **Obéit** (mains en l'air et obéit aux ordres : le script de la boutique
ouvre la caisse), Recule, Fuit, Au sol (renversé ou tué, via `UGTPHitReactionComponent`).

## Tempéraments (fixés par personne à la création)

| Tempérament | Comportement |
| --- | --- |
| Compliant | mains en l'air à 0,4 de peur, obéit après 1 s, ne court qu'à 0,92 (un tir, un coup, la panique autour) |
| Frozen | mains en l'air, obéit après 0,5 s, ne court jamais, même sous le feu ; après une chute, mains en l'air |
| Brave | la peur monte deux fois moins vite ; recule s'il est visé à moins de 4 m ; mains en l'air à 0,65 ; court à 0,97 |
| Fleeing | court à 0,45 |

La peur monte quand une personne **armée** le vise (cône de 12 degrés, 20 m, ligne de vue) : 0,9 par seconde à
bout portant, 0,2 par seconde à 20 m, mais viser seul ne dépasse jamais 0,8. Un tir sur lui ajoute 0,6, un tir à
moins de 18 m ajoute 0,35, une personne effrayée à moins de 12 m ajoute jusqu'à 0,35 par seconde (contagion), un
tressaillement ajoute 0,25. La peur retombe de 0,2 par seconde deux secondes après la fin de la menace. Une main
vide n'effraie personne.

## Contrat

- Entrée : `UGTPThreatSubsystem::SetThreat(Source, bArmed, Origine, Visée)`, `ClearThreat()`, `NotifyShot(Depuis, Cible)`.
  Sans le combat, `-ThreatDemo` fait du joueur une personne armée qui vise où regarde la caméra.
- Sortie, par personne : `OnThreatState(Corps, Ancien, Nouveau)`, `GetFear()`, `GetSecondsSinceAimed()`,
  `ObeysOrders()`, `OnFledToTarget(Corps)` après `SetFleeTarget(lieu)` (la porte de service).
- Côté script de boutique, pas ici : caisse, argent, alarme, `ReportCrime`, tempérament de chaque employé.
- Les personnages (employés) reculent et fuient seuls ; les piétons du pool sont déplacés par le système
  piétons, qui lit `OnThreatState` (`Fuit` remplace sa propre peur).
- Clips (à affecter par nom dans `Clips`) : `AM_HandsUp_Enter`, `AM_HandsUp_Idle` (tools/animation/car_poses.json).

### Les corps de la foule changent de main

Les 24 corps proches de la foule Mass sont mis en commun : chacun est donné tour à tour aux personnes les plus proches. Quand
un corps quitte une personne, sa pause mains en l'air s'arrête (plus personne ne lui ferait baisser les mains : elle restait
60 s, et sur un passage les voitures l'attendaient), et le corps oublie la peur, l'état et la mort de cette personne
(`UGTPThreatResponseComponent::Forget`) : la suivante commence calme. Tant que le joueur braque quelqu'un sur un passage, les
voitures attendent : c'est le braquage.

### Ce qu'un braquage doit montrer

`-GTARun` ne réussit ses deux étapes de braquage que si la personne visée se tient debout, les deux mains plus haut que la
tête, lu sur les os de son corps 1 s après que son état dit « braquée » ; un état qui le dit pendant 5 s sur un corps qui ne
le montre pas échoue aussitôt avec la pose (« hands up in his state, not on his body »). La porte du candidat 48 a passé les
deux étapes sur le compte des états alors que la personne visée courait et qu'une autre marchait dans le joueur.

Pour une personne qui a l'un des 24 corps, la peur du corps est la seule autorité : à chaque image le marcheur est remis en
accord avec elle (braquée = debout immobile, en fuite = en course), et un bruit fort ou la panique des marcheurs laissent
tranquille une personne tenue en joue. Les boucles (marche, attente, course, mains en l'air) cuites sur le squelette propre
du pack sont prises quand elles existent (`/Game/MetaHumans/GTP/AnimsCrowd/CS_*`, `retarget_player_animations.py` avec
`GTP_RTG_TARGET_MESH`, `GTP_RTG_PREFIX=CS_`, `GTP_RTG_FOLDER`). Le journal dit la pose d'un corps 2 s après le début de
chaque type d'animation (« crowd: pose of … »).

### La pose mains en l'air

Aucun pack d'Epic que nous avons ne contient d'animation mains en l'air : la boucle est faite par
`tools/pedestrians/make_handsup_animation.py` à partir de l'attente, sur chaque squelette qui la joue (mannequin UEFN
`A_HandsUp`, MetaHuman `MH_A_HandsUp`, foule City Sample `CS_A_HandsUp`) : bras levés un peu écartés, avant-bras presque
verticaux, paumes vers l'avant. La pose est résolue dans l'espace du corps à partir de la place de l'épaule, du coude, du
poignet et du pouce, donc sans dépendre des axes d'un squelette, et le script prouve son résultat en relisant l'asset
enregistré (« HANDSUP RESULT … : OK (hands N and N cm above the head …) »). Son calcul se vérifie sans l'éditeur
(`GTP_HANDSUP_SELFTEST=1 python3 tools/pedestrians/make_handsup_animation.py`). La première version écrivait des angles
locaux fixes avec les clés de position à zéro : les bras perdaient leur longueur et les mains s'arrêtaient 10 à 19 cm sous
la tête sur les corps de la foule, ce que le test strict du candidat 49 a montré en éditeur.

Une personne renversée par la voiture du joueur reste à terre là où son corps est tombé jusqu'à ce que ce corps soit
donné à quelqu'un d'autre : elle repartait sans corps visible au bout de 10 s, et son état pouvait passer à « mains en
l'air » sous une arme alors que le corps était couché sur la route.

Un corps de la foule qui est tombé (ragdoll) revient debout quand il est donné à la personne suivante : un maillage
squelettique simulé reste attaché et prend l'orientation de son bassin, donc sa place et son orientation sous l'acteur sont
gardées à la chute et remises (`AGTPPedestrian::ResetBody`). Avant, le corps restait couché, sans physique, pour toutes
les personnes à qui il était donné ensuite. La ligne de foule des 10 s finit par « fallen N, lying without physics N » : le
second nombre doit valoir 0. `-CrowdOwnLoops=0` garde les boucles `MH_` au lieu des `CS_`.
Seule une voiture renverse les personnes de la foule : le joueur à pied court à 500 cm/s, juste au seuil de vitesse, et
renversait comme une voiture les personnes dont il s'approchait à moins de 1,7 m.
