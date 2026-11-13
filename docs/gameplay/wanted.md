# Wanted level and police (gameplay)

Code: `GTPWantedLogic.h` (rules, tested in CI), `GTPWanted.*` (component and director), `GTPPoliceTactics.h`, `GTPPoliceOfficer.*`. Decisions: ADR-0004.

## What earns stars

Each crime adds heat; stars are a step of the heat (1 at 20, 2 at 60, 3 at 120, 4 at 220, 5 at 360). A crime counts only if a witness could see or hear it (`GTPWitnessLogic.h`); `-Witnesses=0` makes every crime count.

| What the player does | Crime | Heat | Stars alone | Reported by |
|---|---|---|---|---|
| Takes a car (seen) | CarTheft | 25 | 1 | `AGTPVehicle` |
| Runs into a pedestrian | HitPedestrian | 35 | 1 | `AGTPVehicle` |
| Runs into another car | HitVehicle | 12 | 0 | `AGTPVehicle` |
| Runs into a police car | HitPolice | 45 | 1 | `AGTPVehicle` |
| Hurts someone (gun or fists) | Assault | 15 | 0 | combat events |
| Hurts a police officer | HitPolice | 45 | 1 | combat events |
| Fires a gun (once per 5 s burst) | ShotsFired | 20 | 1 | combat events |
| Points a gun at someone | ArmedThreat | 25 | 1 | combat events |
| Kills a person | Killing | 60 | 2 | combat events |
| Kills a police officer | KillPolice | 130 | 3 | combat events |

Crimes add up: car theft, then a shot, then a hit on a police car is already 3 stars.

## Escalation

See ADR-0004: level 1 one car at 70 km/h; 2 two cars at 90, one comes from in front; 3 three cars at 105 that ram, two roadblock cars; 4 and 5 four cars at 120 and 135 km/h. Officers get out when the player is on foot or has been still in a car for 2 s, and shoot from 2 stars when the player is aggressive, from 3 otherwise.

## Losing the police

- Out of sight, the search timer runs (14 s plus 4 s per star). Seen again: it starts over.
- Hiding: out of sight, still (under 1.5 m/s) and more than 25 m from every unit, the search runs twice as fast.
- Changing car (or getting out) out of sight keeps only 35 % of the search time.

## Arrested

A unit within 7 m of a player on foot or under 12 km/h, three seconds in a row. At the first second the police shout (`OnArrestWarning`, for sound and subtitle). Then: banner, the weapon is holstered and the ammunition taken, the stars are cleared, and after `BustedSeconds` the player reappears at a TargetPoint tagged `GTPPoliceStation`, else at `ArrestReturnLocation`, else at the player start.

## Dead

When the player dies (`OnPlayerDied`), pending reports are dropped; at the respawn at the hospital the stars are cleared.

## Switches

`-PolicePerception=0|1|2` (off, perception only, both = default), `-PoliceEqs=0` (officers sweep a fixed circle again), `-Police=0`, `-WantedStars=N`, `-WantedHold`, `-Witnesses=0`.
