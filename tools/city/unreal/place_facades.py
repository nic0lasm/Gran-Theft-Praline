"""Place the Lyon facade kit on the generated city, inside the Unreal Editor.

Run after import_city.py, same way (no UI, script copied to a path without spaces):
  UnrealEditor GranTheftPraline.uproject -unattended -nosplash -ExecutePythonScript=/tmp/place_facades.py
Settings: GTP_CITY_ZONE (presqu-ile), GTP_CITY_FOCUS (rue-de-la-republique).

Input: gen/<focus>/facade_modules.csv (generate.py), gen/<focus>/kit/facade_kit.glb and
kit_manifest.json (facade_kit/assemble.py), the map /Game/Maps/L_City_<Focus> (import_city.py).

Steps (idempotent):
  1. import the kit into /Game/Environment/Facades/LyonKit/ and check every
     module's size against its nominal size (catches an axis convention change)
  2. open the city map, delete the previous facade actors (folder City/Facades)
  3. one actor per module type holding a hierarchical instanced static mesh
     component; one instance per row of facade_modules.csv, plus a cornice or
     genoise over every street bay of the top storey (same rule as assemble.py).
     Per-instance custom data 0-2 = the building's render colour (linear RGB)
     for M_Master_Facade.
  4. shop signs (tools/city/signs.py): a panel on the fascia of every bay of the
     shop, with the sign texture: the brand's logo when the out-of-Git CityBrands
     plugin is mounted (/CityBrands/Signs/), otherwise the shop's real name in its
     colours (/Game/Environment/City/Signs/); folder City/Signs
  Module variants (shopfronts, window surrounds) are picked per building by
  kit_layout.module_for, the same rule as assemble.py.
  5. save the map, report in Saved/CityImport/facades_report.json
import_city.py places the building shells (roofs, party walls) by default, so
the kit is not doubled by the greybox modules.
"""

import csv
import json
import math
import os
import sys
import time

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "city", "facade_kit"))
import kit_layout  # noqa: E402

ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
GEN = os.path.join(REPO, "assets", "geo", "processed", ZONE, "gen", FOCUS)
KIT_DIR = "/Game/Environment/Facades/LyonKit"
MAP = "/Game/Maps/L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))
SIGN_DIR = "/Game/Environment/City/Signs/" + "".join(p.capitalize() for p in FOCUS.split("-"))
BRAND_SIGN_DIR = "/CityBrands/Signs/" + "".join(p.capitalize() for p in FOCUS.split("-"))
SIZE_TOLERANCE_CM = 40.0

# Draw distances per module family, metres (fil optimisation: numbers to tune after the m1
# measure). Culling starts at CULL_START_SHARE of the end to avoid popping. 0 = never culled.
CULL_END_M = {"ground": 250.0, "upper": 250.0, "eave": 250.0, "court": 250.0, "sign": 150.0}
CULL_START_SHARE = 0.8


def module_family(name):
    n = name.lower()
    if "cornice" in n or "genoise" in n:
        return "eave"
    if "court" in n:
        return "court"
    if any(k in n for k in ("shop", "entrance", "groundwindow", "entresol")):
        return "ground"
    return "upper"


def set_cull(comp, family, report):
    end = CULL_END_M[family] * 100.0
    if not end:
        return
    if isinstance(comp, unreal.InstancedStaticMeshComponent):
        comp.set_editor_property("instance_start_cull_distance", int(end * CULL_START_SHARE))
        comp.set_editor_property("instance_end_cull_distance", int(end))
    else:
        comp.set_editor_property("ld_max_draw_distance", end)
    report.setdefault("cull_end_m", {})[family] = CULL_END_M[family]

eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning("[CityFacades] " + msg)


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def import_kit():
    if eal.does_directory_exist(KIT_DIR):
        eal.delete_directory(KIT_DIR)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(GEN, "kit", "facade_kit.glb"))
    task.set_editor_property("destination_path", KIT_DIR)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    out = {}
    for p in eal.list_assets(KIT_DIR, recursive=True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            out[a.get_name()] = a
    return out


def check_sizes(meshes, manifest):
    """Module width along X and height along Z must match the nominal bay (cm)."""
    bad = []
    for name, m in manifest["modules"].items():
        mesh = meshes.get(name)
        if mesh is None:
            bad.append((name, "missing"))
            continue
        if m["nominal_height_m"] is None:
            continue
        box = mesh.get_bounding_box()
        w, h = box.max.x - box.min.x, box.max.z - box.min.z
        # balconies and cornices stick out, so width can only be checked from above
        if w < m["nominal_width_m"] * 100 - SIZE_TOLERANCE_CM or abs(h - m["nominal_height_m"] * 100) > \
                SIZE_TOLERANCE_CM + 50:
            bad.append((name, round(w), round(h)))
    return bad


def hism_actor(label, mesh):
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    actor.set_actor_label(label)
    actor.set_folder_path("City/Facades")
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root,
                                          new_class=unreal.HierarchicalInstancedStaticMeshComponent)
    handle, fail = sds.add_new_subobject(params)
    comp = sdl.get_object(sdl.get_data(handle))
    comp.set_static_mesh(mesh)
    comp.set_editor_property("num_custom_data_floats", 4)  # building colour, street level
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    return actor, comp


