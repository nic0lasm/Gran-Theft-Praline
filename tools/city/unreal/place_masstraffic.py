"""Place the MassTraffic spawners of the city map (ADR-0015, docs/gameplay/traffic-mass.md), inside
the Unreal Editor, the same way as place_zonegraph.py (no UI).

Needs City Sample's plugins Traffic (module MassTraffic) and RuleProcessor copied into Plugins/ (out of Git) and the ZoneGraph
already placed (place_zonegraph.py: lanes tagged Car, junctions tagged Car + Intersection).

What it does, reporting everything in Saved/CityImport/masstraffic_report.json:
1. Checks MassTraffic's ZoneGraph lane filters through UGTPZoneGraphImport::ConfigureTrafficLaneFilters
   (C++, module GTPMass: neither MassTrafficSettings nor ZoneGraphSettings is exposed to Python in
   5.8). The filter properties are read by reflection, not assumed: every traffic-lane filter gets
   our tag Car, every intersection filter Intersection, at their fixed bits. Nothing is saved: the
   module sets the same filters at every engine start, game and package included. An error if no
   filter is set.
2. Places, after removing the previous ones, City Sample's own spawner Blueprints
   (/Game/AI/Traffic/Spawners: intersections, parked cars, driving cars, kept at their City Sample
   paths), tagged GTPMassTraffic and GTPMassTrafficOrder<n>, with no spawn on begin play: the game
   starts them in that order, only with -Traffic=mass (UGTPMassTrafficSubsystem).
3. Without those Blueprints: plain Mass spawners with the entity configs found under
   /Game/AI/Traffic (or GTP_MASSTRAFFIC_CONFIGS, comma separated asset paths), no generator set. Counts: GTP_MASSTRAFFIC_CARS (default 120),
   GTP_MASSTRAFFIC_PARKED (default 200).
The map is saved. Nothing is changed when the plugin is absent.
"""

import json
import os
import time

import unreal

ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
MAP = "/Game/Maps/L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))
TAG = "GTPMassTraffic"
CARS = int(os.environ.get("GTP_MASSTRAFFIC_CARS", "120"))
PARKED = int(os.environ.get("GTP_MASSTRAFFIC_PARKED", "200"))

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor_sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log_warning("[CityMassTraffic] " + msg)


def write(report, t0):
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "masstraffic_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    log("report: " + json.dumps(report))


def configure_settings(report):
    importer = getattr(unreal, "GTPZoneGraphImport", None)
    if importer is None or not hasattr(importer, "configure_traffic_lane_filters"):
        report["errors"].append("GTPZoneGraphImport.configure_traffic_lane_filters missing: build the editor target")
        return
    result = json.loads(importer.configure_traffic_lane_filters("Car", "Intersection"))
    report["settings"] = result.get("info", {})
    report["settings_counts"] = result.get("counts", {})
    report["errors"] += result.get("errors", [])
    if not report["settings_counts"].get("filters_set"):
        report["errors"].append("no MassTraffic lane filter set: the traffic would find no lane")


# City Sample's own spawner Blueprints (AMassSpawner children with their entity configs and
# ZoneGraph spawn-data generators already set), kept at their City Sample paths on the PC.
# Order = start order in the game: intersections first (lights, lane gating), then parked cars,
# then the driving cars that need both.
SPAWNERS = (
    ("Intersection", "/Game/AI/Traffic/Spawners/BP_MassTrafficIntersectionSpawner", None),
    ("ParkedVehicle", "/Game/AI/Traffic/Spawners/BP_MassTrafficParkedVehicleSpawner", PARKED),
    ("Vehicle", "/Game/AI/Traffic/Spawners/BP_MassTrafficVehicleSpawner", CARS),
)


def entity_configs(report):
    """Fallback when the spawner Blueprints are missing: the Mass entity configs of the traffic."""
    listed = [p for p in os.environ.get("GTP_MASSTRAFFIC_CONFIGS", "").split(",") if p]
    if listed:
        return listed
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for root in ("/Game/AI/Traffic", "/Traffic"):
        for data in registry.get_assets_by_path(root, recursive=True):
            if str(data.asset_class_path.asset_name) != "MassEntityConfigAsset":
                continue
            name = str(data.asset_name)
            if "Vehicle" in name or "Parked" in name:
                found.append(str(data.package_name) + "." + name)
    report["entity_configs_found"] = found
    return found


def tag_spawner(spawner, name, order, count, report):
    spawner.set_actor_label("MassTraffic_" + name)
    spawner.tags = [unreal.Name(TAG), unreal.Name("%sOrder%d" % (TAG, order))]
    spawner.set_folder_path("City/MassTraffic")
    settings = [("auto_spawn_on_begin_play", False)]
    if count is not None:
        settings.append(("count", count))
    for prop, value in settings:
        try:
            spawner.set_editor_property(prop, value)
        except Exception as error:
            report["errors"].append("%s.%s: %s" % (name, prop, error))


def place_blueprint_spawners(report):
    placed = 0
    for order, (name, path, count) in enumerate(SPAWNERS):
        cls = unreal.EditorAssetLibrary.load_blueprint_class(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if cls is None:
            report["errors"].append("missing " + path)
            continue
        spawner = actor_sub.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
        tag_spawner(spawner, name, order, count, report)
        report["spawners"].append({"name": name, "blueprint": path, "count": count, "order": order})
        placed += 1
    return placed


def place_config_spawner(name, order, configs, count, report):
    spawner = actor_sub.spawn_actor_from_class(unreal.MassSpawner, unreal.Vector(0, 0, 0))
    tag_spawner(spawner, name, order, count, report)
    types = []
    for path in configs:
        asset = unreal.load_asset(path)
        if asset is None:
            report["errors"].append("cannot load " + path)
            continue
        entry = unreal.MassSpawnedEntityType()
        entry.set_editor_property("entity_config", asset)
        entry.set_editor_property("proportion", 1.0)
        types.append(entry)
    spawner.set_editor_property("entity_types", types)
    report["spawners"].append({"name": name, "count": count, "configs": configs, "order": order,
                               "note": "no spawn-data generator set: City Sample's spawner Blueprints are missing"})


def main():
    t0 = time.time()
    report = {"map": MAP, "errors": [], "spawners": []}
    if not any("MassTraffic" in c for c in dir(unreal)):
        report["errors"].append("MassTraffic not loaded (Plugins/Traffic and Plugins/RuleProcessor from City Sample): nothing done")
        return write(report, t0)
    level_sub.load_level(MAP)
    for actor in actor_sub.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            actor_sub.destroy_actor(actor)
    configure_settings(report)
    if place_blueprint_spawners(report) == 0:
        configs = entity_configs(report)
        parked = [c for c in configs if "Parked" in c]
        driving = [c for c in configs if "Parked" not in c]
        if parked:
            place_config_spawner("ParkedVehicle", 1, parked, PARKED, report)
        if driving:
            place_config_spawner("Vehicle", 2, driving, CARS, report)
        if not parked and not driving:
            report["errors"].append("no spawner Blueprint nor Mass entity config for traffic: set GTP_MASSTRAFFIC_CONFIGS")
    report["generator_classes"] = sorted(c for c in dir(unreal) if "MassTraffic" in c and "Generator" in c)
    level_sub.save_current_level()
    write(report, t0)


main()
