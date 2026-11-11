"""Pick the leafy meshes that replace the bare street trees at run time. Read only.

Runs inside the Unreal Editor like place_props.py (no UI, script copied to a path
without spaces). place_props.py puts every Grand Lyon tree in one hierarchical
instanced static mesh per catalogue id: W14-63 (bare plane tree) and W14-64
(guarded young tree). This script loads the map without saving it, finds those
two meshes and picks a leafy mesh for each; the report gives the -TreeSwap game
argument and the content folders the package must cook. UGTPStreetTreesSubsystem
does the swap at run time (see GTPStreetTrees.h): the map file never changes.

Environment:
  GTP_TREE_MODE      pick (default) | list
  GTP_TREE_MESH      asset path of the mature tree, or "auto" (default): among the
                     SimpleWind meshes of the pack, the one with the fewest LOD 0
                     triangles that is at least GTP_TREE_MIN_HEIGHT_CM tall (800)
  GTP_SAPLING_MESH   asset path for W14-64; default: lightest sapling, else the
                     mature tree
  GTP_TREE_PATTERN   regular expression on asset paths (default: Maple|Acer|Beech|Alder)
  GTP_CITY_FOCUS     as for place_props.py

Report: Saved/CityImport/trees_report.json (swap_arg, cook_dirs, and for each
picked mesh its LOD count, triangles per LOD, Nanite and height).
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
CHOSEN = "/Game/BlackAlder/M1/SM_BlackAlder_Field_05_M1"
SEARCH_ROOTS = ("/Game/Fab", "/Game/Megascans", "/Game/MSPresets", "/Game")

eal = unreal.EditorAssetLibrary
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


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


def pick_swaps(comps, report):
    """Leafy mesh for each bare tree mesh, as -TreeSwap and the folders to cook."""
    pattern = os.environ.get("GTP_TREE_PATTERN", "Maple|Acer|Beech|Alder")
    min_h = int(os.environ.get("GTP_TREE_MIN_HEIGHT_CM", "800"))
    found = candidates(pattern)
    # default: the tree chosen for the street (DefaultGame.ini), else the lightest one
    want = os.environ.get("GTP_TREE_MESH") or (CHOSEN if eal.does_asset_exist(CHOSEN) else "auto")
    mature = pick(found, False, min_h) if want in ("", "auto") else want
    sapling = os.environ.get("GTP_SAPLING_MESH") or pick(found, True, 0) or mature
    report["candidates"] = found[:60]
    if not mature or not eal.does_asset_exist(mature):
        report["errors"].append("no tree mesh found (pattern %r); import the Fab packs first" % pattern)
        return
    meshes = {"W14-63": mature, "W14-64": sapling}
    pairs, dirs = [], set()
    for cid, comp in sorted(comps.items()):
        bare = comp.static_mesh.get_outermost().get_name() if comp.static_mesh else ""
        if not bare:
            continue
        pairs.append(bare + "~" + meshes[cid])
        dirs.add(meshes[cid].rsplit("/", 1)[0])
        report["picked"][cid] = {"bare": bare, "mesh": meshes[cid], "trees": comp.get_instance_count(),
                                 "mesh_info": mesh_info(meshes[cid])}
    report["swap_arg"] = "+".join(pairs)
    report["cook_dirs"] = sorted(dirs)


def write(report, t0):
    report["seconds"] = round(time.time() - t0, 1)
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "trees_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    unreal.log("swap_street_trees: " + json.dumps(report))


def main():
    t0 = time.time()
    mode = os.environ.get("GTP_TREE_MODE", "pick")
    report = {"map": MAP, "mode": mode, "errors": [], "picked": {}}
    if mode == "list":
        found = candidates(os.environ.get("GTP_TREE_PATTERN", "Maple|Acer|Beech|Alder"))
        report["candidates"] = [mesh_info(p) for p in found]
        return write(report, t0)
    # Read only: the map is loaded to find the bare tree meshes, never saved.
    level_sub.load_level(MAP)
    comps = bare_components()
    if not comps:
        report["errors"].append("no Props_W14-63/64 actor on the map: run place_props.py first")
        return write(report, t0)
    pick_swaps(comps, report)
    write(report, t0)


main()
