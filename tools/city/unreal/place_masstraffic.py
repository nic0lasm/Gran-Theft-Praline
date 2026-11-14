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
2. Our cookable traffic (default): City Sample's intersection and driving-car spawner Blueprints and
   everything of /Game/AI/Traffic they reach (entity configs, data) are copied to /Game/AI/GTPTraffic
   (kept when already there), and every reference in the copies is re-pointed through the engine's
   reflection (UGTPAssetReflection, most MassTraffic classes are not exposed to Python): to our copy
   for City Sample's traffic content; to our car BP_GTP_<Name> for the physics vehicle MassTraffic
   spawns near the player (PhysicsVehicleTemplateActor, by City Sample's car number); to nothing for
   anything that would break the cook (cook_risk below: the display Blueprints of City Sample's
   cars, the drivers). The vehicle types' own lane filters are pointed at our tags. Report
   "gtp_copy": each change, what was removed, and the copies' own cook_risk with its chains.
   City Sample's parked-car spawner is not used: its parking spaces belong to City Sample's city;
   our street parked cars stay AGTPTrafficNetwork's.
   GTP_MASSTRAFFIC_SOURCE=citysample places City Sample's own spawners instead (not cookable on
   14/11: about 180 risky packages), and without them plain Mass spawners with the entity configs
   found under /Game/AI/Traffic (or GTP_MASSTRAFFIC_CONFIGS, comma separated asset paths).
3. The spawners are placed after removing the previous ones, tagged GTPMassTraffic and
   GTPMassTrafficOrder<n>, with no spawn on begin play: the game starts them in that order,
   intersections first, only with -Traffic=mass (UGTPMassTrafficSubsystem). Counts:
   GTP_MASSTRAFFIC_CARS (default 120), GTP_MASSTRAFFIC_PARKED (default 200). The map is saved.
   Nothing is changed when the plugin is absent.
4. Lists what the spawners would pull into the cook that is missing or known to break it
   ("cook_risk": City Sample's BP_VehicleBase_Deformable, vehicle AnimBlueprints, post-process
   AnimBlueprints; the cook of candidate 30 failed on them through our cars). It must be empty
   before a package. GTP_MASSTRAFFIC_REMOVE=1 only removes the spawners and saves the map (for a
   package while the list is not empty). "cook_risk_why" gives the chain that pulls each one in.
5. GTP_MASSTRAFFIC_SURVEY=1 changes nothing: it lists, for City Sample's spawner Blueprints and
   traffic entity configs, their properties (reflection dump), direct dependencies and the chains
   to their cook risks ("survey").
"""

import json
import os
import re
import time

import unreal

ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
MAP = "/Game/Maps/L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))
TAG = "GTPMassTraffic"
# City Sample car number -> our car (tools/vehicles/unreal/make_epic_vehicles.py, CARS).
CAR_NUMBERS = {"01": "Van01", "02": "Car02", "03": "Car03", "05": "Car05", "07": "Car07", "12": "Car12", "13": "Police13"}
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


def place_blueprint_spawners(report, spawners=None):
    """spawners: (name, Blueprint path, count, start order); City Sample's own by default."""
    if spawners is None:
        spawners = [(n, p, c, o) for o, (n, p, c) in enumerate(SPAWNERS)]
    placed = 0
    for name, path, count, order in spawners:
        cls = unreal.EditorAssetLibrary.load_blueprint_class(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if cls is None:
            report["errors"].append("missing " + path)
            continue
        spawner = actor_sub.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
        tag_spawner(spawner, name, order, count, report)
        report["spawners"].append({"name": name, "blueprint": path, "count": count, "order": order})
        placed += 1
    return placed


COOK_RISK = re.compile(r"BP_VehicleBase_Deformable|ABP_Proxy_Post|/ABP_(?!GTP_)", re.IGNORECASE)


def _dependencies(registry, package, hard):
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=not hard,
                                                    include_hard_package_references=hard)
    return [str(d) for d in registry.get_dependencies(package, options) or []]


