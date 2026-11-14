"""Why the player's MetaHuman body does not take the weapon poses, and a retargeter to test (docs/gameplay/combat.md).

Measured on candidate 28b: the aim pose plays on the pawn's (hidden) UEFN mannequin, its right hand 12 cm under
the head and 43 cm ahead, while the MetaHuman body that follows it by the run-time retarget keeps its hand 53 cm
under the head and 3 cm behind. This script, run in the GTAL editor:

1. Dumps, read only, the player's retargeter (PlayerVisualRetargeter of DefaultGame.ini) and Epic's own
   UEFN-to-MetaHuman retargeters of the Game Animation Sample: IK rigs per side, retarget ops (enabled or not),
   chain mapping, the settings the controller exposes for the arm chains, and the target rig's IK goals and
   solvers. Nothing of these assets is modified or saved.
2. Builds our own UEFN-to-MetaHuman retargeter, /Game/Combat/Rigs/RTG_GTP_UEFN_to_MetaHuman (kept if it exists),
   the way RTG_GTP_UE5_to_UEFN was built for the Lyra clips (IK rigs assigned to every op, chains mapped by name,
   bones auto-aligned), and dumps it the same way.

    UnrealEditor GranTheftPraline.uproject -run=pythonscript -script=tools/combat/unreal/player_visual_retarget.py

Report: Saved/GTPCombat/player-visual-retarget.txt (lines also in the log, prefixed "PVR"). To try ours in game:
    -PlayerVisualRetargeter=/Game/Combat/Rigs/RTG_GTP_UEFN_to_MetaHuman.RTG_GTP_UEFN_to_MetaHuman -CombatBench=1
and read the "weapon: hand probe" lines (the body's hand should follow the pawn's).
"""
import os

import unreal

MADE_RTG = "/Game/Combat/Rigs/RTG_GTP_UEFN_to_MetaHuman"
# The pawn's skeleton (the Game Animation Sample's UEFN mannequin), then the pedestrians' copy of its rig.
SOURCE_RIGS = [
    "/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin",
    "/Game/Characters/Pedestrians/Retargeted/IK_SKM_UEFN_Mannequin",
]
TARGET_RIGS = [
    "/Game/MetaHumans/Common/Common/Rigs/IK_Metahuman_Retarget",
]
# Epic's retargeters for MetaHumans in the Game Animation Sample (found by name, wherever they are).
EPIC_RTG_NAMES = ("RTG_UEFN_to_Metahuman_nrw", "RTG_UEFN_to_Metahuman_ovw")
ARM_WORDS = ("arm", "clavicle", "hand")

eal = unreal.EditorAssetLibrary
lines = []


def say(text):
    lines.append(text)
    unreal.log("PVR " + text)


def describe(value):
    """A struct or object as text, short."""
    for fn in ("export_text",):
        if hasattr(value, fn):
            try:
                return getattr(value, fn)()[:600]
            except Exception:
                pass
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    return str(value)[:600]


def ini_retargeter():
    path = os.path.join(unreal.Paths.project_config_dir(), "DefaultGame.ini")
    try:
        with open(path, encoding="utf-8") as handle:
            for line in handle:
                if line.startswith("PlayerVisualRetargeter="):
                    return line.split("=", 1)[1].strip().split(".")[0]
    except OSError as error:
        say("DefaultGame.ini not read: %s" % error)
    return "/Game/MetaHumans/GTP/RTG_UEFNped_to_Metahuman_Fixed"


