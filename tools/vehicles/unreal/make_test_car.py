"""Create /Game/Vehicles/Test/BP_TestCar, the first drivable test car (version 0.3).

Child Blueprint of the C++ AGTPVehicle with the Vehicle template's sports
car mesh and its animation Blueprint (wheel controller). Run in the editor
(headless is fine), after tools/vehicles/install_vehicle_template.sh:

    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
        -script=tools/vehicles/unreal/make_test_car.py -unattended -nosplash

The template mesh's wheel bones (Phys_Wheel_FL, ...) are the names our code
expects (docs/gameplay/vehicles.md, section 9).
"""

import unreal

DEST_DIR = "/Game/Vehicles/Test"
BP_NAME = "BP_TestCar"
WHEEL_BONES = ("Phys_Wheel_FL", "Phys_Wheel_FR", "Phys_Wheel_BL", "Phys_Wheel_BR")


def log(msg):
    unreal.log_warning("[make_test_car] " + msg)


def find_asset(class_name, name_part, roots=("/Game/Vehicles", "/Game/VehicleTemplate")):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for root in roots:
        for data in registry.get_assets_by_path(root, recursive=True):
            cls = str(data.asset_class_path.asset_name) if hasattr(data, "asset_class_path") else str(data.asset_class)
            if cls == class_name and name_part.lower() in str(data.asset_name).lower():
                found.append(str(data.package_name) + "." + str(data.asset_name))
    found.sort(key=len)
    return found


def main():
    meshes = find_asset("SkeletalMesh", "SportsCar")
    anims = find_asset("AnimBlueprint", "SportsCar")
    if not meshes:
        raise RuntimeError("no SportsCar skeletal mesh: run tools/vehicles/install_vehicle_template.sh first")
    mesh = unreal.load_asset(meshes[0])
    log("mesh %s, anim blueprints %s" % (meshes[0], anims))

    log("expected wheel bones (checked by Chaos at runtime): %s" % ", ".join(WHEEL_BONES))

    path = DEST_DIR + "/" + BP_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        bp = unreal.load_asset(path)
        log("updating existing " + path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.GTPVehicle)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BP_NAME, DEST_DIR, unreal.Blueprint, factory)
        log("created " + path)

    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
    body = cdo.get_editor_property("mesh")
    body.set_editor_property("skeletal_mesh_asset", mesh)
    if anims:
        anim_bp = unreal.load_asset(anims[0])
        body.set_editor_property("anim_class", anim_bp.generated_class())
        body.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
    else:
        log("WARNING: no SportsCar animation Blueprint, wheels will not turn visually")

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    log("saved " + path)


main()