def cook_risk(packages, limit=5000, why=None):
    """Dependencies of packages, recursively, hard and soft (the cook follows both), that are
    missing or match COOK_RISK. Engine and script packages are left out. With a dict why, each risk
    also gets the chain that pulls it in, root first ("a > b > c", "~>" for a soft link): the
    package to replace is the last sound one before the risk."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    parent = {p: None for p in packages}
    seen, todo, risks = set(), list(packages), set()
    while todo and len(seen) < limit:
        current = todo.pop(0)
        if current in seen:
            continue
        seen.add(current)
        for hard in (True, False):
            for dep in _dependencies(registry, current, hard):
                if dep.startswith(("/Script/", "/Engine/", "/Temp/")) or dep in seen or dep in parent:
                    continue
                parent[dep] = (current, hard)
                if not unreal.EditorAssetLibrary.does_asset_exist(dep):
                    risks.add(dep + " (missing)")
                    seen.add(dep)
                    continue
                if COOK_RISK.search(dep):
                    risks.add(dep)
                todo.append(dep)
    if why is not None:
        for risk in sorted(risks)[:60]:
            node, chain = risk.replace(" (missing)", ""), []
            while node is not None:
                link = parent.get(node)
                chain.append(node)
                node = link[0] if link else None
                if link:
                    chain.append(">" if link[1] else "~>")
            why[risk] = " ".join(reversed(chain))
    if todo:
        risks = list(risks) + ["(scan stopped at %d packages: not all checked)" % limit]
    return sorted(risks)


def survey(report):
    """Read only (GTP_MASSTRAFFIC_SURVEY=1): what City Sample's traffic content is made of, to build
    cookable spawners of ours. For each spawner Blueprint and each Mass entity config under
    /Game/AI/Traffic and /Traffic: direct hard and soft dependencies, and the chains to its cook
    risks. Also whether the material function MassTraffic's vehicle materials need is there."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    roots = [path for _, path, _ in SPAWNERS if unreal.EditorAssetLibrary.does_asset_exist(path)]
    for root in ("/Game/AI/Traffic", "/Traffic"):
        for data in registry.get_assets_by_path(root, recursive=True):
            if str(data.asset_class_path.asset_name) == "MassEntityConfigAsset":
                roots.append(str(data.package_name))
    items = {}
    for package in roots:
        why = {}
        risks = cook_risk([package], limit=3000, why=why)
        items[package] = {
            "describe": describe(package),
            "hard": sorted(d for d in _dependencies(registry, package, True) if not d.startswith("/Script/")),
            "soft": sorted(d for d in _dependencies(registry, package, False) if not d.startswith("/Script/")),
            "cook_risk_count": len(risks),
            "why": why,
        }
    report["survey"] = items
    found = []
    for root in ("/Traffic", "/Game"):
        for data in registry.get_assets_by_path(root, recursive=True):
            if "UnpackTrafficVehicleInstanceCustomData" in str(data.asset_name):
                found.append(str(data.package_name))
    report["unpack_custom_data_function"] = found or "missing"


# Our cookable traffic (default): copies of City Sample's intersection and driving-car spawners and
# of everything of /Game/AI/Traffic they reach, in /Game/AI/GTPTraffic, with each reference
# re-pointed: to our copy for City Sample's traffic content, to our car (BP_GTP_*, stealable) for
# the physics vehicle MassTraffic spawns near the player, to nothing for what would break the cook
# (the display Blueprints of City Sample's cars, the drivers). City Sample's parked-car spawner is
# left out: its parking spaces come with City Sample's own city; ours are the street parked cars of
# AGTPTrafficNetwork. GTP_MASSTRAFFIC_SOURCE=citysample places City Sample's spawners as they are.
SRC_DIR = "/Game/AI/Traffic"
OUT_DIR = "/Game/AI/GTPTraffic"
OUR_CARS = "/Game/Vehicles/Epic/GTP"
GTP_SPAWNERS = (("Intersection", 0, None), ("Vehicle", 2, CARS))
REF = re.compile(r"""(?P<wrapped>/Script/[A-Za-z0-9_.]+'(?P<q>\\?"?)(?P<wpath>/Game/[^'"\\\s]+)(?P=q)')"""
                 r"""|(?P<bare>/Game/[A-Za-z0-9_\-/]+(?:\.[A-Za-z0-9_\-]+)?(?::[A-Za-z0-9_\-.]+)?)""")
