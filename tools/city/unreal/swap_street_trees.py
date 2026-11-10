"""Stage a leafy copy of the street trees on the city map, or remove it.

Runs inside the Unreal Editor like place_props.py (no UI, script copied to a path
without spaces), after place_props.py: it puts every Grand Lyon tree in one
hierarchical instanced static mesh per catalogue id, W14-63 (bare plane tree) and
W14-64 (guarded sapling). Stage adds one "Props_LeafyTrees_<id>" actor per id
with the same instances in the same order and a leafy mesh, without collision,
and tags both (GTP_LeafyTrees / GTP_BareTrees, GTP_TreeId_<id>). At run time
UGTPStreetTreesSubsystem shows the bare trees unless -Trees=leafy|light|impostor
is given (see GTPStreetTrees.h), so one map and one package hold every variant
and tools/perf/bench.sh compares them with game arguments.

Environment:
  GTP_TREE_MODE      stage (default) | unstage | list
  GTP_TREE_MESH      asset path of the mature tree, or "auto" (default): among the
                     SimpleWind meshes of the pack, the one with the fewest LOD 0
                     triangles that is at least GTP_TREE_MIN_HEIGHT_CM tall (800)
  GTP_SAPLING_MESH   asset path for W14-64; default: lightest sapling, else the
                     mature tree
  GTP_TREE_PATTERN   regular expression on asset paths (default: Maple|Acer|Beech|Alder)
  GTP_CITY_FOCUS     as for place_props.py

Report: Saved/CityImport/trees_report.json (mesh, LOD count, triangles per LOD,
Nanite, height of each picked mesh; list mode gives the same for every candidate).
"""

import json
import os
import re
import time

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
MAP = "/Game/Maps/L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))
OUT = os.path.join(REPO, "GranTheftPraline", "Saved", "CityImport")
TREE_IDS = ("W14-63", "W14-64")
LEAFY_PREFIX = "Props_LeafyTrees_"
SEARCH_ROOTS = ("/Game/Fab", "/Game/Megascans", "/Game/MSPresets", "/Game")

eal = unreal.EditorAssetLibrary
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def safe(fn, default=None):
    try:
        return fn()
    except Exception:
        return default


def bare_components():
    """catalogue id -> HISM component of the Props_<id>_* actor."""
    out = {}
    for a in actor_sub.get_all_level_actors():
        label = a.get_actor_label()
        for cid in TREE_IDS:
            if label.startswith("Props_" + cid + "_"):
                comps = a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
                if comps:
                    out[cid] = comps[0]
    return out


def leafy_actors():
    return [a for a in actor_sub.get_all_level_actors() if a.get_actor_label().startswith(LEAFY_PREFIX)]


def hism_actor(label, mesh):
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    actor.set_actor_label(label)
    actor.set_folder_path("City/Props")
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, _ = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
    comp = sdl.get_object(sdl.get_data(handle))
    comp.set_static_mesh(mesh)
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    comp.set_collision_profile_name("BlockAll")
    return comp


def candidates(pattern):
    """Static meshes whose path matches the pattern, outside our own props."""
    rx = re.compile(pattern, re.IGNORECASE)
    seen, out = set(), []
    for root in SEARCH_ROOTS:
        if not eal.does_directory_exist(root):
            continue
        for path in eal.list_assets(root, recursive=True, include_folder=False):
            if path in seen or path.startswith("/Game/Environment/") or not rx.search(path):
                continue
            seen.add(path)
            if str(eal.find_asset_data(path).asset_class_path.asset_name) == "StaticMesh":
                out.append(path.split(".")[0])
        if out:
            break
    return sorted(out)


def mesh_info(path):
    mesh = eal.load_asset(path)
    ext = safe(lambda: mesh.get_bounds().box_extent)
    lods = safe(lambda: sm_sub.get_lod_count(mesh), 0) or 0
    return {"path": path, "lods": lods,
            "triangles": [safe(lambda i=i: mesh.get_num_triangles(i), -1) for i in range(lods)],
            "nanite": safe(lambda: bool(mesh.get_editor_property("nanite_settings").enabled), False),
            "height_cm": round(2 * ext.z) if ext else 0}


