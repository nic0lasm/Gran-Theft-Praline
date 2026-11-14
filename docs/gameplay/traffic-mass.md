# Traffic on MassTraffic (City Sample)

Status 13/11/2026: prepared, not compiled, waiting for City Sample on the PC. Decision: ADR-0015
(pedestrians and traffic on Mass). French: [traffic-mass.fr.md](traffic-mass.fr.md).

## What is ready

- **ZoneGraph of the map**: `tools/city/zonegraph.py` and `tools/city/unreal/place_zonegraph.py`
  already place our driving lanes (tag `Car`, one-way profile) and junction polygons (tags `Car`
  and `Intersection`), as MassTraffic expects.
- **Build**: module `GTPMass` links MassTraffic only when `Plugins/Traffic/Traffic.uplugin` (City
  Sample's plugin "Traffic", module MassTraffic) exists (`WITH_GTP_MASSTRAFFIC`). Without it the game builds as before.
- **Switch**: `-Traffic=mass`. The legacy network (`AGTPTrafficNetwork`) then keeps the lights, the
  street parked cars to steal, the incidents and the police, but drives no car of its own; the
  subsystem `UGTPMassTrafficSubsystem` starts the Mass spawners tagged `GTPMassTraffic`. If the
  plugin is not loaded, `-Traffic=mass` keeps the legacy traffic and says so in the log.
- **Spawners**: `tools/city/unreal/place_masstraffic.py` copies City Sample's intersection and
  driving-car spawners, with every entity config and data of `/Game/AI/Traffic` they reach, to
  `/Game/AI/GTPTraffic` (out of Git, still Epic content), then re-points each reference in the
  copies by reflection (`UGTPAssetReflection`): to our copy, to our car `BP_GTP_<Name>` for the
  physics vehicle MassTraffic spawns near the player (`PhysicsVehicleTemplateActor`, by City
  Sample's car number), or to nothing for what would break the cook (City Sample's car display
  Blueprints, the drivers). The vehicle entity configs of City Sample's driving-car spawner are
  copied too, for the cars we have only (buses, trucks and the rest left out, City Sample's
  proportions kept). City Sample's spawner Blueprints carry no spawn data generator (theirs were on
  the spawners of City Sample's map, which is why no Mass car appeared on candidate 32 although the
  ZoneGraph had 5 635 lanes): each placed spawner gets MassTraffic's own through
  `UGTPMassSpawnerSetup`, and the vehicle types' own lane filters are pointed at our tags. It places
  them (120 driving cars by default, no spawn on begin play) and writes
  `Saved/CityImport/masstraffic_report.json` (`gtp_copy`: each change, the copies' cook risks and
  the chains that pull them in). City Sample's parked-car spawner is not used: its parking spaces
  belong to its own city; our street parked cars stay. `GTP_MASSTRAFFIC_SURVEY=1` changes nothing
  and dumps City Sample's spawners and configs (properties, dependencies, cook risks).

## Installing (PC)

1. City Sample (Fab, free) downloaded; copy its `Plugins/Traffic` and `Plugins/RuleProcessor`
   folders, as they are, into `GranTheftPraline/Plugins/` (ignored by Git: Epic code, not under our
   MIT licence). Traffic also needs ChaosVehiclesPlugin, ZoneGraph, MassAI, MassCrowd, MassGameplay
   and AnimToTexture, already enabled.
   Apply the fixes listed in the README of github.com/Myxcil/MassTraffic-Test (read, nothing copied).
   Then `python tools/vehicles/patch_citysample_traffic.py <project>/GranTheftPraline`: it turns
   off MSVC's false C4723 ("divide by 0") in MassTrafficSubsystem.cpp, which otherwise fails the
   Development package (idempotent, original kept as .orig, md5 report).
   Then `python tools/vehicles/copy_citysample_vehicle_base.py --src <CitySample> --project
   <project>/GranTheftPraline`: City Sample's car Blueprints need the class below.
   The plugin's `Content` folder comes with it: the vehicle materials use its material function
   `MF_UnpackTrafficVehicleInstanceCustomData` (reported missing on candidate 32: check that
   `Plugins/Traffic/Content/MaterialFunctions` is there and that `Traffic.uplugin` says
   `"CanContainContent": true`).
