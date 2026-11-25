"""Give the backdrop volumes a collision, inside the Unreal Editor (headless, like place_props.py).

The backdrop volumes (tools/city/backdrop.py, placed by import_city.py) came without collision:
they were a skyline seen from the street. The flight of the car theft runs among them, so in gate
65's video the player walked into one at 79 s (the armed-walk step traces a clear way and saw
none) and the camera stayed inside it, black, for more than 10 s. Same fix as the facade kit
(place_facades.kit_collision, 13/11): complex-as-simple collision on each backdrop mesh, and
BlockAll on its component in the street map, so pawns, cars and the camera stop at the walls.

Steps (idempotent): collision on the meshes of /Game/Environment/City/<Zone>/Backdrop/<Focus>,
open the map, BlockAll on the static mesh components in the outliner folder City/Backdrop, save.
Then an audit: every standing thing of the map (2.5 m or more) whose
component stops nothing, counted by outliner folder, and those within 60 m of the line the
flight drives listed by name (walls_without_collision in the report).
A landmark that stands for a whole building (landmarks.json, kind "building") is a LiDAR hull
whose walls let a walker in, although the audit below sees a collision on it (quads wound
either way, gaps where the roof edge reads at street level: which of the two is not known):
at gate 101 the player walked into the cinema at the south end of the street and stayed in it,
the picture black for 47 s. Boxes that are not drawn fill its outline (footprint_fill.strips),
6 m high, in the folder City/Blockers; the report gives landmark_blockers.
GTP_LANDMARK_BLOCKERS=0 leaves them out.
GTP_BACKDROP_COLLISION=0: nothing is changed (report only). Report:
Saved/CityImport/backdrop_collision_report.json.
"""

import json
import os
import time

import unreal

ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
CAMEL = "".join(p.capitalize() for p in FOCUS.split("-"))
ROOT = "/Game/Environment/City/" + "".join(p.capitalize() for p in ZONE.split("-"))
MAP = "/Game/Maps/L_City_" + CAMEL
FOLDER = "City/Backdrop"
LANDMARK_BLOCKERS = os.environ.get("GTP_LANDMARK_BLOCKERS", "1") != "0"
LANDMARK_FOLDER = "City/Landmarks"
BLOCKER_FOLDER = "City/Blockers"
BLOCKER_LABEL = "Landmark_Blockers"
BLOCKER_TALL_CM = 600.0         # over a walker, a car and the camera behind them
CUBE = "/Engine/BasicShapes/Cube.Cube"       # 100 cm, centred on its origin

eal = unreal.EditorAssetLibrary


AUDIT_MIN_HEIGHT_CM = 250.0     # what a walker or the camera can go into
AUDIT_NEAR_CM = 6000.0          # listed when this close to the line the flight drives
AUDIT_LIST = 60


