# Wanted level and police (gameplay)

Code: `GTPWantedLogic.h` (rules, tested in CI), `GTPWanted.*` (component and director), `GTPPoliceTactics.h`, `GTPPoliceOfficer.*`. Decisions: ADR-0004.

## What earns stars

Each crime adds heat; stars are a step of the heat (1 at 20, 2 at 60, 3 at 120, 4 at 220, 5 at 360). A crime counts only if a witness could see or hear it (`GTPWitnessLogic.h`); `-Witnesses=0` makes every crime count. A witness's line of sight ends 150 cm above the place of the crime; the thing the crime is done to (the stolen car, the car that hit somebody) and the player never cut it. When nobody saw it, the log line `crime N not witnessed` says how many people were near and what cut the nearest person's view.

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
| `pursuit` (4) | The flight goes on for 40 s with the level left to itself (no forced stars, arrest possible) | The police follow, and nothing helped them: over the time a chasing car was on the road, the cars average 60 % of the player's own average and never less than 25 km/h (unless they stay within 30 m of him on average: a player boxed in), and a car is within 80 m for a quarter of the time; no replacement car was made nearer than 150 m from the player, and the script never put the player's car back on a lane. Roadblock cars are left out. |
| `outcome` (5) | The player gives up: the car stops and waits | Arrested then released on foot, able to walk, no star left; or killed and back; or the level was lost. Whatever ended the chase since it began counts. An arrest counts only when a police car or an officer was within 10 m when it was made, and when the chase script had not moved the player's car in the 8 s before (the detail says who made the arrest and from how far). Fails when the player is still wanted 60 s after stopping. |
| `wasted` (11) | The player takes a lethal hit through the health component | Dead, back on his feet (hospital point, else the player start) with full health, able to walk, no star left. |

The numbers of the pursuit come from the police director (`FGTPChaseTally`, `GTPChaseTally.h`, tested in `tools/vehicles/wanted_sim/chase_tally_test.cpp`): every frame while the player is wanted, the speed of each chasing car and the distance of the nearest. It logs them every 10 s (`police chase: units mean=... km/h ... nearest mean=... m`), and the chase demo adds `police mean=` and `nearest mean=` to its `DemoChase` line. No unit is ever moved by hand. The cars a level asks for are made in the spawn ring (50 to 120 m, out of sight); once it has had them, a car sent replaces one that was lost (left more than 300 m behind, given up, wrecked) and is made 150 to 255 m away, in front of a moving player when a place is found there: it has to drive to him. The lines count them (`N sent, N of them replacements (nearest made N m from the player)`), and each car says where it was made (`police: <name> on lane N at ..., N m from the player (replacement)`).

The picture of the chase: when the `chase` step ends, the run puts the view for one second on a camera 9 m in front of the player's car, looking back at it and at the nearest police car, by day (the clock is set to 14 h if it is before 9 h or after 16 h), motion blur off, and writes `gtarun-chase.png` in the screenshots folder. A line says what it shows (`GTARun: picture of the chase asked: ... N police car(s) on the road, N of them in the frame with nothing in front`): the player's own camera looks ahead and never shows the cars behind.

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
| Held again in the same place (12 m) after three back-offs, or no wheel on the ground (or on its side) for 5 s | Given up where it is: brakes on, its officers get out when the player is within 60 m, it is taken away once out of sight, and another car is sent at once (two wrecks at most). It is never moved back onto a lane. The 5 s leave the vehicle's own rule its turn: a car lying on its side or roof is put back on its wheels after 2.5 s, the player's and the AI's alike, and a unit righted that way drives on. |

A given-up car no longer counts among the chasing cars, in the speed average either; the count is in the same lines (`... N back-off(s), N unit(s) given up`), and one line per event says where the car is, how it lies and what is in front of it (`police unit: <name> held with the throttle down, backing off at ... ahead: ...`). None of this has run in the engine at the time of writing: the logic is tested outside it only.

## What the second gate showed: corners, dead ends and a test that helped (2026-11-19)

Candidate 49, packaged: `pursuit` passed (22 km/h against 20 wanted, a car within 80 m 56 % of the time), with help: 4 units put back at 50 to 120 m from the player, and the player's car put back on a lane 4 times by the script. Three causes, each reproduced outside the engine.

