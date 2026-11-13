"""One closed spline per building footprint, for City Sample's PCG building primitives.

Input: a JSON of tools/city/pcg_footprints.py (a block, the slice...) named by GTP_PCG_FOOTPRINTS.
In the level GTP_PCG_LEVEL (default /Game/Tests/PCG/L_PCG_<json name>, created when missing,
"current" = the level already open) it places, per building, an actor "Footprint_<id>" whose root
is a SplineComponent: closed, linear points in world space (cm, same coordinates as
L_City_RueDeLaRepublique). The actor and its spline carry the tags
  GTP_PCGFootprint, BuildingId:<id>, MaxHeight:<cm> (eave, or ridge with GTP_PCG_HEIGHT=top),
  Family:<haussmann|lyonnais|moderne>, Storeys:<n>, Roof:<mansard|cornice|genoise|none>, Shops:<0|1>
(the grammar of each building is in the CSV next to the JSON, keyed by BuildingId).
A rerun replaces the actors tagged GTP_PCGFootprint only. Report: Saved/PCGTest/footprints.json.

Run without the UI (one Unreal at a time), the path of the script given by GTP_PY_SCRIPT:
  GTP_PY_SCRIPT=<repo>/tools/city/unreal/create_footprint_splines.py \\
  GTP_PCG_FOOTPRINTS=<repo>/assets/geo/processed/presqu-ile/pcg/block-69382AH76.json \\
  UnrealEditor-Cmd GranTheftPraline.uproject -unattended -nosplash -nosound \\
      "-ExecutePythonScript=__import__('gtp_run').main()"
Nothing is checked in an editor yet beyond the 10-building trial of the PCG thread.
"""

import json
import os

import unreal

SRC = os.environ.get("GTP_PCG_FOOTPRINTS", "")
NAME = "".join(p.capitalize() for p in os.path.splitext(os.path.basename(SRC))[0].replace("_", "-").split("-"))
LEVEL = os.environ.get("GTP_PCG_LEVEL", "/Game/Tests/PCG/L_PCG_" + NAME)
HEIGHT = os.environ.get("GTP_PCG_HEIGHT", "eave")          # eave | top
FOLDER = "PCG/Footprints"

actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def log(msg):
    unreal.log_warning("[PCGFootprints] " + msg)


def add_spline(actor):
    """A SplineComponent as the actor's root (Python cannot add components otherwise)."""
    handles = sds.k2_gather_subobject_data_for_instance(actor)
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.SplineComponent,
                                          blueprint_context=None)
    handle, fail = sds.add_new_subobject(params)
    if str(fail):
        raise RuntimeError("spline component: %s" % fail)
    return sdl.get_object(sdl.get_data(handle))


def main():
    if not os.path.isfile(SRC):
        raise FileNotFoundError("GTP_PCG_FOOTPRINTS is not a file: %r" % SRC)
    with open(SRC, encoding="utf-8") as f:
        data = json.load(f)
    if LEVEL != "current":
        if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
            level_sub.load_level(LEVEL)
        else:
            level_sub.new_level(LEVEL)
    for a in list(actor_sub.get_all_level_actors()):          # rerun: replace our own splines only
        if a.actor_has_tag("GTP_PCGFootprint"):
            actor_sub.destroy_actor(a)
    report = {"source": SRC, "level": LEVEL, "height": HEIGHT, "actors": [], "errors": []}
    for b in data["buildings"]:
        try:
            pts = b["points_cm"]
            cx = sum(p[0] for p in pts) / len(pts)
            cy = sum(p[1] for p in pts) / len(pts)
            z = b["ground_z_cm"]
            actor = actor_sub.spawn_actor_from_class(unreal.Actor, unreal.Vector(cx, cy, z))
            actor.set_actor_label("Footprint_" + b["id"])
            actor.set_folder_path(FOLDER)
            spline = add_spline(actor)
            actor.set_actor_location(unreal.Vector(cx, cy, z), False, False)
            spline.clear_spline_points(False)
            for p in pts:
                spline.add_spline_point(unreal.Vector(p[0], p[1], p[2]), unreal.SplineCoordinateSpace.WORLD, False)
            for i in range(len(pts)):
                spline.set_spline_point_type(i, unreal.SplinePointType.LINEAR, False)
            spline.set_closed_loop(True, False)
            spline.update_spline()
            h = b["max_height_cm"] if HEIGHT == "eave" else b["top_height_cm"]
            g = b.get("grammar") or {}
            tags = [unreal.Name(t) for t in (
                "GTP_PCGFootprint", "BuildingId:" + b["id"], "MaxHeight:%d" % h,
                "Family:" + g.get("family", "none"), "Storeys:%d" % len(g.get("storeys", [])),
                "Roof:" + g.get("roof", "none"), "Shops:%d" % int(bool(b.get("ground_floor_commercial"))))]
            actor.set_editor_property("tags", tags)
            spline.set_editor_property("component_tags", tags)
            report["actors"].append({"label": actor.get_actor_label(), "points": spline.get_number_of_spline_points(),
                                     "closed": spline.is_closed_loop(), "max_height_cm": h,
                                     "length_cm": round(spline.get_spline_length(), 1)})
        except Exception as e:  # noqa: BLE001  keep going, report it
            report["errors"].append("%s: %r" % (b.get("id"), e))
    level_sub.save_current_level()
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "PCGTest")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "footprints.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    log("%d splines, %d errors -> %s" % (len(report["actors"]), len(report["errors"]), out))


main()
