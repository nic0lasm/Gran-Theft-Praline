"""Make our drivable cars out of Epic's City Sample Vehicles, inside the Unreal Editor (no UI).

City Sample's cars (Fab, free, out of Git) are taken from the full City Sample project, copied at
their own path /Game/Vehicle/ by tools/vehicles/copy_citysample_content.py (or from the City Sample
Vehicles pack, /Game/CitySampleVehicles/ or /Game/Vehicles/Epic/). They are Chaos WheeledVehiclePawns on the skeleton SK_Vehicle, whose wheel
bones (Phys_Wheel_FL/FR/BL/BR) are the ones AGTPVehicle expects. For each car listed in CARS this
script creates /Game/Vehicles/Epic/GTP/BP_GTP_<Name>, a child of AGTPVehicle (stealing, damage,
traffic, police), and copies onto it from Epic's Blueprint:
  - the skeletal mesh (its physics asset comes with it) and the wheel AnimBlueprint;
  - the whole Chaos tuning: mass, drag, centre of mass, engine (torque curve), transmission,
    differential, steering, and the wheel setups with Epic's own wheel classes.
Our code then adds the arcade help for the player only (PlayerTorque scaled to the car's torque).

Mesh mode. The full City Sample's car Blueprints derive from a C++ class of the City Sample game
module (CitySampleMassVehicleBase), which our project does not have: they cannot load. Then the car
is built from its parts instead, without Epic's Blueprint: its skeletal mesh (found next to the
Blueprint, on the skeleton SK_Vehicle; GTP_EPIC_MESH_<NN> forces one), the wheel AnimBlueprint (City
Sample's when it loads, else a copy of the Vehicle template's, as import_vehicle.py does), wheel
Blueprints of our own (children of UGTPWheelFront / UGTPWheelRear) and the Chaos figures read from
City Sample's cars on 13/11 (TUNING below: mass, torque, gears, wheels, suspension, brakes).

The Vehicle Variety Pack is not used: its Blueprints derive from PhysX's WheeledVehicle, gone in
5.8 (only its skeletal meshes could be rigged again, later).

Output classes, used by Config/DefaultGame.ini (VehicleClasses, PoliceVehicleClass):
  /Game/Vehicles/Epic/GTP/BP_GTP_<Name>.BP_GTP_<Name>_C
Report: Saved/VehicleImport/epic_vehicles_report.json. Nothing is deleted outside /Game/Vehicles/Epic/GTP.
"""

import json
import os
import re
import time

import unreal

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

OUT = "/Game/Vehicles/Epic/GTP"
ROOTS = ("/Game/Vehicle", "/Game/Vehicles/Epic", "/Game/CitySampleVehicles")
# Epic number -> our name. Cars and vans that fit a European city street; trucks, the bus and
# the trailer are left out of the traffic (too big for the Presqu'ile's lanes).
CARS = {
    "01": "Van01",
    "02": "Car02",
    "03": "Car03",
    "05": "Car05",
    "07": "Car07",
    "12": "Car12",
    "13": "Police13",  # saloon with a light bar: the police car
}
# Chaos figures of City Sample's cars, read from their Blueprints (City Sample Vehicles pack, 13/11).
# Gear lists given in full where read, else (first, last, count), spaced evenly in ratio.
# Wheels: radius cm, friction, spring rate, preload, raise cm, drop cm, brake N.m, handbrake N.m,
# steer degrees. All: drag 0.3, final ratio 3.08, 5 400 rpm, shifts at 4 500 / 2 000 rpm.
_SALOON = {"mass": 1850, "torque": 500, "gears": [2.82, 1.5, 1.0, 0.7],
           "wheel": (35.2, 3.8, 220, 300, 8, 13, 1500, 4000, 46)}
