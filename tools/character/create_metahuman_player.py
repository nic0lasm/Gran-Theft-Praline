"""Creates /Game/Characters/Player/BP_PlayerCharacter_MetaHuman: our player on the sample's MetaHuman.

  UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="<repo>/tools/character/create_metahuman_player.py" -unattended -nosplash -nullrhi

Needs the sample's /Game/MetaHumans copied in (BP_Kellan) and our BP_PlayerCharacter (create_player_assets.py).
The new blueprint is a child of BP_Kellan (the MetaHuman wired to the GASP animation); every property
that our BP_PlayerCharacter sets differently from its parent (input mapping contexts, camera toggle...)
is copied over. Lines "METAHUMAN ..." in the log; a failing step is reported, not fatal. Idempotent.
Try it without touching the config:  -PlayerPawn=/Game/Characters/Player/BP_PlayerCharacter_MetaHuman.BP_PlayerCharacter_MetaHuman_C
Env: GTP_MH_SOURCE (default: first BP_Kellan found under /Game/MetaHumans).
"""
import os

import unreal

OUT_DIR = "/Game/Characters/Player"
NAME = "BP_PlayerCharacter_MetaHuman"
OURS = OUT_DIR + "/BP_PlayerCharacter"
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("METAHUMAN " + msg)


source = os.environ.get("GTP_MH_SOURCE", "")
if not source:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for d in registry.get_assets_by_path("/Game/MetaHumans", recursive=True):
        if str(d.asset_name) == "BP_Kellan":
            source = str(d.package_name) + "." + str(d.asset_name)
            break
if not source:
    raise SystemExit("METAHUMAN no BP_Kellan under /Game/MetaHumans")
log("source " + source)

kellan_bp = unreal.load_asset(source)
kellan_class = kellan_bp.generated_class()
path = OUT_DIR + "/" + NAME
if eal.does_asset_exist(path):
    # Unattended runs cannot delete over an existing asset: reuse it (idempotent, properties are copied again).
    bp = unreal.load_asset(path)
    log("reusing %s" % path)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", kellan_class)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, OUT_DIR, unreal.Blueprint, factory)
    log("created %s (parent %s)" % (path, kellan_class.get_name()))

try:
    ours = unreal.load_asset(OURS).generated_class()
    ours_cdo = unreal.get_default_object(ours)
    # BlueprintGeneratedClass has no get_super_class in 5.8: our blueprint's parent is the sample's character.
    parent_cdo = unreal.get_default_object(unreal.load_class(None, "/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C"))
    target_cdo = unreal.get_default_object(bp.generated_class())
    copied = []
    for prop in dir(ours_cdo):
        if prop.startswith("_") or callable(getattr(ours_cdo, prop, None)):
            continue
        try:
            mine = ours_cdo.get_editor_property(prop)
            theirs = parent_cdo.get_editor_property(prop)
        except Exception:  # noqa: BLE001 (not an editor property)
            continue
        if str(mine) != str(theirs):
            try:
                target_cdo.set_editor_property(prop, mine)
                copied.append(prop)
            except Exception as exc:  # noqa: BLE001
                log("could not copy %s: %r" % (prop, exc))
    log("copied from BP_PlayerCharacter: %s" % ", ".join(copied))
except Exception as exc:  # noqa: BLE001
    log("FAILED property copy: %r" % (exc,))

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_loaded_asset(bp)
log("saved %s" % path)
