"""Read-only probe for Nicolito's skin and eyebrows (docs/gameplay/player-character.md, "Nicolito's face").

The crowd's synthesized skin materials take their colour from atlases (FaceColorAtlas, BodyColorAtlas) at
SourceU / SourceV, with an AtlasSelector, all left at the instances' defaults in our copy (City Sample sets them per
character at run time): BP_Nicolito_Full came out dark brown. And the eyebrows were not bound again because
GroomBlueprintLibrary.create_new_groom_binding_asset_with_path is not exposed. This script, run in the GTAL editor,
changes no asset:

1. For every head material of the six male heads and the crowd's male body material: the atlas parameters
   (AtlasSelector, SourceU, SourceV, the melanin and redness seeds) and the colour textures, side by side.
2. Exports FaceColorAtlas and BodyColorAtlas as PNG to Saved/GTPCombat/atlas/ (to look at where light skin is).
3. The editor's Python names with "Groom" in them, and the calls of each (to bind a groom to another face).

    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=tools/character/nicolito_skin_probe.py -unattended -nosplash

Report: Saved/GTPCombat/nicolito-skin-probe.txt (lines also in the log, prefixed "SKIN").
"""
import os

import unreal

MALE = "/Game/Crowd/Character/Male"
HEADS = ["m_%03d" % number for number in range(1, 7)]
BODY_MATERIAL = MALE + "/NormalWeight/Materials/M_BodySynthesized"
ATLASES = ["/Game/Crowd/Character/Shared/Textures/Atlases/FaceColorAtlas",
           "/Game/Crowd/Character/Shared/Textures/Atlases/BodyColorAtlas"]
WATCHED = ("atlas", "source", "melanin", "redness", "tone", "skin")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
lines = []


def say(text):
    lines.append(text)
    unreal.log("SKIN " + text)


def value_of(material, kind, name):
    try:
        if isinstance(material, unreal.MaterialInstance):
            value = getattr(mel, "get_material_instance_%s_parameter_value" % kind)(material, name)
        else:
            value = getattr(mel, "get_material_default_%s_parameter_value" % kind)(material, name)
    except Exception:  # noqa: BLE001 (not readable this way)
        return "?"
    return value.get_path_name() if hasattr(value, "get_path_name") else value


def describe(label, path):
    material = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if material is None:
        say("%s: %s missing" % (label, path))
        return
    found = []
    for kind, names in (("scalar", mel.get_scalar_parameter_names), ("texture", mel.get_texture_parameter_names)):
        for name in names(material):
            text = str(name)
            if any(word in text.lower() for word in WATCHED) or (kind == "texture" and "color" in text.lower()):
                found.append("%s=%s" % (text, value_of(material, kind, name)))
    say("%s: %s: %s" % (label, path.rsplit("/", 1)[-1], "; ".join(found) or "-"))


def materials():
    say("== 1. Atlas parameters of the head materials and of the body")
    for head in HEADS:
        folder = "%s/%s/Materials" % (MALE, head)
        if not eal.does_directory_exist(folder):
            say("%s: no Materials folder" % head)
            continue
        for path in sorted(eal.list_assets(folder, recursive=False, include_folder=False)):
            name = path.split(".")[0].rsplit("/", 1)[-1]
            if "Head" in name and "LOD" not in name:
                describe(head, path.split(".")[0])
    describe("body", BODY_MATERIAL)


def atlases():
    say("== 2. Atlases exported as PNG")
    folder = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat", "atlas")
    os.makedirs(folder, exist_ok=True)
    for path in ATLASES:
        texture = eal.load_asset(path) if eal.does_asset_exist(path) else None
        if texture is None:
            say("%s missing" % path)
            continue
        size = "%dx%d" % (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) if hasattr(texture, "blueprint_get_size_x") else "?"
        task = unreal.AssetExportTask()
        task.object = texture
        task.filename = os.path.join(folder, path.rsplit("/", 1)[-1] + ".png")
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        ok = unreal.Exporter.run_asset_export_task(task)
        say("%s (%s, %s): exported %s to Saved/GTPCombat/atlas/%s.png" % (path, texture.get_class().get_name(), size, ok, path.rsplit("/", 1)[-1]))


def grooms():
    say("== 3. Groom calls in the editor's Python")
    for name in sorted(n for n in dir(unreal) if "groom" in n.lower()):
        thing = getattr(unreal, name)
        calls = sorted(m for m in dir(thing) if not m.startswith("_") and callable(getattr(thing, m, None)) and m not in dir(unreal.Object))
        say("%s: %s" % (name, ", ".join(calls)[:900]))


def run():
    for step in (materials, atlases, grooms):
        try:
            step()
        except Exception as error:  # noqa: BLE001 (reported, the other steps still run)
            say("BLOCKED in %s: %r" % (step.__name__, error))
    folder = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "nicolito-skin-probe.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/GTPCombat/nicolito-skin-probe.txt")


run()
