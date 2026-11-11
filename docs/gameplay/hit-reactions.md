# Hit reactions (for the combat system, version 0.7)

*French version: [hit-reactions.fr.md](hit-reactions.fr.md)* · ADR-0006

The combat system decides damage; this layer decides what the body does about it.
Code: `GTPHitLogic.h` (rules, no Unreal types, checked by `python3 tools/ragdoll/check_impact_logic.py`),
`GTPHitReaction.h` (`UGTPHitReactionComponent`). Switch: `-HitReactions=0`.

## Contract for the combat system

Add `UGTPHitReactionComponent` to any body that can be hit (player, pedestrians, enemies)
and call, once per resolved hit:

`ReactToHit(FGTPHitInfo{ Damage, HealthAfter, Direction (attacker to body, world), Bone, bMelee })`

It returns what was done and broadcasts `OnReaction(Reaction, Hit, Body)` (combat, police and
pedestrians can listen; `OnDamageDealt` and `OnPawnKilled` stay in the combat system).
The component never changes health.

## Rules

| Hit (share of full health, head x1.5, limb x0.7, melee x1.25) | Reaction |
| --- | --- |
| under 6 % | nothing |
| 6 to 20 % | flinch: the clip for the side it came from (`AM_HitLight_Front/Back/Left/Right`) |
| 20 to 45 % | stagger: `AM_Stagger_Front/Back` (sides use the flinch clips), walking speed halved for 0.6 s |
| 45 % and more | knockdown: ragdoll with a launch along the hit, gets up |
| health 0 | death: ragdoll, stays down |

Anti-stunlock: reactions closer than 0.35 s are ignored (a knockdown always goes through);
the third flinch within 2 s becomes a stagger; no second knockdown for 2 s after a fall.
A launch is `200 + 6 per damage point` cm/s, at most 650, with a lift; a head hit that kills
or knocks down adds a spin. The zone comes from the bone name (head and neck, arms, legs,
the rest is torso). Direction comes from the hit direction against the body's facing.

## Animations

The clips are keyed from data (`tools/animation/car_poses.json`, built in Blender by
`build_car_poses.py`); assign the imported montages to `Clips` of the component by name.
Without a clip the body still staggers (speed) and falls (ragdoll); only the flinch is silent.
The ragdoll stays under the cap of ADR-0006 (3 on m1): beyond it a body stays animated.
