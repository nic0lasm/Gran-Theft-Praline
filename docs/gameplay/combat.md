# Combat and weapons (version 0.7 "Armed")

*French version: [combat.fr.md](combat.fr.md)* · ADR-0011 · switch: `-Combat=0` (off)

Status 2026-11-11: rules written and checked outside Unreal (`tools/combat/combat_logic_test.cpp`,
150 checks, CI job `Combat rules`). Unreal components not written yet: they wait for the ragdoll
thread's hit reactions and threat response to reach `next`.

## Numbers (defaults, all in `DefaultGame.ini` once the components exist)

| What | Value |
| --- | --- |
| Health, armour, stamina | 100, 0 to 100, 100 |
| Armour | takes 70 % of a hit while it lasts; melee and falls go through it |
| Regeneration | after 6 s without damage, 4 points/s, up to 50 (the rest: medkit, food, hospital) |
| Respawn | 6 s after death, nearest fictional hospital, 3 s without damage, armour lost |
| Pistol (fictional 9 mm) | 12 rounds, reserve up to 96, 330 rounds/min semi-automatic, reload 1.5 s (auto when empty) |
| Pistol damage | 20 at the torso up to 15 m, down to 10 at 45 m; head × 2.5, arm × 0.6, leg × 0.7 |
| Spread (half-angle) | hip 3°, aimed 0.5°, +0.7° per shot up to +4°, recovers 6°/s, +2° running, blind fire × 4 |
| Recoil | 1.4° up per shot (± 0.35° sideways), comes back at 10°/s |
| Player as target | takes 45 % of NPC damage, head never more than × 1.5 |
| Fists | 8, 10, then 18 (heavy, knocks down); 8 stamina per blow; combo of 3 |
| Dodge | 0.45 s, invulnerable from 0.05 to 0.30 s, 18 stamina |
| Guard | stops 80 % of a frontal blow (± 70°) for 14 stamina; heavy blow or no stamina = guard broken, stunned 1.2 s |
| Takedown | within 1.5 m, from behind (± 60°) on someone unaware, or on anyone stunned; never on someone aiming a gun |
| Aim assist (gamepad) | snap within 9° and 40 m on aim start (hostiles preferred), friction × 0.45 within 3.5° |
| Cover | taken within 1.2 m of a wall facing the player (± 50°); low (car, low wall) or high; jump to a cover up to 12 m in a 40° cone |

Balance checked by the test: an officer falls to 5 torso or 2 head shots at 10 m; the player survives 12
torso hits without armour, 23 with full armour; a full fist combo (36) never kills a fresh body.

## Contract for the other threads

**Out (delegates of `UGTPCombatSubsystem`):**

| Delegate | Arguments | Who listens |
| --- | --- | --- |
| `OnShotFired` | shooter, origin, direction, weapon, hit actor (may be null) | Police (`ShotsFired`), Audio (gunshot, tail, echo by street), Pedestrians if they need more than fear |
| `OnDamageDealt` | instigator, victim, damage, zone, melee or not, health after | Police (`Assault`, `HitPolice`), HUD (hit marker), Audio (impact) |
| `OnPawnKilled` | killer, victim, melee or not | Police (`Killing`, `KillPolice`), missions, statistics |
| `OnPlayerDied` / `OnPlayerRespawned` | place, hospital | Police (clear the level), economy (hospital bill if wanted), save, HUD |
| `OnArmedThreat` | source, target, aiming or not | Hold-up script (fast-food), Police (`ArmedThreat`) |

**Calls this thread makes:** `UGTPHitReactionComponent::ReactToHit(FGTPHitInfo)` after each hit;
`UGTPThreatSubsystem::SetThreat(Source, bArmed, Origin, Aim)` every frame an armed body aims, `ClearThreat()`
when the aim ends, `NotifyShot(From, Target)` at each shot. This replaces `-ThreatDemo`.