def import_textures(paths, dest):
    tasks = []
    for p in paths:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", p)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    if tasks:
        asset_tools.import_asset_tasks(tasks)


def sign_parent():
    """M_Sign_Fascia: the sign texture as base colour, a little emissive (back-lit letters)."""
    path = SIGN_DIR + "/M_Sign_Fascia"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    mat = asset_tools.create_asset("M_Sign_Fascia", SIGN_DIR, unreal.Material, unreal.MaterialFactoryNew())
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    tex.set_editor_property("parameter_name", "SignTexture")
    mel.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 200)
    glow.set_editor_property("const_b", 0.15)
    mel.connect_material_expressions(tex, "RGB", glow, "A")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 350)
    rough.set_editor_property("r", 0.35)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def sign_materials(report):
    """shop_osm_id -> (material instance, uses the logo) from gen/<focus>/signs (tools/city/signs.py).

    Lettering textures (the real name, no artwork) go under /Game; logo textures
    only when the out-of-Git CityBrands plugin is mounted, under /CityBrands/Signs."""
    signs_dir = os.path.join(GEN, "signs")
    csv_path = os.path.join(signs_dir, "signs.csv")
    if not os.path.exists(csv_path):
        report["errors"].append("no signs.csv: run tools/city/signs.py; shops left without signs")
        return {}
    with open(csv_path, encoding="utf-8") as f:
        rows = [r for r in csv.DictReader(f) if r["has_fascia"] == "True"]
    brands = eal.does_directory_exist("/CityBrands")
    if eal.does_directory_exist(SIGN_DIR):
        eal.delete_directory(SIGN_DIR)
    parent = sign_parent()
    import_textures([os.path.join(signs_dir, r["texture"] + ".png") for r in rows], SIGN_DIR + "/Textures")
    if brands:
        import_textures([os.path.join(signs_dir, r["logo_texture"] + ".png") for r in rows if r["logo_texture"]],
                        BRAND_SIGN_DIR)
    out = {}
    for r in rows:
        use_logo = brands and bool(r["logo_texture"])
        tex_path = "%s/%s" % (BRAND_SIGN_DIR, r["logo_texture"]) if use_logo else \
            "%s/Textures/%s" % (SIGN_DIR, r["texture"])
        tex = eal.load_asset(tex_path)
        if tex is None:
            report["errors"].append("sign texture not imported: " + tex_path)
            continue
        mic_dir = BRAND_SIGN_DIR if use_logo else SIGN_DIR + "/Materials"
        mic = asset_tools.create_asset("MI_Sign_" + r["sign_id"], mic_dir, unreal.MaterialInstanceConstant,
                                       unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mic, parent)
        mel.set_material_instance_texture_parameter_value(mic, "SignTexture", tex)
        eal.save_loaded_asset(mic)
        out[r["shop_osm_id"]] = (mic, use_logo)
    report["sign_textures"] = len(out)
    report["citybrands_mounted"] = brands
    return out


