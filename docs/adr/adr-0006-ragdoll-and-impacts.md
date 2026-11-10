# ADR-0006: Ragdoll and physical reactions

*French version: [adr-0006-ragdoll-and-impacts.fr.md](adr-0006-ragdoll-and-impacts.fr.md)*

## Status

Proposed on 2026-11-10 (the first measurement on the M1 Pro decides the caps).

## Date

2026-11-10

## Decision Makers

nic0lasm (project lead)

## Summary

One shared rule set decides what a hit, a fall or a bail-out does to a body
(`GTPImpactLogic.h`, no Unreal types, checked offline). One component hands a
skeletal mesh to physics and gives it back (`UGTPRagdollComponent`), used by the
player and by pedestrians. A world subsystem caps the number of simulating
ragdolls per graphics profile.

## Context

Pedestrians, police and combat all need a body that reacts. Version 0.4 needs a
pedestrian run over by a car to fly, fall and either get up or stay down, and the
player to stumble, fall off a moving car or be hit. The pedestrian thread already
has a minimal `AGTPPedestrian::StartRagdoll` with a fixed impulse; the vehicle has
montage hooks for entering and leaving. What is missing is the common decision
layer, the player side and a cost limit, in a frame budget that is already tight
(ADR-0002).

## Decision

1. **Rules** in `GTPImpactLogic.h`: severity from the closing speed (none, bump,
   knock, launch, heavy at 4, 12, 28 and 55 km/h), damage curve (a 40 km/h hit
   hurts, 70 km/h kills), launch velocity from momentum (the body leaves at
   about half the car's speed with a lift), landings (stumble, roll, ragdoll by
   vertical speed, crouching halves it), bail-out from a moving car, the ragdoll
   life cycle (fall, settle, pause, get up face up or face down, or stay down),
   and which ragdolls to freeze over the cap.
2. **Mechanics** in `UGTPRagdollComponent`: simulate the mesh, make the character
   capsule follow the pelvis, restore the character on the floor below the pelvis
   when getting up. It ticks only while a ragdoll runs. Get-up montages are
   optional properties (see docs/animation/sources.md); without them the body
   is given back with a pose pop.
3. **Cap** in `UGTPRagdollSubsystem`: hard limit (a fourth body stays animated); m1 3, fidelity 6, ultra 10 simultaneous
   ragdolls (`-MaxRagdolls=N` overrides; `-Ragdoll=0` turns the system off), the farthest then the oldest is frozen
   (pose kept, simulation off), checked twice a second.
4. **Player** in `UGTPPlayerReactionComponent`: finds cars touching the capsule at
   20 Hz, measures the landing speed, reacts to a bail-out, keeps a health value
   0..100 and announces damage with `OnHurt`.
5. **Hooks, additive only**, in files owned by other threads:
   - pedestrians: replace the fixed impulse by `GTPImpact::ClassifyCarHit` and use
     `UGTPRagdollComponent` (or keep `StartRagdoll` and only call the rules);
     broadcast `OnPedestrianHit` with the severity when it is at least a knock;
   - vehicle: `DoExit(bBailOut = true)` calls `ReactToBailOut(Velocity)` on the
     player's reaction component; entering and leaving use the existing
     `EnterMontage`/`ExitMontage` once animations exist.

## Alternatives Considered

- Physical Animation (partial ragdoll, blend-weights on hit): the better look for
  gunshots, left for combat (0.7); this ADR builds the full ragdoll it needs first.
- The ragdoll and motion-matched get-ups of Epic's Game Animation Sample, said to
  be in the 5.8 version (to be verified, see docs/animation/sources.md): preferred
  for the get-up blend once checked; the rules and the cap stay ours either way.
- Per-pedestrian ragdoll with no cap: unbounded physics cost in a crash.

## Consequences

- Without get-up animations the character pops to standing: acceptable for
  pedestrians, to be replaced for the player.
- Capsule and mesh logic is only validated by the offline rules until a Mac slot;
  the Unreal code was written without compiling.
- Damage and health are local to the player component until a health system exists.

## Validation Criteria

- `python3 tools/ragdoll/check_impact_logic.py` PASS (done in the cloud).
- Mac, m1 profile: a car at 20, 40 and 70 km/h into the player and into a
  pedestrian: severity and outcome as in the table of docs/gameplay/ragdoll.md,
  filmed; a fall from 3 m and from 8 m; a bail-out at 20, 40 and 70 km/h.
- Three simultaneous ragdolls cost at most +1 ms game-thread CPU and +0.5 ms GPU
  p95 over the same run without them (to confirm with the optimisation thread).

## Related

ADR-0002 (budget), ADR-0003 (movement), ADR-0005 (pedestrians),
docs/animation/sources.md, production/systems-inventory.fr.md (§ ragdoll).
