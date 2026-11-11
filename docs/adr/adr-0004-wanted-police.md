# ADR-0004: Wanted level and police pursuit

*French version: [adr-0004-wanted-police.fr.md](adr-0004-wanted-police.fr.md)*

## Status

Proposed on 2026-11-10 for the version after 0.3 (code written in the cloud,
not yet compiled or measured on the Mac).

## Decision Makers

nic0lasm (project lead)

## Summary

A GTA-style wanted system in four small pieces, built on the 0.3 traffic
code (ADR-0003) instead of a new AI stack:

1. **Rules without the engine** (`GTPWantedLogic.h`): crimes add heat, heat
   gives 1 to 5 stars, police sight keeps the level, hiding loses it,
   a close police car and a stopped player mean arrest. Plain C++, tested
   outside Unreal (`tools/vehicles/wanted_sim/wanted_test.cpp`, run in CI).
2. **`UGTPWantedComponent`** on the player controller: the single entry
   point `ReportCrime(kind, location)`, the sight test, the arrest and the
   state the HUD reads. Any system can report a crime through it
   (pedestrians, weapons later).
3. **`AGTPPoliceDirector`**: spawns police cars on traffic lanes out of
   sight, drives them (straight at the player when seen, along the lanes
   towards the last known position otherwise), removes them out of sight
   when the level is gone.
4. **HUD**: five stars top right, solid in pursuit, flashing while the
   police search, a blue bar for the search time left, a red bar for the
   arrest, an "ARRÊTÉ" banner. Added to `AGTPHUD` without changing the
   driving panel or the prompts.

## Rules (all values in `FGTPWantedSettings`)

| Crime | Heat | Reported by |
|---|---|---|
| Car taken from its driver, window broken | 25 | `AGTPVehicle::BeginEnter` |
| Person hit by the player's car (> 12 km/h) | 35 | `AGTPVehicle::OnBodyHit` |
| Car hit (> 12 km/h) | 12 | same |
| Police car hit | 45 | same (tag `GTPPolice`) |

Stars at 20, 60, 120, 220, 360 heat. The same crime kind counts once per
2 s. Search time after the last sighting: 14 s + 4 s per star. Arrest:
police within 7 m for 3 s while the player is on foot or below 12 km/h.
Arrest clears the level, makes the player step out and freezes them 3 s.
Fines, losing weapons and a respawn at a station come with the money and
weapons systems.

## Performance budget (M1, ADR-0002)

Police cars cost like traffic cars (same cheap mode: no far shadows, far
LOD, no shadowed lights). Units per level: 1, 2, 2, 3, 4 stars, never more
than `MaxUnits` (2 on M1 until the traffic cost is measured, `-PoliceMax=N` up to 3 for tests). Command line: `-Police=0` turns everything off, `-WantedStars=N` forces a level after 5 s (`-WantedDelay=S`), `-WantedHold` keeps it; a `wanted:` line every 2 s is logged while wanted. Light bar: two unshadowed point
lights per car, switched only when the blink phase changes. The director and
the wanted component do nothing at zero stars except one cheap check.
Target: p95 ≤ 42 ms with three police cars and traffic; to be measured.

## Limits of this first stage

- Cars chase in a straight line when they see the player, otherwise by
  lanes: no pathfinding through the city, no roadblocks, no spike strips.
- Police stay in their cars: arrest is by proximity, no officer on foot.
- No siren sound and a generic car (traffic cars, or `PoliceVehicleClass`
  once a livery exists; real livery and logos stay outside Git).
- Every crime is reported; witnesses and the police radio come later.
- No helicopter at 4-5 stars.

## Coordination

- Lanes, AI drivers and the HUD belong to the 0.3 thread: this work only
  adds `FGTPLaneFollower::SetGoal`, `AGTPTrafficNetwork::PickNextToward`, a
  friend declaration and one HUD method.
- Pedestrian hits: the pedestrian thread calls
  `UGTPWantedComponent::ReportCrime(EGTPCrimeKind::HitPedestrian, Location)`.
- No map change: everything appears at runtime.

## Spawn hitch fix (2026-11-11)

The M1 bench showed one hitch (47 ms CPU spike) each time police units appear: a unit was a full player character (with its gameplay camera system) plus a car, created in one frame, and destroyed when the level was lost, about every 110 s. Now: the car, the driver and the light bar are created on three successive frames; units are kept `IdleLifetime` (150 s) after the level is lost, driving the lanes with lights off, and a kept unit far from a new chase is moved near the player out of sight instead of being created again; `PoliceDriverClass` lets a light character without camera replace the player's character.

## Combat crimes (2026-11-11)

Added for the combat framework, reported through `ReportCrime` (heat): assault 15, shots fired 20, armed threat 25, killing 60 (2 stars), killing a police officer 130 (3 stars). Shots and threats count once per 5 s (a burst), the other crimes once per 2 s. Not yet checked: witnesses (every crime counts).

## Witnesses (2026-11-11)

A crime counts only if somebody saw or heard it (`GTPWitnessLogic.h`, tested in CI): a police car in sight within 60 m reports it at once; a person in sight within 25 m calls the police 4 s later; shots and killings are heard without sight (police 120 m, people 40 m); the person directly involved (the driver pulled out of a car, a person hit by a car) counts as a person who saw it. Nobody around: no heat. `-Witnesses=0` makes every crime count at once (tests, scripted demos); `bRequireWitnesses=False` in the config does the same. Pedestrians are found among the pedestrian actors near the crime; people drawn only as far crowd are not witnesses.
