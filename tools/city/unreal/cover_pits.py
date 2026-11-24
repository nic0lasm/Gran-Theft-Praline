"""Cover the closed pits of the terrain with flat ground, inside the Unreal Editor (headless, like
place_props.py).

Gate 76 video, 92-115 s: the player on foot fell into a granite pit several metres under the
street (walls of the ground material) and stayed there. Gate 81, long take: the stolen car rolled
into a car park ramp at the corner of place de la Republique. The terrain comes from the LiDAR
model, where the ramp of an underground car park or an underpass mouth is a hole with nothing at
the bottom in the game. tools/city/pits.py finds them; this script reads the ground and lays lids.

Steps (idempotent):
1. open the street map, delete the outliner folder City/PitCovers;
2. read the corners of the terrain tiles (folder City/Terrain), with no trace: in the headless
   editor the tiles answer none, and from gate 78 to gate 81 this step read no height at all and
   reported "no pit". First from assets/geo/processed/<zone>/terrain/terrain.glb, kept only if
   its tiles have the boxes of the tiles of the map; else from the meshes themselves;
3. closed depressions deeper than GTP_PIT_MIN_DEPTH_M (1.0) and of 6 to GTP_PIT_MAX_AREA_M2 (3000)
   m2 (the river and the low quays drain away and are not pits);
4. each gets a lid: one box per row run of corners, reaching the dry corners round them, top at
   the pit's spill height (the street around it), 60 cm thick, the terrain tile's own material
   (M_City_Ground reads the world X/Y, so the paving goes on over the lid), collision BlockAll;
   the lanes of the game that pass over a pit are reported (the lanes file has no height: the
   cars find the ground by physics and now drive on the lid); saves the map.
No height read is an error: the step fails instead of reporting an empty list.
GTP_PIT_COVER=0: report only. Report: Saved/CityImport/pit_cover_report.json (heights_from,
cells_read, pits), one log line "[PitCover] ...".
"""

import json
import os
import sys
import time

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "city"))
import lane_clear as lc  # noqa: E402
import pits as pitlib  # noqa: E402
import terrain_file as tf  # noqa: E402

ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
MAP_NAME = "L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))
MAP = "/Game/Maps/" + MAP_NAME
LANES = os.environ.get("GTP_LANES_CSV") or os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "City", "Traffic",
    MAP_NAME + ".lanes.csv")
TERRAIN_GLB = os.environ.get("GTP_TERRAIN_GLB") or os.path.join(
    REPO, "assets", "geo", "processed", ZONE, "terrain", "terrain.glb")
APPLY = os.environ.get("GTP_PIT_COVER", "1") != "0"
MIN_DEPTH_CM = float(os.environ.get("GTP_PIT_MIN_DEPTH_M", "1.0")) * 100.0
MAX_AREA_M2 = float(os.environ.get("GTP_PIT_MAX_AREA_M2", "3000"))
TERRAIN_FOLDER = "City/Terrain"
FOLDER = "City/PitCovers"
CUBE = "/Engine/BasicShapes/Cube.Cube"       # 100 cm, centred on its origin
LID_CM = 60.0
MAX_EMPTY = 0.2         # share of empty cells past which the grid is not the terrain
LOOSE_CM = 100.0        # tiles in the right place, their heights not checked

eal = unreal.EditorAssetLibrary
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def log(msg):
    unreal.log_warning("[PitCover] " + msg)


def terrain_tiles():
    """[(box (x0, y0, x1, y1, z0, z1), component)] of the terrain of the map."""
    tiles = []
    for a in actor_sub.get_all_level_actors():
        if not str(a.get_folder_path()).startswith(TERRAIN_FOLDER):
            continue
        origin, ext = a.get_actor_bounds(False)
        for comp in a.get_components_by_class(unreal.StaticMeshComponent):
            tiles.append(((origin.x - ext.x, origin.y - ext.y, origin.x + ext.x, origin.y + ext.y,
                           origin.z - ext.z, origin.z + ext.z), comp))
    return tiles


def checked(found, boxes):
    """"checked", "in place" (heights not compared) or None for tiles read against the map's."""
    mine = [tf.box(pts) for _, pts in found if pts]
    if tf.same_tiles(mine, boxes):
        return "checked"
    flat = lambda b: b[:4] + (0.0, 0.0)  # noqa: E731
    if tf.same_tiles([flat(b) for b in mine], [flat(b) for b in boxes], LOOSE_CM):
        return "in place"
    return None


def vertex_id(i):
    try:
        return unreal.VertexID(i)
    except Exception:  # noqa: BLE001  no positional constructor in this version
        v = unreal.VertexID()
        v.set_editor_property("id_value", i)
        return v


def from_meshes(tiles):
    """The corners of each tile read from its mesh, moved to where the component stands."""
    out = []
    for _, comp in tiles:
        mesh = comp.get_editor_property("static_mesh")
        desc = mesh.get_static_mesh_description(0)
        xf = comp.get_world_transform()
        pts = []
        for i in range(desc.get_vertex_count()):
            w = unreal.MathLibrary.transform_location(xf, desc.get_vertex_position(vertex_id(i)))
            pts.append((w.x, w.y, w.z))
        out.append((mesh.get_name(), pts))
    return out