**The speed of a corner.** Where two lanes meet at a right angle with their points 10 to 20 m apart, the corner read as a wide bend, one segment further away: 46 to 65 km/h wanted at a corner worth 19 (`GTPDrive::BendSpeed` measured the turn over whole segments and its distance from the point behind the car). The chase demo's car and the units share that code: both left the road at such corners. The turn is now measured 6 m of path each side of the vertex, and its distance from the car itself: 19 km/h whatever the spacing (`bend_test.cpp`). The point the steering aims at is counted from the car along the path, not from its closest point. And the chase settings (`GTPDrive::SetChaseDriving`, shared by the units and the demo's car) plan a braking of 4 m/s2 with a brake that answers the whole error: with 6 or 7 m/s2 planned and a brake at 0.4 of the error the pedals ran behind the plan. A bicycle model driven round a right angle reaches it at 21 to 25 km/h with brakes of 6 to 12 m/s2, where the settings of candidate 49 gave 26 to 54 (`corner_drive_test.cpp`). The calm traffic uses the same decisions: on the lanes of the quarter the traffic simulation goes from 974 frames of contact to none (`tools/vehicles/traffic_sim`, one run of 25 cars, 300 s).

**The way to the player.** At each junction a unit took the exit whose end was the nearest to the player. On the lanes of the quarter (1043 lanes, 137 dead ends) that walk ends in a dead end in 193 chases out of 341 started 60 to 300 m from the target. And a unit stopped at the end of a dead end started again on the same lane, for good. The way is now the shortest one on the lanes (`GTPLaneRoute.h`, Dijkstra, tested in `lane_route_test.cpp`), looked for again every 2 s as the player moves, keeping the lanes the car can no longer avoid at its speed. At the end of a dead end the unit turns round: it takes the lane back the other way, at full lock, backing off when it is held against the kerb (a three-point turn), and that dead end is kept out of its way for half a minute (`police unit: <name> at the end of lane N, a dead end, at ...: turning round`).

**The help.** Units are no longer put back near the player, replacements are made far (above), and the `pursuit` step fails when a replacement was made nearer than 150 m or when the script moved the player's car, whatever the other numbers. Its speed criterion has a fixed floor: 60 % of the player's average made the step easier with a slower player (20 km/h wanted for a player at 33); it is now never under 25 km/h, the speed of ordinary traffic in streets posted at 20 to 30 km/h. Candidate 49 would fail on both counts.

None of this has run in the engine at the time of writing. The lanes used outside it are those of 2026-11-11, not the map of the gate; the real braking of the cars is not known (the settings leave it a margin, they do not measure it).

## What the gate of candidate 51 showed: a car made on another car (2026-11-20)

Candidate 51, packaged, with the changes above: no unit put back, no replacement nearer than 150 m (one, at 152 m), the player's car never moved by the script, a police car 4 m behind the player in the picture of the chase, the player arrested by a chasing car 2 m away. `pursuit` failed all the same: 23 km/h against the floor of 25 (the player averaged 24 over the same seconds, which start before he drives off).

Its lines give one cause. The third unit was made on lane 54 at a height 72 cm above the place it then lay, tilted (2 wheels on the ground), a traffic car 130 cm inside its bumper: it had been made on that car. It stayed 12 s at 0 km/h, backing off three times, before it was given up. Those 12 s are about 15 % of the units' time; without them the average is about 27 km/h (worked out from the lines, not measured).

Why: the place of a new police car was checked against the other police cars only, and the ground under it was found by a trace that stops on a roof. Now a place is refused when that trace stops on a car or a person, when one stands in the room the new car takes, or when any car on the road is within 15 m. Roadblock cars get the first two checks (a roadblock car of the same gate was made 112 cm higher than its neighbours). When no place is found, a line says how many tries failed on what (`police: no place for a car in 60 tries`), at most every 10 s.

Not compiled at the time of writing, and no test outside the engine: these are collision queries.

## Switches

`-PolicePerception=0|1|2` (off, perception only, both = default), `-PoliceEqs=0` (officers sweep a fixed circle again), `-Police=0`, `-WantedStars=N`, `-WantedHold`, `-Witnesses=0`.
