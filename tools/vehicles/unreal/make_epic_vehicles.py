"""Make our drivable cars out of Epic's City Sample Vehicles, inside the Unreal Editor (no UI).

City Sample's cars (Fab, free, migrated out of Git into /Game/Vehicles/Epic/ or left in
/Game/CitySampleVehicles/) are Chaos WheeledVehiclePawns on the skeleton SK_Vehicle, whose wheel
bones (Phys_Wheel_FL/FR/BL/BR) are the ones AGTPVehicle expects. For each car listed in CARS this
script creates /Game/Vehicles/Epic/GTP/BP_GTP_<Name>, a child of AGTPVehicle (stealing, damage,
traffic, police), and copies onto it from Epic's Blueprint:
  - the skeletal mesh (its physics asset comes with it) and the wheel AnimBlueprint;
  - the whole Chaos tuning: mass, drag, centre of mass, engine (torque curve), transmission,
    differential, steering, and the wheel setups with Epic's own wheel classes.
Our code then adds the arcade help for the player only (PlayerTorque scaled to the car's torque).

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
ROOTS = ("/Game/Vehicles/Epic", "/Game/CitySampleVehicles")
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
            match = re.search(r"vehicle(\d\d)_", name, re.IGNORECASE)
            if match and match.group(1) not in found and str(data.package_path) != OUT:
                found[match.group(1)] = str(data.package_name)
    return found


def copy_movement(source, target, report_entry):
    for prop in MOVEMENT_PROPS:
        try:
            target.set_editor_property(prop, source.get_editor_property(prop))
        except Exception as error:  # a property renamed in this engine version
            report_entry.setdefault("not_copied", []).append("%s: %s" % (prop, error))


def make_one(number, name, source_path, report):
    entry = {"epic": source_path}
    source_cls = eal.load_blueprint_class(source_path)
    if source_cls is None:
        entry["error"] = "cannot load"
        report["cars"][name] = entry
        return
    source = unreal.get_default_object(source_cls)
    path = "%s/BP_GTP_%s" % (OUT, name)
    if not eal.does_asset_exist(path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.GTPVehicle)
        tools.create_asset("BP_GTP_" + name, OUT, unreal.Blueprint, factory)
    bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(eal.load_blueprint_class(path))

    src_body = source.get_editor_property("mesh")
    body = cdo.get_editor_property("mesh")
    body.set_editor_property("skeletal_mesh_asset", src_body.get_editor_property("skeletal_mesh_asset"))
    body.set_editor_property("animation_mode", src_body.get_editor_property("animation_mode"))
    body.set_editor_property("anim_class", src_body.get_editor_property("anim_class"))
    # City Sample Blueprints may draw parts (lights, light bar) as extra components: not copied,
    # the light bar of the police car is added by AGTPPoliceDirector in code.

    copy_movement(source.get_editor_property("vehicle_movement_component"),
                  cdo.get_editor_property("vehicle_movement_component"), entry)
    if number == "13":
        cdo.tags = list(cdo.tags) + [unreal.Name("GTPEpicPolice")]
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_asset(path, only_if_is_dirty=False)
    move = cdo.get_editor_property("vehicle_movement_component")
    entry["class"] = "%s.BP_GTP_%s_C" % (path, name)
    entry["mass"] = move.get_editor_property("mass")
    entry["torque"] = move.get_editor_property("engine_setup").get_editor_property("max_torque")
    report["cars"][name] = entry
    log("%s <- %s (%.0f kg, %.0f N.m)" % (path, source_path, entry["mass"], entry["torque"]))


def main():
    t0 = time.time()
    report = {"cars": {}, "missing": []}
    if not eal.does_directory_exist(OUT):
        eal.make_directory(OUT)
    epic = find_epic_blueprints()
    report["epic_found"] = epic
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