def heights(tiles, report):
    """The tiles' corners [(name, [(x, y, z)])] and where they were read, or (None, None)."""
    boxes = [t[0] for t in tiles]
    tried, loose = [], None
    for name, read in (("terrain.glb", lambda: tf.read_glb(TERRAIN_GLB) if os.path.exists(TERRAIN_GLB) else None),
                       ("tile meshes", lambda: from_meshes(tiles))):
        try:
            found = read()
            how = checked(found, boxes) if found else None
        except Exception as e:  # noqa: BLE001  try the next source
            tried.append("%s: %r" % (name, e))
            continue
        if how == "checked":
            report["heights_tried"] = tried
            return found, name + " (boxes of the %d tiles checked against the map)" % len(boxes)
        if how == "in place" and loose is None:
            loose = (found, name + " (tiles in place, heights not checked against the map)")
        tried.append("%s: %s" % (name, "not found" if found is None else how or "not the tiles of the map"))
    report["heights_tried"] = tried
    return loose if loose else (None, None)


def lid_actor(material):
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    actor.set_actor_label("PitCovers")
    actor.set_folder_path(FOLDER)
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, _ = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
    comp = sdl.get_object(sdl.get_data(handle))
    comp.set_static_mesh(eal.load_asset(CUBE))
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    comp.set_collision_profile_name("BlockAll")
    if material is not None:
        comp.set_material(0, material)
    return comp


def main():  # noqa: C901
    t0 = time.time()
    report = {"map": MAP, "apply": APPLY, "pits": [], "covered": 0, "lids": 0, "errors": []}
    try:
        if not level_sub.load_level(MAP):
            report["errors"].append("cannot open " + MAP)
            raise StopIteration
        for a in actor_sub.get_all_level_actors():
            if str(a.get_folder_path()).startswith(FOLDER):
                actor_sub.destroy_actor(a)
        tiles = terrain_tiles()
        report["terrain_components"] = len(tiles)
        if not tiles:
            report["errors"].append("no terrain in folder " + TERRAIN_FOLDER)
            raise StopIteration
        found_tiles, source = heights(tiles, report)
        if not found_tiles:
            report["errors"].append("no terrain height read: %s" % report.get("heights_tried"))
            raise StopIteration
        report["heights_from"] = source
        stats = {}
        z, x0, y0, step = tf.grid(found_tiles, stats=stats)
        report["off_grid"] = stats.get("off_grid", 0)
        report["grid"] = [len(z), len(z[0]) if z else 0]
        report["step_cm"] = step
        report["cells_read"] = sum(1 for row in z for h in row if h is not None)
        report["cells_empty"] = sum(1 for row in z for h in row if h is None)
        report["read_s"] = round(time.time() - t0)
        if not report["cells_read"]:
            report["errors"].append("no terrain height read: empty grid from " + source)
            raise StopIteration
        if report["cells_empty"] > MAX_EMPTY * (report["cells_read"] + report["cells_empty"]):
            # a cell beside an empty one drains: on a grid full of holes no pit can be found
            report["errors"].append("grid mostly empty (%d of %d cells), no pit can be told: step %s from %s"
                                    % (report["cells_empty"], report["cells_read"] + report["cells_empty"],
                                       step, source))
            raise StopIteration
        lanes = lc.read_lanes(LANES) if os.path.exists(LANES) else []
        report["lanes"] = len(lanes)
        found = pitlib.pits(z, step, MIN_DEPTH_CM, max_area_m2=MAX_AREA_M2)
        material = tiles[0][1].get_material(0)
        comp = None
        for p in found:
            cr, cc = p["centre_rc"]
            info = {"centre_ue_cm": [round(x0 + cc * step), round(y0 + cr * step)],
                    "area_m2": round(p["area_m2"]), "depth_m": round(p["depth_cm"] / 100.0, 1),
                    "spill_z_cm": round(p["spill_cm"])}
            cells = set(p["cells"])
            over = sorted({lane["id"] for lane in lanes for pt in lane["points"]
                           if (int(round((pt[1] - y0) / step)), int(round((pt[0] - x0) / step))) in cells})
            if over:
                info["lanes_over"] = over[:8]
            if APPLY:
                if comp is None:
                    comp = lid_actor(material)
                for xa, ya, xb, yb in pitlib.lids(p["cells"], x0, y0, step):
                    comp.add_instance(unreal.Transform(
                        unreal.Vector((xa + xb) / 2, (ya + yb) / 2, p["spill_cm"] - LID_CM / 2),
                        unreal.Rotator(0, 0, 0),
                        unreal.Vector((xb - xa) / 100.0, (yb - ya) / 100.0, LID_CM / 100.0)), True)
                    report["lids"] += 1
                info["covered"] = True
                report["covered"] += 1
            report["pits"].append(info)
        if APPLY and report["lids"]:
            level_sub.save_current_level()
            report["saved"] = True
    except StopIteration:
        pass
    except Exception as e:  # noqa: BLE001  report, never leave the editor on a dialog
        report["errors"].append(repr(e))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "pit_cover_report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    log(json.dumps({k: report.get(k) for k in ("heights_from", "cells_read", "off_grid", "covered", "lids", "grid",
                                                "read_s", "errors", "saved")}))
    for p in report["pits"]:
        log(json.dumps(p))


main()
