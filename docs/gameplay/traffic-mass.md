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
- **Spawners**: `tools/city/unreal/place_masstraffic.py` reads the plugin's settings (the lane
  filters are pointed at our tags), finds its entity configs, places a driving and a parked
  spawner (120 and 200 by default, no spawn on begin play) and writes
  `Saved/CityImport/masstraffic_report.json`.

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
2. `Traffic.uplugin` is not enabled by default: the `.uproject` lists it as `"Optional": true`, so
   it loads where the plugin is copied and is skipped without error elsewhere (Mac, CI).
   RuleProcessor is enabled by default.
3. Build the editor target, run `place_zonegraph.py` then `place_masstraffic.py`. The first one
   writes our ZoneGraph tags (Car, Intersection, Pedestrian, Crossing) to the ZoneGraph settings'
   default config file; the second points MassTraffic's lane filters at the same tag bits, in C++
   (`UGTPZoneGraphImport::ConfigureTrafficLaneFilters`: neither settings class is exposed to Python
   in 5.8), and writes MassTraffic's default config file. Both reports name the files written.
   City Sample's traffic configs and spawner Blueprints live in the City Sample project's own
   content (/Game/AI/Traffic, /Game/AI/Traffic/Spawners, /Game/Vehicle/veh*): they are copied to
   the same /Game paths in our project (out of Git), so their references hold.
4. Read the report: lane filters set and City Sample's spawners placed (intersections, parked
   cars, driving cars, started in that order).
5. Package, launch with `-Traffic=mass`.

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

## Open points

- Near the player MassTraffic swaps its entities for physics actors from its own Blueprints. For
  stealing, crashes and police, those actors must be our `AGTPVehicle` classes (the Epic vehicles
  reparented): to set in the representation config once the plugin is in.
- Traffic lights: MassTraffic has its own; ours (`GTPTrafficLights`) stay for the legacy traffic.
