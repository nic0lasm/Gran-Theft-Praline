# Combat HUD

*French version: [hud-combat.fr.md](hud-combat.fr.md)*

Spec: `docs/gameplay/combat.md` ("HUD"). The HUD only draws; the combat systems expose
the values (`UGTPHealthComponent`, `UGTPWeaponComponent`, `UGTPCombatSubsystem`).
Code: `GTPHudCombat.cpp`, maths in `GTPHudMath.h` (tested by `tools/ui/tests/run_cpp_tests.sh`).
Nothing is drawn with `-Combat=0`.

- **Reticle**: with a weapon drawn, a ring whose radius is the spread cone of the weapon
  on screen (`tan(spread) / tan(fov / 2) * width / 2`, between 5 and 220 px) and a centre dot;
  brighter when aiming. Without a weapon the plain dot of the on-foot HUD stays.
- **Hit marker**: a cross at the centre for 0.15 s, fading; red on a kill.
- **Health and armour**: two thin arcs over the top of the mini-map, filled left to right;
  health goes green, amber, red; armour (light blue) shows only when above 0.
- **Weapon line** beside the mini-map: weapon name (string table key = the weapon key, e.g.
  `Weapon.Pistol9`), `magazine / reserve`, red at 0, "Reloading" during a reload.
- Free keys for combat are kept: full map on M / D-pad up (combat uses D-pad down and left,
  LT / RT, X, R, 1).

Rebase: this branch sits on `feat/combat-core`; once combat is on next, rebase onto next.
