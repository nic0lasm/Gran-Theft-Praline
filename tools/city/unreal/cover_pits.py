"""Cover the closed pits of the terrain with flat ground, inside the Unreal Editor (headless, like
place_props.py).

Gate 76 video, 92-115 s: the player on foot fell into a granite pit several metres under the
street (walls of the ground material) and stayed there. The terrain comes from the LiDAR model,
where the ramp of an underground car park or an underpass mouth is a hole with nothing at the
bottom in the game. tools/city/pits.py finds them; this script samples the ground and lays lids.

Steps (idempotent):
1. open the street map, delete the outliner folder City/PitCovers;
2. sample the terrain alone (the components of folder City/Terrain, complex collision, so
   buildings and props do not count) every GTP_PIT_STEP_M (2 m) over the lanes' box plus 60 m;
3. closed depressions deeper than GTP_PIT_MIN_DEPTH_M (1.0) and of 6 to GTP_PIT_MAX_AREA_M2 (3000)
   m2 (the river and the low quays drain away and are not pits);
4. a pit with a lane of the game running down into it (an underpass the traffic uses) is left
   open and reported;
5. the others get a lid: one box per row run of cells, top at the pit's spill height (the
   street around it), 60 cm thick, the terrain tile's own material (M_City_Ground reads the
   world X/Y, so the paving goes on over the lid), collision BlockAll; saves the map.
GTP_PIT_COVER=0: report only. Report: Saved/CityImport/pit_cover_report.json, one log line
"[PitCover] ...".
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

FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
MAP_NAME = "L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))
MAP = "/Game/Maps/" + MAP_NAME
LANES = os.environ.get("GTP_LANES_CSV") or os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "City", "Traffic",
    MAP_NAME + ".lanes.csv")
APPLY = os.environ.get("GTP_PIT_COVER", "1") != "0"
STEP_CM = float(os.environ.get("GTP_PIT_STEP_M", "2")) * 100.0
MIN_DEPTH_CM = float(os.environ.get("GTP_PIT_MIN_DEPTH_M", "1.0")) * 100.0
MAX_AREA_M2 = float(os.environ.get("GTP_PIT_MAX_AREA_M2", "3000"))
MARGIN_CM = 6000.0
TERRAIN_FOLDER = "City/Terrain"
FOLDER = "City/PitCovers"
CUBE = "/Engine/BasicShapes/Cube.Cube"       # 100 cm, centred on its origin
LID_CM = 60.0

eal = unreal.EditorAssetLibrary
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def log(msg):
    unreal.log_warning("[PitCover] " + msg)


def terrain_tiles():
    tiles = []
    for a in actor_sub.get_all_level_actors():
        if not str(a.get_folder_path()).startswith(TERRAIN_FOLDER):
            continue
        origin, ext = a.get_actor_bounds(False)
        for comp in a.get_components_by_class(unreal.StaticMeshComponent):
            tiles.append(((origin.x - ext.x, origin.y - ext.y, origin.x + ext.x, origin.y + ext.y),
                          origin.z + ext.z + 1000.0, origin.z - ext.z - 1000.0, comp))
    return tiles


BUCKET_CM = 5000.0


def bucket_tiles(tiles):
    """50 m buckets -> the tiles whose box covers them: one or two candidates per sample."""
    out = {}
    for t in tiles:
        x0, y0, x1, y1 = t[0]
        for bx in range(int(x0 // BUCKET_CM), int(x1 // BUCKET_CM) + 1):
            for by in range(int(y0 // BUCKET_CM), int(y1 // BUCKET_CM) + 1):
                out.setdefault((bx, by), []).append(t)
    return out


def ground_at(buckets, x, y):
    for (x0, y0, x1, y1), ztop, zbot, comp in buckets.get((int(x // BUCKET_CM), int(y // BUCKET_CM)), ()):
        if x0 <= x <= x1 and y0 <= y <= y1:
            res = comp.line_trace_component(unreal.Vector(x, y, ztop), unreal.Vector(x, y, zbot), True, False, False)
            if res is None:
                continue
            hit = res[0] if isinstance(res, tuple) else res
            if isinstance(hit, unreal.Vector):
                return hit.z, comp
            if isinstance(res, tuple) and isinstance(res[-1], unreal.HitResult):
                return res[-1].to_tuple()[4].z, comp
    return None, None


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
    report = {"map": MAP, "apply": APPLY, "step_cm": STEP_CM, "pits": [], "covered": 0, "left_open": 0,
              "lids": 0, "errors": []}
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
        lanes = lc.read_lanes(LANES) if os.path.exists(LANES) else []
        report["lanes"] = len(lanes)
        pts = [p for lane in lanes for p in lane["points"]]
        if pts:
            x0, y0 = min(p[0] for p in pts) - MARGIN_CM, min(p[1] for p in pts) - MARGIN_CM
            x1, y1 = max(p[0] for p in pts) + MARGIN_CM, max(p[1] for p in pts) + MARGIN_CM
        else:
            x0, y0 = min(t[0][0] for t in tiles), min(t[0][1] for t in tiles)
            x1, y1 = max(t[0][2] for t in tiles), max(t[0][3] for t in tiles)
        cols, rows = int((x1 - x0) / STEP_CM) + 1, int((y1 - y0) / STEP_CM) + 1
        report["grid"] = [rows, cols]
        buckets = bucket_tiles(tiles)
        z, mat_at = [], {}
        for r in range(rows):
            row = []
            for c in range(cols):
                h, comp = ground_at(buckets, x0 + c * STEP_CM, y0 + r * STEP_CM)
                row.append(h)
                if comp is not None and not mat_at:
                    mat_at["m"] = comp.get_material(0)
            z.append(row)
        report["sampled_s"] = round(time.time() - t0)
        found = pitlib.pits(z, STEP_CM, MIN_DEPTH_CM, max_area_m2=MAX_AREA_M2)
        comp = None
        for p in found:
            cr, cc = p["centre_rc"]
            info = {"centre_ue_cm": [round(x0 + cc * STEP_CM), round(y0 + cr * STEP_CM)],
                    "area_m2": round(p["area_m2"]), "depth_m": round(p["depth_cm"] / 100.0, 1),
                    "spill_z_cm": round(p["spill_cm"])}
            cells = set(p["cells"])
            road = [lane["id"] for lane in lanes for (lx, ly, lz) in lane["points"]
                    if (int(round((ly - y0) / STEP_CM)), int(round((lx - x0) / STEP_CM))) in cells
                    and lz < p["spill_cm"] - 100.0]
            if road:
                info["left_open"] = "lanes %s run down into it" % sorted(set(road))[:6]
                report["left_open"] += 1
                report["pits"].append(info)
                continue
            if APPLY:
                if comp is None:
                    comp = lid_actor(mat_at.get("m"))
                for r, ca, cb in pitlib.lid_runs(p["cells"]):
                    xa, xb = x0 + ca * STEP_CM - STEP_CM / 2, x0 + cb * STEP_CM + STEP_CM / 2
                    cy = y0 + r * STEP_CM
                    comp.add_instance(unreal.Transform(
                        unreal.Vector((xa + xb) / 2, cy, p["spill_cm"] - LID_CM / 2), unreal.Rotator(0, 0, 0),
                        unreal.Vector((xb - xa) / 100.0, STEP_CM / 100.0, LID_CM / 100.0)), True)
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
    log(json.dumps({k: report.get(k) for k in ("covered", "left_open", "lids", "grid", "sampled_s", "errors",
                                                "saved")}))
    for p in report["pits"]:
        log(json.dumps(p))


main()
