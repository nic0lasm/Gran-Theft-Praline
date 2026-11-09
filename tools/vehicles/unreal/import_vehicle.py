"""Import one car prepared by prepare_vehicle.py and build its drivable Blueprint.

Run headless on the Mac through tools/vehicles/import-vehicle-mac.sh:

    tools/vehicles/import-vehicle-mac.sh V-01

Input: assets/staging/vehicles/<id>/ (SKM_<Name>.fbx, SM_<Name>_Glass_<Zone>.fbx,
textures/T_<Name>_BC|ORM|N.png, report.json).

Output, under /Game/Vehicles/Cars/<Name>/:
  SKM_<Name>            skeletal mesh (wheel bones, seat_driver, Dent_* morph targets)
  PA_<Name>             physics asset: one chassis body (two boxes) on the root bone
  SM_<Name>_Glass_*     glass panels (translucent, no collision)
  T_<Name>_*, MI_<Name>_Body, MI_<Name>_Glass
  ABP_<Name>            wheel animation (copy of the Vehicle template's, on our skeleton)
  BP_<Name>             child of AGTPVehicle, ready for -TestVehicleClass

Shared masters (created when missing): /Game/Vehicles/Materials/M_Vehicle_Body
(BaseColor / ORM / Normal, clear coat where the texture says metal) and
M_Vehicle_Glass (translucent).

Each step logs "[import_vehicle]" lines; a failed step raises so the commandlet
exits with an error instead of leaving a half-built car.
"""

import json
import os

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
STAGING = os.environ.get("GTP_VEHICLE_STAGING", os.path.join(REPO, "assets", "staging", "vehicles"))
MASTERS = "/Game/Vehicles/Materials"
ZONES = {"Front": "FRONT", "Rear": "REAR", "Left": "LEFT", "Right": "RIGHT"}
TEMPLATE_ANIM_HINT = "SportsCar"


def log(msg):
    unreal.log_warning("[import_vehicle] " + str(msg))


# ---------------------------------------------------------------- materials
def tex_param(mat, name, x, y, sampler):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("sampler_type", sampler)
    return node


