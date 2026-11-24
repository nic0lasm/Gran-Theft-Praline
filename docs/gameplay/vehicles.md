# Drivable vehicles

*French version: [vehicles.fr.md](vehicles.fr.md).*
*Version 0.3 "Behind the wheel" of the [roadmap](../../production/roadmap.md).*
*Last updated: 2026-11-09. Status: design written, C++ code compiles with no error or warning on the M1 Pro (editor and game targets, 2026-11-09); not tried in game yet (`BP_TestCar` comes first, section 9.1).*

## 1. Goal

In 0.3 the player steals a car parked on Rue de la République and drives it
to the Rhône quay. Driving must feel like a recent GTA: believable but
forgiving physics, one-key entry and exit, a chase camera that settles back
on its own.

Cars are **the ones actually on French roads** (project lead decision,
2026-11-09): real models of every make, in realistic proportions, plus a few
Lyon-specific vehicles (TCL buses, Vélo'v). Non-commercial use: makes and
logos are reproduced faithfully, logo textures stay out of Git
(`Plugins/CityBrands/`, see the [art bible](../art/art-bible.md), section 3.5).

Nothing is reinvented: physics comes from **Chaos Vehicles** (Epic plugin
shipped with the engine, chosen foundation); entry animations will come
later from the Game Animation Sample or a capture.

## 2. What 0.3 does, and does not

| In 0.3 | Later |
| ---- | ---- |
| Enter, exit, bail out of a moving car | Passengers, rear seats (0.6, police) |
| Open a locked car by breaking the window | Alarm, lock picking, keys |
| Pull an NPC driver out | NPCs that resist, flee, call the police (0.6) |
| Physical driving: throttle, brakes, reverse, handbrake, slides | Manual gearbox, per-player tuning |
| Chase camera, interior view (V), look behind | Cinematic camera, bonnet camera |
| Simple damage: dents per zone, broken glass, wreck | Fire, explosion (0.4), flat tyres, torn-off doors |
| 1 to 3 drivable models; a few cars driven by non-player drivers to pull out (§8.2) | Full traffic with lights (0.9), 6 final vehicles (0.16) |

## 3. GTA-style entry and exit

One key: **F** on keyboard, **Y** (top face button) on gamepad.

### Entering

1. On foot, the key picks the nearest car with a door within 3.5 m,
   favouring the one the camera looks at.
2. The character moves to the driver door (left: France drives on the
   right). In 0.3 it is placed there directly; later it will walk there,
   with motion warping to fit the animation.
3. Depending on the car:

| State | What happens | Duration without animation |
| ---- | ---- | ---- |
| Unlocked, empty | opens the door, sits | 0.9 s |
| Locked, empty | breaks the driver window (elbow), opens from inside | 1.6 s |
| Someone at the wheel | opens, drags the driver out to the ground, sits | 2.2 s |

4. The player takes control of the car; the character is attached to the
   seat (`seat_driver` socket). It stays hidden until we have a seated pose
   for our character (`bShowDriverInside` option).

One animation per case (montage) plugs in without code: when the montage
exists, its length replaces the default duration.

### Exiting

- **Stopped or slow** (under 40 km/h): the car brakes to under 4 km/h (3 s
  at most), the character gets out through the driver door and the car
  stays on the handbrake.
- **Fast** (over 40 km/h): the character **jumps** sideways with part of the
  car's speed; the car rolls on without a driver. Falls and ragdoll come
  with 0.4 (character damage).
- **Door blocked** (wall, another car): out through the other door, then
  over the roof; if everything is blocked, the player stays inside.

## 4. Driving (Chaos Vehicles)

The target is the GTA feel: a heavy but lively car, forgiving on a
keyboard, that stays controllable in a slide.

- **Automatic gearbox**, reverse by holding the brake at a standstill.
- **Handbrake** on the rear wheels only, with slightly less rear grip: a
  handbrake tap in a corner swings the car round, as expected.
- **Steering that firms up with speed** (100 % at rest, 40 % at 130 km/h)
  to stay drivable on a keyboard.
- **Base tuning** (C++ class): a French city car such as a Clio V or 208,
  1,150 kg, ~100 hp, front-wheel drive, 195/55 R16 tyres. Each model
  overrides what sets it apart in its Blueprint.

Three driving profiles cover 0.3 and traffic:

| Profile | Examples | Mass | Torque | Drive | Feel |
| ---- | ---- | ---- | ---- | ---- | ---- |
| City car | Clio, 208, C3, Sandero, Yaris, Polo | 1,150 kg | 130 N·m (Chaos has no drivetrain losses) | front | lively, light, oversteers on the handbrake |
| Family car / SUV | 308, 3008, Golf, A-Class, 3 Series | 1,400 kg | 300 N·m | front (rear for BMW) | steadier, faster |
| Van | Kangoo, Berlingo, Trafic, Master | 1,900 kg | 330 N·m | front | slow, rolls, long braking |

Targets to measure in game (video + log): 0 to 50 km/h in 4 to 5 s for the
city car; top speed 150 to 170 km/h; stop from 50 km/h in under 15 m.

## 5. Controls

Driving actions are built **in code** (`UGTPVehicleInput`): no input asset
to author in the editor. AZERTY by default, QWERTY with `-Qwerty` or
`bQwertyKeyboard=True`, as on foot.

| Action | AZERTY | QWERTY | Gamepad |
| ---- | ---- | ---- | ---- |
| Enter / exit | F | F | Y (top) |
| Throttle | Z, ↑ | W, ↑ | right trigger |
| Brake / reverse | S, ↓ | S, ↓ | left trigger |
| Steer | Q / D, ← / → | A / D, ← / → | left stick |
| Handbrake | Space | Space | RB |
| Look | mouse | mouse | right stick |
| Look behind | C | C | right stick click |
| Interior / chase view | V | V | View |
| Pause | Esc | Esc | Menu |

At the wheel the on-foot contexts are removed and replaced by the driving
ones; the enter / exit action stays active at all times, above the others
(priority 2). To check on the first run: that the Game Animation Sample
uses neither F nor Y on foot.

## 6. Camera

GTA camera: third-person chase by default, interior as an option. No
body-camera effects (decision of 2026-11-09).

| Setting | Value | Purpose |
| ---- | ---- | ---- |
| Distance / height | 6.2 m / 1.7 m | the whole car and the road ahead |
| Position / rotation lag | 8 / 10 | the camera follows smoothly |
| Recentre | after 1.5 s without look input, above 10 km/h | swings back behind the car, pitched at -10° |
| Field of view | 75° at rest → 88° at 130 km/h | sense of speed |
| Look behind | while the key is held | rear view, then back |
| Interior view | V, FOV 85°, at the driver's eye height | same key as on foot; the choice is kept when getting out |

The camera avoids walls (spring arm with collision test).

## 7. Simple damage

- **Health** of 1,000 points. Impacts count from an 11 km/h change of
  speed: a 50 km/h frontal hit removes about 130 points.
- **Four zones** (front, rear, left, right), each with a dent **morph
  target** (`Dent_Front`…) applied progressively: fully dented at 400
  points in the zone. Believable deformation at almost no cost.
- **Glass**: separate components tagged `Glass` + zone, hidden at 120
  points in the zone (shards and sound in the Blueprint through
  `OnGlassBroken`). The driver window also breaks when the car is forced.
- **Wreck** at 0 points: the engine stops responding, the car can no longer
  be stolen. Smoke under 35 % health, then fire and explosion with weapons (0.4).
- Weapon damage already goes through `TakeDamage` (zone taken from the
  impact point).

## 8. Code architecture

Module `GranTheftPraline`, prefix `GTP`, branch `feat/vehicles`.

| Class | Role |
| ---- | ---- |
| `AGTPVehicle` (`AWheeledVehiclePawn`) | base tuning, driving input, entry / exit (seat, doors, carjack, bail out), cameras, damage |
| `UGTPWheelFront`, `UGTPWheelRear` (`UChaosVehicleWheel`) | city car wheels: front steer and drive, rear with handbrake |
| `UGTPVehicleInput` | Enhanced Input actions and contexts built in code |
| `UGTPVehicleInteractionComponent` (on the PlayerController) | enter / exit key: picks the car, asks for exit |
| `AGTPPlayerController` (changed) | swaps on-foot / driving contexts on each possession; V also toggles the car's view |
| `UGTPCameraModeComponent` (changed) | lets the car run its own camera and passes it the first / third person choice |
| `AGTPGameMode` (changed) | `-TestVehicle` parks a car in front of the player in any level |
| `UGTPEngineSoundComponent`, `UGTPEngineSoundWave` | engine sound synthesised from the Chaos rpm and the throttle (`GTPEngineSynth.h`), no asset |
| `AGTPHUD` (changed) | driving panel: speed in km/h, gear, state of the car |
| `AGTPTrafficNetwork` | driving lanes of the map, spawns and recycles cars with a non-player driver |
| `UGTPAIDriverComponent` | drives a car along the lanes (`GTPDriveLogic.h`), stops for what is ahead |

Entry flow: F key → `UGTPVehicleInteractionComponent` →
`AGTPVehicle::BeginEnter` (character frozen at the door, window or driver
handled, timer) → `FinishEnter` (seat attach, `Possess`) → the
PlayerController switches to the driving contexts and the car's camera
takes over.

Settings: section `[/Script/GranTheftPraline.GTPVehicle]` of
`Config/DefaultGame.ini` (timings, camera, damage), or per Blueprint. The
`ChaosVehiclesPlugin` plugin is enabled in the `.uproject`.

### 8.1 Engine sound and driving panel

The engine is heard from a procedural sound: a train of firing pulses
(three cylinders for the Clio V TCe: `rpm / 60 × 3 / 2` per second),
sharper and louder under throttle, through a muffler low-pass, with intake
noise. It follows the Chaos engine rpm, so every gear change is heard, and
it plays while someone sits at the wheel. `tools/vehicles/engine_sound/`
renders it outside Unreal and checks pitch, level and clicks; recorded loops
can replace it in the sound pass (0.18).

While driving, the bottom right of the screen shows the speed (km/h), the
gear (R, N, 1-5) and a bar for the state of the car (white, amber, red);
the action prompts move up above it.

### 8.2 Cars driven by non-player characters

Enough traffic to steal a car with someone inside (the full traffic with
lights and priorities is 0.9):

- `tools/city/traffic.py` builds the lanes from the BD TOPO road centre
  lines: car roads only, directions from `sens_de_circulation`, right-hand
  traffic, links at junctions without U-turns. With `--map <Map>` it copies
  them to `Content/City/Traffic/<Map>.lanes.csv`, staged with the game.
- The game mode spawns an `AGTPTrafficNetwork` when that file exists. It
  puts `NumCars` cars (3, the measured M1 budget: p95 40.4 ms with 3 cars, 45.4 ms with 6; `-Traffic=N`, 0 = none) with a driver on the
  lanes, at least 40 m away and out of sight, and takes away cars stuck at
  a dead end or too far, out of sight too.
- `UGTPAIDriverComponent` decides with `GTPDriveLogic.h`: pure pursuit
  steering, speed from the lane limit, lowered before bends and to stop
  4 m behind whatever is in front (cars, people). One car at a time crosses
  a junction (nearby nodes of a big crossing count as one); the others wait
  at a stop line. Lanes of a multi-lane road continue into the lane in the
  same position. Lanes stay within 60 m of a generated building, except the roads of the flight circuit, kept whole ([flight-circuit.md](flight-circuit.md)). The same header runs in
  `tools/vehicles/traffic_sim/` on the real lanes, with pass criteria.
- Carjack: nobody gets into a car above 12 km/h; a car stops for a person
  in front of it. The car brakes while its driver is pulled out; the
  driver then runs 25 m away.

### 8.3 Falling into the Rhône

`UGTPRiverRecovery` (world subsystem) binds the trigger boxes tagged `GTP_RiverRecovery` that the city generation lays on the water, and collects the `GTP_RiverExit` points placed on both embankments (`tools/city/riverside.py`). A riverbed with collision 2.5 m down keeps anything from falling out of the world.

- On foot: the player floats for `SwimSeconds` (4 s), the screen fades to black and the player stands at the nearest exit point.
- In a car: the car is drowned (wrecked, no input); after `SinkSeconds` (2.5 s) the driver is taken out with `AGTPVehicle::EjectDriverTo` and brought to the nearest exit with the same fade. A car is removed `RemoveCarSeconds` (12 s) after it fell, with its non-player driver.

No water plugin and no physics volume: plain overlap events, nearly free on the M1.
### 8.4 Traffic lights (version 0.4)

`GTPTrafficLights.h` (no engine types, also used by `tools/vehicles/traffic_sim`) puts lights on every junction of the lane file reached from two crossing directions. Incoming lanes within 45° of the first approach (either way) form group 0, the others group 1; the groups take turns: green 12 s, amber 3 s, all red 2 s, each junction at its own point of the cycle.

- AI drivers stop at the line on red, and on amber when they can do so at comfortable braking; a car waiting at a red light claims no junction, so the crossing traffic keeps moving. Cars also see the car ahead along their lanes, which the straight probe misses on bends.
- Pedestrians: `AGTPTrafficNetwork::IsWalkAllowed(Location, WalkDirection)` says whether someone may cross there now (always true away from a signalled junction).
- Signal heads at each stop line, no shadow, no collision: the project's vehicle traffic light (`SM_TrafficLight_Vehicle_A`, wave 8 prop, `SignalHeadMesh`) turned towards the incoming cars, with a coloured lens disc on its front, red at the top, amber, green at the bottom. They replace the stand-in seen in gate 59's video (a coloured sphere on a thin pole), which comes back if the mesh does not load or with `-SignalHeads=2`. Not checked on a picture yet: the mesh's front is taken as +X (the convention `tools/city/details.py` places it with) and the lenses as 25, 55 and 85 cm below its top. `-SignalHeadYaw=D` turns it, `SignalLensBelowTop` and `SignalLensStep` move the lenses, and the log line "traffic lights: signal heads are ..." gives the mesh's size. `-TrafficLights=0` turns the lights off, `-SignalHeads=0` hides the heads. Since candidate 67 (gate 65's video: five heads in a row across the square, a car driven through one, three or four lights at a corner): one head per approach, on the kerb side, the rightmost lane of a light group keeping it; no foot less than 2.2 m from the centre line of any lane (`SignalHeadLaneClearance`), moved further right 50 cm at a time up to 3 m, else no head; and where the details layer already has a vehicle light within 6 m facing the same cars, within 60 degrees (`SignalBorrowRadius`; `tools/city/details.py` puts one, the same mesh, at each traffic_signals node of OpenStreetMap), the head's lenses go on that light's front and the head draws no mesh of its own. Lines: "traffic lights: N signal heads, M inner lanes share the kerb-side head of their approach, P moved further right off another lane, Q left out ..." and, when a head is first drawn, "traffic lights: the head of lane L drawn on the street light ...". Not checked on a picture yet.
- Simulation, 300 s on the rue de la République lanes: 3, 6 and 12 cars, no contact, no red light run.
- Measured in the game (after gate 59, the legacy traffic being what the gate judges): every 10 s, "traffic cars
  t=...: driven=D of W wanted (ghosts=G) moving=M (over 1 m/s, upright: P %) stopped=S lying=L not driving (no route or no decisions)=N mean speed=...
  | within 100 m=N (moving, stopped) within 150 m=N (moving, upright, ghosts; far puppets drawn there) within 200 m=N
  nearest=... | stopped for: a car ahead, the queue along the lanes, a person, something else ahead, a red light, a
  junction in use, held (reaction or stop asked), wedged, nothing ahead; stopped over 20 s, at a dead end | recycled
  so far: stuck, far, overturned, by the chase script or the police, reused from the pool", the counterpart of
  "MassTraffic cars:". Each car counted lying gets its own line, "traffic: ... overturned, counted lying: up U
  at X, simulated or kinematic, speed, distance to the player, reaction, seconds after its last placement and
  which" (gate 62: one lying car in the default and in the -GTARun pass, with nothing in the log to say which).
  A driven car overturned past 45 degrees is taken away at once out of the player's sight ("overturned" in the
  line); in sight, it puts itself back on its wheels after 2.5 s stopped. When the player has no pawn, a line
  "traffic: no player pawn from t=..." says so once: no car is then spawned or taken away by its distance to
  the player (gate 62's -GTARun pass, no car within 150 m from 236 s, "nearest=none"). Since candidate 67, a ghost
  found with its physics or its Chaos vehicle on has them switched off again, with a line "traffic: X a ghost with
  its physics on (where, S s after its last placement, speed, up): off again, N so far". Gate 65's -GTARun pass: a
  car placed as a ghost 83 m from the player, simulated 2 s later and thrown, lying, at 87 km/h by the ghost's
  teleports. Which path leaves the physics on is not known yet: that line will say. What stops a car is the nearest stop of its last decision
  (`UGTPAIDriverComponent::GetLimit`): the probe's hit (a car, a pawn, anything else), the queue along the lanes, the
  red light, the stop line of a junction another car crosses, a hold (reaction, stop asked by the player), wedged
  (pushing a second without moving, or backing off).
- Where the driven cars are (candidate 61): made out of sight 40 to 150 m from the player and taken away past 150 m
  out of sight (`MaxCarDistance`, `-TrafficRing=M`; logged at start as "traffic: driven cars made out of sight ...").
  At 250 m, gate 59's default pass ended with 19 of its 22 driven cars past the ghost distance (90 m): spread over a
  ring of 40 to 250 m, about one car in nine lands within 90 m. The chase script of the demo (`-PoliceChaseDemo`,
  `-GTARun`) takes off the road, twice a second, only a traffic car in the player's own way: ahead within 35 m and
  within 2.5 m of the line his car drives on. Until candidate 60 it took every car within 35 m in a cone of 60 degrees,
  oncoming lane included.

### 8.3 GTA pass of 13/11: stealing, driving feel, livelier traffic

Written in one batch, compiled and tried later (`GTPCarjackLogic.h`, checked offline by
`tools/vehicles/carjack_test`).

- **Pulled-out driver**: flees (55 %), comes back at the thief (30 %: punches on foot,
  bangs on the driver door once the thief is inside, gives up after three strikes, 8 s,
  or when the car drives off) or cowers then flees (15 %). `UGTPCarjackVictimComponent`;
  `-CarjackMood=flee|resist|cower` forces one.
- **Taking a car that rolls**: F near a traffic car under 45 km/h (6.5 m): its driver
  stops (`AGTPVehicle::RequestAIStop`), the player pulls them out as soon as it is slow
  enough. Police cars do not stop.
- **Starting**: a parked car starts in 0.7 s, a car with a broken window is hot-wired in
  1.4 s (no throttle, no engine sound until then); a car taken from its driver is running.
- **Driving feel, player's car only**: 300 N.m for 1 150 kg and as much per kilogram for a
  heavier car, or the car's own torque when that is higher (AI cars keep the real figure; 240 until
  candidate 61); handbrake above 20 km/h lowers rear grip and swings the tail with the steering;
  slides without the handbrake are damped so the car straightens; under 35 % health the
  engine loses up to 45 % of its power; a car dented on one side pulls to that side; a car
  on its roof or side, stopped, is put back on its wheels after 2.5 s (a traffic car from 45 degrees).
- **Chase camera**: pulls back 1.5 m more at speed, looks part of the way into slides,
  jolts on impacts.
- **Traffic**: 22 driven cars (16 before) and 30 far ones; Clio V, 208 and Kangoo when their
  Blueprints are in the build (van and 208 profiles by model); each driver has a
  temperament: calm (55 %), hurried (30 %: 15 % faster, honks and goes round sooner, floors
  it at gunpoint or when rammed), timid (15 %: slower, gets out at once at gunpoint).

### 8.5 Second pass of 13/11: cars to steal, crashes, guns, bailing out

Written in one batch, not compiled yet (`GTPIncidentLogic.h`, checked offline by
`tools/vehicles/incident_test`).

- **Cars parked along the streets**: about ten real cars (Clio V, 208, Kangoo) are parked at
  the kerb 50 to 110 m around the player, out of sight, and taken away beyond 150 m when
  unseen. 60 % are locked (a window to break), 40 % of those have an alarm: the horn pulses
  for 15 s when the window is broken or the parked car is hit. Physics only within 35 m.
  `-StreetParked=N` (0 = none). Each is put 2.5 m to the right of a lane (`ParkOffset`) and, since
  candidate 62, never at less than 2.1 m from the centre line of any lane (`ParkLaneClearance`): at gate 59
  a Car03 parked beside lane 96 stood on the chase's lane and the script had to move the player's car. The
  "traffic: parked ..." line ends with the number of spots left out for standing on another lane.
  A spot whose ground stands more than 60 cm above the road at the lane beside it (`ParkKerbRise`) is
  also left out, counted "on street furniture": the ground trace takes the first thing it meets, and in
  gate 61's video a parked van stood on a bus shelter roof, at first-floor height, for 30 s.
  Since candidate 66, a spot is also left out when it stands less than 12 m of lane from its lane's
  start or end (`ParkEndMargin`), where its lane comes more than 40 cm towards the kerb within those
  12 m (`ParkBendAllowance`), or when the car's body (its mesh's bounds) comes within 1.3 m of the
  centre line of another lane (`ParkBodyClearance`); the line counts them "near a lane end or bend" and
  "with the body over a lane". Traffic aims a few metres ahead and cuts the corners, and the next lane
  often starts two metres aside: at gate 62, 8 of the 9 parked cars traffic queued behind stood less
  than 10 m from a lane's end or start, a Clio among them with its bonnet over another lane. The cars
  parked around the player never take such a spot; a request that must be met (a mission, a scripted
  run) takes one when no other spot is free, and its line ends with "no clear spot".