**Proposed to the police thread:** new `EGTPCrimeKind` values `Assault` (fists, 15 heat), `ShotsFired` (20),
`ArmedThreat` (25), `Killing` (60), `KillPolice` (90). At two stars, officers shoot with `UGTPWeaponComponent`
(AI accuracy factor 2.5 × the player's spread, one shooter at a time per unit in 0.7).

**Proposed to the ragdoll thread:** when the body has a `UGTPHealthComponent`, `UGTPPlayerReactionComponent`
sends its car and fall damage there instead of its own `Health`.

**HUD (interface thread):** small dot reticle that opens with the spread (radius = spread angle on screen);
hit marker (a cross, 0.15 s, red on a kill); health and armour as two thin arcs around the mini-map, shown
only when changed or below 50 %; ammunition `12 / 36` bottom right only while armed; red vignette below 25 %
health; cover prompt and takedown prompt in the existing help corner; aim assist on/off and sensitivity in
the controls menu.

**Sounds (audio thread):** pistol shot close (dry, short) and far (crack and tail, delayed by distance),
street echo by width of the street (the zone map exists), dry click, magazine out and in, slide; bullet
impacts per impact kind (dust, sparks, car body, glass, wood, dirt, water, flesh); whizz of a near miss;
fist blows light and heavy, a block, a dodge swish; body falls exist already (ragdoll). Gunshots must duck
the ambience and traffic for 0.5 s.

**Controls (created here, remapped by the interface thread):**

| Action | Keyboard and mouse | Gamepad |
| --- | --- | --- |
| Aim (guard when unarmed) | right button | left trigger |
| Fire (punch when unarmed or not aiming) | left button | right trigger |
| Reload | R | X / Square |
| Dodge | Left Alt | B / Circle |
| Cover (take, leave) | Q | right bumper |
| Swap shoulder | X | right stick click |
| Holster, draw | 1 | d-pad up |

To be checked against the existing actions (`F` enters a car, `E` interacts, `Space` jumps) by the
interface thread.

## Effects budget (proposed to the optimisation thread)

Line in the "night and effects" envelope (2 ms GPU), for the worst case of a firefight on the street at
step C, M1 profile, capped at 40 fps:

| Item | Cap on M1 | Cost target |
| --- | --- | --- |
| Impact effects (Niagara, CPU simulation, no lights, no collisions) | 12 alive at once, 0.6 s life, particles per kind 8 to 48 (`GTPDamage::MaxParticles`) | ≤ 0.25 ms GPU p95 |
| Impact decals (pooled, no normal-map blending) | 32 in the world, oldest recycled, 20 s fade | ≤ 0.05 ms GPU |
| Muzzle flash | sprite + a light of 0.05 s **without shadows**, player only; NPC shots: sprite only | ≤ 0.05 ms GPU |
| Smoke | one small puff per shot (≤ 6 particles, low overdraw), none in rain | included above |
| Tracers | off on M1, on in "fidelity" | 0 on M1 |
| Glass breaking (shop windows) | existing breakables (ragdoll thread's budget) | not counted here |
| Game thread (traces, health ticks at 10 Hz, aim assist) | 3 shooters, 1 trace per shot, cover probes on key press only | ≤ 0.4 ms CPU |

Total proposed: **≤ 0.35 ms GPU p95 and ≤ 0.4 ms game-thread CPU** in the firefight scene, A/B against
`-Combat=0`. When a cap is reached the oldest effect is removed; nothing waits. A "fidelity" profile can
double the caps and enable tracers.

Agreed by the optimisation thread on 2026-11-11 with three amendments, now part of the budget:

- The margin it comes from: on next 8b16340 the gate's GPU p95 is 20.05 / 22.31 / 22.27 / 22.54 ms (fixed /
  route / night / rain) against 24 ms, so 1.5 ms of margin in the worst scene; this line takes a quarter of it.
- Checked **at night and in the rain**, not only at noon (translucent sparks, smoke and glass cost more there).
  Hard caps on live particles and decals, pooled and recycled; no dynamic light casts shadows; muzzle flashes
  never cast shadows.
- **No frame over 100 ms at the first shot**: Niagara systems and their PSOs are preloaded when the level starts
  (a hidden warm-up spawn), and the first shot is measured cold.

## Plan

1. Rules outside Unreal (done, this branch).
2. Once feat/hit-reactions and feat/threat-response are on next: `UGTPHealthComponent`,
   `UGTPCombatSubsystem` (`ApplyHit`, delegates, `-Combat=0`), `UGTPWeaponComponent` with the pistol and
   impact effects, hospital respawn. Control compilation on the PC, then a Mac slot (gate + firefight scene).
3. Fists, dodge, guard, takedown (`UGTPCombatMeleeComponent`).
4. Cover (`UGTPCoverComponent`), blind fire.
5. Police shooting at two stars (with the police thread), 2-minute fight video.