2. `Traffic.uplugin` is not enabled by default: the `.uproject` lists it as `"Optional": true`, so
   it loads where the plugin is copied and is skipped without error elsewhere (Mac, CI).
   RuleProcessor is enabled by default.
3. Build the editor target, run `place_zonegraph.py` then `place_masstraffic.py`. Our ZoneGraph
   tags keep fixed bits (Car 1, Intersection 2, Pedestrian 3, Crossing 4) in every import, and
   MassTraffic's lane filters are pointed at them in C++
   (`UGTPZoneGraphImport::ConfigureTrafficLaneFilters`: neither settings class is exposed to
   Python in 5.8), by the GTPMass module at every engine start, game and package included. No
   config file is written or needed. The log line "traffic lane filters" lists the filters set;
   `place_masstraffic.py` reports an error when none is.
   The engine builds the ZoneGraph data (the lanes MassTraffic drives on) by itself only when
   "build while editing" is on, which it is not: `place_zonegraph.py` then calls
   `UGTPZoneGraphImport::BuildZoneGraphData`, which adds a ZoneGraphData actor to the map if it has
   none and runs the engine's builder (`FZoneGraphBuilder::BuildAll`, forced; the subsystem's own
   rebuild is protected). The lanes are saved with the map. Its report gives `build`: shapes,
   lanes, zones; 0 lanes is an error.
   City Sample's traffic configs and spawner Blueprints live in the City Sample project's own
   content (/Game/AI/Traffic, /Game/AI/Traffic/Spawners, /Game/Vehicle/veh*): they are copied to
   the same /Game paths in our project (out of Git), so their references hold.
4. Read the report: lane filters set and City Sample's spawners placed (intersections, parked
   cars, driving cars, started in that order).
5. Package, launch with `-Traffic=mass`. The log proves what runs: a line "MassTraffic count"
   every 5 s for the first minute, then every 30 s, gives the ZoneGraph data of the map (lanes,
   zones: 0 lanes means the ZoneGraph was not built, so no car can spawn), the cars Mass draws as
   instances and as actors, and the nearest one. `-MassTrafficView` puts the player 12 m from the
   first Mass car seen, for a capture.

## City Sample's car Blueprints

City Sample's car Blueprints (`BP_vehicle` and its children, used by MassTraffic's configs and
spawners) derive from `ACitySampleMassVehicleBase`, a class of City Sample's game module
`CitySample` (`Source/CitySample/Vehicles/CitySampleMassVehicleBase.h` and `.cpp`), not of the
Traffic plugin, which declares no vehicle class, only two interfaces. Without it they do not load.

`copy_citysample_vehicle_base.py` copies Epic's two files, as they are, into a local plugin
`Plugins/CitySampleVehicleBase` (out of Git) whose module is also named `CitySample`: the class
keeps its path `/Script/CitySample.CitySampleMassVehicleBase`, so the Blueprints load with no
redirect and no code of ours for the class. The script writes only the plugin descriptor, the
module rules and a one-line module file, and stops (nothing written) if the two files include other
files of City Sample's game module. The plugin enables itself where it is copied; elsewhere (Mac,
CI) nothing changes.

Ruled out: copying the whole City Sample game module (game mode, player, HUD and the rest, for one
class); declaring our own cars as MassTraffic vehicle types (Epic's configs and spawners all point
at `BP_vehicle`; their entity configs would have to be rebuilt by hand in the editor).

## Candidate 32 on the PC (14/11)

The ZoneGraph is built (5 635 lanes, 2 705 zones) and MassTraffic drives 120 cars on it, in the
editor, 6 min without a fatal error or ensure. What followed:
- the intersections looked for MassCrowd's tracking data on their crosswalk lanes and logged
  "ArePedestriansClearOfIntersection - Null crowd tracking data" 7 million times: MassTraffic's
  crosswalk and pedestrian lane filters now match no lane (tag `GTPNoLane`), so the intersections
  have no crosswalk to wait for (`-TrafficCrosswalks=1` keeps City Sample's filters). Cars waiting
  for our pedestrians at crossings needs MassCrowd's lane tracking: later, with the pedestrians;
- City Sample's vehicle configs leave `PhysicsVehicleTemplateActor` empty ("Vehicles will be forced
  to low simulation LOD"): our copies name our car of the same number, set by name (an empty
  member is not even in the exported text, so the first try left it empty);
