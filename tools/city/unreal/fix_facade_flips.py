"""Turn round the facade kit modules placed the wrong way (Unreal Editor, headless, like
place_props.py).

Gate 75, 70-95 s: the stolen car stood 25 s against a white block, rue de la Bourse. The PC's
probe found it: SM_Facade_Haussmann_ShopClosed_B at (26250, -14825), yaw -90, among modules of
the same facade line at yaw 90. A shop module has its room behind the facade plane; turned round,
the room stands on lane 13 with its plain back walls to the street. Rule: tools/city/facade_flips.py.
A flip keeps the pivot (bottom centre of the bay, on the facade plane) and adds 180 degrees.

Every instanced component of the actors of folder City/Facades is read together (the kit has one
actor per module mesh over the whole city: a module's neighbours are in other actors).
GTP_FACADE_FLIP=0: report only. Report: Saved/CityImport/facade_flip_report.json (each flip with
its mesh, place and votes), one log line "[FacadeFlip] ...".
"""

import json
import os
import sys
import time

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "city"))
import facade_flips as ff  # noqa: E402

FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
MAP_NAME = "L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))
MAP = "/Game/Maps/" + MAP_NAME
APPLY = os.environ.get("GTP_FACADE_FLIP", "1") != "0"
FOLDER = "City/Facades"

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log_warning("[FacadeFlip] " + msg)


def main():
    t0 = time.time()
    report = {"map": MAP, "apply": APPLY, "modules": 0, "flipped": 0, "flips": [], "by_mesh": {}, "errors": []}
    try:
        if not level_sub.load_level(MAP):
            report["errors"].append("cannot open " + MAP)
            raise StopIteration
        comps, inst = [], []
        for actor in actor_sub.get_all_level_actors():
            if not str(actor.get_folder_path()).startswith(FOLDER):
                continue
            for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
                if comp.get_editor_property("static_mesh") is None:
                    continue
                ci = len(comps)
                comps.append((actor, comp))
                for i in range(comp.get_instance_count()):
                    t = comp.get_instance_transform(i, True)
                    p = t.translation
                    inst.append(((ci, i), p.x, p.y, p.z, t.rotation.rotator().yaw))
        report["modules"] = len(inst)
        if not inst:
            report["errors"].append("no facade module in folder " + FOLDER)
            raise StopIteration
        flips = ff.flipped(inst)
        where = {k: (x, y, z, yaw) for k, x, y, z, yaw in inst}
        touched = set()
        for (ci, i), votes in flips:
            actor, comp = comps[ci]
            x, y, z, yaw = where[(ci, i)]
            mesh = comp.get_editor_property("static_mesh").get_path_name().split(".")[0].rsplit("/", 1)[-1]
            report["flips"].append({"actor": actor.get_name(), "mesh": mesh, "index": i, "xyz": [round(x), round(y), round(z)],
                                    "yaw": round(yaw, 1), "votes": votes})
            report["by_mesh"][mesh] = report["by_mesh"].get(mesh, 0) + 1
            if APPLY:
                if ci not in touched:
                    comp.modify()
                    touched.add(ci)
                t = comp.get_instance_transform(i, True)
                r = t.rotation.rotator()
                comp.update_instance_transform(
                    i, unreal.Transform(t.translation, unreal.Rotator(r.roll, r.pitch, yaw + 180.0), t.scale3d),
                    True, True, True)
        report["flipped"] = len(flips)
        if APPLY and flips:
            level_sub.save_current_level()
            report["saved"] = True
    except StopIteration:
        pass
    except Exception as e:  # noqa: BLE001  report, never leave the editor on a dialog
        report["errors"].append(repr(e))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "facade_flip_report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    log(json.dumps({k: report.get(k) for k in ("modules", "flipped", "by_mesh", "errors", "saved")}))


main()
