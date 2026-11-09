"""Batch-import prepared props into the Unreal project (runs INSIDE the editor).

Reads <staging>/<id>/asset.json written by tools/assets/prepare_props.py and creates:

  /Game/Props/<WAVE>/<ID>/SM_<ID>         M1 profile: classic LODs (auto-generated
                                          from the decimated base mesh), no Nanite
  /Game/Props/<WAVE>/Ultra/SM_<ID>_Ultra  Ultra profile: full master, Nanite
  /Game/Props/<WAVE>/<ID>/Textures/T_*    BaseColor / ORM / Normal with proper flags
  /Game/Props/<WAVE>/<ID>/MI_<ID>         material instance of /Game/Props/M_Prop_PBR

Run headless (see tools/assets/import-props-mac.sh):
  GTP_STAGING=<dir> GTP_WAVE=w1 [GTP_IDS="W1-01 W1-02"] UnrealEditor-Cmd <uproject> \
      -ExecutePythonScript=tools/assets/unreal/import_props.py -unattended

Idempotent: assets are replaced on re-import. Never touches anything outside /Game/Props.
"""
import json
import os

import unreal

STAGING = os.environ.get("GTP_STAGING", "")
WAVE = os.environ.get("GTP_WAVE", "w1").upper()
ONLY = set(os.environ.get("GTP_IDS", "").split())
MASTER_MATERIAL = "/Game/Props/M_Prop_PBR"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
sm_subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def log(msg):
    unreal.log(f"[GTP import] {msg}")


# ---------------------------------------------------------------- master material
def ensure_master_material():
    """One shared opaque PBR material with BaseColor / ORM / Normal texture parameters."""
    if eal.does_asset_exist(MASTER_MATERIAL):
        return unreal.load_asset(MASTER_MATERIAL)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    folder, name = MASTER_MATERIAL.rsplit("/", 1)
    mat = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())

    def tex_param(pname, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -500, y)
        node.set_editor_property("parameter_name", pname)
        return node

    base = tex_param("BaseColor", -300)
    orm = tex_param("ORM", 0)
    normal = tex_param("Normal", 300)
    orm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


# ---------------------------------------------------------------- import helpers
def import_files(items, dest):
    """items: list of (source file, asset name). Returns list of imported asset paths."""
    tasks = []
    for src, name in items:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", src)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("destination_name", name)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    return [p for t in tasks for p in t.get_editor_property("imported_object_paths")]


def import_textures(meta, asset_dir, tex_dir):
    out = {}
    jobs = []
    for mat in meta["materials"]:
        for kind, fname in mat["textures"].items():
            stem = os.path.splitext(fname)[0]
            jobs.append((os.path.join(asset_dir, "textures", fname), stem))
    import_files(jobs, tex_dir)
    for mat in meta["materials"]:
        for kind, fname in mat["textures"].items():
            path = f"{tex_dir}/{os.path.splitext(fname)[0]}"
            tex = unreal.load_asset(path)
            if tex is None:
                raise RuntimeError(f"texture not imported: {path}")
            if kind == "base_color":
                tex.set_editor_property("srgb", True)
            elif kind == "orm":
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            else:
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            eal.save_loaded_asset(tex)
            out[(mat["slot"], kind)] = tex
    return out


def make_material_instance(master, meta, slot_info, textures, folder):
    name = f"MI_{meta['id'].replace('-', '_')}_m{slot_info['slot']}"
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = tools.create_asset(name, folder, unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master)
    for pname, kind in (("BaseColor", "base_color"), ("ORM", "orm"), ("Normal", "normal")):
        mel.set_material_instance_texture_parameter_value(mi, pname, textures[(slot_info["slot"], kind)])
    eal.save_loaded_asset(mi)
    return mi


def apply_materials(mesh, material_instances):
    for i, mi in enumerate(material_instances):
        mesh.set_material(i, mi)


