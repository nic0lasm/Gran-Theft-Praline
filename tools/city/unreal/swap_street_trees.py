"""Swap the bare street trees of the city map for a leafy tree, or put them back.

Runs inside the Unreal Editor like place_props.py (no UI, script copied to a path
without spaces). place_props.py puts every Grand Lyon tree in one hierarchical
instanced static mesh per catalogue id: W14-63 (bare plane tree) and W14-64
(guarded sapling). This script only changes the mesh of those two components, so
positions, ground snapping and collision stay as they are, and a later
place_props.py run restores the bare trees anyway.

Environment:
  GTP_TREE_MODE      apply (default) | revert | list
  GTP_TREE_MESH      asset path of the mature tree; default: first match of
                     GTP_TREE_PATTERN under /Game, field trees preferred
  GTP_SAPLING_MESH   asset path for W14-64; default: a sapling of the same pack,
                     else the mature tree
  GTP_TREE_PATTERN   regular expression on asset paths (default: Maple|Acer|Beech|Alder)
  GTP_CITY_ZONE, GTP_CITY_FOCUS  as for place_props.py

Report: Saved/CityImport/trees_report.json. The previous meshes are written to
Saved/CityImport/trees_backup.json, which the revert mode reads.
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
BACKUP = os.path.join(OUT, "trees_backup.json")
TREE_IDS = ("W14-63", "W14-64")
SEARCH_ROOTS = ("/Game/Fab", "/Game/Megascans", "/Game/MSPresets", "/Game")

eal = unreal.EditorAssetLibrary
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def tree_components():
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
            data = eal.find_asset_data(path)
            if str(data.asset_class_path.asset_name) == "StaticMesh":
                out.append(path.split(".")[0])
        if out:
            break
    return sorted(out)


def pick(paths, want_sapling):
    young = [p for p in paths if re.search(r"sapling|seedling|young", p, re.IGNORECASE)]
    if want_sapling:
        return young[0] if young else None
    mature = [p for p in paths if p not in young]
    field = [p for p in mature if re.search(r"field", p, re.IGNORECASE)]
    return (field or mature or [None])[0]


def write(report, t0):
    report["seconds"] = round(time.time() - t0, 1)
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "trees_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    unreal.log("swap_street_trees: " + json.dumps(report))


def main():
    t0 = time.time()
    mode = os.environ.get("GTP_TREE_MODE", "apply")
    report = {"map": MAP, "mode": mode, "errors": [], "changed": {}}
    pattern = os.environ.get("GTP_TREE_PATTERN", "Maple|Acer|Beech|Alder")
    if mode == "list":
        report["candidates"] = candidates(pattern)
        return write(report, t0)
    level_sub.load_level(MAP)
    comps = tree_components()
    report["instances"] = {cid: c.get_instance_count() for cid, c in comps.items()}
    if not comps:
        report["errors"].append("no Props_W14-63/64 actor on the map: run place_props.py first")
        return write(report, t0)

    if mode == "revert":
        if not os.path.exists(BACKUP):
            report["errors"].append("no trees_backup.json: nothing to revert")
            return write(report, t0)
        with open(BACKUP) as f:
            backup = json.load(f)
        targets = backup
    else:
        found = candidates(pattern)
        mature = os.environ.get("GTP_TREE_MESH") or pick(found, False)
        sapling = os.environ.get("GTP_SAPLING_MESH") or pick(found, True) or mature
        report["candidates"] = found[:40]
        if not mature or not eal.does_asset_exist(mature):
            report["errors"].append("no tree mesh found (pattern %r); import the Fab packs first" % pattern)
            return write(report, t0)
        targets = {"W14-63": mature, "W14-64": sapling}
        if not os.path.exists(BACKUP):  # keep the first backup: it holds the bare trees
            os.makedirs(OUT, exist_ok=True)
            with open(BACKUP, "w") as f:
                json.dump({cid: c.static_mesh.get_path_name().split(".")[0] for cid, c in comps.items()
                           if c.static_mesh}, f, indent=2)

    for cid, comp in comps.items():
        path = targets.get(cid)
        mesh = eal.load_asset(path) if path else None
        if mesh is None:
            report["errors"].append("cannot load %s for %s" % (path, cid))
            continue
        comp.set_static_mesh(mesh)
        report["changed"][cid] = path
    level_sub.save_current_level()
    if mode == "revert" and not report["errors"]:
        os.remove(BACKUP)
    write(report, t0)


main()
