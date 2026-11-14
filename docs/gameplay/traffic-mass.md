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

## Open points

- Near the player MassTraffic may hand a car over to its physics vehicle: our copies name our cars
  (`BP_GTP_*`, stealable), but MassTraffic drives such an actor through its interface
  `IMassTrafficVehicleControlInterface` (`SetVehicleInputs`), which `AGTPVehicle` does not
  implement yet. To check on the PC, then to add (a subclass in GTPMass, which may link MassTraffic).
- `MassTrafficSubsystem::ClearAllTrafficLanes()` must be called when a spawner's despawning
  finishes (README of Myxcil/MassTraffic-Test): not done, we never despawn yet.
- Traffic lights: MassTraffic times its own but draws none; ours (`GTPTrafficLights`) are not in
  step with them.
- The player on foot is not a traffic obstacle (City Sample gives the player a Mass agent).
- Crosswalks at traffic lights (MassCrowd's tracking data, see candidate 34): not done.