def lightest(paths, min_height):
    infos = [mesh_info(p) for p in paths]
    ok = [i for i in infos if i["height_cm"] >= min_height and i["triangles"]] or [i for i in infos if i["triangles"]]
    return min(ok, key=lambda i: i["triangles"][0])["path"] if ok else None


def pick(paths, want_sapling, min_height):
    young = [p for p in paths if re.search(r"sapling|seedling|young", p, re.IGNORECASE)]
    if want_sapling:
        return lightest([p for p in young if "seedling" not in p.lower()] or young, 0)
    # SimpleWind meshes are the plain ones; PivotPainter variants need their own material setup
    mature = [p for p in paths if p not in young and not re.search(r"pivotpainter|impostor", p, re.IGNORECASE)]
    return lightest(mature, min_height)


def tag(actor, *names):
    tags = [t for t in actor.get_editor_property("tags") if not str(t).startswith("GTP_")]
    actor.set_editor_property("tags", tags + [unreal.Name(n) for n in names])


def unstage(comps, report):
    for a in leafy_actors():
        actor_sub.destroy_actor(a)
        report["removed"].append(a.get_name())
    for comp in comps.values():
        tag(comp.get_owner())


def stage(comps, report):
    """Leafy copy of every tree, hidden at run time unless -Trees=leafy|light|impostor."""
    pattern = os.environ.get("GTP_TREE_PATTERN", "Maple|Acer|Beech|Alder")
    min_h = int(os.environ.get("GTP_TREE_MIN_HEIGHT_CM", "800"))
    found = candidates(pattern)
    want = os.environ.get("GTP_TREE_MESH", "auto")
    mature = pick(found, False, min_h) if want in ("", "auto") else want
    sapling = os.environ.get("GTP_SAPLING_MESH") or pick(found, True, 0) or mature
    report["candidates"] = found[:60]
    if not mature or not eal.does_asset_exist(mature):
        report["errors"].append("no tree mesh found (pattern %r); import the Fab packs first" % pattern)
        return
    meshes = {"W14-63": mature, "W14-64": sapling}
    for cid, comp in sorted(comps.items()):
        mesh = eal.load_asset(meshes[cid])
        leafy = hism_actor(LEAFY_PREFIX + cid, mesh)
        # the bare trunk keeps the collision; the copy only draws
        leafy.set_collision_profile_name("NoCollision")
        for i in range(comp.get_instance_count()):
            leafy.add_instance(comp.get_instance_transform(i, True), True)
        tag(leafy.get_owner(), "GTP_LeafyTrees", "GTP_TreeId_" + cid)
        tag(comp.get_owner(), "GTP_BareTrees", "GTP_TreeId_" + cid)
        report["staged"][cid] = {"mesh": meshes[cid], "trees": comp.get_instance_count(),
                                 "mesh_info": mesh_info(meshes[cid])}


def write(report, t0):
    report["seconds"] = round(time.time() - t0, 1)
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "trees_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    unreal.log("swap_street_trees: " + json.dumps(report))


def main():
    t0 = time.time()
    mode = os.environ.get("GTP_TREE_MODE", "stage")
    report = {"map": MAP, "mode": mode, "errors": [], "staged": {}, "removed": []}
    if mode == "list":
        found = candidates(os.environ.get("GTP_TREE_PATTERN", "Maple|Acer|Beech|Alder"))
        report["candidates"] = [mesh_info(p) for p in found]
        return write(report, t0)
    level_sub.load_level(MAP)
    comps = bare_components()
    if not comps:
        report["errors"].append("no Props_W14-63/64 actor on the map: run place_props.py first")
        return write(report, t0)
    unstage(comps, report)  # stage always starts from the bare map
    if mode == "stage":
        stage(comps, report)
    report["trees"] = {cid: c.get_instance_count() for cid, c in comps.items()}
    level_sub.save_current_level()
    write(report, t0)


main()