- no car was drawn within 50 m of the player (no display actor any more): the copies' visualization
  now draws near cars as instances too;
- the light types pointed at 5 missing street lamp meshes (cook risk): references to missing
  packages are removed like the other risks, and a copy that keeps cook risks of its own (the
  light types, whose meshes are missing) is dropped with every reference to it. MassTraffic draws no
  light anyway (no TrafficLightInstanceData for our map).

## Candidate 33 on the PC (14/11)

Placement report: no cook risk left in the copies, the physics car set in the 7 configs, no
property that could not be set. 355 s with `-Traffic=mass`: no fatal error, error nor ensure, one
MassTraffic warning left ("No TrafficLightInstanceData asset specified"). But the 120 cars never
came within 50 m of the player (53 to 56 m from t=10 s to 35 s) and none became an actor. The
driving-car spawner was among the "unsafe" copies, and is the one placed in the map. Candidate 34
tells why (below).

## Candidate 34: pedestrians seen by the cars, sparse or frozen traffic

- Epic's way for the cars to see people is the "Traffic Obstacle" trait (`UMassTrafficObstacleTrait`:
  tag `FMassTrafficObstacleTag`, transform, velocity, agent radius). Every frame
  `UMassTrafficFindObstaclesProcessor` binds each obstacle to the car behind it on the traffic lanes
  within `ObstacleSearchRadius`, and `UMassTrafficUpdateDistanceToNearestObstacleProcessor` brakes
  that car from the time to collision (down to 0.8 to 3 m from the obstacle). Our Mass crowd
  (`GTPCrowd`, which walks its own walkway graph, not MassCrowd's lanes) now carries the transform,
  velocity and radius (40 cm) when MassTraffic cars are in the map, and wears the tag only on a
  crossing: on the sidewalk a car passing 2 m away would brake for nothing, every obstacle costs a
  lane search a frame, and the driving-car spawn generator leaves 50 m free around every obstacle
  (`ObstacleExclusionRadius`; Epic's comment: "This won't scale past very few obstacles"). People are
  placed off crossings, so the cars spawned at begin play are not held off.
- `ObstacleSearchRadius` goes from Epic's 100 m to 15 m, set in memory with the lane filters
  (`-TrafficObstacleRadius=<cm>`).
- MassTraffic's other way, at traffic lights: an intersection waits for `NumEntitiesOnLane = 0` on its
  crosswalk lanes, from MassCrowd's tracking data (built for lanes with MassCrowd's `CrossingTag`), and
  opens or closes those lanes (`SetLaneState`). It counts only people that MassCrowd tracks: for our
  crowd that means calling MassCrowd's public API ourselves (`OnEntityLaneChanged`,
  `AcquireWaitingSlot`, `GetLaneState`). Not done: the crosswalk filters still match no lane.
- New log line every count, "MassTraffic cars": the simulated cars (Mass entities), how many move
  (over 1 m/s), their mean speed, how many within 100 and 200 m of the player, the nearest; and once
  the length of the car lanes and the cars per km. It tells sparse traffic (120 cars for the whole
  map) from frozen traffic (cars waiting at intersections that never open).
- Placement: a copy is "unsafe" only for a risk found. A scan cut at its package limit (which also
  counted as a risk before) is listed in `unsafe_scan_cut`; each unsafe copy's chains are in
  `unsafe_why`; a copy's references to itself are kept; a placed spawner that keeps risks is an error.
- On the PC the cause showed: our driving-car spawner still pointed, hard, at City Sample's own
  intersection spawner (and through it at the 5 missing street lamps), a reference the re-pointing
  of the exported text did not reach (most likely its Blueprint graph or compiled code). Every copy now goes through
  the engine's reference replacement first (`UGTPAssetReflection::ReplacePackageReferences`,
  `FArchiveReplaceObjectRef` as "Replace References" does, then the Blueprint is compiled): each
  reference to a copied package goes to our copy. Report: `engine_replaced`, `engine_replace_errors`.

## Candidate 34 on the PC: traffic that freezes