- **Crashes between traffic cars** (within 80 m of the player, a change of speed of
  4.5 m/s or more, two at a time at most): a light bump ends in a blast of the horn (or, one
  time in three, an argument); otherwise one time in five the one who hit floors it while
  the other gets out and shouts after them; a timid driver may run away on foot; most often
  both get out, walk up to each other, argue 6 to 10 s with a shove now and then, walk back
  and drive on. Above 22 m/s everyone runs. A gun, a shot or a stolen car ends it: they run.
  `UGTPTrafficArgueComponent`; `-TrafficIncidents=0` turns it off.
- **Traffic drivers get out for real**: a driver at gunpoint, a timid one, or after a crash
  now stops and leaves the car (before, `PanicExit` did nothing on a traffic car).
- **Reactions to guns on by default**: without the threat system (`-ThreatResponse=1`), the
  traffic reads the player's weapon itself: aiming (cone of 10 degrees, 20 m) and shots (35 m).
- **Bailing out of a moving car**: from 20 km/h (40 before) the player jumps, rolls or is
  thrown, and is hurt, depending on the speed (ADR-0006).
- **Following a moving car (IDM)**: behind a moving car the traffic now uses the Intelligent
  Driver Model (Treiber et al. 2000, the law used by CARLA-style traffic managers, retained
  in the reuse survey) with a 1.4 s time gap, instead of aiming to stop behind every car; a
  queue moves off together. Still obstacles, lights and stop lines keep the old stop rule.
  `tools/vehicles/idm_test`.
