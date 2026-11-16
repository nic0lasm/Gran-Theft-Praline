"""Read-only dump of the Chaos tuning of our cars and of City Sample's, inside the Unreal Editor (no UI).

For the drive probe (docs/gameplay/vehicles.md 8.7): what our cars (/Game/Vehicles/Epic/GTP) and
City Sample's own car Blueprints and wheel Blueprints, copied into the project under /Game/Vehicle,
/Game/Vehicles/Epic or /Game/CitySampleVehicles (out of Git), set for the drivetrain, the wheels and
the suspension. A Blueprint that does not load (City Sample's cars need City Sample's game module)
is listed with its parent class from the asset registry. Nothing is saved or changed.

Run: UnrealEditor-Cmd <project>/GranTheftPraline.uproject -run=pythonscript
     -script=tools/vehicles/unreal/dump_vehicle_chaos.py -unattended -nosplash
Report: Saved/VehicleImport/vehicle_chaos_dump.json, and one log line per Blueprint.
"""

import json
import os

import unreal

ROOTS = ("/Game/Vehicles/Epic/GTP", "/Game/Vehicle", "/Game/Vehicles/Epic", "/Game/CitySampleVehicles")
WHEEL_PROPS = (
    "axle_type", "wheel_radius", "wheel_width", "friction_force_multiplier", "cornering_stiffness",
    "side_slip_modifier", "slip_threshold", "skid_threshold", "affected_by_engine", "affected_by_brake",
    "affected_by_handbrake", "affected_by_steering", "traction_control_enabled", "abs_enabled",
    "max_steer_angle", "max_brake_torque", "max_hand_brake_torque", "suspension_max_raise",
    "suspension_max_drop", "suspension_damping_ratio", "wheel_load_ratio", "spring_rate",
    "spring_preload", "suspension_smoothing", "rollbar_scaling", "suspension_force_offset",
    "suspension_axis", "sweep_shape", "sweep_type",
)
MOVE_PROPS = (
    "mass", "drag_coefficient", "chassis_width", "chassis_height", "enable_center_of_mass_override",
    "center_of_mass_override", "downforce_coefficient", "legacy_wheel_friction_position",
)


def text(value):
    """JSON-safe text of an editor property value."""
    if isinstance(value, (bool, int, float, str)) or value is None:
        return value
    if isinstance(value, (list, tuple)) or type(value).__name__ == "Array":
        return [text(v) for v in value]
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def props(obj, names):
    out = {}
    for name in names:
        try:
            out[name] = text(obj.get_editor_property(name))
        except Exception as error:
            out[name] = "n/a (%s)" % type(error).__name__
    return out


def struct_props(struct, names):
    out = {}
    for name in names:
        try:
            out[name] = text(struct.get_editor_property(name))
        except Exception:
            pass
    return out


def dump_movement(move):
    entry = props(move, MOVE_PROPS)
    try:
        diff = move.get_editor_property("differential_setup")
        entry["differential"] = struct_props(diff, ("differential_type", "front_rear_split"))
    except Exception as error:
        entry["differential"] = "n/a (%s)" % error
    try:
        engine = move.get_editor_property("engine_setup")
        entry["engine"] = struct_props(engine, ("max_torque", "max_rpm", "engine_idle_rpm", "engine_brake_effect",
                                                "engine_rev_up_moi", "engine_rev_down_rate"))
    except Exception as error:
        entry["engine"] = "n/a (%s)" % error
    try:
        gearbox = move.get_editor_property("transmission_setup")
        entry["transmission"] = struct_props(gearbox, ("use_automatic_gears", "forward_gear_ratios", "reverse_gear_ratios",
                                                       "final_ratio", "change_up_rpm", "change_down_rpm", "gear_change_time",
                                                       "transmission_efficiency"))
    except Exception as error:
        entry["transmission"] = "n/a (%s)" % error
    wheels = []
    try:
        for setup in move.get_editor_property("wheel_setups"):
            cls = setup.get_editor_property("wheel_class")
            wheel = {"bone": str(setup.get_editor_property("bone_name")), "class": text(cls)}
            if cls is not None:
                wheel.update(props(unreal.get_default_object(cls), WHEEL_PROPS))
            wheels.append(wheel)
    except Exception as error:
        wheels = "n/a (%s)" % error
    entry["wheels"] = wheels
    return entry


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    report = {"blueprints": {}, "roots_missing": []}
    for root in ROOTS:
        if not unreal.EditorAssetLibrary.does_directory_exist(root):
            report["roots_missing"].append(root)
            continue
        for data in registry.get_assets_by_path(root, recursive=True):
            if str(data.asset_class_path.asset_name) != "Blueprint":
                continue
            package = str(data.package_name)
            if package in report["blueprints"]:
                continue
            entry = {"parent": str(data.get_tag_value("ParentClass") or ""),
                     "native_parent": str(data.get_tag_value("NativeParentClass") or "")}
            try:
                cls = unreal.EditorAssetLibrary.load_blueprint_class(package)
            except Exception as error:
                cls = None
                entry["load_error"] = str(error)
            if cls is None:
                entry["kind"] = "not loaded"
                report["blueprints"][package] = entry
                continue
            cdo = unreal.get_default_object(cls)
            if isinstance(cdo, unreal.ChaosVehicleWheel):
                entry["kind"] = "wheel"
                entry.update(props(cdo, WHEEL_PROPS))
            elif isinstance(cdo, unreal.WheeledVehiclePawn):
                entry["kind"] = "car"
                entry["movement"] = dump_movement(cdo.get_editor_property("vehicle_movement_component"))
            else:
                continue
            report["blueprints"][package] = entry
            unreal.log_warning("[vehicle_chaos] %s %s %s" % (entry["kind"], package, json.dumps(entry, sort_keys=True)[:1500]))
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), "VehicleImport")
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "vehicle_chaos_dump.json")
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=1, sort_keys=True)
    kinds = {}
    for entry in report["blueprints"].values():
        kinds[entry["kind"]] = kinds.get(entry["kind"], 0) + 1
    unreal.log_warning("[vehicle_chaos] %s: %s, roots missing %s" % (path, kinds, report["roots_missing"]))


main()
