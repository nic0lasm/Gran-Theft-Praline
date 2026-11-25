# Wanted level and police (gameplay)

Code: `GTPWantedLogic.h` (rules, tested in CI), `GTPWanted.*` (component and director), `GTPPoliceTactics.h`, `GTPPoliceOfficer.*`. Decisions: ADR-0004.

## What earns stars

Each crime adds heat; stars are a step of the heat (1 at 20, 2 at 60, 3 at 120, 4 at 220, 5 at 360). A crime counts only if a witness could see or hear it (`GTPWitnessLogic.h`); `-Witnesses=0` makes every crime count. A witness's line of sight ends 150 cm above the place of the crime; the thing the crime is done to (the stolen car, the car that hit somebody) and the player never cut it. When nobody saw it, the log line `crime N not witnessed` says how many people were near and what cut the nearest person's view.

A crime of one kind is looked at once every half second (`FGTPCrimeReportGate`): a car scraping another is a hit at every contact, and at gate 85 one collision was reported 26 times in three frames, each time asking every person of the map for a line of sight. The log says how many reports were left out.

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

**What a unit that rams still brakes for.** From three stars a unit rams: it does not brake for the player's car. Until gate 101 it looked at nothing at all ahead of it, and drove at 105 to 135 km/h into the parked cars, the roadblock cars of its own side, the other units and the people on the road. Gate 93's long take, three then four stars: four units with no wheel on the ground in a minute, one on its roof five seconds after it was made and 1.1 m higher than it was made, one 1.6 m higher six seconds after it backed off from a car 75 cm ahead of its bumper, one that had just slowed from 44 to 6 km/h where a roadblock car stood. Their lines all say `obstacle=-1`; none says what each of them climbed, so the cause is read from the code. A unit that rams now brakes for everything but the man it is after and the car he drives (`GTPPoliceTactics::ObstacleFor`). Held up 1.5 s behind a car nearer than 12 m, at whatever speed, it pushes through it for 3 s; behind a colleague or a person it waits (`IsHeldUp`, `ShovesThrough`), and a unit stopped 4 s far from the player still pushes through whatever holds it. The unit's line in the log says `met=` (nothing, quarry, police, person, other) after `obstacle=`. `-PoliceRamBlind`: as before. Not played yet.

## Losing the police

- Out of sight, the search timer runs (14 s plus 4 s per star). Seen again: it starts over.
- Hiding: out of sight, still (under 1.5 m/s) and more than 80 m from every unit (25 m until gate 75), the search runs twice as fast.
- Changing car (or getting out) out of sight keeps only 35 % of the search time.

## Arrested

A unit within 7 m of a player on foot or under 12 km/h, three seconds in a row. At the first second the police shout (`OnArrestWarning`, for sound and subtitle). Then: banner, the weapon is holstered and the ammunition taken, the stars are cleared, and after `BustedSeconds` the player reappears at a TargetPoint tagged `GTPPoliceStation`, else at `ArrestReturnLocation`, else at the player start.

**Officers on foot shove no car.** Gate 99's long take, 97 to 107 s: the stolen car stands empty on place Bellecour
(the player gave himself up), two officers run to it and along it, and it goes sideways at 35 km/h, onto its roof and
over the square. No line of the log says why. Probable cause, read in the code and in Epic's reference, not measured:
an officer is a character, and a character's movement pushes every simulating body it runs into
(`bEnablePhysicsInteraction`, on by default, with a push force that is not scaled to the weight of the body); he
walks along what stands in his way, a hand on it. Officers, the player on foot and whoever is put out of a car no
longer push (`GTPOnFoot::NoShoving`; `-PeopleShoveCars` gives the push back). To read at the gate: `police: officers
on foot do not push the cars they run into`, and, if a car nobody drives is still touched by somebody on foot,
`<car>: with nobody at the wheel, touched by <who> …, impulse N; the car up U, N km/h` (six lines a car at most).
Gate 100 showed the same car on its side 16 s after the arrest, then on its roof, with nothing filmed in between: a
car nobody drives now says once a second, twice a car at most, when it is off its wheels or moves at more than
15 km/h: `<car>: nobody at the wheel and it is off its wheels: up U, N km/h at …; on foot within 5 m: <who> (class) at
D m, V cm/s, pushes 0|1; cars within 8 m: <which> at D m, N km/h`. The player's own car says it too when it is off
its wheels: at gate 99 he was still in it, stopped, waiting to be arrested.