def main():
    t0 = time.time()
    report = {"zone": ZONE, "focus": FOCUS, "map": MAP, "errors": []}
    with open(os.path.join(GEN, "kit", "kit_manifest.json")) as f:
        manifest = json.load(f)
    with open(os.path.join(GEN, "facade_modules.csv"), encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    meshes = import_kit()
    bad = check_sizes(meshes, manifest)
    report["kit_modules"] = len(meshes)
    if bad:
        report["errors"].append("kit size check failed: %s" % (bad[:4],))
        log("STOP: kit modules do not have their nominal size: %s" % (bad[:4],))
        return write(report, t0)

    # Which local side is the outside? Balconies stick out 0.88 m in front of the wall.
    box = meshes["SM_Facade_Haussmann_WindowBalcony"].get_bounding_box()
    yaw_offset = -90.0 if box.max.y > 60 else 90.0 if box.min.y < -60 else None
    report["kit_outward_axis"] = {-90.0: "+Y", 90.0: "-Y", None: "unknown"}[yaw_offset]
    if yaw_offset is None:
        report["errors"].append("cannot tell the outside of the kit modules (balcony bounds %s)" % box)
        return write(report, t0)

    level_sub.load_level(MAP)
    for a in actor_sub.get_all_level_actors():
        if str(a.get_folder_path()).startswith(("City/Facades", "City/Signs")):
            actor_sub.destroy_actor(a)

    top, base = {}, {}
    for r in rows:
        top[r["building_id"]] = max(top.get(r["building_id"], 0), int(r["storey"]))
        base[r["building_id"]] = min(base.get(r["building_id"], 1e9), float(r["ue_z"]))
    comps, counts = {}, {}

    def add(name, x, y, z, yaw, sx, sz, colour, ground):  # noqa: PLR0913
        if name not in comps:
            comps[name] = hism_actor("Facade_" + name.replace("SM_Facade_", ""), meshes[name])[1]
            set_cull(comps[name], module_family(name), report)
            if meshes[name].get_num_lods() < 2:
                report.setdefault("modules_without_lods", []).append(name)
        t = unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw + yaw_offset), unreal.Vector(sx, 1.0, sz))
        i = comps[name].add_instance(t, True)
        rgb = [srgb_to_linear(int(colour.lstrip("#")[k:k + 2], 16) / 255.0) for k in (0, 2, 4)]
        for k, v in enumerate(rgb):
            comps[name].set_custom_data_value(i, k, v, False)
        # Street level of the building (cm): M_City_Kit darkens the plinth from it.
        comps[name].set_custom_data_value(i, 3, ground, False)
        counts[name] = counts.get(name, 0) + 1

    sign_mics = sign_materials(report)
    signs = logos = 0
    for r in rows:
        name = kit_layout.module_for(r["typology"], r["kind"], r["building_id"])
        nominal = manifest["modules"][name]
        x, y, z, yaw = float(r["ue_x"]), float(r["ue_y"]), float(r["ue_z"]), float(r["ue_yaw"])
        w, h = float(r["width_m"]), float(r["height_m"])
        add(name, x, y, z, yaw, w / nominal["nominal_width_m"], h / nominal["nominal_height_m"], r["colour"],
            base[r["building_id"]])
        fam = kit_layout.FAMILY_OF_TYPOLOGY[r["typology"]]
        if r["side"] == "street" and int(r["storey"]) == top[r["building_id"]]:
            eave = kit_layout.KIND_TO_MODULE[fam]["_eave"]
            add(eave, x, y, z + h * 100, yaw, w / manifest["modules"][eave]["nominal_width_m"], 1.0, r["colour"],
                base[r["building_id"]])
        mic = sign_mics.get(r["shop_osm_id"]) if r["kind"] == "shop" else None
        if mic is None:
            continue
        # one panel on every bay of the shop, on the fascia of the module variant placed above
        zc, out_m = kit_layout.sign_frame(name, fam, h, nominal["nominal_height_m"])
        fa = kit_layout.FASCIA[fam]
        a = math.radians(yaw)
        loc = unreal.Vector(x + math.cos(a) * out_m * 100, y + math.sin(a) * out_m * 100, z + zc * 100)
        panel = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, loc,
                                                 unreal.Rotator(0.0, 0.0, yaw + yaw_offset))
        panel.set_actor_label("Sign_%s_%s" % (r["shop_osm_id"].replace("/", "_"), r["bay"]))
        panel.set_folder_path("City/Signs")
        panel.set_actor_scale3d(unreal.Vector(w * fa["width_share"] - 0.04, 1.0,
                                              fa["height_m"] * h / nominal["nominal_height_m"] - 0.04))
        comp = panel.static_mesh_component
        comp.set_static_mesh(meshes["SM_Sign_Panel"])
        comp.set_material(0, mic[0])
        comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        set_cull(comp, "sign", report)
        signs += 1
        logos += 1 if mic[1] else 0

    level_sub.save_current_level()
    report.update({"instances": counts, "instances_total": sum(counts.values()), "signs": signs,
                   "signs_with_logo": logos})
    write(report, t0)


def write(report, t0):
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "facades_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    log("report: " + json.dumps(report))


main()
