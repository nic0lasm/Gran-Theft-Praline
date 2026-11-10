"""Step 1 of the far-crowd mesh (Unreal commandlet): export the player mesh and the walk loop to FBX.

    GTP_PED_OUT=<staging>/ped UnrealEditor-Cmd <uproject> -run=pythonscript \
        -script=tools/pedestrians/export_ped_source.py -unattended -nosplash -stdout

Output: <GTP_PED_OUT>/SKM.fbx, <GTP_PED_OUT>/Anim.fbx, <GTP_PED_OUT>/source.json.
GTP_PED_SKM / GTP_PED_ANIM override the defaults (the mesh of BP_PlayerCharacter, the first forward
walk loop that is not a shuffle, turn, start, stop or side step). No map is loaded.
The pose is frozen and decimated in Blender (bake_ped_pose.py): the bake API of a pose into a static
mesh does not exist in Unreal 5.8 (tried: create_static_mesh_from_skeletal_mesh, force_refpose).
"""
import json
import os

import unreal

OUT = os.environ.get("GTP_PED_OUT", "")
SKM = os.environ.get("GTP_PED_SKM", "")
ANIM = os.environ.get("GTP_PED_ANIM", "")
BAD = ("turn", "start", "stop", "left", "right", "_l_", "_r_", "back", "bwd", "shuffle", "crouch", "stumble")


def log(msg):
    unreal.log_warning("[ped_source] " + str(msg))


def resolve():
    bp = unreal.load_object(None, "/Game/Characters/Player/BP_PlayerCharacter.BP_PlayerCharacter_C")
    mesh = unreal.get_default_object(bp).get_editor_property("mesh").get_editor_property("skeletal_mesh_asset")
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    best, best_score = None, -1
    for a in reg.get_assets_by_path("/Game/Characters", recursive=True):
        n = str(a.asset_name).lower()
        if str(a.asset_class_path.asset_name) != "AnimSequence" or "walk" not in n or any(w in n for w in BAD):
            continue
        score = (2 if "loop" in n else 0) + (2 if ("fwd" in n or "_f_" in n) else 0)
        if score > best_score:
            best, best_score = a.get_asset(), score
    return mesh, best


def export(asset, path):
    task = unreal.AssetExportTask()
    task.set_editor_property("object", asset)
    task.set_editor_property("filename", path)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("prompt", False)
    ok = unreal.Exporter.run_asset_export_task(task)
    if not ok or not os.path.exists(path):
        raise RuntimeError(f"export failed: {asset.get_path_name()} -> {path}")
    log(f"exported {asset.get_path_name()} -> {path} ({os.path.getsize(path)} bytes)")


if not OUT:
    raise SystemExit("set GTP_PED_OUT")
os.makedirs(OUT, exist_ok=True)
skm = unreal.load_asset(SKM) if SKM else None
anim = unreal.load_asset(ANIM) if ANIM else None
if skm is None or anim is None:
    r_skm, r_anim = resolve()
    skm, anim = skm or r_skm, anim or r_anim
if skm is None or anim is None:
    raise SystemExit(f"no mesh/anim found (mesh={skm}, anim={anim})")
export(skm, os.path.join(OUT, "SKM.fbx"))
export(anim, os.path.join(OUT, "Anim.fbx"))
length = anim.get_editor_property("sequence_length") if hasattr(anim, "get_editor_property") else 0.0
with open(os.path.join(OUT, "source.json"), "w", encoding="utf-8") as f:
    json.dump({"skm": skm.get_path_name(), "anim": anim.get_path_name(), "length_s": float(length)}, f, indent=1)
log("DONE")