352 s on the PC: no error, 769 car lanes (14.9 km), 120 cars. At first 99 of them move (19 km/h);
two minutes later 12 do (3 km/h). Most likely cause, read in MassTraffic's source: a lane with no
lane after it is a case MassTraffic does not handle (`UMassTrafficChooseNextLaneProcessor`: "Should
never happen"), its car stops there for good, and our focus area has such dead ends wherever a road
leaves the map (traffic.py: "a lane with no other way on is a dead end, where the game takes the
car away", which the legacy traffic does and MassTraffic does not). City Sample's network is
closed. Two changes:
- `tools/city/zonegraph.py` closes the dead ends it can: where a lane fed by nothing starts within
  12 m the other way (the other side of a two-way road), a turnaround spline (tag Car, 15 km/h)
  joins them, 3 m past the end of the road. Counts `dead_ends_closed` and `dead_ends_left` (one-way
  streets leaving the map stay dead ends);
- the "MassTraffic cars" line counts the cars stopped on a dead-end lane, and once the dead-end
  lanes of the network.

PC, second run: the copies have no cook risk any more (registry read again after the saves). But
zonegraph.py closed only 6 dead ends out of 89, and 50 of the 120 cars ended up stopped on the 83
left (775 car lanes in the ZoneGraph). Next:
- zonegraph.py first joins a dead end to its next lane of lanes.csv when that one starts within
  10 m and is fed by nothing (two roads meeting off a junction, their lanes offset differently): the
  lane's last 6 m are redrawn to end on it. Counts `dead_ends_joined` and
  `dead_ends_left_with_next`;
- the dead ends that remain (one-way streets leaving the map) are MassTraffic's own business: every
  second a car standing at the end of one, 80 m or more from the player, gets
  `FMassTrafficRecyclableVehicleTag`, the tag MassTraffic gives a repaired wreck, and its overseer
  (`UMassTrafficOverseerProcessor`) moves it to a free space on one of the least busy lanes away
  from the player. Its distances (`BusiestLaneDistanceToPlayerRange`,
  `LeastBusiestLaneDistanceToPlayerRange`) go from 500 m, about our whole focus area, to 150 m
  (`-TrafficTransferDistance=<m>`, `-DeadEndKeepNear=<m>`, `-NoDeadEndRecycle`). Logged: "waiting for
  recycling" and "recycled so far".

PC, third run (1b1f38f): 43 dead ends joined, 6 closed, 40 left; only 8 cars stopped on a
dead-end lane. But the whole traffic freezes within two minutes (110 cars, 1 moving at 120 s), and
the 10 cars handed to the recycling were never moved. Read in MassTraffic's source:
- its lane choice does not look ahead, and `ShouldStopAtLaneExit` stops a car for good before a
  next lane that has no lane after it ("NONEXT"): the cars do not stop on the dead-end lanes but on
  the lane before them, and inside the junction when the dead end leaves one. A stop-sign junction
  (`UMassTrafficUpdateIntersectionsProcessor`, none of ours has lights) changes period only once its
  closing lanes are empty: one car stopped inside it and the junction never opens again, for
  anyone; the queues then reach the next junctions;
- the overseer moves a car only onto a lane at least `MinTransferDistance` (500 m) away from it:
  for a car in the middle of our focus area there is none.
Changes (C++ only, the map stays as it is):
- `UGTPMassTrafficSubsystem::CutDeadEndBranches`, once before the spawners: the car lanes from which
  every way ends at a dead end (dead ends, then backwards every lane whose next lanes are all such)
  are removed from MassTraffic's own lane data (`FZoneGraphTrafficLaneData::NextLanes`) for the
  cars' choice; every other lane keeps a way on. Inside a branch the plain next lanes stay and the
  junction lanes go, so that a car spawned there stops outside any junction; the plain lanes of a
  branch are closed (`bIsOpen`), so that the overseer never puts a car back into one. Logged "MassTraffic
  dead ends: N car lane(s) of M only lead to dead ends"; `-NoDeadEndCut` keeps the links;
- the recycling takes every car in such a branch (a moving one in a junction once it is out), and
  the cars stopped at a lane with nothing after it, out of the player's sight (behind the camera
  from 40 m, in front from 120 m, `-DeadEndKeepNear=<m>`); a car without Epic's damage fragment (the
  overseer's query needs it) gets one;
- `MinTransferDistance`: 200 m (`-TrafficMinTransfer=<m>`);
- a "MassTraffic stops" line says where the stopped cars stand: in a dead-end branch, inside a
  junction, for a pedestrian or the player, behind a car, at the lane end (no next lane, junction
  closed, junction open with no room after it, plain lane), other.

PC, candidate 35 (22eb010): 307 of the 775 car lanes only lead to dead ends (182 in junctions),
182 links removed, 54 cars handed to the recycling, 51 of which had no damage fragment and got
one (the overseer's query needs it; whether it then moved them, the log does not say yet). No
car is stopped in a dead end or inside a junction any more, but at 300 s 11 cars of 120 move: of the
109 stopped, 34 wait at a closed junction lane and 72 behind them. 38 errors "Intersection N has
side with no left most intersection lane": MassTraffic builds the sides of its junctions from the
cars' next lanes, so an approach whose every junction lane was cut becomes an empty side (only the
traffic light lookup skips it; its period is empty and passes at once). Read in MassTraffic:
`ShouldStopAtLaneExit` lets a car into a junction only when the lane after it has room for the car
and its gap (2 x radius + up to 5 m, `MinimumDistanceToNextVehicleRange`), and a car stopped at the
line never chooses again; zonegraph.py trims every lane 6 m before and after a junction and keeps
2 m of a short one, so a lane after a junction may never have room: a wall. Changes:
- a junction lane into a lane shorter than 10 m (5 m car, Epic's 5 m upper gap,
  `-TrafficMinRoomAfterJunction=<m>`) counts as a dead end in `CutDeadEndBranches`; the count is
  in the "MassTraffic dead ends" line;
- a "MassTraffic junction waits" line says why each closed junction lane stays closed, in
  MassTraffic's order: no lane after it, lane after it shorter than the car or full, lane in no
  period of its junction, junction clearing a lane of the period before (a car inside, counted but
  empty, reserved), waiting its turn, not seen ready; and the recycled cars left inside a junction.

PC, 6fbd155: 468 of the 775 car lanes end up in cut branches (133 junction lanes into a lane under
10 m): the network shrinks to 40 %. At 300 s 26 cars of 120 move, none within 50 m of the player; of
the 94 stopped, 78 stand behind a car and 14 at a closed junction lane (10 with a full lane after
it, 3 with a car inside the junction, 1 waiting its turn). Three fixes in a row treated symptoms.

## Candidate 35: junctions built as Epic builds them

Compared with what Epic built for City Sample, read in three places: City Sample's
`Config/DefaultPlugins.ini` (ZoneGraph and MassTraffic settings; read from a published copy of the
project, to check against the PC's copy), ZoneGraph's polygon routing
(`ZoneShapeUtilities.cpp`, `TessellatePolygonShape`, `BuildLanesBetweenPoints`) and MassTraffic
(`ShouldStopAtLaneExit`, `MassTrafficIntersectionSpawnDataGenerator.cpp`). What differs here:
- **Roads.** City Sample's roads are two-way splines of 4 m lanes ("Vehicle ↓↑ 400" and wider,
  tags Vehicle + Capillary Road) running from junction to junction; a junction polygon has one
  LaneProfile point per road, with that road's whole profile. Ours: one spline per lane (3.2 m),
  cut wherever BD TOPO cuts a street (every change of attribute), so a lane after a junction may be a
  piece of a few metres; one polygon point per lane end. In the quarter's table (1,043 lanes),
  196 lanes are under 10 m, 122 of the 604 lanes leaving a junction.
- **Junctions.** traffic.py gives nodes 18 m apart one junction id, but the lanes between them stay:
  45 lanes run from a junction back into the same one, inside its polygon, and add points inside it;
  21 lanes of 18 to 22 m join two junctions and keep 6 to 10 m once trimmed 6 m at each end.
  MassTraffic lets a car into a junction only when the one lane after it has room for the car and its
  gap (2 x radius + up to 5 m): such a lane is a wall.
- **Tags and filters.** City Sample: TrafficLaneFilter Vehicle (not Pedestrian), IntersectionLaneFilter
  Vehicle + Intersection + City (not Pedestrian, Freeway, Trunk Road, Crosswalk), TrunkLaneFilter
  "Trunk Road" (its freeway lanes only), LaneChangingLaneFilter Vehicle (not Pedestrian,
  Intersection, Crosswalk). Ours had every car lane as a trunk lane (so every junction side counts as
  coming from the freeway) and lane changes allowed inside junctions.
- **Same as Epic's, kept:** our polygon points are per lane, but ZoneGraph connects a point to every
  other point within LaneConnectionAngle (120 degrees by default, which we keep; City Sample sets 180
  with one point per road): with points per lane, 180 would add U-turns, 120 leaves them out.
- **Not done here:** traffic lights (Small City has lights at 106 of its 172 junctions, per a
  published export of its network; ours are all stop signs), two-way road splines (lane changes),
  density tags.
Changes:
- `tools/city/zonegraph.py` chains a lane that only continues into one lane fed by nothing else
  (`lanes_chained`), joins junctions a lane under 22 m (6 m + 10 m + 6 m) links into one while their
  nodes span at most 45 m (`junctions_merged`), and drops a short lane from a junction back into the
  same one (`junction_lanes_dropped`); `short_after_junction` counts the lanes that still leave a
  junction with less than 10 m. On the quarter's table: 687 lanes instead of 1,043 (38.3 km instead
  of 38.7), 173 polygons instead of 186, lanes leaving a junction under 10 m 10 instead of 122 (all
  10 at the edge of the map, with no lane after them);