def audit(actors):
    """Every standing thing of the map that stops nothing, by outliner folder, and those near the
    driven line by name. Gate 86, 171 s: the player on foot ran into a blind stone wall beside a
    Haussmann front, went through it, and the picture stayed black for 18 s; nobody could say
    which volume it was. {"by_folder": {folder: n}, "near_the_line": [...], "lines": n}."""
    import math
    import sys
    repo = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
    sys.path.insert(0, os.path.join(repo, "tools", "city"))
    import lane_clear as lc
    circuit = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "Test",
                           "Circuits", "L_City_" + CAMEL + ".circuit.csv")
    lines = lc.read_lines(circuit)
    out = {"by_folder": {}, "near_the_line": [], "lines": len(lines)}
    for a in actors:
        open_comps = []
        if str(a.get_folder_path()).startswith("Gameplay"):
            continue                        # a parked car's own parts
        for comp in a.get_components_by_class(unreal.StaticMeshComponent):
            if comp.get_editor_property("static_mesh") is None or isinstance(comp, unreal.InstancedStaticMeshComponent):
                continue                    # instances are the dressing of the fronts, a street long
            try:
                if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
                    continue
            except Exception:  # noqa: BLE001
                continue
            open_comps.append(comp)
        if not open_comps:
            continue
        origin, extent = a.get_actor_bounds(False)
        if 2.0 * extent.z < AUDIT_MIN_HEIGHT_CM:
            continue
        folder = str(a.get_folder_path()) or "(none)"
        out["by_folder"][folder] = out["by_folder"].get(folder, 0) + 1
        gap = lc.end_gap(origin.x, origin.y, lines) - math.hypot(extent.x, extent.y)
        if gap < AUDIT_NEAR_CM and not folder.startswith("City/Horizon"):
            mesh = open_comps[0].get_editor_property("static_mesh")
            out["near_the_line"].append({"actor": a.get_name(), "label": a.get_actor_label(), "folder": folder,
                                         "mesh": mesh.get_name(), "components": len(open_comps),
                                         "gap_cm": round(gap), "origin": [round(origin.x), round(origin.y), round(origin.z)],
                                         "extent": [round(extent.x), round(extent.y), round(extent.z)]})
    out["near_the_line"].sort(key=lambda e: e["gap_cm"])
    out["near_total"] = len(out["near_the_line"])
    out["near_the_line"] = out["near_the_line"][:AUDIT_LIST]
    return out


def hidden_boxes(actor_sub):
    """One actor, one instanced cube that is not drawn in the game and stops everything (as the
    trunk posts of clear_lane_instances.py). Raises when it cannot be hidden: white boxes over a
    building are worse than a hull one can walk into."""
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    sdl = unreal.SubobjectDataBlueprintFunctionLibrary
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    try:
        actor.set_actor_label(BLOCKER_LABEL)
        actor.set_folder_path(BLOCKER_FOLDER)
        root = sds.k2_gather_subobject_data_for_instance(actor)[0]
        handle, _ = sds.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
        comp = sdl.get_object(sdl.get_data(handle))
        comp.set_static_mesh(eal.load_asset(CUBE))
        comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        comp.set_collision_profile_name("BlockAll")
        try:
            comp.set_hidden_in_game(True)
        except Exception:  # noqa: BLE001  the property, when the call is not there
            comp.set_editor_property("hidden_in_game", True)
        for prop in ("cast_shadow", "visible_in_ray_tracing", "affect_distance_field_lighting"):
            try:
                comp.set_editor_property(prop, False)
            except Exception:  # noqa: BLE001  hidden already: these only spare the renderer
                pass
    except Exception:
        actor_sub.destroy_actor(actor)
        raise
    return actor, comp


