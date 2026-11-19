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

## Proof in the packaged game (`-GTARun`, 2026-11-19)

Four steps of the automatic run cover the wanted level, counted on their own line (`GTARun police: N/4 ok`); the seven others keep their line (`GTARun done: N/7 ok`).

| Step | What happens | Passes when |
| ---- | ---- | ---- |
| `witness` (2) | Right after the carjack | A crime phoned in by a person (the victim, a passer-by) reached the police and gave a star. A star forced by the chase script does not count. Fails with `-Witnesses=0`. |
| `pursuit` (4) | The flight goes on for 40 s with the level left to itself (no forced stars, arrest possible) | The police follow: over the time a chasing car was on the road, the cars average 30 km/h (or 60 % of the player's own average when he is slower), and a car is within 80 m for a quarter of the time. Roadblock cars are left out. |
| `outcome` (5) | The player gives up: the car stops and waits | Arrested then released on foot, able to walk, no star left; or killed and back; or the level was lost. Whatever ended the chase since it began counts. An arrest counts only when a police car or an officer was within 10 m when it was made, and when the chase script had not moved the player's car in the 8 s before (the detail says who made the arrest and from how far). Fails when the player is still wanted 60 s after stopping. |
| `wasted` (11) | The player takes a lethal hit through the health component | Dead, back on his feet (hospital point, else the player start) with full health, able to walk, no star left. |

The numbers of the pursuit come from the police director (`FGTPChaseTally`, `GTPChaseTally.h`, tested in `tools/vehicles/wanted_sim/chase_tally_test.cpp`): every frame while the player is wanted, the speed of each chasing car and the distance of the nearest. It logs them every 10 s (`police chase: units mean=... km/h ... nearest mean=... m`), and the chase demo adds `police mean=` and `nearest mean=` to its `DemoChase` line. A unit left more than 180 m behind is put back near the player out of sight: these moves are counted (`put back near the player`), and a move the car did not keep half a second later is logged (`the move did not hold`).

What these steps do not prove: that a player can outrun the police by driving (the test car is driven by a script, and gives up), and who can kill the player (the lethal hit is the test's). After the outcome the run clears what is left of the level and turns the arrest off until its end, so that the steps on foot test the theft and the hold-up, not the police.

## What the first gate showed, and what a unit does when its driving goes wrong (2026-11-19)

Candidate 48, packaged: `pursuit` failed. The chasing cars averaged 13 km/h (24 wanted, player 39) and moved 29 % of their time. Their lines read a throttle of 1.00 in every stopped sample and no obstacle seen: the drivers were asking to go. Two things held the cars:

- a car at 0 km/h for 24 s on its four wheels, in first gear, 2086 N.m on each rear wheel and no wheel turning: pushed against something. A unit never backed off; only the chase demo's car knew how. And the "stopped, pushing through" rule reset the stopped time every 4 s, so the rule that takes a stuck unit away after 12 s never applied;
- cars with their two inner wheels in the air three seconds after pulling away, then with none on the ground: the driving asked for full lock with the throttle held down (a target behind the car, a lane picked at an angle), on tyres that grip harder (friction 0.7 x 3) than the car needs to roll over (centre of mass 83 cm up; the vehicle side of this is the traffic thread's).

The `outcome` step passed on an arrest the test had made itself: the chase script had just put the player's car 11 m from a roadblock car. The script now leaves 60 m to any police car when it moves the player, the arrest is off for 8 s after such a move, and the step checks who made the arrest.

Since then (`GTPUnitRecovery.h`, tested in `tools/vehicles/wanted_sim/unit_recovery_test.cpp`):

| Case | What the unit does |
| ---- | ---- |
| Any driving | The pedals follow the steering: no throttle, and some brake, above the speed the steering asked can be held at (6 m/s2: 15 km/h at full lock); the steering never asks for more than 7.5 m/s2 at the speed the car has; no more than 0.6 of the throttle under 20 km/h. |
| Not moving for 1.5 s with the throttle held | Backs off for 1.8 s, wheels turned the other way, then picks its lane again. Not within 12 m of the wanted player: there a stopped unit boxes him in. |
| Held again in the same place (12 m) after three back-offs, or no wheel on the ground (or on its side) for 3 s | Given up where it is: brakes on, its officers get out when the player is within 60 m, it is taken away once out of sight, and another car is sent at once (two wrecks at most). It is never put back on the road. |

A given-up car no longer counts among the chasing cars, in the speed average either; the count is in the same lines (`... N back-off(s), N unit(s) given up`), and one line per event says where the car is, how it lies and what is in front of it (`police unit: <name> held with the throttle down, backing off at ... ahead: ...`). None of this has run in the engine at the time of writing: the logic is tested outside it only.

## Switches

`-PolicePerception=0|1|2` (off, perception only, both = default), `-PoliceEqs=0` (officers sweep a fixed circle again), `-Police=0`, `-WantedStars=N`, `-WantedHold`, `-Witnesses=0`.
