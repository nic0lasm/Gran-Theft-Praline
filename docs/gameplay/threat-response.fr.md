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
tressaillement ajoute 0,25. La peur retombe de 0,12 par seconde deux secondes après la fin de la menace. Une main
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