PHYSICS = re.compile(r"""PhysicsVehicleTemplateActor=(?P<v>"(?:[^"\\]|\\.)*"|[^,)]+)""")
CAR_NUMBER = re.compile(r"veh(?:icle)?_?(\d\d)", re.IGNORECASE)


def reflection():
    return getattr(unreal, "GTPAssetReflection", None)


def describe(package, depth=3):
    """The reflection dump (UGTPAssetReflection) of an asset, or of a Blueprint's class defaults."""
    tool = reflection()
    if tool is None:
        return "GTPAssetReflection missing: build the editor target"
    asset = unreal.load_asset(package)
    if asset is None:
        return "cannot load"
    if isinstance(asset, unreal.Blueprint):
        cls = unreal.EditorAssetLibrary.load_blueprint_class(package)
        asset = unreal.get_default_object(cls) if cls else asset
    try:
        return json.loads(tool.describe_object(asset, depth))
    except Exception as error:
        return "describe failed: %s" % error


def traffic_closure(roots):
    """Packages under SRC_DIR that roots reach, hard or soft, through SRC_DIR packages only."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found, todo = set(), list(roots)
    while todo:
        package = todo.pop()
        if package in found:
            continue
        found.add(package)
        for hard in (True, False):
            for dep in _dependencies(registry, package, hard):
                if dep.startswith(SRC_DIR + "/") and dep not in found:
                    todo.append(dep)
    return sorted(found)


def our_car_class(text):
    """Our car (BP_GTP_*) for the City Sample car a reference names, by its number; else None."""
    match = CAR_NUMBER.search(text)
    name = CAR_NUMBERS.get(match.group(1)) if match else None
    if name is None:
        return None
    path = "%s/BP_GTP_%s" % (OUR_CARS, name)
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    return "/Script/Engine.BlueprintGeneratedClass'%s.BP_GTP_%s_C'" % (path, name)


def repoint(text, copied, risk_cache, record):
    """text with every /Game reference re-pointed (see above). record(kind, before, after)."""
    def physics(match):
        value = match.group("v")
        ours = our_car_class(value)
        if ours is None:
            ours = our_car_class("vehicle02")  # a car of ours for the vans and trucks we do not have
        if ours is None or value.strip('"') == "None":
            return match.group(0)
        record("physics car", value, ours)
        return 'PhysicsVehicleTemplateActor="%s"' % ours

    text = PHYSICS.sub(physics, text)

    def one(match):
        path = match.group("wpath") or match.group("bare")
        package = path.split(".")[0].split(":")[0]
        if package.startswith(OUR_CARS + "/") or package.startswith(OUT_DIR + "/"):
            return match.group(0)
        if package in copied:
            new_path = OUT_DIR + path[len(SRC_DIR):]
            record("copy", path, new_path)
            return match.group(0).replace(path, new_path)
        if package not in risk_cache:
            risk_cache[package] = cook_risk([package], limit=2000)
        risks = risk_cache[package]
        if all(r.startswith("/Traffic/") and r.endswith("(missing)") for r in risks):
            if risks:
                record("needs Plugins/Traffic/Content", path, path)
            return match.group(0)
        record("removed (cook risk: %s)" % risks[0], path, "None")
        return "None" if match.group("wrapped") else ""

    return REF.sub(one, text)


def copy_traffic(report):
    """Makes /Game/AI/GTPTraffic (see above) and returns our spawner Blueprints, by name. Copies
    already there are kept and only re-pointed again (idempotent; nothing is deleted)."""
    tool = reflection()
    importer = getattr(unreal, "GTPZoneGraphImport", None)
    if tool is None or importer is None or not hasattr(importer, "point_lane_filters_at"):
        report["errors"].append("GTPAssetReflection or GTPZoneGraphImport.point_lane_filters_at missing: build the editor target")
        return {}
    eal = unreal.EditorAssetLibrary
    wanted = {name for name, _, _ in GTP_SPAWNERS}
    sources = {name: path for name, path, _ in SPAWNERS if name in wanted}
    missing = [p for p in sources.values() if not eal.does_asset_exist(p)]
    if missing:
        report["errors"].append("City Sample spawners missing: " + ", ".join(missing))
        return {}
    packages = traffic_closure(list(sources.values()))
    copy = {"packages": len(packages), "copied": [], "kept": [], "changes": [], "set_errors": [],
            "too_long": [], "lane_filters": {}}
    report["gtp_copy"] = copy
    for package in packages:
        target = OUT_DIR + package[len(SRC_DIR):]
        if eal.does_asset_exist(target):
            copy["kept"].append(target)
        elif eal.duplicate_asset(package, target) is None:
            report["errors"].append("cannot copy %s to %s" % (package, target))
        else:
            copy["copied"].append(target)
    copied = set(packages)
    risk_cache = {}
    removed = 0
    for package in packages:
        target = OUT_DIR + package[len(SRC_DIR):]
        if not eal.does_asset_exist(target):
            continue
        dump = describe(target)
        if not isinstance(dump, dict):
            report["errors"].append("%s: %s" % (target, dump))
            continue
        todo = [dump]
        while todo:
            item = todo.pop()
            todo += item.get("subobjects", [])
            for name, text in item.get("properties", {}).items():
                if "/Game/" not in text:
                    continue
                if text.endswith("..."):
                    copy["too_long"].append("%s.%s" % (item["path"], name))
                    continue
                changes = []
                new = repoint(text, copied, risk_cache, lambda kind, a, b: changes.append((kind, a, b)))
                if new == text:
                    continue
                error = tool.set_property_text(item["path"], name, new)
                if error:
                    copy["set_errors"].append("%s.%s: %s" % (item["path"], name, error))
                for kind, before, after in changes:
                    removed += after == "None"
                    if len(copy["changes"]) < 400:
                        copy["changes"].append({"object": item["path"], "property": name, "kind": kind, "from": before, "to": after})
            # The vehicle types of the spawn data generators carry their own lane filter.
            obj = unreal.load_object(None, item["path"])
            if obj is not None:
                result = json.loads(importer.point_lane_filters_at(obj, "Car", "Intersection"))
                if result.get("counts", {}).get("filters_set"):
                    copy["lane_filters"][item["path"]] = result.get("info", {})
                report["errors"] += result.get("errors", [])
        eal.save_asset(target, only_if_is_dirty=False)
    copy["removed"] = removed
    why = {}
    copy["cook_risk"] = cook_risk([OUT_DIR + p[len(SRC_DIR):] for p in packages], why=why)
    copy["cook_risk_why"] = why
    return {name: OUT_DIR + path[len(SRC_DIR):] for name, path in sources.items()}


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
    if os.environ.get("GTP_MASSTRAFFIC_SURVEY") == "1":
        survey(report)
        return write(report, t0)
    level_sub.load_level(MAP)
    for actor in actor_sub.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            actor_sub.destroy_actor(actor)
            report["removed"] = report.get("removed", 0) + 1
    if os.environ.get("GTP_MASSTRAFFIC_REMOVE") == "1":
        level_sub.save_current_level()
        return write(report, t0)
    configure_settings(report)
    source = os.environ.get("GTP_MASSTRAFFIC_SOURCE", "gtp")
    report["source"] = source
    if source == "gtp":
        ours = copy_traffic(report)
        if place_blueprint_spawners(report, [(name, ours[name], count, order)
                                             for name, order, count in GTP_SPAWNERS if name in ours]) == 0:
            report["errors"].append("none of our spawners placed (see gtp_copy)")
    elif place_blueprint_spawners(report) == 0:
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
    report["cook_risk_why"] = {}
    report["cook_risk"] = cook_risk([s["blueprint"] for s in report["spawners"] if s.get("blueprint")]
                                    + [c.split(".")[0] for s in report["spawners"] for c in s.get("configs", [])],
                                    why=report["cook_risk_why"])
    level_sub.save_current_level()
    write(report, t0)


main()
