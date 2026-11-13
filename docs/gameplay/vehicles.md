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
  same position. Lanes stay within 60 m of a generated building. The same header runs in
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
- Stand-in signal heads (a coloured sphere on a pole at each stop line, no shadow, no collision) until the street furniture model exists. `-TrafficLights=0` turns the lights off, `-SignalHeads=0` hides the heads.
- Simulation, 300 s on the rue de la République lanes: 3, 6 and 12 cars, no contact, no red light run.

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
- **Driving feel, player's car only**: 240 N.m instead of 130 (AI cars keep the real
  figure); handbrake above 20 km/h lowers rear grip and swings the tail with the steering;
  slides without the handbrake are damped so the car straightens; under 35 % health the
  engine loses up to 45 % of its power; a car dented on one side pulls to that side; a car
  on its roof or side, stopped, is put back on its wheels after 2.5 s.
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
  `-StreetParked=N` (0 = none).
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
- **Shooting from a car**: not done; the weapon only fires on foot.

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
