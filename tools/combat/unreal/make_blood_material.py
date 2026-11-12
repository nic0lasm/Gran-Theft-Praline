"""Builds the blood pool decal material of the combat system (ADR-0011).

Run in the Unreal editor (Python), once per clone:
    UnrealEditor GranTheftPraline.uproject -run=pythonscript -script=tools/combat/unreal/make_blood_material.py

Creates /Game/Combat/Materials/M_GTP_BloodPool: a deferred decal, dark red, wet
(low roughness), round with a soft edge. UGTPCombatSubsystem loads it by that path
(BloodPoolMaterial) and lays it under dead bodies ("GTA-like" blood, Nico 2026-11-11).
Made from engine nodes only: no third-party content, nothing to download.
"""
import unreal

MAT_DIR = "/Game/Combat/Materials"
NAME = "M_GTP_BloodPool"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning("[CombatMaterials] " + msg)


def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, inp, out=""):
    names = inp if isinstance(inp, tuple) else ((inp,) if inp else ("", "Input", "X"))
    for name in names:
        if mel.connect_material_expressions(a, out, b, name):
            return
    raise RuntimeError("cannot connect %s -> %s.%s" % (a.get_name(), b.get_name(), inp))


def build():
    path = MAT_DIR + "/" + NAME
    if eal.does_asset_exist(path):
        log("%s already exists, left as it is" % path)
        return
    factory = unreal.MaterialFactoryNew()
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, MAT_DIR, unreal.Material, factory)
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    # Round pool: opacity = (1 - 2 * |uv - 0.5|) ^ 0.6, soft edge.
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    half = node(mat, unreal.MaterialExpressionConstant2Vector, -1200, 120, r=0.5, g=0.5)
    centred = node(mat, unreal.MaterialExpressionSubtract, -1000, 0)
    link(uv, centred, "A")
    link(half, centred, "B")
    dist2 = node(mat, unreal.MaterialExpressionDotProduct, -850, 0)
    link(centred, dist2, "A")
    link(centred, dist2, "B")
    dist = node(mat, unreal.MaterialExpressionSquareRoot, -700, 0)
    link(dist2, dist, "")
    two = node(mat, unreal.MaterialExpressionConstant, -700, 120, r=2.0)
    scaled = node(mat, unreal.MaterialExpressionMultiply, -550, 0)
    link(dist, scaled, "A")
    link(two, scaled, "B")
    inv = node(mat, unreal.MaterialExpressionOneMinus, -400, 0)
    link(scaled, inv, "")
    sat = node(mat, unreal.MaterialExpressionSaturate, -260, 0)
    link(inv, sat, "")
    exp = node(mat, unreal.MaterialExpressionConstant, -260, 120, r=0.6)
    soft = node(mat, unreal.MaterialExpressionPower, -120, 0)
    link(sat, soft, "Base")
    link(exp, soft, ("Exp", "Exponent"))  # UE 5.8 names the Power exponent pin "Exp"

    colour = node(mat, unreal.MaterialExpressionConstant3Vector, -260, -200, constant=unreal.LinearColor(0.11, 0.004, 0.004, 1.0))
    rough = node(mat, unreal.MaterialExpressionConstant, -260, -100, r=0.12)

    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(soft, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("built %s" % path)


build()