TUNING = {
    "01": {"mass": 2100, "torque": 400, "gears": (5.25, 0.52, 10), "up_rpm": 3500,
           "wheel": (35.3, 3.1, 200, 300, 15, 14, 1500, 6000, 45)},
    "02": {"mass": 1200, "torque": 175, "gears": [3.62, 1.96, 1.29, 1.04, 0.84, 0.7],
           "wheel": (35.3, 3.0, 100, 300, 10, 17, 900, 3500, 47)},
    "03": dict(_SALOON),
    "05": {"mass": 2300, "torque": 425, "gears": (4.71, 0.85, 8),
           "wheel": (35.3, 3.6, 260, 300, 10, 12, 1700, 5000, 42)},
    "07": {"mass": 1400, "torque": 250, "gears": (5.07, 0.67, 8),
           "wheel": (31.3, 3.2, 160, 300, 15, 11.75, 1200, 4500, 45)},
    "12": dict(_SALOON),
    "13": dict(_SALOON, torque=700),
}
SKIP_MESH = re.compile(r"wheel|door|glass|broken|damage|interior|driver|lod|hood|trunk|bumper|light|plate|siren|proxy", re.IGNORECASE)

MOVEMENT_PROPS = (
    "mass", "drag_coefficient", "chassis_width", "chassis_height", "enable_center_of_mass_override",
    "center_of_mass_override", "engine_setup", "transmission_setup", "differential_setup",
    "steering_setup", "wheel_setups",
)


def log(msg):
    unreal.log_warning("[epic_vehicles] " + msg)


def find_epic_blueprints():
    """Number -> Blueprint package path, from names like BP_vehicle02_Car."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = {}
    for root in ROOTS:
        for data in registry.get_assets_by_path(root, recursive=True):
            if str(data.asset_class_path.asset_name) != "Blueprint":
                continue
            name = str(data.asset_name)
            match = re.search(r"veh(?:icle)?_?(\d\d)", name, re.IGNORECASE)
            if not name.startswith("BP_") or "trailer" in name.lower():
                continue
            if match and str(data.package_path) != OUT:
                found.setdefault(match.group(1), []).append((name, str(data.package_name)))
    # Several Blueprints per car in the full City Sample (variants): the plain BP_vehicleNN_Kind
    # first, else the shortest name. All candidates go to the report.
    chosen = {}
    for number, candidates in found.items():
        candidates.sort(key=lambda c: (re.fullmatch(r"BP_vehicle\d\d_[A-Za-z]+", c[0]) is None, len(c[0])))
        chosen[number] = candidates[0][1]
    find_epic_blueprints.candidates = {n: [c[1] for c in cs] for n, cs in found.items()}
    return chosen


def missing_dependencies(package, limit=400):
    """Packages the Blueprint needs (recursively, /Game only) that are not in the project: what
    the file copy left behind, to copy next."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True)
    seen, todo, missing = set(), [package], []
    while todo and len(seen) < limit:
        current = todo.pop()
        if current in seen:
            continue
        seen.add(current)
        for dep in registry.get_dependencies(current, options) or []:
            dep = str(dep)
            if not dep.startswith("/Game/") or dep in seen:
                continue
            if not eal.does_asset_exist(dep):
                missing.append(dep)
                seen.add(dep)
            else:
                todo.append(dep)
    return sorted(set(missing))


def copy_movement(source, target, report_entry):
    for prop in MOVEMENT_PROPS:
        try:
            target.set_editor_property(prop, source.get_editor_property(prop))
        except Exception as error:  # a property renamed in this engine version
            report_entry.setdefault("not_copied", []).append("%s: %s" % (prop, error))


def gear_list(spec):
    if isinstance(spec, list):
        return [float(g) for g in spec]
    first, last, count = spec
    step = (last / first) ** (1.0 / (count - 1))
    return [round(first * step ** i, 3) for i in range(count)]