def ensure_body_master():
    path = MASTERS + "/M_Vehicle_Body"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = tools.create_asset("M_Vehicle_Body", MASTERS, unreal.Material, unreal.MaterialFactoryNew())
    base = tex_param(mat, "BaseColor", -600, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    orm = tex_param(mat, "ORM", -600, 0, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    normal = tex_param(mat, "Normal", -600, 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    # Car paint: a clear coat where the texture is metallic paint, none on tyres and plastics.
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_CUSTOM_DATA0)
    coat_rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 550)
    coat_rough.set_editor_property("parameter_name", "ClearCoatRoughness")
    coat_rough.set_editor_property("default_value", 0.06)
    mel.connect_material_property(coat_rough, "", unreal.MaterialProperty.MP_CUSTOM_DATA1)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mat.set_editor_property("used_with_morph_targets", True)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("created " + path)
    return mat


def ensure_glass_master():
    path = MASTERS + "/M_Vehicle_Glass"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = tools.create_asset("M_Vehicle_Glass", MASTERS, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    mat.set_editor_property("two_sided", True)

    def scalar(name, value, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        return node

    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -300)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(0.02, 0.025, 0.03, 1.0))
    mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar("Roughness", 0.04, 0), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar("Specular", 0.9, 150), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(scalar("Opacity", 0.55, 300), "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("created " + path)
    return mat


def make_instance(name, folder, parent, textures=None):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = tools.create_asset(name, folder, unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    for pname, tex in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(mi, pname, tex)
    eal.save_loaded_asset(mi)
    return mi


# ---------------------------------------------------------------- import
def import_task(src, dest, name):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
    paths = list(t.get_editor_property("imported_object_paths"))
    log(f"imported {os.path.basename(src)} -> {paths}")
    return paths


def find_loaded(paths, cls):
    for p in paths:
        asset = unreal.load_asset(p)
        if isinstance(asset, cls):
            return asset
    return None


def move_to(asset, target):
    current = asset.get_path_name().split(".")[0]
    if current == target:
        return asset
    if eal.does_asset_exist(target):
        eal.delete_asset(target)
    if not eal.rename_asset(current, target):
        raise RuntimeError(f"could not move {current} to {target}")
    return unreal.load_asset(target)


def import_textures(src_dir, name, folder):
    out = {}
    for suffix, pname in (("BC", "BaseColor"), ("ORM", "ORM"), ("N", "Normal")):
        fname = f"T_{name}_{suffix}.png"
        paths = import_task(os.path.join(src_dir, "textures", fname), folder, f"T_{name}_{suffix}")
        tex = find_loaded(paths, unreal.Texture2D)
        if tex is None:
            raise RuntimeError("texture not imported: " + fname)
        tex = move_to(tex, f"{folder}/T_{name}_{suffix}")
        if suffix == "BC":
            tex.set_editor_property("srgb", True)
        elif suffix == "ORM":
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        else:
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        eal.save_loaded_asset(tex)
        out[pname] = tex
    return out


def set_skeletal_materials(mesh, material):
    mats = mesh.get_editor_property("materials")
    fixed = []
    for m in mats:
        m.set_editor_property("material_interface", material)
        fixed.append(m)
    mesh.set_editor_property("materials", fixed)
    log(f"{len(fixed)} material slot(s) on {mesh.get_name()}")


def physics_asset_of(mesh, imported_paths, folder, name):
    pa = mesh.get_editor_property("physics_asset")
    if pa is None:
        pa = find_loaded(imported_paths, unreal.PhysicsAsset)
    if pa is None:
        for entry in eal.list_assets(folder, recursive=True, include_folder=False):
            asset = unreal.load_asset(entry.split(".")[0])
            if isinstance(asset, unreal.PhysicsAsset):
                pa = asset
                break
    if pa is None:
        raise RuntimeError("the importer made no physics asset for " + mesh.get_name())
    return move_to(pa, f"{folder}/PA_{name}")


def wheel_anim_blueprint(skeleton, folder, name):
    """Copy of the Vehicle template's wheel AnimBlueprint, re-targeted to our skeleton.

    Its graph is a WheelController node that reads the wheel bones from the
    movement component, so it works on any skeleton with the same bone names."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    source = None
    for root in ("/Game/Vehicles", "/Game/VehicleTemplate"):
        for data in registry.get_assets_by_path(root, recursive=True):
            cls = str(data.asset_class_path.asset_name)
            if cls == "AnimBlueprint" and TEMPLATE_ANIM_HINT.lower() in str(data.asset_name).lower():
                source = str(data.package_name)
                break
        if source:
            break
    if not source:
        log("WARNING: no template wheel AnimBlueprint (install_vehicle_template.sh), wheels will not turn")
        return None
    target = f"{folder}/ABP_{name}"
    if eal.does_asset_exist(target):
        eal.delete_asset(target)
    abp = eal.duplicate_asset(source, target)
    abp.set_editor_property("target_skeleton", skeleton)
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    eal.save_loaded_asset(abp)
    log(f"wheel animation {target} (from {source})")
    return abp


def make_blueprint(folder, name, mesh, abp, glass):
    path = f"{folder}/BP_{name}"
    if not eal.does_asset_exist(path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.GTPVehicle)
        tools.create_asset(f"BP_{name}", folder, unreal.Blueprint, factory)
        log("created " + path)
    bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(eal.load_blueprint_class(path))
    body = cdo.get_editor_property("mesh")
    body.set_editor_property("skeletal_mesh_asset", mesh)
    if abp is not None:
        body.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
        body.set_editor_property("anim_class", abp.generated_class())
    cdo.set_editor_property("glass_meshes", {getattr(unreal.GTPDamageZone, ZONES[z]): sm for z, sm in glass.items()})
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_asset(path, only_if_is_dirty=False)
    log("saved " + path)
    return path


def main():
    vid = os.environ.get("GTP_VEHICLE_ID", "V-01")
    src = os.path.join(STAGING, vid)
    with open(os.path.join(src, "report.json")) as f:
        report = json.load(f)
    name = report["name"]
    folder = f"/Game/Vehicles/Cars/{name}"
    log(f"{vid} {name} from {src} to {folder}")

    body_master, glass_master = ensure_body_master(), ensure_glass_master()
    textures = import_textures(src, name, folder + "/Textures")
    body_mi = make_instance(f"MI_{name}_Body", folder, body_master, textures)
    glass_mi = make_instance(f"MI_{name}_Glass", folder, glass_master)

    paths = import_task(os.path.join(src, f"SKM_{name}.fbx"), folder, f"SKM_{name}")
    mesh = find_loaded(paths, unreal.SkeletalMesh)
    if mesh is None:
        raise RuntimeError(f"no skeletal mesh imported from SKM_{name}.fbx ({paths})")
    mesh = move_to(mesh, f"{folder}/SKM_{name}")
    b = mesh.get_bounds()
    log(f"bounds origin {b.origin}, half size {b.box_extent} (cm, expect about 202 x 102 x 73)")
    skeleton = mesh.get_editor_property("skeleton")
    morphs = [m.get_name() for m in mesh.get_editor_property("morph_targets")]
    log(f"skeleton {skeleton.get_path_name() if skeleton else None}, morph targets {morphs}")
    set_skeletal_materials(mesh, body_mi)

    pa = physics_asset_of(mesh, paths, folder, name)
    summary = unreal.GTPVehicleSetupLibrary.build_chassis_physics_asset(mesh, pa)
    if not summary:
        raise RuntimeError("chassis physics asset failed (root bone or wheel bones missing?)")
    log("chassis " + summary)
    eal.save_loaded_asset(pa)
    eal.save_loaded_asset(mesh)

    glass = {}
    for zone in ZONES:
        fbx = os.path.join(src, f"SM_{name}_Glass_{zone}.fbx")
        if not os.path.exists(fbx):
            continue
        sm = find_loaded(import_task(fbx, folder, f"SM_{name}_Glass_{zone}"), unreal.StaticMesh)
        if sm is None:
            raise RuntimeError("glass not imported: " + fbx)
        sm = move_to(sm, f"{folder}/SM_{name}_Glass_{zone}")
        sm.set_material(0, glass_mi)
        eal.save_loaded_asset(sm)
        glass[zone] = sm

    # Leftovers of the importer (materials and textures it made from the FBX).
    for entry in eal.list_assets(folder, recursive=True, include_folder=False):
        path = entry.split(".")[0]
        base = path.rsplit("/", 1)[1]
        if isinstance(unreal.load_asset(path), unreal.MaterialInterface) and not base.startswith("MI_"):
            eal.delete_asset(path)
            log("removed importer material " + path)

    abp = wheel_anim_blueprint(skeleton, folder, name)
    bp_path = make_blueprint(folder, name, mesh, abp, glass)
    log(f"DONE {bp_path}.BP_{name}_C")


main()