def find_by_name(names):
    """Retargeters with these names anywhere under /Game (the registry walk lyra_weapons_setup.py uses on 5.8)."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.wait_for_completion()
    found = []
    for data in registry.get_assets_by_path("/Game", recursive=True):
        if str(data.asset_class_path.asset_name) == "IKRetargeter" and str(data.asset_name) in names:
            found.append(str(data.package_name))
    return found


def chain_names(rig):
    controller = unreal.IKRigController.get_controller(rig)
    for fn in ("get_retarget_chains", "get_retarget_chain_names"):
        if hasattr(controller, fn):
            return [str(c.chain_name) if hasattr(c, "chain_name") else str(c) for c in getattr(controller, fn)()]
    return []


def dump_rig(rig, label):
    if not rig:
        say("  %s rig: none" % label)
        return
    say("  %s rig: %s" % (label, rig.get_path_name()))
    controller = unreal.IKRigController.get_controller(rig)
    if hasattr(controller, "get_retarget_chains"):
        for chain in controller.get_retarget_chains():
            say("    chain %s" % describe(chain))
    for fn in ("get_num_solvers",):
        if hasattr(controller, fn):
            try:
                count = getattr(controller, fn)()
                for i in range(count):
                    solver = controller.get_solver_at_index(i) if hasattr(controller, "get_solver_at_index") else None
                    enabled = "?"
                    for efn in ("get_solver_enabled", "is_solver_enabled"):
                        if hasattr(controller, efn):
                            try:
                                enabled = getattr(controller, efn)(i)
                            except Exception:
                                pass
                    say("    solver %d: %s enabled=%s" % (i, describe(solver) if solver else "?", enabled))
            except Exception as error:
                say("    solvers not listed: %s" % error)
    for fn in ("get_all_goals",):
        if hasattr(controller, fn):
            try:
                for goal in getattr(controller, fn)():
                    say("    goal %s" % describe(goal))
            except Exception as error:
                say("    goals not listed: %s" % error)


def dump_retargeter(path, show_api=False):
    say("== %s" % path)
    rtg = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if not rtg:
        say("  missing")
        return
    controller = unreal.IKRetargeterController.get_controller(rtg)
    if show_api:
        say("  controller getters: %s" % ", ".join(sorted(m for m in dir(controller) if m.startswith("get_") or m.startswith("is_"))))
    rigs = {}
    for side in (unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET):
        rig = None
        try:
            rig = controller.get_ik_rig(side)
        except Exception as error:
            say("  get_ik_rig %s failed: %s" % (side, error))
        rigs[side] = rig
        say("  %s IK rig: %s" % (side, rig.get_path_name() if rig else "none"))
    # Retarget ops (UE 5.6+): which run, in which order.
    op_names = []
    count = -1
    for fn in ("get_num_retarget_ops",):
        if hasattr(controller, fn):
            try:
                count = getattr(controller, fn)()
            except Exception as error:
                say("  ops not counted: %s" % error)
    say("  ops: %s" % count)
    for i in range(max(count, 0)):
        name = "?"
        for fn in ("get_op_name", "get_retarget_op_name"):
            if hasattr(controller, fn):
                try:
                    name = str(getattr(controller, fn)(i))
                    break
                except Exception:
                    pass
        enabled = "?"
        for fn in ("get_op_enabled", "get_retarget_op_enabled"):
            if hasattr(controller, fn):
                try:
                    enabled = getattr(controller, fn)(i)
                    break
                except Exception:
                    pass
        op = None
        if hasattr(controller, "get_retarget_op_at_index"):
            try:
                op = controller.get_retarget_op_at_index(i)
            except Exception:
                op = None
        op_names.append(name)
        say("  op %d %s enabled=%s %s" % (i, name, enabled, describe(op) if op is not None else ""))
    # Chain mapping, for every target chain (arms first in mind), per op when the API wants an op name.
    target = rigs.get(unreal.RetargetSourceOrTarget.TARGET)
    for chain in chain_names(target) if target else []:
        mapped = []
        for fn in ("get_source_chain",):
            if not hasattr(controller, fn):
                continue
            for args in [(chain,), (unreal.Name(chain),)] + [(chain, unreal.Name(op)) for op in op_names if op != "?"]:
                try:
                    mapped.append("%s%s" % (getattr(controller, fn)(*args), (" (op %s)" % args[1]) if len(args) > 1 else ""))
                except Exception:
                    pass
        say("  target chain %s <- %s" % (chain, "; ".join(mapped) or "?"))
        if any(word in chain.lower() for word in ARM_WORDS):
            # Every getter taking the chain name, read only (FK/IK settings, goals).
            for fn in sorted(m for m in dir(controller) if m.startswith("get_") and ("chain" in m or "setting" in m) and m != "get_source_chain"):
                for args in ((chain,), (unreal.Name(chain),)):
                    try:
                        say("    %s(%s) = %s" % (fn, chain, describe(getattr(controller, fn)(*args))))
                        break
                    except Exception:
                        pass
    dump_rig(rigs.get(unreal.RetargetSourceOrTarget.SOURCE), "source")
    dump_rig(target, "target")


def first_asset(paths):
    for path in paths:
        if eal.does_asset_exist(path):
            return path, eal.load_asset(path)
    return None, None


def make_retargeter():
    if eal.does_asset_exist(MADE_RTG):
        say("retargeter kept: %s" % MADE_RTG)
        return
    source_path, source = first_asset(SOURCE_RIGS)
    target_path, target = first_asset(TARGET_RIGS)
    if not source or not target:
        say("BLOCKED: IK rigs missing (source %s, target %s)" % (source_path, target_path))
        return
    folder, name = MADE_RTG.rsplit("/", 1)
    if not eal.does_directory_exist(folder):
        eal.make_directory(folder)
    rtg = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(rtg)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target)
    if hasattr(controller, "add_default_ops"):
        controller.add_default_ops()
    if hasattr(controller, "assign_ik_rig_to_all_ops"):
        controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, source)
        controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target)
    for mode in ("EXACT", "FUZZY"):
        try:
            controller.auto_map_chains(getattr(unreal.AutoMapChainType, mode), True)
            break
        except Exception as error:
            say("auto_map_chains %s failed: %s" % (mode, error))
    by_lower = {n.lower(): n for n in chain_names(source)}
    mapped = []
    for chain in chain_names(target):
        wanted = by_lower.get(chain.lower())
        if not wanted:
            continue
        for args in ((wanted, chain), (unreal.Name(wanted), unreal.Name(chain))):
            try:
                controller.set_source_chain(*args)
                mapped.append(chain)
                break
            except Exception:
                pass
    say("chains mapped by name: %s" % (", ".join(mapped) or "none"))
    for side in (unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET):
        try:
            controller.auto_align_all_bones(side)
        except Exception as error:
            say("auto_align_all_bones skipped (%s)" % error)
    eal.save_loaded_asset(rtg)
    say("built %s from %s and %s" % (MADE_RTG, source_path, target_path))


def main():
    say("== 1. Retargeters in use and Epic's (read only)")
    dump_retargeter(ini_retargeter(), show_api=True)
    for path in find_by_name(EPIC_RTG_NAMES):
        dump_retargeter(path)
    say("== 2. Our retargeter")
    try:
        make_retargeter()
    except Exception as error:
        say("BLOCKED: could not build %s (%s)" % (MADE_RTG, error))
    dump_retargeter(MADE_RTG)
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "player-visual-retarget.txt")
    with open(out, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/GTPCombat/player-visual-retarget.txt")


main()