def landmark_blockers(actor_sub):
    """Boxes that are not drawn inside the outline of every landmark of the map that stands for a
    building. {"on", "landmarks": {label: boxes}, "instances"}; the map changed when instances
    or removed is not 0."""
    import sys
    repo = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
    sys.path.insert(0, os.path.join(repo, "tools", "city"))
    import footprint_fill
    import squares
    out = {"on": LANDMARK_BLOCKERS, "landmarks": {}, "instances": 0, "removed": 0}
    hulls = {}
    for a in actor_sub.get_all_level_actors():
        folder = str(a.get_folder_path())
        if folder.startswith(BLOCKER_FOLDER):
            actor_sub.destroy_actor(a)          # idempotent: laid again from the outlines
            out["removed"] += 1
        elif folder.startswith(LANDMARK_FOLDER):
            hulls[a.get_actor_label()] = a
    if not LANDMARK_BLOCKERS:
        return out
    path = os.path.join(repo, "assets", "geo", "processed", ZONE, "landmarks.json")
    if not os.path.exists(path):
        out["skipped"] = "no landmarks.json for the zone"
        return out
    with open(path, encoding="utf-8") as f:
        records = json.load(f).get("landmarks", [])
    todo = []
    for r in records:
        hull = hulls.get(r.get("mesh"))
        if r.get("kind") != "building" or hull is None or not r.get("polygon_xy"):
            continue
        origin, extent = hull.get_actor_bounds(False)
        foot = origin.z - extent.z
        tall = min(BLOCKER_TALL_CM, 2.0 * extent.z)
        boxes = []
        for ring in r["polygon_xy"]:
            boxes += footprint_fill.strips(squares.to_ue(ring))
        # the outline is where the hull stands: a landmark moved since is left alone
        boxes = [b for b in boxes if abs(0.5 * (b[0] + b[2]) - origin.x) <= extent.x
                 and abs(0.5 * (b[1] + b[3]) - origin.y) <= extent.y]
        out["landmarks"][r["mesh"]] = len(boxes)
        todo += [(b, foot, tall) for b in boxes]
    if not todo:
        return out
    actor, comp = hidden_boxes(actor_sub)
    try:
        for (x0, y0, x1, y1), foot, tall in todo:
            comp.add_instance(unreal.Transform(unreal.Vector(0.5 * (x0 + x1), 0.5 * (y0 + y1), foot + 0.5 * tall),
                                               unreal.Rotator(0, 0, 0),
                                               unreal.Vector((x1 - x0) / 100.0, (y1 - y0) / 100.0, tall / 100.0)), True)
    except Exception:
        actor_sub.destroy_actor(actor)
        raise
    try:
        actor.set_editor_property("is_spatially_loaded", False)
    except Exception:  # noqa: BLE001  plain level: no such property to set
        pass
    out["instances"] = len(todo)
    out["actor"] = actor.get_actor_label()
    return out


def main():
    t0 = time.time()
    report = {"map": MAP, "errors": [], "meshes": 0, "components": 0}
    try:
        if os.environ.get("GTP_BACKDROP_COLLISION", "1") == "0":
            report["skipped"] = "GTP_BACKDROP_COLLISION=0"
            raise StopIteration
        sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        folder = ROOT + "/Backdrop/" + CAMEL
        if not eal.does_directory_exist(folder):
            report["errors"].append("no backdrop meshes in " + folder)
            raise StopIteration
        for path in eal.list_assets(folder, recursive=True, include_folder=False):
            mesh = eal.load_asset(path)
            if not isinstance(mesh, unreal.StaticMesh):
                continue
            body = mesh.get_editor_property("body_setup")
            if body is not None and body.get_editor_property("collision_trace_flag") != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                mesh.set_editor_property("body_setup", body)
                for section in range(mesh.get_num_sections(0)):
                    sm_sub.enable_section_collision(mesh, True, 0, section)
                eal.save_loaded_asset(mesh)
            report["meshes"] += 1
        level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not level_sub.load_level(MAP):
            report["errors"].append("cannot open " + MAP)
            raise StopIteration
        for a in actor_sub.get_all_level_actors():
            if not str(a.get_folder_path()).startswith(FOLDER):
                continue
            for comp in a.get_components_by_class(unreal.StaticMeshComponent):
                comp.modify()
                comp.set_collision_profile_name("BlockAll")
                comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
                report["components"] += 1
        if report["components"]:
            level_sub.save_current_level()
        else:
            report["errors"].append("no component in folder " + FOLDER)
        try:
            report["landmark_blockers"] = landmark_blockers(actor_sub)
            if report["landmark_blockers"]["instances"] or report["landmark_blockers"]["removed"]:
                level_sub.save_current_level()
        except Exception as e:  # noqa: BLE001  the backdrop is solid: the blockers never fail the step
            report["landmark_blockers"] = {"on": LANDMARK_BLOCKERS, "error": repr(e)}
        try:
            report["walls_without_collision"] = audit(actor_sub.get_all_level_actors())
        except Exception as e:  # noqa: BLE001  the audit never fails the step
            report["audit_error"] = repr(e)
    except StopIteration:
        pass
    except Exception as e:  # noqa: BLE001  report, never leave the editor on a dialog
        report["errors"].append(repr(e))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "backdrop_collision_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    unreal.log_warning("[BackdropCollision] " + json.dumps(report))


main()