**The car of an arrest does not stay for good.** The player is taken out of his car when he is arrested and reappears
at the police station; the car stayed where it was, and at gate 100 the second flight stopped against it on place
Bellecour at 235 s. It is now a car left in the street like one its driver ran from (`GTPLeftCars.h`): taken away
once the player is 80 m away and does not see it. Lines: `traffic: <car>, the car the player was arrested in, stays
at … until he is 80 m away and does not see it`, then `traffic: <car>, left in the street … taken away … far from the
player and out of sight`. `-KeepArrestCars` leaves it. **Not compiled, not played.**

## Dead

When the player dies (`OnPlayerDied`), pending reports are dropped; at the respawn at the hospital the stars are cleared.

## Proof in the packaged game (`-GTARun`, 2026-11-19)

Four steps of the automatic run cover the wanted level, counted on their own line (`GTARun police: N/4 ok`); the seven others keep their line (`GTARun done: N/7 ok`); the crash step, informative, has its own (`GTARun informative: N/1 ok (crash)`, see traffic-mass.md).

| Step | What happens | Passes when |
| ---- | ---- | ---- |
| `witness` (2) | Right after the carjack | A crime phoned in by a person (the victim, a passer-by) reached the police and gave a star. A star forced by the chase script does not count. Fails with `-Witnesses=0`. |
| `pursuit` (4) | The flight goes on for 40 s with the level left to itself (no forced stars, arrest possible), then until the car is on the place the [flight circuit](flight-circuit.md) ends on (the place Bellecour), 20 s more at most | The police follow, and nothing helped them: over the time a chasing car was on the road, the cars average 60 % of the player's own average and never less than 25 km/h (unless they stay within 30 m of him on average: a player boxed in), and a car is within 80 m for a quarter of the time; no replacement car was made nearer than 150 m from the player, and the script never put the player's car back on a lane. Roadblock cars are left out. |
| `outcome` (5) | The player gives up: the car stops and waits | Arrested then released on foot, able to walk, no star left; or killed and back; or the level was lost. Whatever ended the chase since it began counts. An arrest counts only when a police car or an officer was within 10 m when it was made, and when the chase script had not moved the player's car in the 8 s before (the detail says who made the arrest and from how far). Fails when the player is still wanted 60 s after stopping. |
| `wasted` (11) | The player takes a lethal hit through the health component | Dead, back on his feet (hospital point, else the player start) with full health, able to walk, no star left. |

The numbers of the pursuit come from the police director (`FGTPChaseTally`, `GTPChaseTally.h`, tested in `tools/vehicles/wanted_sim/chase_tally_test.cpp`): every frame while the player is wanted, the speed of each chasing car and the distance of the nearest. It logs them every 10 s (`police chase: units mean=... km/h ... nearest mean=... m`), and the chase demo adds `police mean=` and `nearest mean=` to its `DemoChase` line. No unit is ever moved by hand. The cars a level asks for are made in the spawn ring (50 to 120 m, out of sight); once it has had them, a car sent replaces one that was lost (left more than 300 m behind, given up, wrecked) and is made 150 to 255 m away, in front of a moving player when a place is found there: it has to drive to him. The lines count them (`N sent, N of them replacements (nearest made N m from the player)`), and each car says where it was made (`police: <name> on lane N at ..., N m from the player (replacement)`).

Behind a player who drives, a car is made upstream of him (`GTPPoliceUpstream.h`, tested in `tools/vehicles/wanted_sim/police_upstream_test.cpp`): on the lanes that lead to the one he is on, walked back from his place until the distances above are met (50 to 120 m in a straight line for the cars of a level, 150 to 255 m for a replacement), at a place free of cars and out of his sight. It then has his own road to drive and nothing else, and it rolls from its first frame at his speed, 50 km/h at most, as a traffic car does when it is handed back to the solver. Until gate 91 every car was made on any lane of the ring, heading wherever that lane went: in a quarter of one-way streets it had the block to drive round first, and the nearest unit stayed 98 to 165 m from the player on average (gates 83 to 91; at gate 91 no unit came within 97 m). The flank cars of a level (every other one, from two stars) still come from in front; a replacement is looked for upstream first, then in front, then anywhere. The line of the car says so (`(upstream: N m of road to him, rolling at N km/h; N made so, N time(s) no place there)`), and `-PoliceUpstream=0` goes back to the ring alone. Not compiled nor played when written: whether the units now stay in the mirror is for the next gate to say.

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

