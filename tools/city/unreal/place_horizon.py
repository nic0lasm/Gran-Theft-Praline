"""Place the far view of tools/city/horizon.py in the street map (Unreal Editor, headless).

Gate 76 video, 60 s: from the quay the far bank ended on a dark blue band under the sky, like a
sea, and the far volumes stood alone on it. This step puts in the map, in the outliner folder
City/Horizon: a ground to the real horizon, 1 m under the lowest terrain; the rows of far
blocks and the towers of the Part-Dieu, with the procedural Lyon facade (M_City_Horizon_<n>,
built by apply_city_materials.py: run the materials step first); the two hills. Engine basic
shapes only (cube, plane, sphere): no mesh to import, about 240 actors of 12 triangles that the
renderer batches by material. No collision, no shadow, not in the ray tracing scene.

Idempotent: the folder is emptied first. GTP_HORIZON=0 empties it and places nothing (undo).
Saves the map. Report: Saved/CityImport/horizon_report.json.
"""

import json
import os
import sys
import time

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import horizon  # noqa: E402  pure Python, shared with the tests

ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
CAMEL = "".join(p.capitalize() for p in FOCUS.split("-"))
ROOT = "/Game/Environment/City/" + "".join(p.capitalize() for p in ZONE.split("-"))
MAT_DIR = ROOT + "/Materials"
MAP = "/Game/Maps/L_City_" + CAMEL
FOLDER = "City/Horizon"
TERRAIN_FOLDER = "City/Terrain"
ENABLED = os.environ.get("GTP_HORIZON", "1") != "0"
SEED = int(os.environ.get("GTP_HORIZON_SEED", "7"))
FALLBACK_BASE_CM = -1500.0     # when the map has no terrain actor to measure
BELOW_TERRAIN_CM = 100.0
HILL_FOOT_M = 6.0              # a block on a slope goes this far into the hill

eal = unreal.EditorAssetLibrary


def quiet(comp, report):
    """Far scenery: seen, and nothing else."""
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    for prop, value in (("cast_shadow", False), ("visible_in_ray_tracing", False),
                        ("affect_dynamic_indirect_lighting", False), ("affect_distance_field_lighting", False),
                        ("generate_overlap_events", False), ("can_ever_affect_navigation", False)):
        try:
            comp.set_editor_property(prop, value)
        except Exception:  # noqa: BLE001  a property renamed between engine versions: noted, not fatal
            report["skipped_properties"] = sorted(set(report.get("skipped_properties", [])) | {prop})


def main():  # noqa: C901
    t0 = time.time()
    report = {"map": MAP, "errors": [], "removed": 0, "blocks": 0, "towers": 0, "hills": 0, "skirt": 0}
    try:
        level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not level_sub.load_level(MAP):
            report["errors"].append("cannot open " + MAP)
            raise StopIteration
        lowest = None
        for a in actor_sub.get_all_level_actors():
            folder = str(a.get_folder_path())
            if folder.startswith(FOLDER):
                actor_sub.destroy_actor(a)
                report["removed"] += 1
            elif folder.startswith(TERRAIN_FOLDER):
                origin, extent = a.get_actor_bounds(False)
                bottom = origin.z - extent.z
                lowest = bottom if lowest is None else min(lowest, bottom)
        if not ENABLED:
            report["skipped"] = "GTP_HORIZON=0"
            if report["removed"]:
                level_sub.save_current_level()
            raise StopIteration
        base = (lowest if lowest is not None else FALLBACK_BASE_CM) - BELOW_TERRAIN_CM
        report["base_cm"] = round(base, 1)
        report["terrain_measured"] = lowest is not None

        shapes = {n: eal.load_asset("/Engine/BasicShapes/" + n) for n in ("Cube", "Plane", "Sphere")}
        names = ["M_City_Horizon_%d" % i for i in range(horizon.COLOURS)] + \
                ["M_City_HorizonGround", "M_City_HorizonWood", "M_City_HorizonTown"]
        mats = {n: eal.load_asset(MAT_DIR + "/" + n) if eal.does_asset_exist(MAT_DIR + "/" + n) else None
                for n in names}
        missing = [n for n, m in shapes.items() if m is None] + [n for n, m in mats.items() if m is None]
        if missing:
            report["errors"].append("missing (run the materials step first): " + ", ".join(missing))
            raise StopIteration

        def place(shape, label, loc_cm, scale, mat, yaw=0.0):
            a = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc_cm),
                                                 unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            a.set_actor_label(label)
            a.set_folder_path(FOLDER)
            comp = a.static_mesh_component
            comp.set_static_mesh(shapes[shape])
            comp.set_material(0, mats[mat])
            a.set_actor_scale3d(unreal.Vector(*scale))
            quiet(comp, report)
            try:
                a.set_editor_property("is_spatially_loaded", False)    # a partitioned map must not unload it
            except Exception:  # noqa: BLE001  plain level: no such property to set
                pass
            return a

        plan = horizon.layout(SEED)
        # Basic shapes are 100 cm across, centred: a scale of 1 is 1 m.
        side = 2.0 * plan["skirt"]["half"]
        place("Plane", "Horizon_Ground", (0.0, 0.0, base), (side, side, 1.0), "M_City_HorizonGround")
        report["skirt"] = 1
        for h in plan["hills"]:
            height_cm = h["top"] * 100.0 - base
            place("Sphere", "Horizon_Hill_" + h["name"], (h["x"] * 100.0, h["y"] * 100.0, base),
                  (2.0 * h["half_x"], 2.0 * h["half_y"], 2.0 * height_cm / 100.0),
                  "M_City_HorizonWood" if h["wooded"] else "M_City_HorizonTown")
            report["hills"] += 1
        for i, b in enumerate(plan["blocks"]):
            foot = (b["ground"] - HILL_FOOT_M) * 100.0 if b["ground"] > 0.0 else base
            top = b["top"] * 100.0
            place("Cube", "Horizon_%s_%03d" % (b["kind"].capitalize(), i),
                  (b["x"] * 100.0, b["y"] * 100.0, 0.5 * (foot + top)),
                  (b["width"], b["depth"], (top - foot) / 100.0), "M_City_Horizon_%d" % b["colour"], b["yaw"])
            report["towers" if b["kind"] == "tower" else "blocks"] += 1
        level_sub.save_current_level()
    except StopIteration:
        pass
    except Exception as e:  # noqa: BLE001  report, never leave the editor on a dialog
        report["errors"].append(repr(e))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "horizon_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    unreal.log_warning("[Horizon] " + json.dumps(report))


main()
