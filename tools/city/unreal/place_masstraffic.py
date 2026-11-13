"""Place the MassTraffic spawners of the city map (ADR-0015, docs/gameplay/traffic-mass.md), inside
the Unreal Editor, the same way as place_zonegraph.py (no UI).

Needs MassTraffic copied from City Sample into Plugins/MassTraffic (out of Git) and the ZoneGraph
already placed (place_zonegraph.py: lanes tagged Car, junctions tagged Car + Intersection).

What it does, reporting everything in Saved/CityImport/masstraffic_report.json:
1. Lists the MassTraffic settings properties that filter ZoneGraph lanes (names differ between
   versions of the plugin, so they are read, not assumed) and points every traffic-lane filter at
   our tag Car, every intersection filter at Intersection. The settings are saved to
   Config/DefaultMassTraffic.ini by the editor (local file of the PC, not in Git while the plugin
   is not).
2. Finds the Mass entity config assets of the plugin and of the content (names with "Traffic" or
   "Parked"), or takes GTP_MASSTRAFFIC_CONFIGS (comma separated asset paths).
3. Places, after removing the previous ones, one Mass spawner for driving cars and one for parked
   cars, tagged GTPMassTraffic, with no spawn on begin play: the game starts them only with
   -Traffic=mass (UGTPMassTrafficSubsystem). Counts: GTP_MASSTRAFFIC_CARS (default 120),
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


def tag_filter(tag_name):
    """A ZoneGraph tag filter that accepts any lane carrying tag_name, or None if the tag is unknown."""
    settings = unreal.get_default_object(unreal.ZoneGraphSettings)
    for index, info in enumerate(settings.get_editor_property("tags")):
        if str(info.get_editor_property("name")) == tag_name:
            mask = unreal.ZoneGraphTagMask()
            try:
                mask.set_editor_property("mask", 1 << index)
            except Exception:
                return None
            flt = unreal.ZoneGraphTagFilter()
            flt.set_editor_property("any_tags", mask)
            return flt
    return None


def configure_settings(report):
    cls = getattr(unreal, "MassTrafficSettings", None)
    if cls is None:
        report["errors"].append("MassTrafficSettings class not found")
        return
    settings = unreal.get_default_object(cls)
    car, junction = tag_filter("Car"), tag_filter("Intersection")
    report["settings"] = {}
    for prop in dir(settings):
        if not prop.endswith("lane_filter"):
            continue
        wanted = junction if "intersection" in prop else car
        if "crosswalk" in prop or "pedestrian" in prop or wanted is None:
            report["settings"][prop] = "left as is"
            continue
        try:
            settings.set_editor_property(prop, wanted)
            report["settings"][prop] = "Intersection" if wanted is junction else "Car"
        except Exception as error:  # read-only or another type in this version
            report["settings"][prop] = "not set: %s" % error
    try:
        settings.save_config()
    except Exception as error:
        report["errors"].append("settings not saved: %s" % error)


def entity_configs(report):
    listed = [p for p in os.environ.get("GTP_MASSTRAFFIC_CONFIGS", "").split(",") if p]
    if listed:
        return listed
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for root in ("/MassTraffic", "/Game"):
        for data in registry.get_assets_by_path(root, recursive=True):
            if str(data.asset_class_path.asset_name) != "MassEntityConfigAsset":
                continue
            name = str(data.asset_name)
            if "Traffic" in name or "Parked" in name or "Vehicle" in name:
                found.append(str(data.package_name) + "." + name)
    report["entity_configs_found"] = found
    return found


def place_spawner(world, name, configs, count, report):
    spawner = actor_sub.spawn_actor_from_class(unreal.MassSpawner, unreal.Vector(0, 0, 0))
    spawner.set_actor_label(name)
    spawner.tags = [unreal.Name(TAG)]
    spawner.set_folder_path("City/MassTraffic")
    for prop, value in (("auto_spawn_on_begin_play", False), ("count", count)):
        try:
            spawner.set_editor_property(prop, value)
        except Exception as error:
            report["errors"].append("%s.%s: %s" % (name, prop, error))
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
    report["spawners"].append({"name": name, "count": count, "configs": configs})
    # The spawn points come from the plugin's ZoneGraph generators, set on the spawner by hand once
    # their class names are known on the PC: listed here so the next run can set them.
    report["generator_classes"] = sorted(c for c in dir(unreal) if "MassTraffic" in c and "Generator" in c)


def main():
    t0 = time.time()
    report = {"map": MAP, "errors": [], "spawners": []}
    if getattr(unreal, "MassTrafficSettings", None) is None and not any("MassTraffic" in c for c in dir(unreal)):
        report["errors"].append("MassTraffic not installed (Plugins/MassTraffic): nothing done")
        return write(report, t0)
    level_sub.load_level(MAP)
    world = editor_sub.get_editor_world()
    for actor in actor_sub.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            actor_sub.destroy_actor(actor)
    configure_settings(report)
    configs = entity_configs(report)
    driving = [c for c in configs if "Parked" not in c]
    parked = [c for c in configs if "Parked" in c]
    if driving:
        place_spawner(world, "MassTraffic_Driving", driving, CARS, report)
    if parked:
        place_spawner(world, "MassTraffic_Parked", parked, PARKED, report)
    if not driving and not parked:
        report["errors"].append("no Mass entity config for traffic found: set GTP_MASSTRAFFIC_CONFIGS")
    level_sub.save_current_level()
    write(report, t0)


main()
