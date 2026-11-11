# Cost of AI and police cars, and how to go past 3 + 3

*French version: [vehicle-cost.fr.md](vehicle-cost.fr.md)*

Status: analysis and a first switchable step (2026-11-10). Nothing here is
measured per component yet: the numbers marked *estimate* come from the
known measures and from reading the code, and the bench decides.

## 1. What one car costs

Known measures: before the 0.3 optimisations, 6 AI cars with drivers added
about 10 ms of GPU time (about 1.7 ms per car, 0.3 thread, M1 profile); the
provisional budget of ADR-0002 is 0.17 ms per car; the 0.3.0 release, with
all systems on, measured p95 44.5 ms for a target of 42 ms.

| Part | Side | What it is | Cost driver | Already done |
|---|---|---|---|---|
| Body mesh | GPU | 1.5 M triangles at LOD 0, skinned (wheel bones) | triangles x skinning, Substrate materials | forced far LOD beyond 25 m |
| Shadows | GPU | moving caster: its virtual shadow map pages are redrawn every frame | number of pages touched | no shadow beyond 25 m; lights never cast shadows |
| Lights | GPU | head, tail, brake lights (3-5 per car), police: 2 more | lit pixels x lights | unshadowed |
| Driver | GPU | skinned character at the wheel | skinning, shadow | hidden, no pose update, no shadow |
| Chaos physics | CPU | 4 wheel traces, suspension, 3 sub-steps | per car per frame | none |
| AI decision | CPU | obstacle sweep, queue along lanes, lights, junctions | per car per decision | none |
| Animation | CPU | wheel animation blueprint | per car per frame | none |
| Drawing | CPU | 4 glass meshes + body + wheels, draw calls | per visible car | not drawn beyond 150 m |

The GPU side is the one that limits the M1 (frame time is GPU bound in all
measures), so it comes first; the CPU side matters once there are 12 cars or
more.

## 2. How to go past 3 traffic and 3 police cars

| Idea | Side | Expected gain (estimate) | Risk | State |
|---|---|---|---|---|
| Driver decisions by distance: every frame under 35 m, 10 Hz under 90 m, 4 Hz beyond | CPU | 60 to 80 % of the sweeps of far cars, about 0.1 to 0.3 ms at 12 cars | low: pedals keep their last value | done, `-VehicleLod=1` |
| No pose refresh for a car not drawn | CPU | small, 0.05 ms per hidden car | low | done, same switch |
| Police light bar dark beyond 60 m | GPU | about 0.1 ms per police car out of view range | none | done, same switch |
| Far cars as kinematic ghosts: physics off beyond 90 m or out of sight, the car slides along its lane | CPU | the whole Chaos cost of far cars, about 0.2 ms per car | medium: re-enabling physics near the player, queues and junctions must keep working | proposed, test after the bench |
| Impostor mesh (LOD 3, about 2 k triangles) beyond 60 m | GPU | about 0.3 ms per far visible car | medium: import work (import thread) | proposed |
| Pool cars instead of spawning and destroying | CPU | removes spawn hitches | low | proposed |
| Limit live cars by cost, not by count: far cars out of sight count for one third | both | lets traffic reach 8 to 12 live cars for the cost of 5 | low | proposed |

The ghost mode is the one that decides whether 12 cars is reachable on the
M1, which is why it is next after the first measure.

## 3. The switch and the one-command test

- `-VehicleLod=1` (or `bVehicleLod=True` under `[/Script/GranTheftPraline.GTPTrafficNetwork]`)
  turns the three first rows of section 2 on. Default off until measured.
  Distances: `LodNearDistance` 35 m, `LodMidDistance` 90 m, `LightBarDistance` 60 m.
- `tools/perf/vehicle-count-bench.sh <build_dir>` runs the bench at 3, 6 and 12
  cars, each with the switch off and on (`COUNTS="3 6 12"`, `POLICE=1` adds a
  2-star police chase, `DURATION=90`) and writes `vehicle-cost.md`. It uses
  `tools/perf/bench.sh` (branch feat/perf-bench) and needs the Mac.
- Reading the result: the cost per car is the slope between 3 and 12 cars of
  GPU p50 and of frame p95; the switch is kept on if p95 does not get worse
  and the slope drops.

## 4. Ghost mode and car pool (measured 10 Nov 2026)

- `-VehicleGhost=1` (or `bVehicleGhost=True`): past `GhostDistance` (90 m) from
  the player an AI car turns into a ghost: physics and the Chaos vehicle off,
  it slides along its planned path 20 times a second under the same stops as
  a driven car (bends, the car ahead along the lanes, red lights, junction
  stop lines), on the ground found a few times a second. It is driven again
  inside `GhostWakeDistance` (70 m), at the ghost's speed. No sweep for
  ghosts. Logic in `GTPTrafficGhost.h`, checked by `tools/vehicles/ghost_test`.
