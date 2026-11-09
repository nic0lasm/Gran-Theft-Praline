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
  4. shop signs: the real logo when the brand's texture exists in CityBrands
     (/CityBrands/Logos/T_Brand_<Wikidata QID>, kept out of Git), otherwise a
     text actor with the shop's real name; folder City/Signs
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
LOGO_DIR = "/CityBrands/Logos"
SIZE_TOLERANCE_CM = 40.0

eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


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
    comp.set_editor_property("num_custom_data_floats", 3)
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    return actor, comp


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

    top = {}
    for r in rows:
        top[r["building_id"]] = max(top.get(r["building_id"], 0), int(r["storey"]))
    comps, counts = {}, {}

    def add(name, x, y, z, yaw, sx, sz, colour):  # noqa: PLR0913
        if name not in comps:
            comps[name] = hism_actor("Facade_" + name.replace("SM_Facade_", ""), meshes[name])[1]
        t = unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw + yaw_offset), unreal.Vector(sx, 1.0, sz))
        i = comps[name].add_instance(t, True)
        rgb = [srgb_to_linear(int(colour.lstrip("#")[k:k + 2], 16) / 255.0) for k in (0, 2, 4)]
        for k, v in enumerate(rgb):
            comps[name].set_custom_data_value(i, k, v, False)
        counts[name] = counts.get(name, 0) + 1

    signs = logos = 0
    for r in rows:
        name = kit_layout.module_for(r["typology"], r["kind"])
        nominal = manifest["modules"][name]
        x, y, z, yaw = float(r["ue_x"]), float(r["ue_y"]), float(r["ue_z"]), float(r["ue_yaw"])
        w, h = float(r["width_m"]), float(r["height_m"])
        add(name, x, y, z, yaw, w / nominal["nominal_width_m"], h / nominal["nominal_height_m"], r["colour"])
        fam = kit_layout.FAMILY_OF_TYPOLOGY[r["typology"]]
        if r["side"] == "street" and int(r["storey"]) == top[r["building_id"]]:
            eave = kit_layout.KIND_TO_MODULE[fam]["_eave"]
            add(eave, x, y, z + h * 100, yaw, w / manifest["modules"][eave]["nominal_width_m"], 1.0, r["colour"])
        if r["kind"] == "shop" and r["sign_continued"] == "False" and (r["sign_brand"] or r["sign_name"]):
            nh = nominal["nominal_height_m"]
            zf = z + h * 100 * ((nh - 1.125) / nh if fam == "haussmann" else (nh - 1.075) / nh)
            out = 3.0 if fam == "haussmann" else 18.0
            a = math.radians(yaw)
            loc = unreal.Vector(x + math.cos(a) * out, y + math.sin(a) * out, zf)
            logo_path = "%s/T_Brand_%s" % (LOGO_DIR, r["sign_wikidata"]) if r["sign_wikidata"] else ""
            if logo_path and eal.does_asset_exist(logo_path):
                decal = actor_sub.spawn_actor_from_class(unreal.DecalActor, loc, unreal.Rotator(0, 0, yaw + 180))
                decal.set_actor_label("Sign_" + r["sign_wikidata"])
                decal.set_folder_path("City/Signs")
                decal.decal.set_editor_property("decal_size", unreal.Vector(10, w * 40, 30))
                decal.set_editor_property("tags", [r["sign_wikidata"]])  # material bound by the CityBrands plugin
                logos += 1
            else:
                txt = actor_sub.spawn_actor_from_class(unreal.TextRenderActor, loc, unreal.Rotator(0, 0, yaw))
                txt.set_actor_label("Sign_" + (r["sign_brand"] or r["sign_name"])[:40])
                txt.set_folder_path("City/Signs")
                c = txt.text_render
                c.set_editor_property("text", r["sign_brand"] or r["sign_name"])
                c.set_editor_property("world_size", 30.0)
                c.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
                c.set_editor_property("vertical_alignment", unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
            signs += 1

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