A second unit of that run stood on its four wheels, 751 N.m forwards then 1287 N.m backwards on rear wheels that did not turn, nothing within 3 m of its bumper: three back-offs that moved it 30 cm, 12 s at 0 km/h. Why it was held is not known. A back-off that moves the car less than 0.5 m did nothing, and two of them in a row now give the car up (`moves neither forwards nor backwards`): about 7 s instead of 13 before its replacement is sent (`GTPUnitRecovery.h`, tested outside the engine). This shortens the time a stuck car weighs on the average of the `pursuit` step; the criterion itself is unchanged.

The player's car stood at 0 km/h when it was arrested: it had braked for a pedestrian at 0 m, then was held with the throttle down, the wheel turned (850 rpm, first gear) for 2 s. Against that pedestrian or against the scenery beside it: the lines do not say.

Not compiled at the time of writing, and no test outside the engine: these are collision queries.

## The officers' uniform (2026-11-22)

In the video of candidate 59 the officers on foot were the player's orange mannequin with a pistol: `AGTPPoliceOfficer` took the body of `AGTPLiteDriver`, which is the player's mesh when the config names none. An officer is now an assembled character of City Sample Crowds, like the people of the street: the tall normal-weight male body, a shirt, long trousers and shoes that follow its pose, a head, the run loop baked on that skeleton, a colour for the garments, and rigid pieces (cap, vest, belt) hung on bones of the body. Everything is read from `[/Script/GranTheftPraline.GTPPoliceOfficer]` in `DefaultGame.ini`; an asset that does not load is left out and named in the line `police: officer in uniform: ...`, and without the body or the face the officer keeps the mannequin. `-PoliceUniform=0` keeps it too.

Not known at the time of writing, and not compiled: how the garments' materials take a colour (the navy blue is given only once their parameters are named in the config), the name of the upper spine bone for the vest, whether the head `m_002` and the run loop `CS_M_Neutral_Run_Loop_F_R_20` are in the project. The cap, the vest and the belt do not exist yet.

## The police car's livery and the officers' navy blue (2026-11-23)

Video of candidate 62: the police car is City Sample's saloon (vehicle13 of City Sample Vehicles) in Epic's own paint with our two lights, and nothing on it says "police". Two things are added to every police car when it is made (`AGTPPoliceDirector::AddLivery`): a blue band on both sides, which is one deferred decal projected across the car at door height, between the wheel arches, so that it lies on the body's own shape; and white paint, a material per body slot. The officers' garments get navy blue materials the same way.

None of these materials is in Git: `tools/police/unreal/make_police_look.py`, run once in the editor, makes the band's decal material, and children of Epic's paint and garment materials with their colour parameters set. The names of those parameters are not known in the repository: the script takes the vector parameters whose name reads as a colour, makes nothing for a slot that has none, and writes every slot, material and parameter to `Saved/PoliceLook/police_look_report.txt` (with the bones from the head to the root of the officers' body). Without the script's assets the game runs as before. The log says what was found: `police: livery on ...`, `police: officer in uniform: ...`. `-PoliceLivery=0` turns the livery off.

Not compiled and not seen at the time of writing. Not known: whether Epic's paint takes a decal, where the band falls on the door (the place is worked out from the mesh's box and set by `LiveryBand*` in the config), which parameters the script will find.

First run on the PC (23/11): the band's material and five navy materials were made, no paint. The police car's mesh has a single slot, `lambert1`, with no material in it, so the sentence above about Epic's own paint is not established: no picture read so far shows a police car closely enough. The script now reports, for such a slot, what the car's Blueprint puts on it and what the mesh of Epic's it was copied from holds (and the skeletal meshes next to it, in case ours is a stand-in). Only when nothing draws the slot but the engine's grey default material does it make a paint of its own, `M_GTP_PolicePlain`: white, darkened at the windows' height and at the bottom of the car. One slot covers the whole car, so glass, pillars and tyres are told apart by height only (`GTP_POLICE_GLASS`, `GTP_POLICE_SKIRT`, shares of the mesh's box). Not run and not seen at the time of writing.