- `ConfigureTrafficLaneFilters`: no trunk lane (GTPNoLane), no lane change inside a junction.

PC, f8fc873: zonegraph.py chained 55 lanes, merged 2 junctions, dropped 14 lanes, 5 lanes still leave
a junction with less than 10 m (166 car lanes, 35 junctions, 40 dead ends left); 5,443 ZoneGraph lanes.
City Sample's `DefaultPlugins.ini` on the PC (md5 0b1a975c, another version) has the same filters,
densities and build settings as the copy read. 349 s: at 300 s 32 cars of 120 move, the nearest 128 m
from the player; 307 of the 577 car lanes are still cut as dead-end branches (208 of them in
junctions). The cut keeps more than half the map, most of it around the player, without traffic, and
crowds the 120 cars onto the rest: of 88 stopped, 69 stand behind a car; of 18 at a closed junction
lane, 15 have a full lane after it. The other two differences with City Sample's settings are left:
its speed PID drives the physics vehicles near the player only (MassTraffic's `PIDVehicleControlFragment`,
not used while `AGTPVehicle` has no `IMassTrafficVehicleControlInterface`), and its
`LaneConnectionAngle` of 180 goes with one polygon point per road (see above).

## Candidate 35: a closed network, as a GTA city

A GTA city has no dead end: a car leaves the streets the player sees and another comes in.
MassTraffic expects the same (City Sample's network is closed), and moves a car onto its next lane by
lane and distance only (`MoveVehicleToNextLane`): a next lane need not touch the lane before it.
`UGTPMassTrafficSubsystem::LinkDeadEndsToEntries`, once before the spawners (default,
`-TrafficDeadEnds=link`):
- the junction lanes into a lane under 10 m, or with other than one lane after them, are taken out of
  the choice of the lanes before them (MassTraffic never lets a car into them);
- each road lane with no lane after it (a road leaving the map) leads on to an entry, a road lane no
  lane leads into (a road coming into the map), the least used one then the nearest: the car leaves
  the map at one end and comes back at the other, and no car stops for good before a dead end;
- no lane is closed: the whole map keeps its traffic. Logged "MassTraffic dead ends: N of M car
  lane(s) lead nowhere, K linked to E entry lane(s) ..., X m away on average".
`-TrafficDeadEnds=cut` keeps the branch cut of 6fbd155, `-TrafficDeadEnds=none` (or `-NoDeadEndCut`)
leaves the lanes as built. A new line "MassTraffic lanes near the player" gives the nearest road lane
and the road lanes within 50 m: no car can come nearer than the nearest lane.

No car leaves or comes into the map in the player's sight (`GuardLoops`, every 0.25 s): an entry lane
is closed (`bIsOpen`) while the player may see one of its two ends, the dead end a car leaves from or
the entry's own start (within 300 m, `-TrafficLoopSight=<m>`, in the camera's field plus 15 degrees or
within 30 m, and not hidden by a building: a visibility line trace). MassTraffic stops a car before a
closed lane (`ShouldStopAtLaneExit`): the cars wait at the dead end, as at a red light, and go on once
out of sight. "MassTraffic loops" counts the cars that looped and those that did it in sight (a car
already past the line when the entry closed); "MassTraffic stops" counts the cars "held at the map's
edge". `-NoLoopGuard` leaves the entries open.

