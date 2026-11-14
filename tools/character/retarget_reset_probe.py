"""Which blueprint puts the sample's retargeter back on the player's body at start. Read only: changes no asset.

Candidate 39 in the editor (2026-11-14): the game mode points the body's retarget at PlayerVisualRetargeter
(RTG_UEFNped_to_Metahuman_Fixed), then "body retarget: 1 setting(s) put back by a blueprint (... var IKRetargeter
RTG_UEFN_to_Metahuman_nrw)", set again a quarter of a second later, then steady. The variable is the retarget
anim blueprint's (ABP_GenericRetarget); something writes the Game Animation Sample's retargeter into it once.

    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=tools/character/retarget_reset_probe.py -unattended -nosplash

1. Every package that refers to RTG_UEFN_to_Metahuman_nrw (asset registry referencers, hard and soft).
2. Each of those, and BP_Kellan, BP_Nicolito_Full and ABP_GenericRetarget, exported as text (T3D: the blueprint's
   variables with their defaults and its graphs' nodes with their pins) to Saved/GTPCombat/rig/; every line that
   names the retargeter is reported with the objects it sits in (graph, node), so the event or function that writes
   it shows. The anim blueprint's class default of IKRetargeter is read too.
Report: Saved/GTPCombat/retarget-reset-probe.txt (lines also in the log, prefixed "RTGRESET").
"""
import os
import re

import unreal

SAMPLE_RTG = "RTG_UEFN_to_Metahuman_nrw"
ALSO = ("BP_Kellan", "BP_Nicolito_Full", "ABP_GenericRetarget")
MAX_HITS = 40

eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
lines = []
folder = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat")


def say(text):
    lines.append(text)
    unreal.log("RTGRESET " + text)


def by_name(names):
    """{asset name: package} for these names under /Game."""
    registry.wait_for_completion()
    found = {}
    for data in registry.get_assets_by_path("/Game", recursive=True):
        name = str(data.asset_name)
        if name in names and name not in found:
            found[name] = str(data.package_name)
    return found


def referencers(package):
    options = unreal.AssetRegistryDependencyOptions()
    for name, value in (("include_hard_package_references", True), ("include_soft_package_references", True),
                        ("include_searchable_names", False), ("include_soft_management_references", False),
                        ("include_hard_management_references", False)):
        try:
            options.set_editor_property(name, value)
        except Exception:  # noqa: BLE001 (left at its default)
            pass
    return sorted(str(name) for name in (registry.get_referencers(package, options) or []))


def export_text(asset, name):
    """The asset as text, with the T3D exporter first (a blueprint has no other), or None."""
    target = os.path.join(folder, "rig")
    os.makedirs(target, exist_ok=True)
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(target, name + ".t3d")
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    for exporter in (getattr(unreal, "ObjectExporterT3D", None), None):
        try:
            task.exporter = unreal.new_object(exporter) if exporter else None
            if unreal.Exporter.run_asset_export_task(task) and os.path.exists(task.filename):
                return task.filename
        except Exception as error:  # noqa: BLE001 (the other exporter)
            say("  %s: exporter %s failed (%r)" % (name, exporter.__name__ if exporter else "automatic", error))
    return None


def hits(path):
    """Lines naming the retargeter, each with the chain of objects it sits in."""
    stack = []
    found = []
    with open(path, encoding="utf-8", errors="replace") as handle:
        for number, line in enumerate(handle, 1):
            text = line.strip()
            begin = re.match(r"Begin Object(?: Class=(\S+))? Name=\"?([^\" ]+)", text)
            if begin:
                stack.append("%s(%s)" % (begin.group(2), (begin.group(1) or "").rsplit(".", 1)[-1]))
            elif text.startswith("End Object") and stack:
                stack.pop()
            if SAMPLE_RTG in text:
                found.append("line %d in %s: %s" % (number, " > ".join(stack[-3:]) or "the asset", text[:400]))
    return found


def main():
    paths = by_name((SAMPLE_RTG,) + ALSO)
    rtg = paths.get(SAMPLE_RTG)
    say("%s: %s" % (SAMPLE_RTG, rtg or "not found under /Game"))
    refs = referencers(rtg) if rtg else []
    say("== 1. %d package(s) refer to it: %s" % (len(refs), ", ".join(refs) or "none"))
    todo = list(refs)
    for name in ALSO:
        if paths.get(name) and paths[name] not in todo:
            todo.append(paths[name])
    say("== 2. Text exports")
    for package in todo:
        name = package.rsplit("/", 1)[-1]
        asset = eal.load_asset(package)
        if asset is None:
            say("%s: not loaded" % package)
            continue
        say("%s (%s), refers to %s: %s" % (package, asset.get_class().get_name(), SAMPLE_RTG, package in refs))
        if isinstance(asset, unreal.Blueprint):
            try:
                default = unreal.get_default_object(asset.generated_class())
                value = default.get_editor_property("IKRetargeter")
                say("  class default IKRetargeter: %s" % (value.get_path_name() if value else "none"))
            except Exception:  # noqa: BLE001 (no such variable)
                pass
        path = export_text(asset, "reset_" + name)
        if path is None:
            say("  not exported")
            continue
        found = hits(path)
        say("  exported (%d bytes), %d line(s) name %s" % (os.path.getsize(path), len(found), SAMPLE_RTG))
        for hit in found[:MAX_HITS]:
            say("    " + hit)
        if len(found) > MAX_HITS:
            say("    ... and %d more" % (len(found) - MAX_HITS))


def run():
    try:
        main()
    except Exception as error:  # noqa: BLE001 (reported, the report is still written)
        say("BLOCKED: %r" % (error,))
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "retarget-reset-probe.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/GTPCombat/retarget-reset-probe.txt")


run()