- **Shooting from a car**: done by the combat work (feat/combat-gta-shooting); traffic reacts to
  drive-by aiming and shots too (the weapon of the driver seated in the player's car is read).

### 8.6 Epic's City Sample cars (13/11)

Not compiled. City Sample Vehicles (Fab, free, out of Git) gives 14 Chaos cars on the skeleton
SK_Vehicle (their wheel bones are not named like ours: see 8.7). `tools/vehicles/unreal/make_epic_vehicles.py` rebuilds
seven of them on `AGTPVehicle` (stealing, damage, reactions) with Epic's own Chaos tuning:
vehicle02, 03, 05, 07, 12 and the van 01 join the traffic and the street parked cars (trucks,
bus and trailer are too big for the Presqu'ile); vehicle13, a 1 850 kg saloon with a light bar,
is the police car (`PoliceVehicleClass`). Missing classes are skipped, so nothing changes before
the script has run on the PC. The Vehicle Variety Pack is not usable as is: its Blueprints derive
from PhysX's WheeledVehicle, gone in 5.8.

In the full City Sample project (the pack alone is no longer on the PC), a car's Blueprints are
MassTraffic display actors (`CitySampleMassVehicleBase`, an actor, not a Chaos vehicle). The script
then takes a Chaos Blueprint of that car if there is one; otherwise it reads the body's skeletal
mesh from the Blueprint's components and puts the Chaos figures read from the pack on our class
(Chaos wheel AnimBlueprint only: a display actor's is skipped). One failed car no longer stops the
others, and the report is always written.

Cook-safe (cook of candidate 30): City Sample's vehicle AnimBlueprints cast to
`BP_VehicleBase_Deformable`, which needs City Sample gameplay packages we do not have, and its
skeletal meshes carry post-process AnimBlueprints that do not load cleanly. Each of our cars gets a
copy of Epic's skeletal mesh in `/Game/Vehicles/Epic/GTP/Mesh` without that post-process
AnimBlueprint, and the wheel AnimBlueprint copied from the Vehicle template. The report lists, per
car, what it would pull into the cook that is missing or known to break it (`cook_risk`), which
must be empty before a package; `place_masstraffic.py` does the same for the traffic spawners.

One physics body (candidate 31): in traffic, City Sample's cars were in contact on every frame
(551 000 ignored impact lines in 2 min, Chaos solver at 32 ms, frame p95 416 ms). Suspected, not
yet proven: their physics assets bring bodies of their own besides the chassis (the report's body
count came back empty). `AGTPVehicle` now keeps only the
chassis body, the one Chaos drives, and switches the other bodies and every constraint off, at
begin play and every second after (`-CarAllBodies` keeps them). The log says it once per class
("vehicle physics: ... keeps its chassis body", with the count: no line means one body, and the
cause is elsewhere). Impacts the car ignores are logged once every 2 s
at most, with their count, and two bodies of one car touching are no impact. The chase camera's
spring arm only ticks on the player's car. Candidate 31b measured it (PC, 14/11): the City Sample
cars carry 29 to 32 bodies and 37 to 40 constraints each (PA_Collision_vehCar_vehicle*); with the
chassis alone, 91 ignored impacts in 2 min instead of 551 116, game p50 12.9 ms and p95 21.9 ms
instead of 22 and 416. Then, for margin: a car's skeletal mesh only evaluates its pose when
rendered, with Unreal's update rate optimisations, and an engine sound the voice budget silenced
stops ticking.

What the Clio, 208 and Kangoo take from City Sample's city car (vehicle02, 1 200 kg): soft
springs on a high preload with long travel (100 / 300, +10 / -17 cm instead of 250 / 50,
+8 / -10) so the body rolls and dives, 45 degree steering (38 before) for tighter U-turns, and
3 500 N.m handbrake. Kept ours: brakes (stronger), gearbox (shorter first gear), grip. The
player's arcade torque is now relative to the car: 240 N.m for the 1 150 kg Clio, as much per
kilogram for a heavier car, its own torque for a car stronger than that (by mass since 16/11, see 8.7).

### 8.7 Grip of the City Sample cars (16/11)

Police's smoke of candidate 43: the stolen Car05 does 3 to 8 km/h for 4 s in first gear at 4 300 to
4 850 rpm, and 49 km/h after 15 s in fourth at 3 568 rpm, which is 68 km/h at the driven wheels; the
police car averages 2.1 km/h. The driven wheels turn much faster than the car moves.

Read in Epic's Chaos code (`FSimpleWheelSim::Simulate`, `UChaosWheeledVehicleSimulation`): a wheel
pushes with at most its load times the ground's friction (its physical material) times its
`FrictionForceMultiplier`; the load is half the spring's force plus half the static share of the
car's weight (`WheelLoadRatio` 0.5), and that static share comes from the springs' positions around
the component's origin, not around the centre of mass (`ComputeSprungMasses`). The driven wheels are
set by the differential (`DifferentialSetup`), whatever `bAffectedByEngine` says, for a wheel with
an axle. Our City Sample cars are built from their parts (`make_epic_vehicles.py`, City Sample's car
Blueprints need its game module): front wheel drive from `AGTPVehicle`, Epic's figures for mass,
torque, gears and wheels, nothing for the differential, the centre of mass or the torque curve. The
player gets 240 N.m times the car's torque over 130: 785 N.m for Car05 (425), 1 292 for Police13.

Candidates, none proven: torque beyond the grip; little load on the front wheels (origin or centre of
mass far back, weight going to the rear when pulling away); a low friction on our road surface; the
chassis body resting on or scraping the ground. The drive probe tells them apart in one run
(`AGTPVehicle::ProbeDrive`, on unless `-NoDriveProbe`):
- "drive config" once per class: mass, differential and powered wheels, wheel figures, where the
  origin and the centre of mass sit between the axles, the chassis box's height above the ground;
- "drive probe" every second for the player's car (every 3 s, 40 lines, for a police car): pedals,
  gear, rpm, torque, the share of the weight the springs carry, the chassis' ground contacts, and
  per wheel its contact, spring force, drive torque, own speed, slip, and the ground's material and
  friction;
- "0-50 km/h in", "0-100 km/h in" from a standstill with the throttle held, and the top speed.

`tools/vehicles/unreal/dump_vehicle_chaos.py` (read only) dumps the Chaos tuning of our cars and of
City Sample's car and wheel Blueprints copied in the project, with their parent class when they do
not load.

Probe of candidate 44 (bac7e06, PC, 16/11): the springs carry 0 to 50 % of the weight (Police13 mostly
under 10 %), the driven front wheels slip all the time (slip 280 to 1 160 for a threshold of 20), and
Chaos does not shift up while a wheel slips, so the police cars stay in first gear, 72.7 km/h at most.
The dump shows that the figures are not City Sample's: all seven cars run on `UGTPWheelFront` /
`UGTPWheelRear`, the city car's wheel (31 cm, spring 100, +10 / -17 cm, friction 3.0), not on the
wheel Blueprints `make_epic_vehicles.py` wrote for each car (35.3 cm and spring 260 for Car05): the
wheel classes set on the wheel setups as they were read did not reach the saved Blueprints. And all
seven are front wheel drive, where City Sample's vehicle03, 05, 12 and 13 keep Chaos' rear wheel
drive (only vehicle02, 07 and the van01 set front wheel drive).

Fix, not yet measured: `make_epic_vehicles.py` gives each car's wheel Blueprints every figure of City
Sample's own wheel Blueprints for that car (`WHEEL_COPY`; those load, unlike its cars), writes each
wheel setup back into the array, sets City Sample's differential, and its report says where each
wheel came from and which wheel classes and differential the saved Blueprint holds. The cars must be
built again on the PC. The "drive config" line names each wheel's class; the chassis height is read
on static geometry only (the visibility trace could stop on the seated driver). `-DriveLaunchTest`,
with the game started in the car (`-PlayerPawn=<car class>`), waits 3 s then drives straight at full
throttle for 25 s: 0-50, 0-100, top speed and distance ("drive launch test"). Since candidate 49 the
car is first put on the longest clear straight of the map (`PlaceForLaunchTest`: lanes within 1.5 m
of a line, on through the next lane that keeps the heading; a car-wide box swept 0.45 to 1.55 m
above the road finds what stands on it), held on the line, and measured up to half a second and
15 m before its end, then brakes; the line adds 0-130, the speed every 100 m, where the run ended on
the straight and the torque, mass and centre of mass. Run it with `-Pedestrians=0 -Police=0
-StreetParked=0`: the box does not see pawns nor vehicles. The PC's first run (candidate 49, Car05)
found 181 m clear, the longest lane line of the map, and ended at once at 0 km/h: the car was no
longer on that straight when the launch started, and braking to a stop before its end left too
little of it for 100 km/h. Where the car stands is now logged as the launch starts ("drive launch
test: car at ..., last placed by ..."); a car off the straight is put on it again, twice at most.
The packaged run of the candidate 49 gate found it back at the player start three times out of
three: the solver undoes the teleport of a simulated car, as GTPDemoRoute's chase relocation found on
16/11. The car is now moved with its physics off and held there half a second before the physics is
turned back on (line "drive launch test: physics back on at ..."), as that relocation does. For
0-130 km/h, Car05 at 480 N.m needs about 400 m (estimate from the 94 km/h after 112 m of that run, not
measured); the map's longest lane line within 1.5 m of a straight line is 181 m.

Gate 59 (b42ea35, packaged, Car05 at 480 N.m and 2 300 kg): 0-50 km/h in 2.4 s, 0-100 in 7.9 s, 103 km/h
after 148 m in 8.3 s, at which point the run ends, 15 m and half a second before the end of the 181 m
straight, still gaining about 2 m/s2. The peak is the straight's, not the car's, and short of the
105 to 135 km/h of milestone A. The 0-100 time goes as the inverse of the torque (4.8 s at 785 N.m at
candidate 47 in the editor, 7.9 s at 480: 3 770 and 3 790 N.m.s). Candidate 62 gives the player's car 300 N.m per
1 150 kg instead of 240 (600 N.m for Car05): about 6.3 s to 100 km/h by that rule, and about 110 to
115 km/h at the end of the same straight by a simple model fitted to the gate 59 run (not measured).

Candidate 45 (adaf9c1, PC, 16/11): every car has its own wheels and City Sample's differential
(Car03, 05, 12 and Police13 rear wheel drive), `-GTARun` 7/7 with a 154 m chase. Car05 at full
throttle: 0-50 km/h in 4.4 s, gears 1 to 4, 70 km/h after 10.4 s (the run stopped at 11.4 s). Left: once
moving the springs carry 1 to 21 % of the weight, the chassis box is 2 to 7 cm under the road and the
front left wheel loses contact at times, so the chassis still rests on the road. To find which body
does it, `KeepChassisBodyOnly` logs once per class every body of the physics asset ("vehicle
collision": bone, shapes, from how high to how high above the wheels' contact plane, against the
suspension's raise), and "drive config" gives the chassis collision's height above that plane.

Candidate 46 (acc4a66, PC, 16/11): no collision body reaches below the wheels' contact plane (the
lowest, the chassis, sits 22 to 36 cm above it), but "drive config" puts all four wheels at X 0 Z 0,
wheelbase 0 cm. City Sample's car meshes have no `Phys_Wheel_*` bones: Chaos places a wheel at its
bone's reference pose position (`LocateBoneOffset`), and a bone the mesh lacks reads as the origin, so
the four springs of every City Sample car sat at one point under it since 13/11. Car05 at full
throttle: 0-50 km/h in 4.4 s, 71 km/h at most, 143 m before an obstacle, seconds with no wheel down.
Fix, not yet measured: `AGTPVehicle::FindWheelBones`, in a game world before the components register,
takes the four wheel bones from the mesh when a wheel setup's bone is missing: bones named like a
wheel (wheel, whl, tire, tyre; not steer or spare) at the four corners of their reference pose, a
"phys" bone first, then the deepest one; once per class, a "vehicle wheels" line names them, or lists
the mesh's bones when none fit.

Candidate 47 (82c56ff, PC, 16/11): the seven classes find their bones (`wheel_front_l`, `wheel_front_r`,
`wheel_rear_no_spin_l`, `wheel_rear_no_spin_r`), wheelbase 281 cm for Car05, 297 for Police13. Car05
at full throttle: 0-50 km/h in 1.4 s, 0-100 in 4.8 s, 111 km/h before an obstacle after 112 m; the
chase drives 320 m, 83 km/h at most. Two faults left. Police13 lifts its nose: its centre of mass sits
at 0.29 of the wheelbase from the rear axle and a front wheel is off the ground in 147 probes out of
160 (4.6 km/h on average). And the player's car is faster than any real saloon. Fixes, not yet
measured: the centre of mass of a City Sample car goes over the chassis body's origin, where Chaos
shares the weight between the springs (`ComputeSprungMasses`), at its own height
(`CentreMassOverOrigin`, "vehicle balance" line); the player's arcade torque is the Clio's per
kilogram (240 N.m for 1 150 kg), or the car's own torque when that is higher: 480 N.m for Car05
instead of 785, 700 for Police13 instead of 1 292, 500 for Car03 and Car12; our Clio keeps 240, the 208 and the Kangoo get 240 too (258 and 369 before:
their Chaos mass is the class's 1 150 kg).

Traffic at candidate 47 (PC, 19/11, same map, mass pass of 310 s): 47.8 cars moving instead of 102.0
at candidate 46, 86 stopped (64 behind another car). MassTraffic simulates the cars near the player
with a simple physics it extracts from one of our cars spawned for that (`PhysicsVehicleTemplateActor`,
`place_masstraffic.py`): with the wheels at the origin until 46, with the real wheels and City Sample's
centre of mass far back from 47. Likely, not proven: those cars lift their nose and stall as Police13
did. The centre of mass is now moved before the vehicle is created (`FindWheelBones`, from the volume
weighted centre of the chassis body's shapes, "vehicle balance ... set before the vehicle is created"),
so that MassTraffic's template gets it too; set from the first tick (candidate 48), it reached only
the actors.

Candidate 48 (19/11): 53.3 cars moving, 75 stopped (50 behind another car); without parked cars
(`-StreetParked=0`, candidate 47) 44.3: neither the centre of mass nor the parked cars explain the
drop. Stopgap: the car MassTraffic spawns to mine its physics from
(`UMassTrafficSubsystem::GetOrExtractVehiclePhysicsTemplate`: default spawn parameters, so at the
origin, with no owner and no instigator, which none of our spawns uses) is recognised
(`IsPhysicsTemplateSpawn`) and left exactly as at candidate 46: no wheel bones found, no centre of
mass moved, one line per class "vehicle physics template: ... left as at candidate 46". The cars
driven by the player and the police keep their real wheels. Still to explain after the gate: how
the real wheels make MassTraffic's simple physics stop its cars.

Candidate 48 gate (2ff03c2, packaged): Car05 0-50 km/h in 2.4 s, top 91 km/h, stopped by an obstacle
after 112 m, no 0-100 (47 in the editor: 1.4 s, 4.8 s, 111 km/h). The 47 figure was taken with the
785 N.m arcade torque, the 48 one with 480 N.m (torque by mass): 1.4 s x 785 / 480 = 2.3 s, the gap
is the torque, not the packaged game (not measured apart). MassTraffic's simple physics
(`UMassTrafficVehiclePhysicsProcessor`) traces its suspensions against the plane of the car's lane,
applies tyre and suspension forces at the wheel positions mined from the template and holds the car
near its lane (`ClampLateralDeviation`); `-MassTemplateWheels` gives the template the real wheels
again, and "MassTraffic physics template" / "MassTraffic physics car" lines (GTPMassTraffic, with each
count) give what was mined and how the 6 nearest cars drive: speed, inputs, gear, wheels on the road,
height and place against the lane.

Those lines, read in the candidate 49 mass pass (PC, 19/11), show that the stopgap's 97 cars
"moving" hide a broken medium distance traffic: the template left as at candidate 46 gives
MassTraffic's simple physics its four wheels at one point, the origin, under a centre of mass 63 to
93 cm higher. The car stands on one spring like a pencil on its tip and falls over: 5 of the 6 cars
probed 51 to 67 m from the player lay on their side (roll 102 to 174 deg), no wheel on the road, full
throttle and full lock, still over 1 m/s and so counted as moving; 44 of the 69 cars under that
physics had a wheel off the road at 295 s. The "physics cars" line now also counts the cars lying
over (roof more than 45 deg from the vertical). The real wheels with City Sample's centre of mass
(`-MassTemplateWheels`) are measured in a 300 s mass pass at the candidate 51 gate; the stopgap
should give way to them as soon as they keep the traffic moving. After gate 54 (branch
`fix/masstraffic-no-simple-physics`, see traffic-mass.md) the template gets the real wheels and City
Sample's centre of mass by default, though no car is given that physics any more unless
`-MassTrafficPhysicsRadius=<m>` (line "vehicle physics template: ... given the real wheels and
City Sample's centre of mass"), `-MassTemplate46` gives back the stopgap.

Since candidate 52 the "MassTraffic cars" line counts as `moving=` only the upright cars (roof within
45 deg of the vertical) and gives the old count beside it (`moving_with_lying=`, `lying_over=`); a line
"MassTraffic lying over" gives the cars lying over within 100 m of the player, in number and share.
`-MassTrafficPicture[=150,290]` takes, at those times, a picture of a traffic car 50 to 70 m from the
player (the one lying over the most, else the one nearest 60 m), seen from where the player stands or
else 60 m behind or ahead of it (`Saved/Screenshots/<platform>/mass-traffic-<n>.png`), and logs the
car, its distances, roll and pitch ("MassTraffic picture").

Police cars turned over in the candidate 48 chase stayed on their side: the "put back on its wheels"
rule ran for the player's car only. It now runs for the cars driven by the AI too (`PutBackOnWheels`,
line "put back on its wheels at ... (AI driver)"). The centre of mass of City Sample's cars is set in
their Blueprints or physics assets, which our project cannot load (their parent classes are in City
Sample's game module): `tools/vehicles/read_uasset_props.py FOLDER` reads, without the editor, the
centre of mass override, COMNudge, inertia scale and mass override those packages store.

Read on the PC on 19/11 (bytes at the property's place, still to be confirmed in City Sample's
editor), City Sample's Sandbox Blueprints put the centre of mass, in cm in the body's frame, at
(26, 0, 56) for vehicle03, vehicle12 and vehicle13 (Police13), (38, 0, 60) for vehicle05, (0, 0, 54)
for vehicle02, (30, 0, 50) for vehicle07 and (33, 0, 40) for the van; their COMNudge is zero (the
deformable parent's (10, 0, -15) is overridden). We had it over the origin at the chassis shapes'
height: (0, 0, 83) for Police13, 27 cm higher and 26 cm further back, which lifts the nose under
throttle and lays the car down in turns. `FindWheelBones` now sets City Sample's value for each
`BP_GTP_*` car before the vehicle is created (line "vehicle balance: ... City Sample's own override"),
and `make_epic_vehicles.py` writes it in the Blueprints it builds (`com` in `TUNING`). MassTraffic's
template still keeps candidate 46's setting unless `-MassTemplateWheels`: that switch now tests the
real wheels with City Sample's centre of mass, the candidate for one setting shared by traffic, player
and police.

### 8.8 City Sample cars drawn, traffic during the escape (23/11)

Invisible cars, seen at gate 67: the player steals a Car05 and drives it for a minute without a car
around them (38 to 61 km/h on the speedometer, Nicolito sliding upright), and no police car is seen on
any image of gates 51 to 68. Cause, read in `epic_vehicles_report.json` and in Police's
`police_look_report.txt` (PC): City Sample's car Blueprints are MassTraffic display actors whose
components the script cannot read, so `make_epic_vehicles.py` took the skeletal mesh with the shortest
name, `SKM_veh..._vehicleNN`, for all seven cars. That mesh is the rig: the bones, the physics asset,
one slot `lambert1` without a material and a box of 0 x 0 x 0 cm. The body is drawn by
`SKM_Exterior_...` (paint, tyres, windscreen, 9 slots) and `SKM_Destructible_...` (glass, lights,
light bar, 22 slots), next to it.

- `AGTPVehicle::LookMeshes`: the meshes that draw the car. Each becomes a component on the body
  without collision, posed by it (leader pose, as the pedestrians' parts), with its own LOD. The rig
  stays the body (physics, wheel bones, unchanged driving). Since the rig is never rendered, its pose
  is refreshed while a look part was drawn in the last second or the car is near (traffic), always for
  a car outside the traffic; the traffic's shadow and LOD levers apply to the look parts too.
- `make_epic_vehicles.py` copies both meshes to `/Game/Vehicles/Epic/GTP/Mesh/SKM_GTP_<Name>_Exterior`
  and `_Destructible` without their post-process AnimBlueprint and fills `LookMeshes`, only when the
  body's box is under 1 m. Report: `body_box_cm`, `look`, `drawn` per car, `not_drawn` (must be empty
  before a package).
- Log: "vehicle look: <class> drawn | NOT DRAWN, N look part(s) built: <each mesh, its box, its
  slots>" once per class at its first spawn; "traffic: car class <class> drawn, in the traffic | NOT
  DRAWN, left out of the traffic and the parked cars" for each traffic class at begin play. A class that
  draws nothing is left out of the traffic, the street parked cars and the police (a traffic car
  stands in, "police: car class ... NOT DRAWN"); when no class is drawn, all are kept.
- The script's run on the PC (candidate 72) logged 34 Error lines and the job failed, none from our
  assets (`cook_risk` empty for the seven cars, no post-process AnimBlueprint on the 14 look parts): 12
  from loading City Sample's MassTraffic display actors (their `ABP_Proxy_Post_*`, "Unknown structure",
  and `ABP_vehCar_vehicle05`), 22 "FindAssetData failed" for City Sample packages not copied (BrokenGlass,
  `CAS_*`, `NS_FlashingLight`) looked up while finding the wheels. The script now reads the asset
  registry for those lookups and loads a Blueprint only when the registry says it is a Chaos vehicle or
  a wheel. The look parts are on City Sample's `SK_Proxy_*` skeletons, the rig on `SK_veh*`: the leader
  pose matches bones by name, unchecked.
- Not proven: whether the exterior's tyres follow the wheel bones (it is on the same skeleton
  `SK_Vehicle`, given in the report), the draw cost of real bodies on 6 classes of 9 (the gates
  until 68 measured invisible cars), and the look of City Sample's materials once cooked.

Traffic during the escape (gate 67: 7.1 cars moving within 150 m for a threshold of 8; 68, same
traffic code, 10.1). Two causes in `legacy-traffic-lines-gtarun.txt`:

- At 83 s, GTARun moved the player 700 m: every car was taken away as far, and the cars line, read
  in that same tick, said "driven=0 of 22". The pool gave them back 4 a tick, once cooled down for 2 s.
  Now a jump (more than `MaxCarDistance` since the last tick with a player) refills from the pool
  at once for `PoolCooldown` + 1 s ("traffic: the player moved N m in one tick"); the cooldown counts
  from when the car's physics went off (a far ghost pooled has cooled down already); a reused car
  starts rolling at the lane's speed (30 km/h at most) instead of standing; the cars line is read at
  the end of the tick.
- From 103 s, a Kangoo stood 1.3 m behind a street parked Car07, and 4 cars waited there in turn, each
  until taken away as stuck (35 s), with 9 to 16 behind them (moving within 150 m: 5 at 114 s, 1 at
  124 s). The Kangoo had been pooled in panic and came back three times still panicking: a car put back
  on a lane now starts calm (`UGTPAIDriverComponent::Start` resets its reactions). A street parked car of
  the keeper that a traffic car has waited behind for 8 s (`StreetParkedBlockSeconds`) is taken away
  once out of sight ("traffic: street parked ... taken away out of sight, in the way"), kept and logged
  once while in view; cars placed on request (GTARun) are never taken. Not proven: why that parked car
  stood in the Kangoo's way (placement checks passed: no lane under its body).

### 8.9 The flight keeps to the main axes (24/11)

Gate 75's filmed flight (`-GTARun -GTARunEncore`) averaged 24 to 34 km/h. On its frames: from 22 to
28 s, turning off the quai Jean Moulin, the car cut the corner onto a traffic light post and stood
there; at 29 s the script put it on another lane (held up twice in the same place), a narrow street
driven at 57 to 72 km/h, where it was wedged between two walls from 39 to 46 s before being put
elsewhere again; from 72 to 95 s it stood against a white block at the corner of a square, then the
run's outcome step stopped it (the player gives up) until the arrest. The chase script knew nothing
of the width of a street: at a junction it took the straightest exit among those with 250 m of open
road (6 junctions ahead), and it put a car held up or at the end of the lanes on a lane drawn at
random 80 to 150 m away.

The filmed pass's log, read against the 235 lanes the game reads (`L_City_RueDeLaRepublique`), shows
fixed obstacles of the map (props with a hierarchical instanced static mesh, collision on) standing
on five lanes: `StaticMeshActor_27`, the white block, 0.5 m from the middle of the rue de la Bourse
(lane 13); `StaticMeshActor_107` on the way from the quai Jean Moulin to the rue Gentil (lanes 87 and
211), met 15 times in 2.5 km by the flight of until gate 75, which went round that block;
`StaticMeshActor_22` on the rue Claudia (lane 4); `StaticMeshActor_0` on the rue Antoine Salles
(lane 62). Those belong to the dressing of the map (Réalisme): to be moved off the lanes.

- `GTPRoadWidth.h`, outside Unreal (`tools/vehicles/wanted_sim/road_width_test.cpp`, in CI):
  - the carriageway around a lane: the `width_m` of the lanes of the same BD TOPO road (`road_id`)
    within 20 m of its middle, both directions. Under 5 m a narrow street, 7 m and over a main axis.
    On the 235 lanes: 130 main axis, 46 middle, 59 narrow. Both columns were already in the lanes
    file: nothing to regenerate;
  - the lanes that never end: from them some way goes on for ever without a dead end or a blocked
    lane (the dead ends peeled off, then the lanes that lead only to them, and so on): 103 of 235;
  - the worth of the way on from an exit (`WayQuality`): its best way within 500 m and 10
    junctions, each metre weighted by width (1 on a main axis, 0.7 in a middle street, 0.25 in a
    narrow street), halved on the last 25 lanes driven so the flight does not circle one block, and
    stopped at a blocked lane;
  - the exit taken at a junction (`FGTPLaneFollower::bPreferStraight`, used by the chase script
    only): never a blocked one when another exists; one that never ends first; then the best way
    on; then the straightest. A first version (an exit that never ends, then the widest exit) did
    worse on the 235 lanes than the old choice: 20 % on main axes, 61 % in narrow streets;
  - the speed of the flight from 12 s: 70 km/h on a main axis, 60 in a middle street or on a road of
    unknown width, 45 in a narrow street (90 everywhere until gate 75);
  - where a car is put (`UGTPDemoRoute::RelocateChase`), pass after pass: a main axis that never
    ends, a middle street that never ends, any lane that never ends, then as before; never a blocked
    lane, and but for the last pass never one driven lately.
- Held up on its lane (within 3 m of its path) against something that does not move (no simulated
  body, not a pawn, within 3 m ahead, within 3.5 m of the lane): the lane is marked blocked
  (`AGTPTrafficNetwork::MarkLaneBlocked`, the lanes that never end are found again without it) and
  the car is put on another lane at once. Off its lane, the car backs off at once instead of pushing
  on it for 3 s.
- Logs: "traffic: lanes for a flight: N of M never end (no dead end ahead); carriageway: ..." at
  begin play; "traffic: lane N blocked by ... flights keep off it" once per lane; the relocation
  line gives "never ends", the carriageway, its class and the pass; the "held up" line ends with ",
  does not move: backing off" when it does.
- Replayed with the game's own rules on the 235 lanes (`road_width_test <lanes.csv>
  [x_cm,y_cm|all] [[learn=]blocked lanes]`), 2.5 km from the carjack of -GTARun on the quai Jean
  Moulin: until gate 75, 49 % on main axes, 51 % in middle streets, round the block of the rue
  Gentil; now 72 % on main axes, 19 % middle, 8 % narrow (quais Jean Moulin and Jules Courmont, rue
  Saint-Bonaventure, rue Grolée, place des Cordeliers), no dead end. The five blocked lanes met first
  then learnt: 2 meetings (lanes 211 and 87, 190 m from the carjack) instead of 15; known from the
  start: 65 %, 23 %, 12 % and none. From each of the 103 lanes that never end: 56 % on main axes
  (45 % until gate 75), no dead end (4 until then), 19 meetings with the blocked lanes (260 until then).
- Not proven: in the game. Until `StaticMeshActor_107` is moved, the flight of -GTARun is expected
  to meet it once about 190 m after the carjack and be put on another lane there, which the pursuit
  step counts as help. The police's part of the flight (gate 75's measured pass: units at 23 km/h on
  average for 25 wanted) belongs to the Police thread.

## 9. Making a drivable car

### 9.1 First test car (no Tripo)

For the first compile, the car of Epic's *Vehicle* project template
(engine content, kept locally out of Git like the GASP) is enough:

1. Copy `SportsCar` from the *Vehicle* template into `Content/Vehicles/Test/`.
2. Create `BP_TestCar`, child of `AGTPVehicle`: mesh `SKM_SportsCar`, its
   Animation Blueprint (*Wheel Controller* node).
3. Run with `-TestVehicle`: the car is parked in front of the player.

That model's wheel bones (`Phys_Wheel_FL`…) are the ones our code expects:
our own cars follow the same convention.

### 9.2 Game cars (Tripo → Blender → Unreal)

| Step | Convention |
| ---- | ---- |
| Tripo master | whole car, real scale, no logo (applied later as a decal); reference images of the real model |
| Blender split | body, 4 separate wheels, glass as separate objects (windscreen, rear window, left windows, right windows) |
| Skeleton | `root` on the ground at the centre, wheel bones `Phys_Wheel_FL`, `Phys_Wheel_FR`, `Phys_Wheel_BL`, `Phys_Wheel_BR` at each wheel centre, X axis forward |
| Morph targets | `Dent_Front`, `Dent_Rear`, `Dent_Left`, `Dent_Right` (sculpted once, reusable per silhouette) |
| Sockets | `seat_driver` at the driver's hip |
| Budget | art bible: ≤ 1.5 M triangles in Ultra, 120 k in M1, 4 LODs, 4K / 2K textures |
| Unreal | `Content/Vehicles/<Model>/`: `SKM_`, `PA_` (physics asset), `ABP_` (Wheel Controller), `BP_` child of `AGTPVehicle` with tagged glass |

The script `tools/vehicles/prepare_vehicle.py` does all of this without
Unreal, in Blender or with the `bpy` Python module (5.2):

```
python -I tools/vehicles/prepare_vehicle.py -- --master assets/tripo/v/V-01.glb \
    --id V-01 --out assets/staging/vehicles --preview
```

Per-model settings in `tools/vehicles/vehicles.json` (real length, tyre
radius and width, seat). Output in `assets/staging/vehicles/<id>/` (out of
Git): `SKM_<Name>.fbx` (body + wheels, skeleton, 4 dents, 1.36 M
triangles), `SM_<Name>_Glass_<Zone>.fbx`, 4K textures, `report.json` and
two previews (wheels in red, glass in blue).

First run (2026-11-09) on Clio V, 208 II and Kangoo II: front detected
automatically, 4 wheels found and cut cleanly on all three, about 2 min per
car. Known limits:

- **Glass**: Tripo paints the windows with the body's reflections (same
  colour, roughness and metal), so the texture cannot tell them apart. By
  default the script takes the whole upper part (above the belt line,
  without the roof). For a priority car, `vehicles.json` can give the real
  window outlines (`"mode": "outline"`: polygons in top view for the
  windscreen and rear window, in side view for the side windows); the Clio V
  uses it (2026-11-09), mirrors and pillars stay body. Real transparency
  needs a cabin interior (later).
- **Proportions**: at real length, the masters are ~13 % too tall. When
  `height_m` is set, the script compresses only what is above the wheels
  (2.15 wheel radii), so the wheels stay round: Clio V 1.46 m (real 1.44 m).
- **Dents**: the four morph targets are exported at weight 0 (the car
  starts undamaged).
- **M1 profile LODs**: generated by Unreal's skeletal mesh reduction on
  import (LOD 1 ≈ 120 k triangles); Blender's decimation breaks the shading
  of these surfaces at that ratio.
- Logos: painted into the texture by Tripo, kept; their decal version out of
  Git is still to do.

## 10. Models to generate (proposal sent to the Tripo thread)

The fleet on French roads, not only French makes. Shares give the target
mix for traffic and parked streets (estimate from the 2025 fleet, to refine).

### Priority 1: drivable in 0.3 (3 models)

| Id | Model | Why |
| ---- | ---- | ---- |
| V-01 | Renault Clio V (2019+) | most common car in France |
| V-02 | Peugeot 208 II (2019+) | second in the recent fleet |
| V-03 | Renault Kangoo II (tradesman, white) | the van of every Lyon street |

### Priority 2: traffic and parked cars (0.7)

| Id | Model | Target share |
| ---- | ---- | ---- |
| V-04 | Dacia Sandero III | 6 % |
| V-05 | Citroën C3 III | 5 % |
| V-06 | Peugeot 308 II | 5 % |
| V-07 | Renault Captur II | 4 % |
| V-08 | Peugeot 3008 II | 4 % |
| V-09 | Volkswagen Golf VII | 4 % |
| V-10 | Toyota Yaris IV (hybrid) | 3 % |
| V-11 | Volkswagen Polo VI | 3 % |
| V-12 | Renault Twingo III | 3 % |
| V-13 | Fiat 500 | 3 % |
| V-14 | Renault Mégane IV | 3 % |
| V-15 | Dacia Duster II | 3 % |
| V-16 | Mercedes A-Class (W177) | 2 % |
| V-17 | BMW 3 Series (G20) | 2 % |
| V-18 | Tesla Model Y | 2 % |
| V-19 | Citroën Berlingo III (van) | 3 % |
| V-20 | Renault Trafic III | 2 % |
| V-21 | Peugeot Boxer / Citroën Jumper | 2 % |
| V-22 | Taxi: Toyota Corolla hybrid estate ("TAXI" roof sign) | Lyon taxis |

Clio and 208 together make about 20 %; the rest (≈ 20 %) is covered by
colour and model-year variants of the models above.

### Priority 3: city vehicles and two-wheelers (0.6 to 0.16)

| Id | Model | Version |
| ---- | ---- | ---- |
| V-23 | TCL articulated bus (Heuliez GX 437 type, TCL livery) | 0.7 |
| V-24 | TCL trolleybus (Hess lighTram type, C lines) | 0.7 |
| V-25 | Peugeot 5008 Police nationale (livery) | 0.6 |
| V-26 | Renault Mégane / Kangoo Lyon municipal police | 0.6 |
| V-27 | SDMIS fire brigade ambulance van (VSAV) | 0.16 |
| V-28 | Renault Kangoo La Poste (yellow) | 0.7 |
| V-29 | Métropole refuse truck | 0.7 |
| V-30 | Vélo'v (Lyon bike share) | 0.13 |
| V-31 | Shared e-scooter | 0.13 |
| V-32 | Piaggio Liberty / Vespa scooter | 0.13 |
| V-33 | Yamaha TMAX | 0.13 |
| V-34 | Delivery cargo bike | 0.13 |

Estimated cost at the current Studio rate (55 credits per master, no
variants): 34 models ≈ 1,870 credits. For V-01 to V-03, ask Tripo for the
whole car with reference images; wheels and glass are split in Blender
(section 9.2).

## 11. Tests and expected evidence

- Automatic playtest (like `tools/level/playtest_street.py`): enter, drive
  30 s straight then through a turn, exit; log of speeds and states.
- 3-minute video in the Shipping build (recorded by the game), frame-time
  CSV: 95 % of frames ≤ 33.3 ms while driving (M1 profile).
- Hand checks by the thread: AZERTY keyboard, gamepad, exit blocked by a
  wall, bail out at 60 km/h, broken window on a locked car.

## 12. Open points

- **First run in game**: the code compiles (2026-11-09); the Chaos
  settings (engine, gearbox, wheels) still have to be proven by driving.
- Enter / exit / seated animations: to find (the GASP has none); until
  then the character is hidden at the wheel.
- Chaos Vehicles is still marked experimental in 5.8.2: to watch, with a
  possible fallback to *Mover* later.
- Traffic cost on the M1: 15 moving vehicles planned by the art bible; to
  measure in 0.7.
