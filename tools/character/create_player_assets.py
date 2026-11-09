"""Create our player character and on-foot input assets (run in GranTheftPraline).

  UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="<repo>/tools/character/create_player_assets.py" -unattended -nosplash -nullrhi

Needs the Game Animation Sample content in Content/ (install_gasp_content.sh).
Creates or rebuilds, idempotently:
  /Game/Characters/Player/BP_PlayerCharacter   child of SandboxCharacter_CMC
  /Game/Input/GTP/IA_ToggleCameraMode
  /Game/Input/GTP/IMC_OnFoot_KBM_AZERTY, IMC_OnFoot_KBM_QWERTY, IMC_OnFoot_Gamepad
The mappings use the sample's input actions with the same modifiers as its
IMC_Sandbox (swizzle and negate on keys, dead zones and scaling on sticks).
Escape and the gamepad Menu button are left to the pause menu.
"""

import unreal

PLAYER_DIR = "/Game/Characters/Player"
INPUT_DIR = "/Game/Input/GTP"
SAMPLE_INPUT = "/Game/Input/"
PARENT_CHARACTER = "/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("[PlayerAssets] " + msg)


def fresh_asset(name, folder, asset_class, factory):
    path = folder + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    asset = asset_tools.create_asset(name, folder, asset_class, factory)
    if asset is None:
        raise RuntimeError("could not create " + path)
    return asset


def action(name):
    asset = unreal.load_asset(SAMPLE_INPUT + name)
    if asset is None:
        raise RuntimeError("missing sample input action " + name)
    return asset


# Modifiers --------------------------------------------------------------

def swizzle(outer):
    m = unreal.new_object(unreal.InputModifierSwizzleAxis, outer=outer)
    m.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    return m


def negate(outer):
    return unreal.new_object(unreal.InputModifierNegate, outer=outer)


def dead_zone(outer, lower, kind):
    m = unreal.new_object(unreal.InputModifierDeadZone, outer=outer)
    m.set_editor_property("lower_threshold", lower)
    m.set_editor_property("upper_threshold", 1.0)
    m.set_editor_property("type", kind)
    return m


def scalar(outer, x, y, z=1.0):
    m = unreal.new_object(unreal.InputModifierScalar, outer=outer)
    m.set_editor_property("scalar", unreal.Vector(x, y, z))
    return m


def exponential(outer):
    return unreal.new_object(unreal.InputModifierResponseCurveExponential, outer=outer)


def mapping(act, key_name, modifiers=()):
    m = unreal.EnhancedActionKeyMapping()
    m.set_editor_property("action", act)
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    m.set_editor_property("key", key)
    m.set_editor_property("modifiers", list(modifiers))
    return m


def build_context(name, rows):
    imc = fresh_asset(name, INPUT_DIR, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    data = imc.get_editor_property("default_key_mappings")
    data.set_editor_property("mappings", [mapping(a, k, mods(imc)) for a, k, mods in rows])
    imc.set_editor_property("default_key_mappings", data)
    eal.save_loaded_asset(imc)
    log("%s: %d mappings" % (name, len(rows)))
    return imc


def keyboard_rows(forward, left, back, right, toggle_camera):
    move, look = action("IA_Move"), action("IA_Look")
    jump, traverse = action("IA_Jump"), action("IA_Traverse")
    none = lambda o: []
    return [
        (move, forward, lambda o: [swizzle(o)]),
        (move, back, lambda o: [swizzle(o), negate(o)]),
        (move, left, lambda o: [negate(o)]),
        (move, right, none),
        (look, "Mouse2D", lambda o: [scalar(o, 1.0, -1.0)]),
        (action("IA_Sprint"), "LeftShift", none),
        (action("IA_Walk"), "LeftAlt", none),
        (jump, "SpaceBar", none),
        (traverse, "SpaceBar", none),
        (action("IA_Crouch"), "C", none),
        (toggle_camera, "V", none),
    ]


def gamepad_rows(toggle_camera):
    jump, traverse = action("IA_Jump"), action("IA_Traverse")
    none = lambda o: []
    return [
        (action("IA_Move"), "Gamepad_Left2D", lambda o: [dead_zone(o, 0.3, unreal.DeadZoneType.RADIAL)]),
        (action("IA_Look_Gamepad"), "Gamepad_Right2D",
         lambda o: [dead_zone(o, 0.2, unreal.DeadZoneType.UNSCALED_RADIAL), exponential(o), scalar(o, 90.0, 70.0)]),
        (action("IA_Sprint"), "Gamepad_FaceButton_Bottom", none),
        (action("IA_Walk"), "Gamepad_LeftThumbstick", none),
        (jump, "Gamepad_FaceButton_Left", none),
        (traverse, "Gamepad_FaceButton_Left", none),
        (action("IA_Crouch"), "Gamepad_FaceButton_Right", none),
        (toggle_camera, "Gamepad_Special_Left", none),
    ]


def create_toggle_action():
    ia = fresh_asset("IA_ToggleCameraMode", INPUT_DIR, unreal.InputAction, unreal.InputAction_Factory())
    ia.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    ia.set_editor_property("triggers", [unreal.new_object(unreal.InputTriggerPressed, outer=ia)])
    eal.save_loaded_asset(ia)
    return ia


def create_player_blueprint():
    parent = unreal.load_class(None, PARENT_CHARACTER)
    if parent is None:
        raise RuntimeError("missing parent class " + PARENT_CHARACTER)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = fresh_asset("BP_PlayerCharacter", PLAYER_DIR, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)
    log("BP_PlayerCharacter created, parent " + parent.get_name())


def main():
    eal.make_directory(PLAYER_DIR)
    eal.make_directory(INPUT_DIR)
    toggle = create_toggle_action()
    # Letters: Unreal reads the character typed, so on an AZERTY keyboard the key
    # labelled Z arrives as Z. ZQSD is the AZERTY layout of WASD.
    build_context("IMC_OnFoot_KBM_AZERTY", keyboard_rows("Z", "Q", "S", "D", toggle))
    build_context("IMC_OnFoot_KBM_QWERTY", keyboard_rows("W", "A", "S", "D", toggle))
    build_context("IMC_OnFoot_Gamepad", gamepad_rows(toggle))
    create_player_blueprint()
    log("done")


main()