- `-TrafficPool=1` (or `bTrafficPool=True`): a car taken away is hidden and
  kept, then reused for the next spawn, instead of destroy and spawn.
- Police cars are driven by the police director, not by an AI driver: they
  never turn into ghosts.
- `tools/perf/vehicle-count-bench.sh` now runs three tiers per count: none,
  `-VehicleLod=1`, then `-VehicleLod=1 -VehicleGhost=1 -TrafficPool=1`.

Measured on 10 Nov 2026 (M1 Pro, profile m1, street-demo, 90 s, build
4c7906b, map 799c8d5 with facade LODs): 12 cars with
`-VehicleLod=1 -VehicleGhost=1 -TrafficPool=1` give frame p50/p95
28.9/38.8 ms, 86 % of frames under 33 ms and no hitch, the same as 3 cars
(28.9/38.2). Without the tiers, 12 cars give 41.5/55.0.

## 5. Parked cars as instances (switch, not measured yet)

- `tools/city/parking.py --map <Map>` writes `Content/City/Traffic/<Map>.parking.csv`:
  one slot every 5.6 m where the measured carriageway leaves 2.1 m between the
  outer lane and the kerb, 15 m away from the junctions, 70 % of the slots
  taken (the same ones at every run). About 1,100 slots on the whole
  presqu'île zone.
- `-ParkedCars=1` (or `bParkedCars=True` on the traffic network) spawns
  `AGTPParkedCars`: one hierarchical instanced static mesh per model
  (`SM_Parked_<Name>_A` from `tools/vehicles/unreal/import_parked_static.py`),
  body colour per instance in `PerInstanceCustomData[0..2]`, no shadows,
  faded out between 120 and 150 m, simple box collision. No physics, no tick.
- Not done yet: swapping a parked instance for a real car when the player
  walks up to it (to steal it).

## 6. Far traffic and the CPU budget per car (switch, not measured yet)

The aim is 25, then 40 vehicles in view without 25 or 40 Chaos cars with a
driver. Past the ghost distance, a car does not need to be an actor at all.

- `-FarTraffic=N` (or `NumFarCars` on the traffic network) creates
  `AGTPFarTraffic`: N more cars past the ghost distance, with no physics and
  no driver at work. The switch turns the ghost mode on. They are drawn in
  one of two ways:
  - puppets (default): cars of the traffic classes, spawned once at the
    start. Each is kept at its lowest LOD, with no shadow, no pose update and
    no collision, hidden past 150 m, and moved by `AGTPFarTraffic`;
  - instances (`-FarInstances=1`): moving instances of the parked car meshes,
    one `InstancedStaticMeshComponent` per model, with no actor at all. This
    needs good `/Game/Vehicles/Parked` meshes; without them the game falls
    back to puppets.
- A far car follows the lanes and takes a random next lane at the end. It
  keeps 8 m to the car ahead on its lane, driven cars included, and stops at
  red lights. Junction claims and the obstacle probe are left out: at that
  distance nobody sees two cars cross. The logic is in `GTPTrafficFar.h`,
  checked by `tools/vehicles/far_test`.
- Handover: a far car that comes within 90 m of the player swaps with the
  farthest driven car, which is a ghost by then. With puppets the two cars
  swap roles: the puppet becomes a driven car (a ghost, waking up at 70 m),
  and the driven car becomes the puppet of that far car. The same car stays
  in view, colour included. With instances, the driven car takes the far
  car's lane, position and speed, and the far car takes the driven car's
  old place. When no driven car can be freed, a far
  car out of sight that comes within 50 m starts again elsewhere. One in
  view keeps going and never vanishes. Out of sight, a far car past 250 m or
  at a dead end also starts again elsewhere, 120 to 250 m away.
- Perceived traffic is the driven cars plus the far cars: palier C (25) is
  `-Traffic=12 -FarTraffic=13`, the target (40) is
  `-Traffic=12 -FarTraffic=28`.
- CPU budget: every 5 s the game logs one line,
  `traffic budget: D driven (G ghosts), P pooled, F far | drivers x ms/frame
  (y us per driver tick), network z ms/frame, far w ms/frame`, Shipping
  included.
- One command: `tools/perf/traffic-perceived-bench.sh <build_dir>` measures
  12 driven cars (palier B reference), 25 and 40 driven cars, then 12+13 and
  12+28 with far traffic, and 16+24 with parked cars. It writes the table
  and the budget lines. Parked cars and `-FarInstances=1` need the
  `/Game/Vehicles/Parked` meshes in the build; puppets do not.
- One Mac slot: from the root of the clone,
  `caffeinate -dimsu tools/perf/traffic-pass-mac.sh` packages, runs a 30 s
  control launch, then each case. Every step runs under a time guard:
  package 50 min, control 3 min, a case 2 x (duration + 60 s). Past its
  limit, a step is killed and the pass stops. A heartbeat line is written
  every minute. `DRY_RUN=1` checks the paths and guards without Unreal.