Gates 67 and 68 (23/11): that paint did not compile. The script had taken the first enum name ending with `WORLD` in alphabetical order instead of the plain world space, the material asked for a transform that does not exist, was cooked without shaders, and the game logged three `LogMaterial` errors when the first police car loaded it (`invalid ShaderMap`, `uncooked shader map ID`). The script now takes the plain name, writes the material's pixel shader instructions in its report (0 = not compiled) and makes the paint again white all over when the mask does not compile. The paint stays off in the config (`LiveryPaintFolder` points at a folder that does not exist) until a run of the script reports instructions and a gate loads it without errors.

The same day the pictures were read again for the car itself: no picture of gates 51 to 68 shows a police car, not even the one of gate 51 taken when the log gives a unit 4 m from the player, and in the video of gate 67 the player steals and drives a City Sample car that is not drawn at all. Read on pictures, not measured: City Sample's cars rebuilt by `make_epic_vehicles.py` (the police car among them) have no visible body in the game. Until they have one, the police drive the white van (`PoliceVehicleClass` = the Kangoo, `LightBarOffset` on its roof), which is drawn, takes the band's decal and needs no paint. It is a van: heavier and slower than the saloon (`AGTPVehicle` gives it 200 N.m for 1 500 kg), so the pursuit's mean speed has to be read again at the next gate.

Gates 70 to 74 (24/11). The white van was not read as police on the video of gate 72, and `make_epic_vehicles.py` now gives City Sample's cars their real bodies as look parts posed by the rig (`AGTPVehicle::LookMeshes`), so the police are back on vehicle13 at 74, with the band and without paint: `AddLivery` painted the rig, which has no geometry. From the candidate after 74: the paint goes on every mesh that draws the car (body and look parts), asked for as `MI_GTP_PolicePaint_<mesh>_<slot>`; the script makes it for the paint slots (`veh_carPaint`) as a child of Epic's own paint with its colour parameters set to white, as it does the officers' navy, whose children have loaded without an error since gate 66. A child that only changes colours has no shader of its own, so nothing home-made is cooked for the paint; `M_GTP_PolicePlain` is no longer made nor loaded (its pixel shader instructions read 0 in the commandlet whatever its nodes: that figure could not tell whether it compiled). The band is taller (26 cm) and a thin red stripe goes above it (`M_GTP_PoliceStripe`, the band's material in red). Not known: the names of the colour parameters of Epic's paint (the script guesses and reports them; when it finds none it makes nothing and the car keeps Epic's cream white), and where the band and the stripe fall on the saloon's door.

## What gate 75 showed: scenery, an officer behind a building, a player who stops (2026-11-24)

Gate 75 passed and showed the police car at last: white, a blue band, a red stripe, its light bar (`2 of 32 material slot(s) painted on 3 mesh(es)`). The pursuit did not pass: in the filmed run the units averaged 18 km/h (25 wanted) and moved 62 % of their time, and the player, stopped against a block at three stars, was lost 19 s later with no arrest. The lines of that run say why.

