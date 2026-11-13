"""List what stands on a path of the street map: every actor whose bounds cross the path, and
every instance of an instanced component (facade kit, props) within 3 m of it.

Asked by the 18dd6fb run (13/11): the demo pawn walked from the PlayerStart through a bare
beige wall with no collision that the cloud data does not have. This names the culprit.

Run without the UI, from a copy in a folder without spaces (the editor quits at the end):
    UnrealEditor GranTheftPraline.uproject -unattended -nosplash -nullrhi \
        -ExecutePythonScript=<copy of this file>
GTP_PROBE_MAP   map to open (default /Game/Maps/L_City_RueDeLaRepublique)
GTP_PROBE_PATH  x0,y0,x1,y1 in Unreal cm (default: the 18dd6fb demo walk 9535,49530,10287,47326)
GTP_PROBE_Z     z0,z1 in cm, the height band to test (default -200,400)
Output: <project>/Saved/CityImport/probe_actors.json, and one log line per hit ([probe_actors]).
"""

import json
import math
import os

import unreal

MAP = os.environ.get("GTP_PROBE_MAP", "/Game/Maps/L_City_RueDeLaRepublique")
X0, Y0, X1, Y1 = [float(v) for v in os.environ.get("GTP_PROBE_PATH", "9535,49530,10287,47326").split(",")]
Z0, Z1 = [float(v) for v in os.environ.get("GTP_PROBE_Z", "-200,400").split(",")]
NEAR_CM = 300.0
STEP_CM = 50.0

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log_warning("[probe_actors] " + msg)


def path_points():
    n = max(1, int(math.hypot(X1 - X0, Y1 - Y0) / STEP_CM))
    return [(X0 + (X1 - X0) * k / n, Y0 + (Y1 - Y0) * k / n) for k in range(n + 1)]


def dist_to_path(x, y):
    dx, dy = X1 - X0, Y1 - Y0
    t = max(0.0, min(1.0, ((x - X0) * dx + (y - Y0) * dy) / (dx * dx + dy * dy or 1.0)))
    return math.hypot(x - (X0 + t * dx), y - (Y0 + t * dy))


def collision_of(comp):
    try:
        return "%s/%s" % (comp.get_collision_profile_name(), comp.get_collision_enabled())
    except Exception:  # noqa: BLE001
        return "?"


def hidden(actor):
    try:
        return bool(actor.get_editor_property("hidden"))
    except Exception:  # noqa: BLE001
        return None


def mesh_of(comp):
    try:
        m = comp.get_editor_property("static_mesh")
        return m.get_path_name() if m else ""
    except Exception:  # noqa: BLE001
        return ""


def main():
    if not level_sub.load_level(MAP):
        raise RuntimeError("could not load " + MAP)
    pts = path_points()
    hits, instances = [], []
    for a in actor_sub.get_all_level_actors():
        origin, ext = a.get_actor_bounds(False)
        if ext.x <= 0 and ext.y <= 0:
            continue
        if origin.z + ext.z < Z0 or origin.z - ext.z > Z1:
            continue
        on_path = [p for p in pts if abs(p[0] - origin.x) <= ext.x and abs(p[1] - origin.y) <= ext.y]
        comps = a.get_components_by_class(unreal.PrimitiveComponent)
        isms = [c for c in comps if isinstance(c, unreal.InstancedStaticMeshComponent)]
        for c in isms:
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                if dist_to_path(t.translation.x, t.translation.y) <= NEAR_CM and Z0 - 500 <= t.translation.z <= Z1:
                    instances.append({"actor": a.get_actor_label(), "mesh": mesh_of(c), "index": i,
                                      "xyz": [round(t.translation.x), round(t.translation.y), round(t.translation.z)],
                                      "yaw": round(t.rotation.rotator().yaw, 1), "collision": collision_of(c)})
        if not on_path or (isms and len(isms) == len(comps)):
            continue
        big = ext.x > 20000 or ext.y > 20000
        hits.append({"label": a.get_actor_label(), "class": a.get_class().get_name(),
                     "folder": str(a.get_folder_path()), "hidden_in_game": hidden(a),
                     "bounds_min": [round(origin.x - ext.x), round(origin.y - ext.y), round(origin.z - ext.z)],
                     "bounds_max": [round(origin.x + ext.x), round(origin.y + ext.y), round(origin.z + ext.z)],
                     "first_path_point": [round(v) for v in on_path[0]], "path_points_inside": len(on_path),
                     "large_bounds": big,
                     "components": [{"name": c.get_name(), "class": c.get_class().get_name(), "mesh": mesh_of(c),
                                     "collision": collision_of(c), "visible": c.is_visible()}
                                    for c in comps if not isinstance(c, unreal.InstancedStaticMeshComponent)][:6]})
    hits.sort(key=lambda h: (h["large_bounds"], -h["path_points_inside"]))
    for h in hits:
        log("%s (%s, %s) bounds %s..%s first on path at %s, %d points%s: %s" % (
            h["label"], h["class"], h["folder"], h["bounds_min"], h["bounds_max"], h["first_path_point"],
            h["path_points_inside"], " LARGE" if h["large_bounds"] else "",
            "; ".join("%s %s %s" % (c["mesh"].split("/")[-1], c["collision"], "" if c["visible"] else "hidden")
                      for c in h["components"])))
    for i in instances[:60]:
        log("instance %s #%d %s at %s yaw %s %s" % (i["actor"], i["index"], i["mesh"].split("/")[-1], i["xyz"], i["yaw"], i["collision"]))
    out = os.path.join(unreal.Paths.project_saved_dir(), "CityImport", "probe_actors.json")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump({"map": MAP, "path": [X0, Y0, X1, Y1], "z": [Z0, Z1], "actors": hits,
                   "instances_near_path": instances}, f, indent=1)
    log("%d actors cross the path, %d instances within %d cm; report %s" % (len(hits), len(instances), NEAR_CM, out))
    unreal.SystemLibrary.quit_editor()


main()