## Candidate 36r: junctions that let a queue through

PC result of 1225555 (364 s smoke, ZoneGraph of f8fc873): 0 fatal, 0 error, 0 `IntersectionDetail::Build`
(51 before). Cars moving: 85 at 30 s, 58 at 120 s, 45 at 300 s (32 with f8fc873, 26 with 6fbd155);
nearest car 50 m at 300 s. Of the 75 stopped cars, 64 stand behind another car, 7 before a closed
junction lane, 4 at a plain lane end: the queues grow behind the junctions.

Our junctions have no traffic lights, so MassTraffic runs them as stop signs
(`UMassTrafficUpdateIntersectionsProcessor`): one period per side, 5 s (`StandardMinimumTrafficGoSeconds`),
the lane closes as the first car enters it (`CloseLaneAndAllItsSplitLanes`), and the next period
waits for the junction to be clear. One car per side per turn, through junctions up to 45 m wide.
At a junction with lights the period stays open while cars come and ends early once the junction
is clear with no car waiting; Small City has lights at 106 of its 172 junctions.
`UGTPMassTrafficSubsystem::LightJunctions`, once the junction entities are there (checked every
second), gives every junction built as a stop sign that behaviour (`bHasTrafficLights`) and makes its
side periods last 10 s (`-TrafficJunctionGoSeconds=<s>`). The periods stay those MassTraffic built,
one side at a time, so no two crossing flows are open together; no light is drawn. Logged
"MassTraffic junctions: N of M junction(s) built as stop signs given traffic-light turns".
`-TrafficStopSigns` keeps the stop signs.