- **Scenery.** Nine times in 68 s a unit was "held with the throttle down"; eight times its nose was against the street's furniture (an instanced static mesh: lamp post, lights, tree), on its lane (0.1 to 1.3 m off its path) as often as far off it (10 to 53 m, driving at a lane across a square). A unit only looked ahead for cars and people, and at three stars and more not at all. It now looks ahead for what does not move (`GTPUnitAvoid.h`): a box at bonnet height pushed along its heading, leaned into the turn, as far as it needs to stop; when something is there, the same look 35 degrees to each side, the freer side taken and kept 1.5 s, the wheel turned the more the nearer it is, the speed cut to what it can stop from. While it goes round, the look is wider than the car, so it does not turn back in before its side is past. With no way round it stands short of it, and the recovery backs it off as before. `tools/vehicles/wanted_sim/unit_avoid_test.cpp` drives the car of a chase at a post on its lane (it runs into it without the look, goes round it without touching with it, 0.2 s lost), past parked cars 30 cm from its side (not slowed), at a wall (it turns along it) and into a dead end (it stops short).
- **A player stopped off the lanes.** A unit 91 m from the stopped player drove away from him, to 200 m: with a car or a tree's crown in the line of sight it took the lanes, and the lanes do not stop where the player is. Within `CloseInDistance` (60 m) of a player whose place is known, a unit now drives straight at him when no scenery stands between (a sweep as wide as the car, cars and people left out). And a unit driving straight at a player on foot or under 12 km/h slows as it comes and stops 5 m from him (`CloseInStop`): at 3 stars it ran into the stopped car, pushed it, and the arrest, begun (40 %), was lost.
- **The officer.** One officer was out for the whole run and stayed 36 m from the stopped player for 25 s: he walked straight at his goal, whatever stood between. He now follows what he meets (a wall, a car, his own first), the same hand on it, until the way to his goal is free, and tries the other hand when he has not moved 60 cm in 2 s. In the same test he goes round a building 12 m wide in 13 s, and out of a corner.
- **Roadblocks.** The roadblock "ahead" of the stopped player was taken away and made again every 2 s (about 40 cars in 80 s): a car that stands has a velocity that points anywhere, and the roadblock was "passed". It is now passed only by a player driving away from it, and put only ahead of one driving at 20 km/h and more (`RoadblockMinSpeed`).
- **Roadblocks and the filmed flight.** A roadblock stands across the player's road ahead: for the flight the chase script drives, that is the line of its circuit. Gate 91's long take: 19 roadblock cars, 11 of them within 8 m of that line (8 within 1.5 m). One stood in the rue Bellecordière 13 m past the parked car that ended the first flight, short of the place Bellecour; at another the second flight stood 5 s with an officer in front of its bonnet and was put elsewhere. The script follows a line: it cannot go round two cars across a street, and it has no reason to stop before the place the circuit ends on. While it drives, it tells the police (`AGTPPoliceDirector::KeepFlightLineClear`, asked again every frame, gone 2 s after the script stops driving), and no roadblock is put on a place nearer than 8 m to a driving line of the circuit (`RoadblockFlightClearance`; the two cars reach 3.8 m from the middle of their lane and the flight strays up to 2.3 m from its line). Roadblocks still close the streets beside the circuit, and a player at the wheel gets them as before. In the log: `police: a script drives the flight, no roadblock within 8.0 m of the driving lines of the map's circuit while it does`, then each `police: roadblock car N at ..., M m from the flight circuit's line`. `-RoadblocksOnFlightLine`: as before. Not compiled nor played when written.
- **Hiding.** Three stars were gone 13 s after the police stopped being told where he was: stopped in the street, 36 m from an officer, the player was "hiding" (out of sight, still, more than 25 m from every unit), and the search ran twice as fast. Hiding now asks for 80 m (`HiddenMinDistance`).
- **Paint.** The 30 slots that have no paint were each asked for once (a line each in the log). The folder is now listed once from the asset registry and only what it holds is loaded.

`-PoliceAvoid=0` turns the look for scenery and the straight way off (to compare). The unit's line every 3 s now ends with `scenery=<m> side=<-1|0|1> rounds=<n> stood=<n> straight=<0|1>`. Not compiled and not played at the time of writing: what the units do in the real streets is to be read at the next gate (the back-offs, `rounds`, the mean speed), and so is whether the box catches the ground on a ramp (a hit whose face looks up is taken for ground and left out).

## Switches

`-PolicePerception=0|1|2` (off, perception only, both = default), `-PoliceEqs=0` (officers sweep a fixed circle again), `-Police=0`, `-PoliceAvoid=0` (units do not look at the scenery), `-PoliceCircuitLines=0` (on the lanes of the [flight circuit](flight-circuit.md) units drive the lanes' own points, not the circuit's line), `-WantedStars=N`, `-WantedHold`, `-Witnesses=0`. `-FlightCircuit=0`: the filmed flight is free on the lanes instead of keeping to its circuit ([flight-circuit.md](flight-circuit.md)).

`-DemoFreeCar=Car02` (with `-PoliceChaseDemo` or `-GTARun`): the car the automatic run steals is made of the traffic class whose name holds that text, instead of the nearest free car, whose model changes from run to run. For playing a run again with the same car: the theft was seen by passers-by in Car05 and Car12 and not in Car02 (gates 49 to 52), and only a run in Car02 can say whether the witness's line of sight is repaired.
