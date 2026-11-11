# HUD de combat

*English version: [hud-combat.md](hud-combat.md)*

Spécification : `docs/gameplay/combat.fr.md` (« HUD »). Le HUD ne fait que dessiner ; les
systèmes de combat exposent les valeurs (`UGTPHealthComponent`, `UGTPWeaponComponent`,
`UGTPCombatSubsystem`). Code : `GTPHudCombat.cpp`, maths dans `GTPHudMath.h` (testées par
`tools/ui/tests/run_cpp_tests.sh`). Rien n'est dessiné avec `-Combat=0`.

- **Réticule** : avec une arme sortie, un anneau dont le rayon est le cône de dispersion de
  l'arme à l'écran (`tan(dispersion) / tan(fov / 2) * largeur / 2`, entre 5 et 220 px) et un
  point central ; plus lumineux en visée. Sans arme, le point simple du HUD à pied reste.
- **Marqueur de touche** : une croix au centre pendant 0,15 s, qui s'efface ; rouge sur une mort.
- **Santé et armure** : deux arcs fins au-dessus de la mini-carte, remplis de gauche à droite ;
  la santé passe du vert à l'ambre puis au rouge ; l'armure (bleu clair) n'apparaît que si > 0.
- **Ligne d'arme** à côté de la mini-carte : nom (clé de la table de chaînes = clé de l'arme,
  p. ex. `Weapon.Pistol9`), `chargeur / réserve`, rouge à 0, « Rechargement » pendant la recharge.
- Les touches libres du combat sont respectées : carte plein écran sur M / croix haut (le combat
  prend croix bas et gauche, LT / RT, X, R, 1).

Rebase : cette branche repose sur `feat/combat-core` ; à l'arrivée du combat sur next, rebaser
sur next.
