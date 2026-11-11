"""Import the static bodies made by make_parked_static.py, for parked cars in instances.

Run in the editor commandlet (Mac slot, no other Unreal running):
    GTP_PARKED_IDS="V-01 V-02 V-03" UnrealEditor-Cmd <uproject> -run=pythonscript \
        -script=tools/vehicles/unreal/import_parked_static.py -unattended -nosplash -stdout

Output under /Game/Vehicles/Parked/: SM_Parked_<Name>_A (Nanite off, LODs 1/0.5/0.25/0.125,
box collision, tag CatalogId), T_Parked_<Name>_*, MI_Parked_<Name>, and the master
M_Parked_Body (BaseColor / ORM / Normal x a CarPaint tint read from PerInstanceCustomData 0..2,
white when not set, so the pavement team varies the colour per instance).
"""
import json
import os

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
# get_editor_subsystem() returns None for this subsystem in a commandlet: build it directly then.
sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.StaticMeshEditorSubsystem()

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
STAGING = os.environ.get("GTP_VEHICLE_STAGING", os.path.join(REPO, "assets", "staging", "vehicles"))
IDS = os.environ.get("GTP_PARKED_IDS", "V-01 V-02 V-03").split()
DEST = "/Game/Vehicles/Parked"
MASTER = DEST + "/M_Parked_Body"


def log(msg):
    unreal.log_warning("[import_parked] " + str(msg))


def import_one(path, dest, name, kind="static"):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    return unreal.load_asset(f"{dest}/{name}")


def tex_param(mat, pname, y, sampler):
    n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, y)
    n.set_editor_property("parameter_name", pname)
    n.set_editor_property("sampler_type", sampler)
    return n


def ensure_master(default_masks, default_normal, default_base):
    if eal.does_asset_exist(MASTER):
        return unreal.load_asset(MASTER)
    mat = tools.create_asset("M_Parked_Body", DEST, unreal.Material, unreal.MaterialFactoryNew())
    base = tex_param(mat, "BaseColor", -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    orm = tex_param(mat, "ORM", 0, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    nrm = tex_param(mat, "Normal", 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    base.set_editor_property("texture", default_base)
    orm.set_editor_property("texture", default_masks)
    nrm.set_editor_property("texture", default_normal)
    parts = []
    for i in range(3):
        c = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -700, 600 + 80 * i)
        c.set_editor_property("data_index", i)
        c.set_editor_property("const_default_value", 1.0)
        parts.append(c)
    # tint = (c0, c1, c2): append the three scalars, multiply with the base colour
    ap1 = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -500, 640)
    ap2 = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -350, 640)
    mel.connect_material_expressions(parts[0], "", ap1, "A")
    mel.connect_material_expressions(parts[1], "", ap1, "B")
    mel.connect_material_expressions(ap1, "", ap2, "A")
    mel.connect_material_expressions(parts[2], "", ap2, "B")
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, -200)
    mel.connect_material_expressions(base, "RGB", mul, "A")
    mel.connect_material_expressions(ap2, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def process(vid):
    root = os.path.join(STAGING, vid, "parked")
    meta = json.load(open(os.path.join(root, "parked.json"), encoding="utf-8"))
    name = meta["name"]
    tex = {}
    for kind in ("BC", "ORM", "N"):
        t = import_one(os.path.join(root, "textures", f"T_Parked_{name}_{kind}.png"), DEST, f"T_Parked_{name}_{kind}")
        if kind == "ORM":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            t.set_editor_property("srgb", False)
        if kind == "N":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("srgb", False)
        eal.save_loaded_asset(t)
        tex[kind] = t
    master = ensure_master(tex["ORM"], tex["N"], tex["BC"])
    mi_name = f"MI_Parked_{name}"
    if eal.does_asset_exist(f"{DEST}/{mi_name}"):
        eal.delete_asset(f"{DEST}/{mi_name}")
    mi = tools.create_asset(mi_name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master)
    for pname, kind in (("BaseColor", "BC"), ("ORM", "ORM"), ("Normal", "N")):
        mel.set_material_instance_texture_parameter_value(mi, pname, tex[kind])
    eal.save_loaded_asset(mi)

    sm_name = f"SM_Parked_{name}_A"
    mesh = import_one(os.path.join(root, f"{sm_name}.fbx"), DEST, sm_name)
    mesh.set_material(0, mi)
    ns = sm_sub.get_nanite_settings(mesh)
    ns.set_editor_property("enabled", False)
    sm_sub.set_nanite_settings(mesh, ns, True)
    sm_sub.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
    opts = unreal.EditorScriptingMeshReductionOptions()
    opts.set_editor_property("auto_compute_lod_screen_size", False)
    settings = []
    for k, screen in enumerate((1.0, 0.5, 0.25, 0.1)):  # cull distance 0.05 is set by the placement
        st = unreal.EditorScriptingMeshReductionSettings()
        st.set_editor_property("percent_triangles", 0.5 ** k)
        st.set_editor_property("screen_size", screen)
        settings.append(st)
    opts.set_editor_property("reduction_settings", settings)
    sm_sub.set_lods(mesh, opts)
    unreal.EditorAssetLibrary.set_metadata_tag(mesh, "CatalogId", vid)
    eal.save_loaded_asset(mesh)
    log(f"{vid}: {sm_name} ({meta['triangles']} tris)")


for v in IDS:
    process(v)
log("DONE")
