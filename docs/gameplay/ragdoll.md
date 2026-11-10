# Ragdoll and physical reactions

*French version: [ragdoll.fr.md](ragdoll.fr.md)* · Decision: [ADR-0006](../adr/adr-0006-ragdoll-and-impacts.md)

Code: `GTPImpactLogic.h` (rules), `GTPRagdoll.h` (component, cap), `GTPPlayerReaction.h`
(player). Check: `python3 tools/ragdoll/check_impact_logic.py`.

## What a car does to someone on foot

Closing speed along the line between car and person; a car backing away or standing still hurts nobody.

| Closing speed | Severity | Effect | Damage | Outcome |
| --- | --- | --- | --- | --- |
| below 4 km/h | none | nothing | 0 | - |
| 4 to 12 | bump | stumble (slower for 0.7 s) | 0-6 | keeps walking |
| 12 to 28 | knock | ragdoll, short push | 12-45 | gets up after about 2 s |
| 28 to 55 | launch | ragdoll, thrown with lift and spin | 45-100 | gets up after about 4 s if alive |
| 55 and more | heavy | ragdoll, long launch | 100 | stays down |

The body leaves at about 55 % of the car's speed (shared by mass), with a lift of
35 % for launches, at most 18 m/s. The car loses under 2 km/h.

## Falls and bail-outs (player)

| Vertical speed on landing | Reaction |
| --- | --- |
| under 6.5 m/s | nothing |
| 6.5 to 9 | stumble |
| 9 to 12.5 | roll (stumble for now, montage later) |
| over 12.5 | ragdoll |

Crouching halves the speed counted. Leaving a car moving at 8-30 km/h: stumble;
30-55: knocked down; over 55: thrown. About 3 m of fall is a stumble, 8 m a ragdoll.

## Cost

At most 3 ragdolls simulate on m1 (6 fidelity, 10 ultra); beyond that the farthest
is frozen. No cost when nothing is ragdolling (no tick). Measure on the Mac before
raising the caps.

## Hooks for other systems

- `UGTPPlayerReactionComponent::OnHurt(Damage, HealthLeft)`: HUD, wanted level.
- `UGTPRagdollComponent::OnRagdollEnded(Ragdoll, bGotUp)`: police (body on the
  ground), pedestrians (return to the pool).
- `GTPImpact::ClassifyCarHit`, `ClassifyLanding`, `ClassifyBailOut`: pure
  functions, callable from any system.

## Command line and automatic test

- `-Ragdoll=0`: no physical reaction at all (the build logs `ragdoll: on` or `off`).
- `-MaxRagdolls=N`: overrides the cap of the profile.
- `-RagdollTest` (+ `-RagdollTestQuit`): scenario without a player: car hits at 20, 40
  and 70 km/h, falls of 3 and 8 m, four bodies at once against the cap of three;
  logs `GTPRagdollTest: ... PASS` or `FAIL` and the average frame time.

## Breakable street furniture

`GTPBreakableLogic.h` and `UGTPBreakableSubsystem` (street props are instanced: one
actor per asset, so the subsystem swaps the hit instance for a physics actor).
Tags on the instanced components, set from the placement tables by the street
script: `GTP_Breakable_R` (tips over), `GTP_Breakable_C` (breaks), `GTP_MassKg=<n>`,
optional `GTP_BrokenMesh=<asset path>`. Fixed objects (bollards) need no tag.
Rules: a tippable falls from a 3 km/h push; a breakable needs 400 J + 20 J per kg
(a 250 kg lamp breaks under a car at about 11 km/h, a 25 kg sign at 5 km/h; a person
cannot break a lamp). The body is thrown with a share of the impactor's speed and the
car loses a little. At most 6 bodies live on m1 (16 elsewhere), 25 s each.
`-Breakables=0` turns it off. List of objects: docs/gameplay/breakable-street-furniture.md
(street realism thread).