PC result of 35add4d (two 330 s smokes, 0 fatal, 0 error, 0 ensure): 35 of 35 junctions given
traffic-light turns (116 side periods); 44 of 577 car lanes lead nowhere, all 44 linked to the 29
entries (249 m away on average). Cars moving at 30, 120 and 300 s: 95, 78, 59 with the guard, 79, 88,
71 with `-NoLoopGuard`; 5 cars stopped before a closed junction lane at 300 s (7 with 1225555). The
guard held no entry in either smoke ("0 at most"), so it changed nothing for the cars: the gap between
the two smokes is the spread of one run, about 65 moving at 300 s on average. 314 loops in 5 min, none
in sight.

Two points left:
- Mean speeds of 144 and 141 km/h at one time each. Near the player (MassTraffic's High/Medium LOD,
  200 m by default in its code) MassTraffic drives a car with its simple physics (`FMassTrafficVehiclePhysicsFragment`)
  and takes its speed from how far the body moved (`UpdateCoMVelocity`). A loop moves the car's lane
  250 m in one frame: the body is pulled there and the car leaves at thousands of km/h. Since 36r,
  `GuardLoops` also holds an entry closed while such a car is on one of its dead ends, and
  `RecycleAtDeadEnds` hands a car stopped there out of sight to MassTraffic's recycling (the overseer
  moves it, no physics runs on a recyclable car). "MassTraffic loops" counts the loops made under
  physics (to stay 0) and the entries held for them; "MassTraffic cars" leaves the speeds over
  200 km/h out of the mean and counts them.
- No car within 50 m: the nearest road lane to the start is 50 m away, at every time. "MassTraffic
  lanes near the player" now gives a tenth of a metre, the nearest junction lane and the player's
  position, to see whether a street is missing near the start or the start is 50 m from any street.

