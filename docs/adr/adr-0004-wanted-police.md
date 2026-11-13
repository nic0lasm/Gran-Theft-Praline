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

## Cost of a police unit and `-PoliceLite=1` (2026-11-12)

M1 Shipping, fixed scene, `-WantedStars=2 -WantedHold -PoliceMax=6`, 600 s: GPU p50 18.1 ms and p95 29.2 ms against 13.2 / 20.2 without a chase, so about 0.8 ms (p50) and 1.5 ms (p95) per unit. Estimate by part (not measured per part): the roof lights are two unshadowed point lights with one lit at a time, about 0.05 to 0.1 ms per car; the emissive material and the seated driver cost almost nothing; the rest (about 0.6 to 1.3 ms) is the car body at full detail with its moving shadow, because a chase keeps every unit within 25 m of the player, so none gets the far LOD or the shadow cut. `-PoliceLite=1` (default off, for A/B): one light per car whose colour alternates, lights only within 30 m, shadow and full detail only within 15 m (instead of 25 m), and at most 3 units unless `-PoliceMax` is given. Expected: about 0.1 ms per unit from the lights and a third to a half of the body cost beyond 15 m; to be measured with a chase scene at 3 and 6 units, Lite off and on.

## Officers on foot, shooting and the search (2026-11-12)

Rules in `GTPPoliceTactics.h` (plain C++, tested in CI by `tactics_test`), behaviour in `AGTPPoliceOfficer` (a light body, no navigation mesh yet: it steers straight and slides along walls).

- **Out of the car**: with the player on foot within 35 m of a unit, or at any time for a stuck unit within 60 m, the director puts an officer on the ground; quota by level 1/2/2/3/4, at most `MaxOfficers` (3, `-PoliceOfficers=N`, 0 = cars only; 2 with `-PoliceLite=1`). They count for sight, witnesses and arrest like the cars.
- **On foot**: they run (4.3 m/s, slower than a sprinting player: officers alone do not catch a runner, the cars and the stars do) at the player while seen, to the last known position otherwise, then sweep a circle around it that widens by 0.6 m/s from 15 m up to 60 m, one sector per officer.
- **Shooting**: never at one star; at two only for 20 s after the player fired, threatened or hit someone; from three stars on sight within 35 m with a clear line, through `UGTPWeaponComponent::FireAt`. Aim improves and fire speeds up with the level (spread 2.5 at three stars, 0.35 less per star; 1.1 s down to 0.5 s between shots).
- **Off duty**: 15 s after the level is gone out of the player's sight, all of them after 60 s.
- Not done: roadblocks and encirclement (logic in `feat/police-roadblocks`, frozen until the alleys scenario), a run animation (config `RunAnimation`, `-PoliceRunAnim=`), real paths through alleys.
- Demo and proof: `-WantedStars=3 -WantedHold -PoliceOfficers=3` with the player on foot; the `wanted:` trace gives `officers=N`, the log `police: officer out`.

## GTA-style chase when the player drives (2026-11-13)

`GTPPoliceTactics::Profile(stars)` is the escalation table, tested in CI. Level 1: one car at 70 km/h, no ramming. Level 2: two cars at 90 km/h, every other car comes from in front of a moving player (spawn point 100-170 m ahead within 35 degrees of the direction of travel, out of sight). Level 3: three cars at 105 km/h that ignore the obstacle probe (they ram traffic and the player's car), and two roadblock cars (a V across a road the player is driving along, 140-240 m ahead, handbrake on, lights on; they go once the player is past them, or 8 s after the level is lost). Levels 4 and 5: four cars at 120 and 135 km/h. A unit slower than 15 km/h with something within 12 m ahead for 1.5 s shoves through it for 3 s (levels 2 and up). Cars use the lanes at 90 % of the chase speed with the lane's posted limit ignored, and a harder driving setup (lateral 6 m/s2, braking 7 m/s2, gap 3 m, throttle gain 0.8). `MaxUnits` is 4 (3 with `-PoliceLite=1`). Officers on foot, arrest and shooting are as described above. Not compiled when written; StateTree, AI Perception and EQS are not enabled in the project yet, so these rules stay plain logic until then.

## Crimes, arrest, death, hiding (2026-11-13)

The combat events (shot, threat, damage, kill) now feed the wanted component, so every row of the crime table in `docs/gameplay/wanted.md` has a source (`BindCombat`); police officers carry the `GTPPolice` tag so a hit on one is HitPolice or KillPolice. Out of sight the search runs 2x faster for a still player far from the police, and a change of car out of sight keeps 35 % of it (`FGTPWantedState::ShortenSearch`, tested). Officers also get out when the player has been still in a car for 2 s. Arrest warns once (`OnArrestWarning`), takes the weapon and ammunition, and respawns the player at a `GTPPoliceStation` target point or the player start; death clears the level at the respawn. Light bars are brighter (3000 cd, 4500 cm) and shown to 120 m so they read from the end of the street. StateTree is enabled in the project but these rules stay plain logic: the tested headers are the contract, a StateTree would only call them. Not compiled when written.
