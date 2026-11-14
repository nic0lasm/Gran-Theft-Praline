"""Does the retarget of the Lyra weapon clips lower the pelvis? (docs/gameplay/combat.md, read only)

Measured on candidate 31b while aiming: the pawn's pelvis 62 cm under the capsule centre instead of 3 above, the
head 66 cm lower. Either the retargeted clip itself has a low pelvis (retarget: pelvis translation, root), or the
clip is fine and the drop comes from how it is played. This script, run in the GTAL editor, changes nothing:

1. For every retargeted sequence in /Game/Combat/Weapons/Anim (suffix _UEFN) and its Lyra source (same name
   without the suffix, found anywhere under /Game): root and pelvis height in component space at frame 0 and at
   mid-clip, next to the reference pose of each skeleton (their pelvis standing height).
2. The settings of the retargeter that made them (RTG_GTP_UE5_to_UEFN): every op, enabled or not, with its
   settings as text (the pelvis motion op holds the translation mode and scales).

    UnrealEditor GranTheftPraline.uproject -run=pythonscript -script=tools/combat/unreal/check_weapon_clip_pelvis.py

Report: Saved/GTPCombat/weapon-clip-pelvis.txt (lines also in the log, prefixed "PELVIS"). Reading it: a
retargeted pelvis close to its skeleton's reference height (ratio near the source's) means the clip is fine.
"""
import os

import unreal

OUT_DIR = "/Game/Combat/Weapons/Anim"
SUFFIX = "_UEFN"
RETARGETERS = ["/Game/Combat/Weapons/RTG_GTP_UE5_to_UEFN"]
BONES = ("root", "pelvis", "head")

eal = unreal.EditorAssetLibrary
lines = []


def say(text):
    lines.append(text)
    unreal.log("PELVIS " + text)


def describe(value):
    for fn in ("export_text",):
        if hasattr(value, fn):
            try:
                return getattr(value, fn)()[:900]
            except Exception:
                pass
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    return str(value)[:900]


def pose_at(sequence, time):
    """The animation's pose at that time (AnimPose, root motion left in the root), or None."""
    options = unreal.AnimPoseEvaluationOptions()
    for name, value in (("extract_root_motion", False), ("evaluation_type", unreal.AnimDataEvalType.RAW)):
        try:
            options.set_editor_property(name, value)
        except Exception:
            pass
    try:
        result = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
        return result[0] if isinstance(result, tuple) else result
    except Exception as error:
        say("  pose at %.2f s not read (%s)" % (time, error))
        return None


def heights(pose):
    """Component-space height of each bone of BONES present in the pose (cm)."""
    found = {}
    if pose is None:
        return found
    for bone in BONES:
        try:
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            found[bone] = transform.translation.z
        except Exception:
            pass
    return found


def reference_heights(skeleton):
    try:
        return heights(unreal.AnimPoseExtensions.get_reference_pose(skeleton))
    except Exception as error:
        say("  reference pose of %s not read (%s)" % (skeleton.get_name() if skeleton else None, error))
        return {}


def text(values):
    return ", ".join("%s %.1f" % (bone, values[bone]) for bone in BONES if bone in values) or "-"


def sources_by_name():
    """Every AnimSequence under /Game outside OUT_DIR, by asset name (registry, nothing loaded)."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.wait_for_completion()
    found = {}
    for data in registry.get_assets_by_path("/Game", recursive=True):
        if str(data.asset_class_path.asset_name) != "AnimSequence":
            continue
        package = str(data.package_name)
        if package.startswith(OUT_DIR + "/"):
            continue
        found.setdefault(str(data.asset_name), package)
    return found


def clip_report(label, sequence, reference):
    length = sequence.get_play_length() if hasattr(sequence, "get_play_length") else 0.0
    start = heights(pose_at(sequence, 0.0))
    middle = heights(pose_at(sequence, length * 0.5)) if length > 0 else {}
    ratio = ""
    if "pelvis" in start and reference.get("pelvis"):
        ratio = ", pelvis at %.0f%% of its standing height" % (100.0 * start["pelvis"] / reference["pelvis"])
    say("  %s %s (%.2f s): frame 0: %s; middle: %s%s" % (label, sequence.get_name(), length, text(start), text(middle), ratio))
    return start


def check_clips():
    say("== 1. Pelvis height, Lyra source against retargeted (component space, cm)")
    sources = sources_by_name()
    references = {}
    for package in sorted(eal.list_assets(OUT_DIR, recursive=False, include_folder=False)):
        package = package.split(".")[0]
        name = package.rsplit("/", 1)[-1]
        if not name.endswith(SUFFIX):
            continue
        made = eal.load_asset(package)
        if not isinstance(made, unreal.AnimSequence):
            continue
        say("-- %s" % name[: -len(SUFFIX)])
        for label, sequence in (("source", eal.load_asset(sources[name[: -len(SUFFIX)]]) if name[: -len(SUFFIX)] in sources else None), ("retargeted", made)):
            if sequence is None:
                say("  %s: not found" % label)
                continue
            skeleton = sequence.get_editor_property("skeleton")
            key = skeleton.get_path_name() if skeleton else ""
            if key not in references:
                references[key] = reference_heights(skeleton) if skeleton else {}
                say("  reference pose of %s: %s" % (key, text(references[key])))
            clip_report(label, sequence, references[key])


def check_retargeters():
    say("== 2. Retargeter ops")
    for path in RETARGETERS:
        say("-- %s" % path)
        rtg = eal.load_asset(path) if eal.does_asset_exist(path) else None
        if not rtg:
            say("  missing")
            continue
        controller = unreal.IKRetargeterController.get_controller(rtg)
        say("  controller calls with 'op': %s" % ", ".join(sorted(m for m in dir(controller) if "op" in m and not m.startswith("_"))))
        count = controller.get_num_retarget_ops() if hasattr(controller, "get_num_retarget_ops") else 0
        for index in range(count):
            name = enabled = "?"
            for fn in ("get_op_name", "get_retarget_op_name"):
                if hasattr(controller, fn):
                    try:
                        name = str(getattr(controller, fn)(index))
                        break
                    except Exception:
                        pass
            for fn in ("get_retarget_op_enabled", "get_op_enabled"):
                if hasattr(controller, fn):
                    try:
                        enabled = getattr(controller, fn)(index)
                        break
                    except Exception:
                        pass
            say("  op %d %s enabled=%s" % (index, name, enabled))
            # The op's settings, by whichever call this engine has.
            for fn, args in (("get_op_controller", (index,)), ("get_retarget_op_at_index", (index,)),
                             ("get_retarget_op_by_index", (index,)), ("get_op_settings", (index,)),
                             ("get_op_settings", (unreal.Name(name),))):
                if not hasattr(controller, fn):
                    continue
                try:
                    value = getattr(controller, fn)(*args)
                except Exception:
                    continue
                if value is None:
                    continue
                say("    %s: %s" % (fn, describe(value)))
                if hasattr(value, "get_settings"):
                    try:
                        say("    settings: %s" % describe(value.get_settings()))
                    except Exception as error:
                        say("    settings not read (%s)" % error)
                break


def main():
    try:
        check_clips()
    except Exception as error:
        say("BLOCKED: clips not read (%s)" % error)
    try:
        check_retargeters()
    except Exception as error:
        say("BLOCKED: retargeter not read (%s)" % error)
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat")
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "weapon-clip-pelvis.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/GTPCombat/weapon-clip-pelvis.txt")


main()