PC result of c492790 (349 s, 0 fatal): 100, 73 and 65 cars moving at 30, 120 and 300 s; no speed
over 200 km/h and no odd mean (22, 14, 9 km/h). 368 loops, none in sight, 3 made under physics; an
entry held for a car under physics, 2 at most; 4 cars recycled. The nearest road lane is 50.1 m from
the start, the nearest junction lane 52.4 m (player at X 9535, Y 49530 cm): the start is in the
pedestrian rue de la République, and no car can pass within 50 m of it. Since then a car under
physics also holds the entry while it heads into a dead end (its next lane chosen), as the check
every 0.25 s could miss one crossing a short dead end. One new error, "LogMassRepresentation: Template
actor type -1 is not referring to a valid type": the display Blueprints of City Sample's cars are left
out of our copies (they break the cook), so a car close enough for MassTraffic's high-resolution
actor has none (to confirm by the time and the lines before it).

The criterion "a car within 50 m" is meant to prove that the player sees traffic where they meet a
carriageway, so it is measured at the first car street the player meets on the way north: rue
Childebert, a one-way street that the pedestrian rue de la République crosses 205 m up from the
start. The watch point is on rue de la République, 5 m short of rue Childebert's lane (X 16100,
Y 30820 cm, from BD TOPO and our lanes; `-TrafficWatchPoint=X,Y` moves it). "MassTraffic watch
point" gives, every 5 s, the cars within 50 m of it (horizontal distance), the nearest, and how many
counts so far had at least one. The error "Template actor type -1" came once, 27 s in, while the
editor was still building the static meshes of City Sample's cars (vehCar_vehicle03 and 07); the PC
reads it as a cold-cache editor effect, to be checked in the package.

## Gate of candidate 36 (package, 14/11)

Package of cbd9207, `-Traffic=mass`, 300 s with the guard: 110 cars moving at 7 s, a dip to 75-86
from 20 s to 2 min (cars waiting their turn at red lights as they all reach the junctions at once,
with the cars behind them; never more than 3 before a full lane), then 101 to 112 from 147 s on;
nearest car 46 m at 297 s. Mean moving from 120 to 300 s: 103.9, against 84.0 for candidate 35's
package (stop signs, the same dip, then 79 to 92). The watch point on rue Childebert had a car
within 50 m in 11 of 19 counts. The guard costs nothing (game p50 8.18 ms with it, 8.24 ms without);
without it the package logs 9060 "Template actor type -1" errors (cars under physics that loop),
none with it. From candidate 37 the gate judges the traffic on the mean moving from 120 to 300 s,
at least that of candidate 35 less 10 % (75.6); the 120 s count is kept for information.

Gate of candidate 37 (b3c2dba): mean moving from 120 to 300 s 98.3 (threshold 75.6), the watch point
had a car within 50 m in 12 of 19 counts. With the guard the package still logged 410 "Template actor
type -1" errors from about 289 s on. MassRepresentation logs it when a car close enough to the player
for MassTraffic's high-resolution representation (`HighResSpawnedActor`) has no actor class: our
copies of City Sample's car configs leave out its car Blueprints (they break the cook). It then falls
back to the instance and tries again at the next update, as long as the car stays that close.
`UGTPMassTrafficSubsystem::ShowNearCarsAsInstances`, before the spawners, gives those LODs the
instance straight away (`StaticMeshInstance`) and drops the entity templates the spawners built when
registered, so `DoSpawning` builds them again. The cars look the same. Logged "MassTraffic visuals: N
of M visualization trait(s) asked for a display actor they do not have". `-TrafficActorLODs` leaves
them as they are.

## Open points

- Near the player MassTraffic may hand a car over to its physics vehicle: our copies name our cars
  (`BP_GTP_*`, stealable), but MassTraffic drives such an actor through its interface
  `IMassTrafficVehicleControlInterface` (`SetVehicleInputs`), which `AGTPVehicle` does not
  implement yet. To check on the PC, then to add (a subclass in GTPMass, which may link MassTraffic).
- `MassTrafficSubsystem::ClearAllTrafficLanes()` must be called when a spawner's despawning
  finishes (README of Myxcil/MassTraffic-Test): not done, we never despawn yet.
- Traffic lights: MassTraffic times its own (every junction since 36r) but draws none; ours
  (`GTPTrafficLights`) are not in step with them.
- The player on foot is not a traffic obstacle (City Sample gives the player a Mass agent).
- Crosswalks at traffic lights (MassCrowd's tracking data, see candidate 34): not done.
