"""Give the backdrop volumes a collision, inside the Unreal Editor (headless, like place_props.py).

The backdrop volumes (tools/city/backdrop.py, placed by import_city.py) came without collision:
they were a skyline seen from the street. The flight of the car theft runs among them, so in gate
65's video the player walked into one at 79 s (the armed-walk step traces a clear way and saw
none) and the camera stayed inside it, black, for more than 10 s. Same fix as the facade kit
(place_facades.kit_collision, 13/11): complex-as-simple collision on each backdrop mesh, and
BlockAll on its component in the street map, so pawns, cars and the camera stop at the walls.

Steps (idempotent): collision on the meshes of /Game/Environment/City/<Zone>/Backdrop/<Focus>,
open the map, BlockAll on the static mesh components in the outliner folder City/Backdrop, save.
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

eal = unreal.EditorAssetLibrary


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
