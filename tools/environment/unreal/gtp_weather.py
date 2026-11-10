"""Weather nodes for the street material generators (Unreal Editor Python).

The day-night and weather subsystem (GTPEnvironmentSubsystem) writes the
parameter collection /Game/Environment/Weather/MPC_Environment at run time.
Material generators owned by other tools call add_weather() at the end of
their graph, after connecting base colour, roughness and emissive, so the
wet look and the night emissive survive every regeneration:

    sys.path.insert(0, os.path.join(REPO, "tools", "environment", "unreal"))
    import gtp_weather
    gtp_weather.add_weather(mat, ("wet", "puddles", "emissive"))   # then recompile

Roles: "wet" darkens the albedo and lowers the roughness with Wetness (less on
walls), "puddles" adds puddles on flat ground with Puddles, "emissive"
multiplies the emissive by EmissiveScale (1 by day, about 20 at night).
The collection and the puddle texture are created on first use, so the city
import can run before or after tools/environment/setup-environment-mac.sh.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import textures  # noqa: E402

FOLDER = "/Game/Environment/Weather"
MPC_PATH = FOLDER + "/MPC_Environment"
PUDDLE_TEXTURE = FOLDER + "/T_PuddleNoise"
SCALARS = (("Wetness", 0.0), ("Puddles", 0.0), ("RainIntensity", 0.0), ("CityLights", 0.0),
           ("EmissiveScale", 1.0), ("Cloudiness", 0.0), ("TimeOfDay", 12.0), ("CloudLuminance", 1000.0))
WALL_WETNESS = 0.35      # share of the wetness kept on vertical surfaces
WET_DARKEN = 0.55        # albedo multiplier of a soaked porous surface
WET_ROUGHNESS = 0.12
PUDDLE_ROUGHNESS = 0.03
PUDDLE_TILE_CM = 600.0


eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def collection():
    """Load MPC_Environment, creating it (with every parameter) when missing."""
    if eal.does_asset_exist(MPC_PATH):
        mpc = eal.load_asset(MPC_PATH)
    else:
        mpc = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MPC_Environment", FOLDER, unreal.MaterialParameterCollection,
            unreal.MaterialParameterCollectionFactoryNew())
    existing = {str(p.get_editor_property("parameter_name")): p for p in mpc.get_editor_property("scalar_parameters")}
    params = []
    for name, default in SCALARS:
        p = existing.get(name)
        if p is None:
            p = unreal.CollectionScalarParameter()
            p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", default)
        params.append(p)
    mpc.set_editor_property("scalar_parameters", params)
    eal.save_loaded_asset(mpc)
    return mpc


def texture(name, generator):
    """Load a procedural texture of FOLDER, generating and importing it when missing."""
    path = FOLDER + "/" + name
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    saved = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Environment")
    os.makedirs(saved, exist_ok=True)
    png = os.path.join(saved, name + ".png")
    w, h, px = generator()
    textures.write_png(png, w, h, px)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = eal.load_asset(path)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    eal.save_loaded_asset(tex)
    return tex


class Graph:
    """Small helper over MaterialEditingLibrary (positions only matter for readability)."""

    def __init__(self, mat, x=-1400):
        self.mat, self.x, self.y = mat, x, 0

    def node(self, cls, **props):
        e = mel.create_material_expression(self.mat, cls, self.x, self.y)
        self.y += 140
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, b, b_in="", a_out=""):
        mel.connect_material_expressions(a, a_out, b, b_in)

    def collection(self, mpc, name):
        return self.node(unreal.MaterialExpressionCollectionParameter, collection=mpc, parameter_name=name)

    def lerp(self, a, b, alpha, a_out="", b_out="", alpha_out=""):
        n = self.node(unreal.MaterialExpressionLinearInterpolate)
        for src, pin, out in ((a, "A", a_out), (b, "B", b_out), (alpha, "Alpha", alpha_out)):
            if isinstance(src, (int, float)):
                n.set_editor_property({"A": "const_a", "B": "const_b", "Alpha": "const_alpha"}[pin], float(src))
            else:
                self.link(src, n, pin, out)
        return n

    def mul(self, a, b, a_out="", b_out=""):
        n = self.node(unreal.MaterialExpressionMultiply)
        for src, pin, out in ((a, "A", a_out), (b, "B", b_out)):
            if isinstance(src, (int, float)):
                n.set_editor_property({"A": "const_a", "B": "const_b"}[pin], float(src))
            else:
                self.link(src, n, pin, out)
        return n

    def saturate(self, a):
        n = self.node(unreal.MaterialExpressionSaturate)
        self.link(a, n)
        return n


def input_of(mat, prop):
    node = mel.get_material_property_input_node(mat, prop)
    if node is None:
        return None, ""
    return node, mel.get_material_property_input_node_output_name(mat, prop) or ""


def add_weather(mat, roles, mpc=None, puddle_tex=None):
    """Insert the weather nodes between the material's current inputs and its pins."""
    mpc = mpc or collection()
    if puddle_tex is None and "puddles" in roles:
        puddle_tex = texture(PUDDLE_TEXTURE.rsplit("/", 1)[1], textures.puddle_noise)
    g = Graph(mat, x=-2200)
    if "wet" in roles:
        normal = g.node(unreal.MaterialExpressionVertexNormalWS)
        up_mask = g.node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
        g.link(normal, up_mask)
        up = g.saturate(up_mask)
        wet = g.mul(g.collection(mpc, "Wetness"), g.lerp(WALL_WETNESS, 1.0, up))

        wet_rough = wet
        darken = g.lerp(1.0, WET_DARKEN, wet)
        if "puddles" in roles:
            pos = g.node(unreal.MaterialExpressionWorldPosition)
            xy = g.node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
            g.link(pos, xy)
            uv = g.mul(xy, 1.0 / PUDDLE_TILE_CM)
            noise = g.node(unreal.MaterialExpressionTextureSample, texture=puddle_tex,
                           sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
            g.link(uv, noise, "UVs")
            # puddle where the noise is above 1 - Puddles, on flat ground only
            level = g.node(unreal.MaterialExpressionSubtract)
            g.link(noise, level, "A", "R")
            g.link(g.lerp(1.0, 0.0, g.collection(mpc, "Puddles")), level, "B")
            flat = g.saturate(g.lerp(-7.0, 1.0, up))
            puddle = g.mul(g.saturate(g.mul(level, 4.0)), flat)
        src, out = input_of(mat, unreal.MaterialProperty.MP_ROUGHNESS)
        rough = g.lerp(src if src else 0.5, WET_ROUGHNESS, wet_rough, a_out=out)
        if "puddles" in roles:
            rough = g.lerp(rough, PUDDLE_ROUGHNESS, puddle)
            darken = g.mul(darken, g.lerp(1.0, 0.85, puddle))
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

        src, out = input_of(mat, unreal.MaterialProperty.MP_BASE_COLOR)
        if src:
            mel.connect_material_property(g.mul(src, darken, a_out=out), "", unreal.MaterialProperty.MP_BASE_COLOR)
    if "emissive" in roles:
        src, out = input_of(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if src:
            scaled = g.mul(src, g.collection(mpc, "EmissiveScale"), a_out=out)
            mel.connect_material_property(scaled, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    eal.set_metadata_tag(mat, "GTPWeather", "1")


