"""Read-only probe of the player's runtime retarget and of the eyebrows and hair the project has, for Nicolito's
visual (docs/gameplay/player-character.md, "Nicolito's face"). Changes no asset.

The portrait bench's numbers (2026-11-14, "rig: ..." lines): the visual body follows the pawn's pose (left upper
arm 17 to 18 degrees from straight down on both: the "A pose" seen is the sample's idle), but its pelvis sits at the
component's origin while every other bone is in place. Every vertex weighted to the pelvis (a shirt's hem, the
crotch) is pulled towards the feet: the panel hanging between the legs, with Kellan's body and outfit as with the
crowd's. The pelvis is the retarget's root, set by the retargeter's ops (pelvis motion, root motion) and the IK rigs'
retarget root. And the eyebrows bound without transfer landed on the upper lip.

    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=tools/character/nicolito_rig_probe.py -unattended -nosplash

1. RTG_UEFNped_to_Metahuman (PlayerVisualRetargeter) and its source and target IK rigs: retarget root, chains,
   skeletal mesh, the retargeter's ops and the controller's op calls; each of the three exported as text (T3D: every
   property, the ops' settings included) to Saved/GTPCombat/rig/.
2. Assets whose name has "brow" under /Game/Crowd and /Game/MetaHumans, the groom assets and groom bindings under
   both, and the crowd's hair assets: the crowd's own eyebrows if City Sample ships them, and the hair to choose from.
Report: Saved/GTPCombat/nicolito-rig-probe.txt (lines also in the log, prefixed "RIGPROBE").
"""
import os

import unreal

RETARGETER = "/Game/MetaHumans/GTP/RTG_UEFNped_to_Metahuman"
ROOTS = ["/Game/Crowd", "/Game/MetaHumans"]
MAX_LINES = 200

eal = unreal.EditorAssetLibrary
lines = []
folder = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat")


def say(text):
    lines.append(text)
    unreal.log("RIGPROBE " + text)


def export_text(asset, name):
    """The asset as text (every property), or why not."""
    target = os.path.join(folder, "rig")
    os.makedirs(target, exist_ok=True)
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(target, name + ".t3d")
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    exporter = getattr(unreal, "ObjectExporterT3D", None)
    for attempt in (None, exporter):
        if attempt is not None:
            try:
                task.exporter = unreal.new_object(attempt)
            except Exception as error:  # noqa: BLE001 (reported)
                return "not exported (%r)" % (error,)
        try:
            if unreal.Exporter.run_asset_export_task(task) and os.path.exists(task.filename):
                return "exported to Saved/GTPCombat/rig/%s.t3d (%d bytes)" % (name, os.path.getsize(task.filename))
        except Exception as error:  # noqa: BLE001 (the other exporter)
            last = error
            continue
        last = "no exporter"
    return "not exported (%s)" % (last,)


def bone_of(value):
    for read in (lambda: value.get_editor_property("bone_name"), lambda: value):
        try:
            return str(read())
        except Exception:  # noqa: BLE001 (the other way)
            pass
    return "?"


def describe_rig(label, rig):
    if rig is None:
        say("%s IK rig: none" % label)
        return
    controller = unreal.IKRigController.get_controller(rig)
    parts = []
    for name, read in (("retarget root", lambda: str(controller.get_retarget_root())),
                       ("skeletal mesh", lambda: controller.get_skeletal_mesh().get_path_name())):
        try:
            parts.append("%s %s" % (name, read()))
        except Exception as error:  # noqa: BLE001 (reported)
            parts.append("%s ? (%s)" % (name, type(error).__name__))
    try:
        chains = ["%s=%s..%s" % (bone_of(chain.get_editor_property("chain_name")), bone_of(chain.get_editor_property("start_bone")),
                                 bone_of(chain.get_editor_property("end_bone"))) for chain in controller.get_retarget_chains()]
        parts.append("%d chains: %s" % (len(chains), ", ".join(chains)))
    except Exception as error:  # noqa: BLE001 (reported)
        parts.append("chains ? (%r)" % (error,))
    say("%s IK rig %s: %s" % (label, rig.get_path_name(), "; ".join(parts)))
    say("  %s" % export_text(rig, label.lower() + "_" + rig.get_name()))


def retarget():
    say("== 1. The player's retarget (%s)" % RETARGETER)
    retargeter = eal.load_asset(RETARGETER) if eal.does_asset_exist(RETARGETER) else None
    if retargeter is None:
        say("missing")
        return
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    for label, side in (("Source", unreal.RetargetSourceOrTarget.SOURCE), ("Target", unreal.RetargetSourceOrTarget.TARGET)):
        try:
            rig = controller.get_ik_rig(side)
        except Exception:  # noqa: BLE001 (the property instead)
            rig = None
        if rig is None:
            try:
                rig = retargeter.get_editor_property("%s_ik_rig_asset" % label.lower())
            except Exception:  # noqa: BLE001 (reported as none)
                rig = None
        describe_rig(label, rig)
    try:
        count = controller.get_num_retarget_ops()
        for index in range(count):
            enabled = "?"
            for name in ("get_retarget_op_enabled", "get_op_enabled"):
                if hasattr(controller, name):
                    try:
                        enabled = getattr(controller, name)(index)
                    except Exception:  # noqa: BLE001 (left unknown)
                        pass
                    break
            say("op %d: %s, enabled %s" % (index, controller.get_retarget_op_at_index(index), enabled))
    except Exception as error:  # noqa: BLE001 (reported)
        say("ops not listed (%r)" % (error,))
    say("controller calls: %s" % ", ".join(sorted(m for m in dir(controller) if not m.startswith("_") and callable(getattr(controller, m, None))
                                                  and m not in dir(unreal.Object))))
    say("retargeter %s" % export_text(retargeter, retargeter.get_name()))


def assets():
    say("== 2. Eyebrows, grooms, bindings and hair under %s" % ", ".join(ROOTS))
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for root in ROOTS:
        if eal.does_directory_exist(root):
            found.extend(registry.get_assets_by_path(root, recursive=True))
    say("%d assets under those folders" % len(found))
    groups = {"brow": [], "GroomAsset": [], "GroomBindingAsset": [], "hair": []}
    for data in found:
        name = str(data.asset_name)
        try:
            kind = str(data.asset_class_path.asset_name)
        except Exception:  # noqa: BLE001 (older field)
            kind = str(getattr(data, "asset_class", "?"))
        line = "%s %s" % (kind, data.package_name)
        if "brow" in name.lower():
            groups["brow"].append(line)
        if kind in ("GroomAsset", "GroomBindingAsset"):
            groups[kind].append(line)
        elif "hair" in name.lower() and str(data.package_name).startswith("/Game/Crowd"):
            groups["hair"].append(line)
    for key, entries in groups.items():
        say("-- %s: %d" % (key, len(entries)))
        for entry in sorted(entries)[:MAX_LINES]:
            say("  " + entry)
        if len(entries) > MAX_LINES:
            say("  ... and %d more" % (len(entries) - MAX_LINES))


def run():
    for step in (retarget, assets):
        try:
            step()
        except Exception as error:  # noqa: BLE001 (reported, the other step still runs)
            say("BLOCKED in %s: %r" % (step.__name__, error))
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "nicolito-rig-probe.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/GTPCombat/nicolito-rig-probe.txt")


run()
