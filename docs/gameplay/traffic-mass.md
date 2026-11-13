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
2. If `Traffic.uplugin` or `RuleProcessor.uplugin` says `"EnabledByDefault": false`, enable them in
   the local `.uproject` (not pushed while the plugins are out of Git).
3. Build the editor target, run `place_zonegraph.py` then `place_masstraffic.py`.
   City Sample's traffic configs and spawner Blueprints live in the City Sample project's own
   content (/Game/AI/Traffic, /Game/AI/Traffic/Spawners, /Game/Vehicle/veh*): they are copied to
   the same /Game paths in our project (out of Git), so their references hold.
4. Read the report: lane filters set and City Sample's spawners placed (intersections, parked
   cars, driving cars, started in that order).
5. Package, launch with `-Traffic=mass`.

## Open points

- Near the player MassTraffic swaps its entities for physics actors from its own Blueprints. For
  stealing, crashes and police, those actors must be our `AGTPVehicle` classes (the Epic vehicles
  reparented): to set in the representation config once the plugin is in.
- Traffic lights: MassTraffic has its own; ours (`GTPTrafficLights`) stay for the legacy traffic.
