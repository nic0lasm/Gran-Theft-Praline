"""Step 3 of the far-crowd mesh (Unreal commandlet): import SM_PedFar_A, built by bake_ped_pose.py.

    GTP_PED_OUT=<staging>/ped UnrealEditor-Cmd <uproject> -run=pythonscript \
        -script=tools/pedestrians/import_ped_far.py -unattended -nosplash -stdout

Output /Game/Characters/Far/SM_PedFar_A: Nanite off, one material (MI_PedFar on M_PedFar, a flat
colour parameter Tint, roughness 0.8, no texture), LODs 1/0.5/0.25/0.125, no collision, tag
CatalogId=PF-A. No map is loaded.
"""
import json
import os

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
OUT = os.environ.get("GTP_PED_OUT", "")
DEST, NAME = "/Game/Characters/Far", "SM_PedFar_A"
MASTER = DEST + "/M_PedFar"


def log(msg):
    unreal.log_warning("[ped_far] " + str(msg))


def ensure_master():
    if eal.does_asset_exist(MASTER):
        return unreal.load_asset(MASTER)
    mat = tools.create_asset("M_PedFar", DEST, unreal.Material, unreal.MaterialFactoryNew())
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(0.32, 0.27, 0.22, 1.0))
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property("r", 0.8)
    mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


if not OUT:
    raise SystemExit("set GTP_PED_OUT")
for stray in (DEST + "/M_Parked",):  # left here by an earlier run
    if eal.does_asset_exist(stray):
        eal.delete_asset(stray)
        log(f"removed stray {stray}")
meta = json.load(open(os.path.join(OUT, "ped.json"), encoding="utf-8"))
task = unreal.AssetImportTask()
task.set_editor_property("filename", os.path.join(OUT, NAME + ".fbx"))
task.set_editor_property("destination_path", DEST)
task.set_editor_property("destination_name", NAME)
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", True)
tools.import_asset_tasks([task])
mesh = unreal.load_asset(f"{DEST}/{NAME}")
if mesh is None:
    raise RuntimeError("import failed")

master = ensure_master()
mi_path = f"{DEST}/MI_PedFar"
if eal.does_asset_exist(mi_path):
    eal.delete_asset(mi_path)
mi = tools.create_asset("MI_PedFar", DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mi.set_editor_property("parent", master)
eal.save_loaded_asset(mi)
mesh.set_material(0, mi)

sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
if sub is None:
    sub = unreal.StaticMeshEditorSubsystem()
ns = sub.get_nanite_settings(mesh)
ns.set_editor_property("enabled", False)
sub.set_nanite_settings(mesh, ns, True)
sub.remove_collisions(mesh)
opts = unreal.EditorScriptingMeshReductionOptions()
opts.set_editor_property("auto_compute_lod_screen_size", False)
settings = []
for k, screen in enumerate((1.0, 0.5, 0.25, 0.1)):
    st = unreal.EditorScriptingMeshReductionSettings()
    st.set_editor_property("percent_triangles", 0.5 ** k)
    st.set_editor_property("screen_size", screen)
    settings.append(st)
opts.set_editor_property("reduction_settings", settings)
sub.set_lods(mesh, opts)
eal.set_metadata_tag(mesh, "CatalogId", "PF-A")
eal.save_loaded_asset(mesh)
log(f"{NAME}: {meta['triangles']} tris, {meta['height_m']} m")
log("DONE")