def apply_collision(mesh, recipe):
    sm_subsystem.remove_collisions(mesh)
    kind = recipe.get("type", "none")
    if kind == "none":
        return
    if kind == "box":
        sm_subsystem.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
    else:
        sm_subsystem.set_convex_decomposition_collisions(mesh, int(recipe.get("hulls", 1)), 16, 100000)


def build_lods(mesh, plan):
    options = unreal.EditorScriptingMeshReductionOptions()
    options.set_editor_property("auto_compute_lod_screen_size", False)
    settings = []
    for entry in plan:
        s = unreal.EditorScriptingMeshReductionSettings()
        s.set_editor_property("percent_triangles", entry["percent_triangles"])
        s.set_editor_property("screen_size", entry["screen_size"])
        settings.append(s)
    options.set_editor_property("reduction_settings", settings)
    sm_subsystem.set_lods(mesh, options)


def set_nanite(mesh, enabled):
    ns = sm_subsystem.get_nanite_settings(mesh)
    ns.set_editor_property("enabled", enabled)
    sm_subsystem.set_nanite_settings(mesh, ns, True)


def import_mesh(glb, dest, name):
    paths = import_files([(glb, name)], dest)
    mesh = unreal.load_asset(f"{dest}/{name}")
    if mesh is None or not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"static mesh not imported from {glb} (got {paths})")
    return mesh


def drop_importer_materials(dest, meta):
    """The glTF importer creates placeholder materials named after our slots."""
    for entry in unreal.EditorAssetLibrary.list_assets(dest, recursive=True, include_folder=False):
        if "M_Imported_" in entry:
            eal.delete_asset(entry.split(".")[0])


# ---------------------------------------------------------------- per asset
def process(meta_path, master):
    asset_dir = os.path.dirname(meta_path)
    with open(meta_path, encoding="utf-8") as f:
        meta = json.load(f)
    aid = meta["id"]
    safe = aid.replace("-", "_")
    base = f"/Game/Props/{WAVE}/{aid}"
    ultra_dir = f"/Game/Props/{WAVE}/Ultra"

    textures = import_textures(meta, asset_dir, f"{base}/Textures")
    mis = [make_material_instance(master, meta, s, textures, base) for s in meta["materials"]]

    m1 = import_mesh(os.path.join(asset_dir, meta["files"]["m1"]), base, f"SM_{safe}")
    apply_materials(m1, mis)
    set_nanite(m1, False)
    apply_collision(m1, meta["collision"])
    build_lods(m1, meta["lod_plan"])
    eal.save_loaded_asset(m1)

    ultra = import_mesh(os.path.join(asset_dir, meta["files"]["ultra"]), ultra_dir, f"SM_{safe}_Ultra")
    apply_materials(ultra, mis)
    set_nanite(ultra, True)
    apply_collision(ultra, meta["collision"])
    eal.save_loaded_asset(ultra)

    drop_importer_materials(base, meta)
    drop_importer_materials(ultra_dir, meta)
    log(f"{aid}: SM_{safe} ({meta['triangles']['m1']} tris, {len(meta['lod_plan'])} LODs) "
        f"+ SM_{safe}_Ultra ({meta['triangles']['ultra']} tris, Nanite)")


def main():
    if not STAGING or not os.path.isdir(STAGING):
        raise SystemExit(f"GTP_STAGING must point to the staging folder (got {STAGING!r})")
    master = ensure_master_material()
    ok, failed = [], []
    for name in sorted(os.listdir(STAGING)):
        meta_path = os.path.join(STAGING, name, "asset.json")
        if not os.path.exists(meta_path) or (ONLY and name.upper() not in {i.upper() for i in ONLY}):
            continue
        try:
            process(meta_path, master)
            ok.append(name)
        except Exception as exc:  # keep going: one bad asset must not stop the batch
            unreal.log_error(f"[GTP import] {name} failed: {exc}")
            failed.append(name)
    log(f"done: {len(ok)} imported, {len(failed)} failed {failed}")
    if failed:
        raise SystemExit(1)


main()
