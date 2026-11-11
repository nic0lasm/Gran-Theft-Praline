# Réactions aux coups (pour le combat, version 0.7)

*Version anglaise : [hit-reactions.md](hit-reactions.md)* · ADR-0006

Le combat décide des dégâts ; cette couche décide de ce que fait le corps. Code :
`GTPHitLogic.h` (règles, sans types Unreal, vérifiées par
`python3 tools/ragdoll/check_impact_logic.py`), `GTPHitReaction.h`
(`UGTPHitReactionComponent`). Interrupteur : `-HitReactions=0`.

## Contrat pour le combat

Ajouter `UGTPHitReactionComponent` à tout corps qui peut être touché (joueur, piétons,
ennemis) et appeler, une fois par coup résolu :

`ReactToHit(FGTPHitInfo{ Damage, HealthAfter, Direction (de l'attaquant vers le corps, monde), Bone, bMelee })`

Elle renvoie ce qui a été fait et diffuse `OnReaction(Reaction, Hit, Corps)` (combat, police
et piétons peuvent écouter ; `OnDamageDealt` et `OnPawnKilled` restent dans le combat). Le
composant ne touche jamais à la santé.

## Règles

| Coup (part de la santé totale, tête x1,5, membre x0,7, corps à corps x1,25) | Réaction |
| --- | --- |
| moins de 6 % | rien |
| 6 à 20 % | tressaillement : le clip du côté d'où vient le coup (`AM_HitLight_Front/Back/Left/Right`) |
| 20 à 45 % | chancellement : `AM_Stagger_Front/Back` (les côtés utilisent les clips de tressaillement), vitesse de marche divisée par deux 0,6 s |
| 45 % et plus | renversement : ragdoll lancé dans le sens du coup, se relève |
| santé à 0 | mort : ragdoll, reste au sol |

Anti-enchaînement : les réactions à moins de 0,35 s sont ignorées (un renversement passe
toujours) ; le troisième tressaillement en 2 s devient un chancellement ; pas de second
renversement pendant 2 s après une chute. Le lancer vaut `200 + 6 par point de dégât` cm/s,
650 au plus, avec de la hauteur ; un coup à la tête qui tue ou renverse ajoute une vrille. La
zone vient du nom d'os (tête et cou, bras, jambes, le reste est le torse). La direction vient
de celle du coup face à l'orientation du corps.

## Animations

Les clips sont posés depuis des données (`tools/animation/car_poses.json`, construits dans
Blender par `build_car_poses.py`) ; affecter les montages importés à `Clips` du composant par
leur nom. Sans clip, le corps chancelle (vitesse) et tombe (ragdoll) quand même ; seul le
tressaillement reste muet. Le ragdoll reste sous le plafond de l'ADR-0006 (3 en m1) : au-delà,
un corps reste animé.