def blueprint_child(path, parent):
    """Creates the Blueprint at path (child of parent) when missing; returns (blueprint, CDO)."""
    folder, asset = path.rsplit("/", 1)
    if not eal.does_asset_exist(path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        tools.create_asset(asset, folder, unreal.Blueprint, factory)
    return unreal.load_asset(path), unreal.get_default_object(eal.load_blueprint_class(path))


def find_mesh(number, blueprint_path, entry):
    forced = os.environ.get("GTP_EPIC_MESH_" + number)
    if forced:
        return unreal.load_asset(forced)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    folder = blueprint_path.rsplit("/", 1)[0]
    candidates = []
    for root in (folder, "/Game/Vehicle"):
        for data in registry.get_assets_by_path(root, recursive=True):
            if str(data.asset_class_path.asset_name) != "SkeletalMesh":
                continue
            asset_name = str(data.asset_name)
            if not re.search(r"veh(?:icle)?_?%s(?!\d)" % number, asset_name, re.IGNORECASE) and root != folder:
                continue
            candidates.append((SKIP_MESH.search(asset_name) is not None, len(asset_name), str(data.package_name), asset_name))
        if candidates:
            break
    candidates.sort()
    entry["mesh_candidates"] = [c[3] for c in candidates]
    for _, _, package, _ in candidates:
        mesh = unreal.load_asset(package)
        if mesh is not None:
            return mesh
    return None


def wheel_anim_class(mesh, blueprint_path, entry):
    """City Sample's wheel AnimBlueprint when it loads, else a copy of the Vehicle template's."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    folder = blueprint_path.rsplit("/", 1)[0]
    for data in registry.get_assets_by_path(folder, recursive=True):
        if str(data.asset_class_path.asset_name) == "AnimBlueprint":
            abp = unreal.load_asset(str(data.package_name))
            if abp is not None and abp.generated_class() is not None:
                entry["anim"] = str(data.package_name)
                return abp.generated_class()
    skeleton = mesh.get_editor_property("skeleton")
    target = "%s/ABP_GTP_%s" % (OUT, skeleton.get_name())
    if not eal.does_asset_exist(target):
        source = None
        for root in ("/Game/Vehicles", "/Game/VehicleTemplate"):
            for data in registry.get_assets_by_path(root, recursive=True):
                if str(data.asset_class_path.asset_name) == "AnimBlueprint" and "sportscar" in str(data.asset_name).lower():
                    source = str(data.package_name)
                    break
            if source:
                break
        if not source:
            entry["anim"] = "none: no Vehicle template AnimBlueprint, wheels will not turn"
            return None
        abp = eal.duplicate_asset(source, target)
        abp.set_editor_property("target_skeleton", skeleton)
        unreal.BlueprintEditorLibrary.compile_blueprint(abp)
        eal.save_loaded_asset(abp)
    abp = unreal.load_asset(target)
    entry["anim"] = target
    return abp.generated_class()


def build_from_parts(number, name, blueprint_path, entry):
    """Mesh mode: Epic's Blueprint cannot load (parent class of the City Sample game module)."""
    tuning = TUNING.get(number)
    mesh = find_mesh(number, blueprint_path, entry)
    if tuning is None or mesh is None:
        entry["error"] = "mesh mode: " + ("no tuning" if tuning is None else "no skeletal mesh found")
        return None
    radius, friction, spring, preload, rise, drop, brake, handbrake, steer = tuning["wheel"]
    wheel_classes = {}
    for axle, parent in (("Front", unreal.GTPWheelFront), ("Rear", unreal.GTPWheelRear)):
        wbp, wcdo = blueprint_child("%s/BP_GTP_%s_Wheel%s" % (OUT, name, axle), parent)
        for prop, value in (("wheel_radius", radius), ("friction_force_multiplier", friction), ("cornering_stiffness", 1000.0),
                            ("spring_rate", spring), ("spring_preload", preload), ("suspension_max_raise", rise),
                            ("suspension_max_drop", drop), ("max_brake_torque", brake)):
            wcdo.set_editor_property(prop, float(value))
        if axle == "Front":
            wcdo.set_editor_property("max_steer_angle", float(steer))
        else:
            wcdo.set_editor_property("max_hand_brake_torque", float(handbrake))
        unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
        eal.save_loaded_asset(wbp)
        wheel_classes[axle] = eal.load_blueprint_class(wbp.get_path_name().split(".")[0])

    path = "%s/BP_GTP_%s" % (OUT, name)
    bp, cdo = blueprint_child(path, unreal.GTPVehicle)
    body = cdo.get_editor_property("mesh")
    body.set_editor_property("skeletal_mesh_asset", mesh)
    anim = wheel_anim_class(mesh, blueprint_path, entry)
    if anim is not None:
        body.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
        body.set_editor_property("anim_class", anim)
    move = cdo.get_editor_property("vehicle_movement_component")
    move.set_editor_property("mass", float(tuning["mass"]))
    move.set_editor_property("drag_coefficient", 0.3)
    engine = move.get_editor_property("engine_setup")
    engine.set_editor_property("max_torque", float(tuning["torque"]))
    engine.set_editor_property("max_rpm", 5400.0)
    move.set_editor_property("engine_setup", engine)
    gearbox = move.get_editor_property("transmission_setup")
    gearbox.set_editor_property("forward_gear_ratios", gear_list(tuning["gears"]))
    gearbox.set_editor_property("final_ratio", 3.08)
    gearbox.set_editor_property("change_up_rpm", float(tuning.get("up_rpm", 4500)))
    gearbox.set_editor_property("change_down_rpm", 2000.0)
    move.set_editor_property("transmission_setup", gearbox)
    setups = move.get_editor_property("wheel_setups")
    for index, setup in enumerate(setups):
        setup.set_editor_property("wheel_class", wheel_classes["Front" if index < 2 else "Rear"])
    move.set_editor_property("wheel_setups", setups)
    entry["mode"] = "parts"
    entry["mesh"] = mesh.get_path_name()
    return bp, cdo, path


def make_one(number, name, source_path, report):
    entry = {"epic": source_path}
    entry["missing_dependencies"] = missing_dependencies(source_path)
    source_cls = eal.load_blueprint_class(source_path)
    if source_cls is None:
        built = build_from_parts(number, name, source_path, entry)
        report["cars"][name] = entry
        if built is None:
            return
        bp, cdo, path = built
        finish(number, name, bp, cdo, path, entry, source_path)
        return
    source = unreal.get_default_object(source_cls)
    path = "%s/BP_GTP_%s" % (OUT, name)
    bp, cdo = blueprint_child(path, unreal.GTPVehicle)
    entry["mode"] = "blueprint"

    src_body = source.get_editor_property("mesh")
    body = cdo.get_editor_property("mesh")
    body.set_editor_property("skeletal_mesh_asset", src_body.get_editor_property("skeletal_mesh_asset"))
    body.set_editor_property("animation_mode", src_body.get_editor_property("animation_mode"))
    body.set_editor_property("anim_class", src_body.get_editor_property("anim_class"))
    # City Sample Blueprints may draw parts (lights, light bar) as extra components: not copied,
    # the light bar of the police car is added by AGTPPoliceDirector in code.

    copy_movement(source.get_editor_property("vehicle_movement_component"),
                  cdo.get_editor_property("vehicle_movement_component"), entry)
    report["cars"][name] = entry
    finish(number, name, bp, cdo, path, entry, source_path)


def finish(number, name, bp, cdo, path, entry, source_path):
    if number == "13":
        cdo.tags = list(cdo.tags) + [unreal.Name("GTPEpicPolice")]
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_asset(path, only_if_is_dirty=False)
    move = cdo.get_editor_property("vehicle_movement_component")
    entry["class"] = "%s.BP_GTP_%s_C" % (path, name)
    entry["mass"] = move.get_editor_property("mass")
    entry["torque"] = move.get_editor_property("engine_setup").get_editor_property("max_torque")
    log("%s <- %s (%.0f kg, %.0f N.m)" % (path, source_path, entry["mass"], entry["torque"]))


def main():
    t0 = time.time()
    report = {"cars": {}, "missing": []}
    if not eal.does_directory_exist(OUT):
        eal.make_directory(OUT)
    epic = find_epic_blueprints()
    report["epic_found"] = epic
    report["epic_candidates"] = getattr(find_epic_blueprints, "candidates", {})
    for number, name in CARS.items():
        if number not in epic:
            report["missing"].append(number)
            continue
        make_one(number, name, epic[number], report)
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "VehicleImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "epic_vehicles_report.json"), "w") as f:
        json.dump(report, f, indent=2, default=str)
    log("report: " + json.dumps(report, default=str))


main()
