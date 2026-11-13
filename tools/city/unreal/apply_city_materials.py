"""Replace the greybox material of the generated city with art-bible ground and walls.

Run after import_city.py (and place_facades.py), same way, no UI:
  UnrealEditor GranTheftPraline.uproject -unattended -nosplash -NSAppSleepDisabled YES \
      "-ExecutePythonScript=__import__('gtp_run').main()" (with GTP_PY_SCRIPT=tools/city/unreal/apply_city_materials.py)
Settings: GTP_CITY_ZONE (presqu-ile), GTP_CITY_FOCUS (rue-de-la-republique),
GTP_CITY_GROUND_DIR (default assets/geo/processed/<zone>/gen/<focus>/ground, made by
tools/city/ground_mask.py).

Steps (idempotent, only material assets and the mesh slots change, the map is not rebuilt):
  1. import T_GroundMask.png (linear, masks) into /Game/Environment/City/<Zone>/Materials
  2. M_City_Ground: the mask is read at world X/Y (ground.json extent):
     asphalt carriageway, kerb stones (mask without alpha: the PNG importer would
     fill the colour of transparent texels), granite paving on streets closed to cars,
     limestone slabs on sidewalks, dashed centre lines; joints and a slow tone
     variation are procedural (no texture fetch beyond the mask)
  3. M_City_Shell: the building colour the generator wrote in the vertex colours,
     stone courses on the walls, rougher walls than roofs
  4. assign them to every terrain tile and building shell mesh, save the meshes
  4b. M_City_Kit and one instance per facade kit slot (colours of kit.py; stone slots take
     the building colour from per-instance custom data) on every kit mesh, in place of the
     Interchange glTF instances that render as the default checker in the packaged game
  5. report in Saved/CityImport/materials_report.json (with a check of the facade
     instances and of the kit materials in the map)
"""

import json
import os
import sys
import time

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
GROUND_DIR = os.environ.get("GTP_CITY_GROUND_DIR") or \
    os.path.join(REPO, "assets", "geo", "processed", ZONE, "gen", FOCUS, "ground")
CAMEL = "".join(p.capitalize() for p in FOCUS.split("-"))
ROOT = "/Game/Environment/City/" + "".join(p.capitalize() for p in ZONE.split("-"))
MAT_DIR = ROOT + "/Materials"
MAP = "/Game/Maps/L_City_" + CAMEL
KIT_DIR = "/Game/Environment/Facades/LyonKit"

# Art bible, dry daylight albedos (linear).
# Kept well below the facade stone: under the bright Noon exposure the first values
# (0.34 / 0.21) read almost white in game and the street lost all contrast.
SIDEWALK = (0.19, 0.18, 0.165)     # limestone slabs
PAVING = (0.13, 0.125, 0.12)       # grey granite setts and slabs
KERB = (0.10, 0.10, 0.10)          # granite kerb
ASPHALT = (0.025, 0.025, 0.028)
MARKING = (0.5, 0.5, 0.48)
# GTP_GROUND_DEBUG=1: loud colours to check the mask in game (asphalt red, paving green,
# kerb blue, marking yellow, sidewalk white). Rerun without it afterwards.
if os.environ.get("GTP_GROUND_DEBUG") == "1":
    SIDEWALK, PAVING, KERB, ASPHALT, MARKING = (0.8, 0.8, 0.8), (0.05, 0.6, 0.05), (0.05, 0.05, 0.8), \
        (0.8, 0.03, 0.03), (0.9, 0.8, 0.0)

# Facade kit slots (tools/city/facade_kit/kit.py MATERIALS): linear albedo, roughness, metallic.
# The glTF import gives them Interchange instances that render as the default checker in
# the packaged game; they are replaced by instances of one lean master, M_City_Kit.
KIT_SLOTS = {
    "MI_Facade_LimestoneCream": (0.66, 0.60, 0.50, 0.82, 0.0),
    "MI_Facade_LimestoneGreyPlinth": (0.42, 0.41, 0.39, 0.75, 0.0),
    "MI_Facade_Render": (0.70, 0.52, 0.34, 0.92, 0.0),
    "MI_Facade_PierreDoree": (0.62, 0.48, 0.30, 0.85, 0.0),
    "MI_Window_PaintedWood": (0.80, 0.79, 0.76, 0.45, 0.0),
    "MI_Glass_Window": (0.02, 0.025, 0.03, 0.04, 0.0),
    "MI_Ironwork_Painted": (0.025, 0.025, 0.028, 0.75, 0.0),   # 0.55: thin bars sparkled under the street lamps (PC V5 13/11)
    "MI_Shutter_Painted": (0.30, 0.36, 0.33, 0.55, 0.0),
    "MI_Shopfront_Aluminium": (0.20, 0.20, 0.21, 0.35, 1.0),
    "MI_Shopfront_Wood": (0.10, 0.17, 0.15, 0.40, 0.0),
    "MI_Door_Wood": (0.13, 0.07, 0.035, 0.45, 0.0),
    "MI_Genoise_Tile": (0.50, 0.24, 0.14, 0.80, 0.0),
    "MI_Sign_Fascia": (0.04, 0.04, 0.045, 0.40, 0.0),
    "MI_Shopfront_Steel": (0.03, 0.03, 0.032, 0.35, 1.0),
    "MI_Shutter_Metal": (0.45, 0.46, 0.47, 0.5, 1.0),
    "MI_Shop_Interior": (0.80, 0.78, 0.74, 0.7, 0.0),
    "MI_Shop_Floor": (0.35, 0.33, 0.31, 0.4, 0.0),
    "MI_Shop_Display": (0.25, 0.22, 0.2, 0.6, 0.0),
    "MI_Sign_Panel": (0.5, 0.5, 0.5, 0.35, 0.0),
    "MI_Glass_Shop": (0.05, 0.06, 0.065, 0.05, 0.0),   # translucent: M_City_GlassShop
    "MI_Persienne": (0.62, 0.60, 0.56, 0.65, 0.0),     # folded metal shutters (facade kit v1)
    "MI_Roof_Zinc": (0.40, 0.42, 0.44, 0.45, 0.6),      # mansard modules (facade_kit/roof_kit.py)
}
KIT_EMISSIVE = {"MI_Shop_Interior": (1.0, 0.92, 0.8, 1.2)}
# Share of the facade weathering each slot takes (1 when not listed).
KIT_WEATHERING = {"MI_Window_PaintedWood": 0.5, "MI_Shutter_Painted": 0.5, "MI_Door_Wood": 0.5,
                  "MI_Shopfront_Wood": 0.4, "MI_Ironwork_Painted": 0.3, "MI_Sign_Fascia": 0.3,
                  "MI_Shopfront_Aluminium": 0.2, "MI_Shopfront_Steel": 0.2, "MI_Shutter_Metal": 0.4,
                  "MI_Glass_Window": 0.15, "MI_Glass_Shop": 0.1, "MI_Shop_Interior": 0.0,
                  "MI_Shop_Floor": 0.0, "MI_Shop_Display": 0.0, "MI_Sign_Panel": 0.0,
                  "MI_Persienne": 0.5}
# Stone slots take the building colour place_facades.py writes in per-instance custom data 0-2.
KIT_BUILDING_COLOUR = ("MI_Facade_LimestoneCream", "MI_Facade_Render", "MI_Facade_PierreDoree")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning("[CityMaterials] " + msg)


class Graph:
    """Small helper over MaterialEditingLibrary: x placement only matters for readability."""

    def __init__(self, mat):
        self.mat, self.n = mat, 0

    def node(self, cls, **props):
        self.n += 1
        e = mel.create_material_expression(self.mat, cls, -1200 + 160 * (self.n % 8), 120 * (self.n // 8))
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def const(self, v):
        if isinstance(v, (int, float)):
            return self.node(unreal.MaterialExpressionConstant, r=float(v))
        if len(v) == 2:
            return self.node(unreal.MaterialExpressionConstant2Vector, r=float(v[0]), g=float(v[1]))
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(*v, 1.0))

    def link(self, a, out, b, inp):
        # Single-input nodes (Frac, Abs, Saturate, ComponentMask, Sine) take "" in most
        # versions; try the usual property names before giving up.
        for name in ((inp,) if inp else ("", "Input", "X")):
            if mel.connect_material_expressions(a, out, b, name):
                return
        raise RuntimeError("cannot connect %s.%s -> %s.%s" % (a.get_name(), out, b.get_name(), inp))

    def op(self, cls, a, b, a_out="", b_out=""):
        e = self.node(cls)
        self.link(a, a_out, e, "A")
        self.link(b, b_out, e, "B")
        return e

    def mul(self, a, b, a_out="", b_out=""):
        return self.op(unreal.MaterialExpressionMultiply, a, b, a_out, b_out)

    def add(self, a, b, a_out="", b_out=""):
        return self.op(unreal.MaterialExpressionAdd, a, b, a_out, b_out)

    def sub(self, a, b, a_out="", b_out=""):
        return self.op(unreal.MaterialExpressionSubtract, a, b, a_out, b_out)

    def div(self, a, b, a_out="", b_out=""):
        return self.op(unreal.MaterialExpressionDivide, a, b, a_out, b_out)

    def lerp(self, a, b, alpha, alpha_out=""):
        e = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, "", e, "A")
        self.link(b, "", e, "B")
        self.link(alpha, alpha_out, e, "Alpha")
        return e

    def unary(self, cls, src, src_out="", **props):
        # "src", not "a": ComponentMask has a property named "a" (alpha channel).
        e = self.node(cls, **props)
        self.link(src, src_out, e, "")
        return e

    def world_position(self):
        """Absolute world position as plain floats. The raw node is a large-world (LWC)
        double; fed into texture UVs, Frac or Sine it can make the material fall back to
        the engine's grey grid in the cooked game. The city spans ~1.4 km: floats suffice."""
        wp = self.node(unreal.MaterialExpressionWorldPosition)
        cls = getattr(unreal, "MaterialExpressionTruncateLWC", None)
        return self.unary(cls, wp) if cls else wp

    def mask(self, a, r=False, g=False, b=False):
        return self.unary(unreal.MaterialExpressionComponentMask, a, r=r, g=g, b=b, a=False)

    def joints(self, xy, size_cm, width):
        """1 on the joint lines of a square grid of size_cm, 0 inside the slabs."""
        f = self.unary(unreal.MaterialExpressionFrac, self.div(xy, self.const(float(size_cm))))
        d = self.unary(unreal.MaterialExpressionAbs, self.sub(f, self.const(0.5)))
        m = self.op(unreal.MaterialExpressionMax, self.mask(d, r=True), self.mask(d, g=True))
        t = self.mul(self.sub(m, self.const(0.5 - width)), self.const(1.0 / width))
        return self.unary(unreal.MaterialExpressionSaturate, t)

    def done(self):
        mel.layout_material_expressions(self.mat)
        mel.recompile_material(self.mat)
        eal.save_loaded_asset(self.mat)


def surfaces():
    """Scanned surfaces (tools/city/unreal/gtp_surfaces.py, street realism thread): each hook does
    nothing until import_surfaces.py has written Saved/CityImport/surfaces.json, or with
    GTP_SURFACES=0, so the procedural look stays the default. Called before weather()."""
    import sys
    here = os.path.join(REPO, "tools", "city", "unreal")
    if here not in sys.path:
        sys.path.insert(0, here)
    import gtp_surfaces
    return gtp_surfaces


def weather(mat, roles):
    """Hook of the day/night and weather thread (tools/environment/unreal/gtp_weather.py):
    wetness, puddles and night emissive scale inserted between our sources and the material
    pins, neutral by day. Skipped while that module is not in the tree."""
    path = os.path.join(REPO, "tools", "environment", "unreal")
    if not os.path.exists(os.path.join(path, "gtp_weather.py")):
        return
    import sys
    if path not in sys.path:
        sys.path.insert(0, path)
    import gtp_weather
    gtp_weather.add_weather(mat, roles)


# Usage flags the cooked game needs. A material compiles shaders only for the usages saved
# with it: the Mac editor sets missing ones in memory when a mesh uses the material, but the
# saved asset kept none, so on Windows (Nanite on, DX12) 280 city and sign materials drew with
# the default material (11/11). Set them at creation, and on any material reused as it is.
MATERIAL_USAGES = ("used_with_nanite", "used_with_instanced_static_meshes")


def ensure_usage(mat, save=True):
    changed = False
    for flag in MATERIAL_USAGES:
        if not mat.get_editor_property(flag):
            mat.set_editor_property(flag, True)
            changed = True
    if changed and save:
        unreal.MaterialEditingLibrary.recompile_material(mat)
        unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def new_material(name):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return ensure_usage(asset_tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew()),
                        save=False)   # Graph.done() compiles and saves


def import_mask():
    path = os.path.join(GROUND_DIR, "T_GroundMask.png")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", MAT_DIR)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    asset_tools.import_asset_tasks([task])
    tex = eal.load_asset(MAT_DIR + "/T_GroundMask")
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    eal.save_loaded_asset(tex)
    return tex


def ground_material(tex, extent):
    x0, y0, x1, y1 = extent
    mat = new_material("M_City_Ground")
    g = Graph(mat)
    wp = g.world_position()
    xy = g.mask(wp, r=True, g=True)
    uv = g.div(g.sub(xy, g.const((x0, y0))), g.const((x1 - x0, y1 - y0)))
    s = g.node(unreal.MaterialExpressionTextureSample, texture=tex,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    g.link(uv, "", s, "UVs")

    # Slow tone variation (stains, wear) without a texture: product of two long sines.
    vx = g.unary(unreal.MaterialExpressionSine, g.mul(g.mask(wp, r=True), g.const(1.0 / 900.0)))
    vy = g.unary(unreal.MaterialExpressionSine, g.mul(g.mask(wp, g=True), g.const(1.0 / 1300.0)))
    tone = g.add(g.mul(g.mul(vx, vy), g.const(0.08)), g.const(1.0))

    slab = g.lerp(g.const(1.0), g.const(0.62), g.joints(xy, 60.0, 0.035))     # 60 cm limestone slabs
    sett = g.lerp(g.const(1.0), g.const(0.6), g.joints(xy, 22.0, 0.08))       # granite setts
    col = g.mul(g.const(SIDEWALK), slab)
    col = g.lerp(col, g.mul(g.const(PAVING), sett), s, "G")
    # B is a road marking on the carriageway (R) and a kerb stone off it.
    on_road = g.mask(s, r=True)
    b_ch = g.mask(s, b=True)
    kerb = g.mul(b_ch, g.sub(g.const(1.0), on_road))
    mark = g.mul(b_ch, on_road)
    col = g.lerp(col, g.const(KERB), kerb)
    # Asphalt patches (trench repairs, worn wheel paths): two shorter sines, asphalt only.
    px_ = g.unary(unreal.MaterialExpressionSine, g.mul(g.mask(wp, r=True), g.const(1.0 / 230.0)))
    py_ = g.unary(unreal.MaterialExpressionSine, g.add(g.mul(g.mask(wp, g=True), g.const(1.0 / 310.0)), g.const(0.7)))
    patch = g.add(g.const(1.0), g.mul(g.mul(px_, py_), g.const(0.22)))
    col = g.lerp(col, g.mul(g.const(ASPHALT), patch), s, "R")
    col = g.lerp(col, g.const(MARKING), mark)
    col = g.mul(col, tone)
    rough = g.lerp(g.lerp(g.const(0.78), g.const(0.92), s, "R"), g.const(0.6), mark)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    surfaces().ground(mat, s)
    weather(mat, ("wet", "puddles"))
    g.done()
    return mat


def shell_material():
    mat = new_material("M_City_Shell")
    mat.set_editor_property("two_sided", True)
    g = Graph(mat)
    vc = g.node(unreal.MaterialExpressionVertexColor)
    nz = g.unary(unreal.MaterialExpressionAbs, g.mask(g.node(unreal.MaterialExpressionVertexNormalWS), b=True))
    wall = g.unary(unreal.MaterialExpressionSaturate, g.mul(g.sub(g.const(0.8), nz), g.const(2.0)))
    wp = g.world_position()
    f = g.unary(unreal.MaterialExpressionFrac, g.div(g.mask(wp, b=True), g.const(42.0)))   # stone courses
    d = g.unary(unreal.MaterialExpressionAbs, g.sub(f, g.const(0.5)))
    course = g.unary(unreal.MaterialExpressionSaturate, g.mul(g.sub(d, g.const(0.46)), g.const(25.0)))
    shade = g.lerp(g.const(1.0), g.const(0.82), g.mul(course, wall))
    col = g.mul(g.mask(vc, r=True, g=True, b=True), shade)
    weathered, _ = weathering(g, col, per_instance=False)
    col = g.lerp(col, weathered, wall)                     # walls only, not the roofs
    rough = g.lerp(g.const(0.55), g.const(0.88), wall)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    surfaces().shell(mat)
    weather(mat, ("wet",))
    g.done()
    return mat


def street_level(g):
    """Street level of the building in cm, written by place_facades.py in custom data 3."""
    cls = getattr(unreal, "MaterialExpressionPerInstanceCustomData", None)
    if cls is None:
        return g.const(0.0)
    e = g.node(cls, data_index=3)
    for prop in ("const_default_value", "default_value"):
        try:
            e.set_editor_property(prop, 0.0)
            break
        except Exception:  # noqa: BLE001  property name differs between versions
            continue
    return e


def weathering(g, col, per_instance=True):
    """Art bible weathering, all ALU (no texture, cheap on M1): slow tint drift across
    the block, a darker plinth over the first 1-4 m (splashes, traffic soot) and rain
    streaks running down from each cornice and sill, strongest just below them.
    Scaled per slot by the Weathering parameter (stone 1, glass and interiors ~0)."""
    w = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Weathering", default_value=1.0)
    wp = g.world_position()
    x, y, z = g.mask(wp, r=True), g.mask(wp, g=True), g.mask(wp, b=True)
    h = g.sub(z, street_level(g)) if per_instance else z   # shells: the zone is flat near 0
    sat = unreal.MaterialExpressionSaturate
    one = g.const(1.0)
    plinth = g.add(g.mul(g.unary(sat, g.sub(one, g.div(h, g.const(120.0)))), g.const(0.30)),
                   g.mul(g.unary(sat, g.sub(one, g.div(h, g.const(400.0)))), g.const(0.10)))
    along = g.add(x, y)
    s1 = g.unary(unreal.MaterialExpressionSine, g.div(along, g.const(37.0)))
    s2 = g.unary(unreal.MaterialExpressionSine, g.add(g.div(along, g.const(83.0)), g.const(1.3)))
    streak = g.unary(sat, g.sub(g.mul(g.mul(s1, s2), g.const(2.0)), g.const(0.6)))
    t = g.unary(unreal.MaterialExpressionFrac, g.div(h, g.const(330.0)))     # storey height
    streak = g.mul(g.mul(streak, g.mul(t, t)), g.const(0.18))
    grime = g.mul(g.add(plinth, streak), w)
    vx = g.unary(unreal.MaterialExpressionSine, g.div(x, g.const(1700.0)))
    vy = g.unary(unreal.MaterialExpressionSine, g.div(y, g.const(2300.0)))
    tone = g.add(one, g.mul(g.mul(g.mul(vx, vy), g.const(0.06)), w))
    return g.mul(g.mul(col, tone), g.sub(one, grime)), grime


def kit_master():
    mat = new_material("M_City_Kit")
    g = Graph(mat)
    base = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="BaseColor",
                  default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    use = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="UseBuildingColour", default_value=0.0)
    # UE 5.8: the default is an input pin; the constant fallback is const_default_value.
    pic = g.node(unreal.MaterialExpressionPerInstanceCustomData3Vector, data_index=0,
                 const_default_value=unreal.LinearColor(0.66, 0.60, 0.50, 1.0))
    # Keep the slot's value contrast (cream stone vs plinth) and take the building's hue.
    col = g.lerp(g.mask(base, r=True, g=True, b=True), pic, use)
    col, grime = weathering(g, col)
    rough = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Roughness", default_value=0.8)
    rough = g.add(rough, g.mul(grime, g.const(0.1)))
    metal = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Metallic", default_value=0.0)
    emis = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="Emissive",
                  default_value=unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(g.mask(emis, r=True, g=True, b=True), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    surfaces().kit(mat)
    weather(mat, ("wet", "wet_matte", "emissive"))
    g.done()
    return mat


# Shop windows show the lit shop room behind them (~370 on the street side); the upper
# floor windows stay opaque dark glass. Cheapest translucent lighting on Metal.
SHOP_GLASS = "MI_Glass_Shop"
SHOP_GLASS_OPACITY = 0.12
# GTP_SHOP_GLASS=opaque: fallback if the translucent windows cost more than 0.8 ms on m1
# (fil optimisation): opaque reflective glass with the shop light faked in emissive.
# Default "interior" (perf thread, M1): opaque glass with a fake room by interior mapping.
SHOP_GLASS_MODE = os.environ.get("GTP_SHOP_GLASS", "interior")
SHOP_GLASS_FAKE_GLOW = (0.9, 0.78, 0.6, 0.35)


def shop_glass_master():
    mat = new_material("M_City_GlassShop")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("translucency_lighting_mode",
                            unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(mat)
    base = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="BaseColor",
                  default_value=unreal.LinearColor(0.05, 0.06, 0.065, 1.0))
    op = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Opacity", default_value=SHOP_GLASS_OPACITY)
    # A touch of Fresnel: grazing views read as glass, straight-on views see the shop.
    fres = g.node(unreal.MaterialExpressionFresnel)
    opacity = g.unary(unreal.MaterialExpressionSaturate, g.add(op, g.mul(fres, g.const(0.5))))
    rough = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Roughness", default_value=0.05)
    mel.connect_material_property(g.mask(base, r=True, g=True, b=True), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.const(0.9), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    g.done()
    return mat


# The interior mapping works in each window's own frame. The kit is drawn by instanced components
# (HISM, Nanite on Windows): "Local" space is the component's, shared by thousands of instances
# spread over the street, so the room grid fell anywhere and read as flat colour blocks (PC 13/11).
# "Instance & Particle Space" is the instance's own frame. Older engines without it keep Local.
INSTANCE_POS = getattr(unreal.MaterialPositionTransformSource, "TRANSFORMPOSSOURCE_INSTANCE",
                       unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
INSTANCE_VEC = getattr(unreal.MaterialVectorCoordTransform, "TRANSFORM_INSTANCE",
                       unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)


# GTP_SHOP_GLASS=interior: opaque glass showing a fake lit shop room by interior mapping (one
# ray-box test per pixel in the module's local frame): no translucency, no overdraw, and the
# kit's room geometry behind it is not needed. Room cell = one shop bay.
INTERIOR_HLSL = r"""
float3 d = normalize(-V);                       // view ray, local frame, into the facade
float3 cell = float3(300.0, 250.0, 300.0);      // bay width, room depth, room height (cm)
float2 id = floor(float2(P.x + 150.0, P.z - 12.0) / cell.xz);
float3 u = float3(frac((P.x + 150.0) / cell.x), 0.0, frac((P.z - 12.0) / cell.z));
float3 dn = float3(d.x / cell.x, abs(d.y) / cell.y, d.z / cell.z);
float tx = (dn.x > 0 ? 1.0 - u.x : -u.x) / (abs(dn.x) > 1e-5 ? dn.x : 1e-5);
float tz = (dn.z > 0 ? 1.0 - u.z : -u.z) / (abs(dn.z) > 1e-5 ? dn.z : 1e-5);
float ty = 1.0 / max(dn.y, 1e-5);
float t = min(min(tx, tz), ty);
float3 h = u + dn * t;
float hash = frac(sin(dot(id, float2(12.9898, 78.233)) + Seed) * 43758.5453);
float3 wallc = lerp(float3(0.66, 0.58, 0.48), float3(0.58, 0.52, 0.46), hash) * 0.8;   // warm: grey-blue read as glass (PC V5)
// goods on shelves: coloured blocks in rows, a dark shelf board under each row
float2 sq = t == ty ? h.xz : float2(h.y, h.z);   // back wall: across x; side walls: along the depth
float row = frac(sq.y * 5.0);
float shelfy = step(0.12, sq.y) * step(sq.y, 0.82);
float goods = frac(sin(floor(sq.x * 16.0) * 7.13 + floor(sq.y * 5.0) * 3.7 + hash * 11.0) * 9137.0);
float gap = step(0.12, frac(sq.x * 16.0)) * step(0.3, row) * step(row, 0.92);
float3 item = lerp(float3(0.55, 0.20, 0.10), float3(0.40, 0.28, 0.16), goods);   // reds, browns, ochres
item = lerp(item, float3(0.16, 0.24, 0.36), step(0.8, goods));                   // a few blues
item = lerp(item, float3(0.80, 0.70, 0.48), step(0.6, goods) * step(goods, 0.8)) * (0.45 + 0.4 * goods);
float3 shelves = row < 0.3 ? float3(0.10, 0.08, 0.07) : lerp(float3(0.06, 0.05, 0.05), item, gap);
float3 col;
if (t == ty) {                                   // back wall: full shelving
    col = lerp(wallc, shelves, shelfy * step(0.05, h.x) * step(h.x, 0.95));
} else if (t == tz) {
    if (dn.z < 0) {                              // floor: tiles, darker under the shelves
        float2 tl = frac(h.xy * float2(6.0, 5.0));
        col = float3(0.22, 0.20, 0.18) * (0.85 + 0.15 * step(0.05, min(tl.x, tl.y)));
    } else {                                     // ceiling: grey with strip lights across the room
        float strip = step(frac(h.y * 2.5 + 0.1), 0.12) * step(0.15, h.x) * step(h.x, 0.85);
        col = lerp(float3(0.30, 0.29, 0.27), float3(1.6, 1.5, 1.3), strip);
    }
} else {                                         // side walls: shelving over the whole depth (seen at an angle
    col = lerp(wallc, shelves, shelfy * step(0.04, h.y));   // from the pavement, they fill the window)
}
col *= 0.6 + 0.4 * saturate(1.0 - h.y * 0.7);    // light falls off into the room
col *= float3(1.15, 0.95, 0.72);                 // warm shop lighting (3000 K): cold rooms read as vending machines (PC V5)
return col * Glow;
"""


def custom_input(name):
    # FCustomInput's fields are EditAnywhere but not Blueprint-visible: its Python constructor
    # takes no argument (Mac slot 11/11: "call() takes at most 0 arguments"), the setter works.
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", name)
    return ci


def shop_interior_master():
    mat = new_material("M_City_ShopInterior")
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(mat)
    lp = g.node(unreal.MaterialExpressionTransformPosition,
                transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                transform_type=INSTANCE_POS)
    g.link(g.world_position(), "", lp, "")
    cv = g.node(unreal.MaterialExpressionTransform,
                transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                transform_type=INSTANCE_VEC)
    g.link(g.node(unreal.MaterialExpressionCameraVectorWS), "", cv, "")
    glow = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=1.4)
    seed = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Seed", default_value=0.0)
    custom = g.node(unreal.MaterialExpressionCustom, code=INTERIOR_HLSL,
                    output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="ShopInterior",
                    inputs=[custom_input(n) for n in ("P", "V", "Glow", "Seed")])
    for src, name in ((lp, "P"), (cv, "V"), (glow, "Glow"), (seed, "Seed")):
        g.link(src, "", custom, name)
    # grazing views read as glass: the room fades behind the reflection
    # At night a lit shop outshines the dark street it reflects: the fade and the glass specular drop
    # (PC V5 13/11: at an angle the windows read as the grey reflection of the street, not the room).
    night = city_lights(g)
    fade = g.const(0.8) if night is None else g.lerp(g.const(0.8), g.const(0.2), night)
    fres = g.node(unreal.MaterialExpressionFresnel)
    room = g.mul(custom, g.sub(g.const(1.0), g.mul(fres, fade)))
    emis = g.mul(room, g.node(unreal.MaterialExpressionScalarParameter, parameter_name="EmissiveScale",
                              default_value=1.0))
    emis = lit_by_hour(g, emis, SHOP_DAY_GLOW, SHOP_NIGHT_SCREEN)
    spec = g.const(1.0) if night is None else g.lerp(g.const(1.0), g.const(SHOP_NIGHT_SPECULAR), night)
    mel.connect_material_property(g.const((0.02, 0.022, 0.025)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(g.const(0.04), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.const(0.0), "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    weather(mat, ("wet",))   # no night EmissiveScale (x20): lit_by_hour sets the night level on screen
    g.done()
    return mat


# Daylight boost of the shop rooms seen through the windows. The glow (1.4) was set for the night
# exposure (EV100 about 3.5, times EmissiveScale); under the day exposure (EV100 14 to 14.5,
# 2000 times less sensitive) the rooms read black (PC stills, 12/11). By day the room is
# multiplied by DayGlow, at night by 1 (MPC CityLights, the night factor of the environment).
SHOP_DAY_GLOW = float(os.environ.get("GTP_SHOP_DAY_GLOW", "1000"))   # darker room content since 54df8ce: 600 read dark (PC V3 13/11); 1200 washed out the old pale room


# At night the rooms keep a fixed brightness on screen, whatever the exposure: the night exposure
# runs from EV100 about 7.5 at dusk to 3.5 at midnight, and a fixed emissive (1.4 nits) read black
# at dusk (V5, 17:38 in December, PC 13/11) and dim at midnight. EyeAdaptationInverse (no input:
# 1 / exposure) times NightScreen gives the room's value on screen; 1.0 is about a white wall in
# daylight. The day path is unchanged (DayGlow, exposure EV100 14).
SHOP_NIGHT_SCREEN = float(os.environ.get("GTP_SHOP_NIGHT_SCREEN", "0.2"))   # 0.7 and 0.35: white aplats at night (PC V5 13/11)
SHOP_NIGHT_SPECULAR = float(os.environ.get("GTP_SHOP_NIGHT_SPECULAR", "0.0"))   # 0.25 still reflected: at grazing angles Fresnel tends to 1 whatever F0; 0 switches the specular off (PC V5 13/11)
WINDOW_NIGHT_SCREEN = float(os.environ.get("GTP_WINDOW_NIGHT_SCREEN", "0.3"))   # 0.6: pale white windows, and lit windows seen between thin balcony bars flickered under TSR (PC V5 +18, 13/11)


def city_lights(g):
    path = os.path.join(REPO, "tools", "environment", "unreal")
    if not os.path.exists(os.path.join(path, "gtp_weather.py")):
        return None
    if path not in sys.path:
        sys.path.insert(0, path)
    import gtp_weather
    return g.node(unreal.MaterialExpressionCollectionParameter, collection=gtp_weather.collection(),
                  parameter_name="CityLights")


def lit_by_hour(g, emis, day_glow, night_screen):
    """Emissive of a lit room: emis x DayGlow by day, emis x NightScreen on screen at night."""
    night = city_lights(g)
    day = g.mul(emis, g.node(unreal.MaterialExpressionScalarParameter, parameter_name="DayGlow",
                             default_value=day_glow))
    if night is None:
        return day
    inv_cls = getattr(unreal, "MaterialExpressionEyeAdaptationInverse", None)
    k = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="NightScreen", default_value=night_screen)
    at_night = g.mul(emis, k)
    if inv_cls is not None:
        at_night = g.mul(at_night, g.node(inv_cls))
    return g.lerp(day, at_night, night)


# Upper-floor windows (MI_Glass_Window): opaque glossy glass over a fake flat by interior mapping,
# like the shops: a room per window (cell = one 3.3 m bay x one storey x 4 m deep, in the module's
# local frame), wallpaper and parquet varied per instance, sheer curtains or drapes, lit at night
# in about one window out of three. Reflections come from the glass itself (Lumen). PC default;
# GTP_WINDOW_GLASS=opaque keeps the dark v0 glass (M1 fallback).
WINDOW_GLASS = "MI_Glass_Window"
WINDOW_GLASS_MODE = os.environ.get("GTP_WINDOW_GLASS", "interior" if sys.platform == "win32" else "opaque")
WINDOW_DAY_GLOW = float(os.environ.get("GTP_WINDOW_DAY_GLOW", "150"))
WINDOW_HLSL = r"""
float3 d = normalize(-V);
float3 cell = float3(330.0, 400.0, 330.0);
float3 u = float3(frac((P.x + 165.0) / cell.x), 0.0, frac(P.z / cell.z));
float3 dn = float3(d.x / cell.x, abs(d.y) / cell.y, d.z / cell.z);
float tx = (dn.x > 0 ? 1.0 - u.x : -u.x) / (abs(dn.x) > 1e-5 ? dn.x : 1e-5);
float tz = (dn.z > 0 ? 1.0 - u.z : -u.z) / (abs(dn.z) > 1e-5 ? dn.z : 1e-5);
float ty = 1.0 / max(dn.y, 1e-5);
float t = min(min(tx, tz), ty);
float3 h = u + dn * t;
float r1 = frac(Seed * 7.31 + 0.17), r2 = frac(Seed * 13.7 + 0.53), r3 = frac(Seed * 29.1 + 0.71);
float3 paper = lerp(lerp(float3(0.78, 0.74, 0.66), float3(0.62, 0.68, 0.66), r1), float3(0.72, 0.6, 0.5), r2 * r2);
float3 col;
if (t == ty) {                                    // back wall: a door or a picture, a dado rail
    float door = step(0.55, r3) * step(abs(h.x - 0.3 - r1 * 0.4), 0.1) * step(h.z, 0.72);
    float pict = (1.0 - step(0.55, r3)) * step(abs(h.x - 0.5), 0.12) * step(abs(h.z - 0.55), 0.08);
    col = paper * (0.9 + 0.1 * step(0.3, h.z));
    col = lerp(col, float3(0.32, 0.2, 0.12), door);
    col = lerp(col, lerp(float3(0.5, 0.35, 0.2), float3(0.2, 0.3, 0.45), r2), pict);
} else if (t == tz) {
    float plank = frac(h.x * 22.0 + floor(h.y * 3.0) * 0.37);
    col = dn.z < 0 ? lerp(float3(0.26, 0.15, 0.08), float3(0.33, 0.2, 0.11), plank) : float3(0.9, 0.88, 0.84);
} else {
    col = paper * 0.85;
}
col *= 0.45 + 0.55 * saturate(1.0 - h.y * 0.7);   // daylight falls off into the room
// curtains, seen at the glass: none, sheer net, drapes at the sides, or both
float x = u.x;
float folds = 0.75 + 0.25 * sin(x * 260.0 + r1 * 6.0);
float sheer = step(0.35, r2) * 0.6;
float drape = step(0.5, r1) * (step(x, 0.39) + step(0.61, x));
float3 net = float3(0.92, 0.9, 0.86) * folds;
float3 cloth = lerp(float3(0.55, 0.12, 0.1), float3(0.75, 0.68, 0.55), r3) * folds;
col = lerp(col, net, sheer);
col = lerp(col, cloth, saturate(drape) * 0.95);
float lit = step(0.66, frac(Seed * 3.17 + 0.29));
float3 warm = float3(1.0, 0.82, 0.6) * lit * 1.2 + 0.015;
return col * lerp(float3(1.0, 1.0, 1.0), warm, Night);
"""


def window_interior_master():
    mat = new_material("M_City_WindowInterior")
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(mat)
    lp = g.node(unreal.MaterialExpressionTransformPosition,
                transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                transform_type=INSTANCE_POS)
    g.link(g.world_position(), "", lp, "")
    cv = g.node(unreal.MaterialExpressionTransform,
                transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                transform_type=INSTANCE_VEC)
    g.link(g.node(unreal.MaterialExpressionCameraVectorWS), "", cv, "")
    seed = g.node(unreal.MaterialExpressionPerInstanceRandom)
    night = city_lights(g) or g.const(0.0)
    custom = g.node(unreal.MaterialExpressionCustom, code=WINDOW_HLSL,
                    output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="WindowInterior",
                    inputs=[custom_input(n) for n in ("P", "V", "Seed", "Night")])
    for src, name in ((lp, "P"), (cv, "V"), (seed, "Seed"), (night, "Night")):
        g.link(src, "", custom, name)
    fres = g.node(unreal.MaterialExpressionFresnel)
    emis = g.mul(custom, g.sub(g.const(1.0), g.mul(fres, g.const(0.85))))
    emis = lit_by_hour(g, emis, WINDOW_DAY_GLOW, WINDOW_NIGHT_SCREEN)
    mel.connect_material_property(g.const((0.015, 0.017, 0.02)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(g.const(0.03), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.const(0.0), "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    weather(mat, ("wet",))   # no night EmissiveScale (x20): lit_by_hour sets the night level on screen
    g.done()
    return mat


def interior_or_none(report):
    """M_City_ShopInterior, or None (opaque glass) when it cannot be built or does not compile
    for this editor's shader platform (Metal on the Mac): the regeneration goes on either way."""
    try:
        mat = shop_interior_master()
    except Exception as e:
        report["shop_glass_fallback"] = "interior material not built: %r" % (e,)
        return None
    # Without a GPU (-nullrhi, run_city_import.sh) the statistics are empty: trust the build then.
    # On Windows (DX12 SM6) the statistics read 0 instructions while the shader is still compiling
    # (cand20b: "does not compile", although the window variant of the same code draws): trust the
    # build there too; the check stays for the Mac, where it caught a real Metal failure.
    stats = None if (os.environ.get("GTP_NULLRHI") == "1" or sys.platform == "win32") else \
        getattr(mel, "get_statistics", None)
    if stats is not None:
        try:
            n = stats(mat).get_editor_property("num_pixel_shader_instructions")
            report["shop_interior_ps_instructions"] = n
            if n <= 0:   # a material that failed to compile has no pixel shader
                report["shop_glass_fallback"] = "interior material does not compile on this platform"
                return None
        except Exception as e:
            report["shop_interior_stats"] = repr(e)
    return mat


def kit_instances(master, report=None):
    out = {}
    window = None
    if WINDOW_GLASS_MODE == "interior":
        try:
            window = window_interior_master()
        except Exception as e:  # noqa: BLE001  keep the dark v0 glass
            if report is not None:
                report["window_glass_error"] = repr(e)
    if report is not None:
        report["window_glass"] = "interior" if window else "opaque"
    glass = shop_glass_master() if SHOP_GLASS_MODE == "translucent" else \
        interior_or_none(report if report is not None else {}) if SHOP_GLASS_MODE == "interior" else None
    for name, (r, gr, b, rough, metal) in KIT_SLOTS.items():
        path = MAT_DIR + "/" + name.replace("MI_", "MI_City_", 1)
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        mic = asset_tools.create_asset(path.rsplit("/", 1)[1], MAT_DIR, unreal.MaterialInstanceConstant,
                                       unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mic, glass if (name == SHOP_GLASS and glass) else
                                         window if (name == WINDOW_GLASS and window) else master)
        if name == WINDOW_GLASS and window:
            eal.save_loaded_asset(mic)
            out[name] = mic
            continue
        mel.set_material_instance_vector_parameter_value(mic, "BaseColor", unreal.LinearColor(r, gr, b, 1.0))
        if name == SHOP_GLASS and glass is None:
            er, eg, eb, k = SHOP_GLASS_FAKE_GLOW
            mel.set_material_instance_vector_parameter_value(mic, "Emissive", unreal.LinearColor(er * k, eg * k, eb * k, 1.0))
        if name == SHOP_GLASS and glass:
            if SHOP_GLASS_MODE == "translucent":
                mel.set_material_instance_scalar_parameter_value(mic, "Roughness", rough)
            eal.save_loaded_asset(mic)
            out[name] = mic
            continue
        mel.set_material_instance_scalar_parameter_value(mic, "Roughness", rough)
        mel.set_material_instance_scalar_parameter_value(mic, "Metallic", metal)
        surfaces().kit_params(mic, name)
        mel.set_material_instance_scalar_parameter_value(mic, "UseBuildingColour",
                                                         1.0 if name in KIT_BUILDING_COLOUR else 0.0)
        mel.set_material_instance_scalar_parameter_value(mic, "Weathering", KIT_WEATHERING.get(name, 1.0))
        if name in KIT_EMISSIVE:
            er, eg, eb, k = KIT_EMISSIVE[name]
            mel.set_material_instance_vector_parameter_value(mic, "Emissive", unreal.LinearColor(er * k, eg * k, eb * k, 1.0))
        eal.save_loaded_asset(mic)
        out[name] = mic
    return out


def slot_key(slot_name):
    """Interchange may suffix slot names (_1, _Mat); match on the longest known prefix."""
    s = str(slot_name)
    best = None
    for k in KIT_SLOTS:
        if s.startswith(k) and (best is None or len(k) > len(best)):
            best = k
    return best


def assign_kit(mics, report):
    meshes, slots, unknown = 0, 0, set()
    for p in eal.list_assets(KIT_DIR, recursive=True):
        a = eal.load_asset(p)
        if not isinstance(a, unreal.StaticMesh) or a.get_name() == "SM_Sign_Panel":
            continue   # sign panels keep their per-shop MI set on the component
        for i, sm in enumerate(a.get_editor_property("static_materials")):
            key = slot_key(sm.get_editor_property("material_slot_name"))
            if key is None:
                mi = sm.get_editor_property("material_interface")
                key = slot_key(mi.get_name()) if mi else None
            if key is None:
                unknown.add(str(sm.get_editor_property("material_slot_name")))
                key = "MI_Facade_LimestoneCream"
            a.set_material(i, mics[key])
            slots += 1
        eal.save_loaded_asset(a)
        meshes += 1
    report["kit_meshes"], report["kit_slots"] = meshes, slots
    report["kit_unknown_slots"] = sorted(unknown)[:20]


def water_material():
    """Dark, glossy river surface with small moving waves: a world-space normal from three
    sines drifting downstream (the Rhone flows south, +Y in Unreal). Opaque and texture
    free: a few ALU, no extra pass, unlike the Water plugin."""
    mat = new_material("M_City_Water")
    mat.set_editor_property("tangent_space_normal", False)
    g = Graph(mat)
    wp = g.world_position()
    x, y = g.mask(wp, r=True), g.mask(wp, g=True)
    t = g.node(unreal.MaterialExpressionTime)
    sin, cos = unreal.MaterialExpressionSine, unreal.MaterialExpressionCosine
    # (wavelength cm, direction x, direction y, speed cm/s, slope)
    dx, dy = g.const(0.0), g.const(0.0)
    for lam, ux, uy, speed, slope in ((340.0, 0.2, 1.0, 60.0, 0.05), (130.0, -0.6, 0.8, 35.0, 0.035),
                                      (55.0, 0.9, 0.45, 20.0, 0.02)):
        k = 6.2832 / lam
        ph = g.mul(g.sub(g.add(g.mul(x, g.const(ux)), g.mul(y, g.const(uy))), g.mul(t, g.const(speed))), g.const(k))
        c = g.mul(g.unary(cos, ph), g.const(slope))
        dx = g.add(dx, g.mul(c, g.const(ux)))
        dy = g.add(dy, g.mul(c, g.const(uy)))
    n = g.node(unreal.MaterialExpressionAppendVector)
    g.link(g.sub(g.const(0.0), dx), "", n, "A")
    g.link(g.sub(g.const(0.0), dy), "", n, "B")
    n3 = g.node(unreal.MaterialExpressionAppendVector)
    g.link(n, "", n3, "A")
    g.link(g.const(1.0), "", n3, "B")
    mel.connect_material_property(g.unary(unreal.MaterialExpressionNormalize, n3), "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(g.const((0.012, 0.022, 0.026)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(g.const(0.06), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.const(0.6), "", unreal.MaterialProperty.MP_SPECULAR)
    g.done()
    return mat


def assign(folder, mat, by_name=None):
    n = 0
    for p in eal.list_assets(folder, recursive=True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            m = (by_name or {}).get(a.get_name(), mat)
            for i in range(len(a.get_editor_property("static_materials"))):
                a.set_material(i, m)
            eal.save_loaded_asset(a)
            n += 1
    return n


def facade_check(report):
    """Facade instances and kit materials present in the map (diagnostic only)."""
    kit = [p for p in eal.list_assets(KIT_DIR, recursive=True)] if eal.does_directory_exist(KIT_DIR) else []
    report["kit_assets"] = len(kit)
    report["kit_materials"] = sum(1 for p in kit if "/MI_" in p or "/M_" in p)
    level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not level_sub.load_level(MAP):
        report["errors"].append("cannot open " + MAP)
        return
    inst, actors = 0, 0
    for a in actor_sub.get_all_level_actors():
        if str(a.get_folder_path()).startswith("City/Facades"):
            actors += 1
            for c in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
                inst += c.get_instance_count()
    report["facade_actors"], report["facade_instances"] = actors, inst
    lock_exposure(report, actor_sub, level_sub)


def lock_exposure(report, actor_sub, level_sub):
    """Fix the auto exposure of the map to the art bible's daylight EV100.

    With the default range the eye adaptation brightened the dark ground until the
    asphalt read mid grey and the paving almost white. min = max = EV100 keeps the
    albedos readable. GTP_CITY_EV100 (default 14, sunny midday) or 'off'."""
    ev = os.environ.get("GTP_CITY_EV100", "14")
    if ev == "off":
        return
    ev = float(ev)
    done = 0
    for a in actor_sub.get_all_level_actors():
        if not isinstance(a, unreal.PostProcessVolume):
            continue
        st = a.get_editor_property("settings")
        for k, v in (("auto_exposure_min_brightness", ev), ("auto_exposure_max_brightness", ev)):
            st.set_editor_property("override_" + k, True)
            st.set_editor_property(k, v)
        a.set_editor_property("settings", st)
        done += 1
    if done and os.environ.get("GTP_PROTECT_REF_MAP") == "1":
        # Reference map guard (PC 12/11): materials may change, the map file may not.
        report["map_save_refused"] = "GTP_PROTECT_REF_MAP=1: exposure set in memory only, " + MAP + " not saved"
        done = 0
    if done:
        level_sub.save_current_level()
    report["exposure_locked_ev100"] = ev if done else None
    report["post_process_volumes"] = done


def cell_level(report):
    """Streaming cell (GTP_CITY_LEVEL=cell): the terrain, the kit and their materials are shared
    with the persistent level and already set; only the cell's own shells get the shell
    material, reused as it is so the levels built before keep a valid reference."""
    shell_path = MAT_DIR + "/M_City_Shell"
    shell = ensure_usage(eal.load_asset(shell_path)) if eal.does_asset_exist(shell_path) else shell_material()
    report["shell_meshes"] = assign(ROOT + "/Buildings/" + CAMEL, shell)
    if eal.does_directory_exist(ROOT + "/Landmarks/" + CAMEL):
        report["landmark_meshes"] = assign(ROOT + "/Landmarks/" + CAMEL, shell)
    report["level"] = "cell"


def main():
    t0 = time.time()
    report = {"map": MAP, "ground_dir": GROUND_DIR, "errors": []}
    try:
        if os.environ.get("GTP_CITY_LEVEL", "street") == "cell":
            cell_level(report)
            raise StopIteration
        if os.environ.get("GTP_CITY_LEVEL", "street") == "quarter":
            # Persistent level: proxies and terrain carry their materials already; only the
            # exposure of its post-process volume is locked like the street map's.
            level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
            if not level_sub.load_level(MAP):
                raise RuntimeError("cannot open " + MAP)
            lock_exposure(report, unreal.get_editor_subsystem(unreal.EditorActorSubsystem), level_sub)
            report["level"] = "quarter"
            raise StopIteration
        with open(os.path.join(GROUND_DIR, "ground.json")) as f:
            meta = json.load(f)
        if not eal.does_directory_exist(MAT_DIR):
            eal.make_directory(MAT_DIR)
        tex = import_mask()
        if tex is None:
            raise RuntimeError("T_GroundMask did not import into " + MAT_DIR)
        report["mask_texture"] = {"path": tex.get_path_name(), "size": [tex.blueprint_get_size_x(), tex.blueprint_get_size_y()],
                                  "srgb": tex.get_editor_property("srgb")}
        ground = ground_material(tex, meta["extent_ue_cm"])
        shell = shell_material()
        report["terrain_meshes"] = assign(ROOT + "/Terrain", ground)
        report["shell_meshes"] = assign(ROOT + "/Buildings/" + CAMEL, shell)
        if eal.does_directory_exist(ROOT + "/Landmarks/" + CAMEL):
            report["landmark_meshes"] = assign(ROOT + "/Landmarks/" + CAMEL, shell)
        river_dir = ROOT + "/Riverside/" + CAMEL
        if eal.does_directory_exist(river_dir):
            report["riverside_meshes"] = assign(river_dir, shell, {"SM_River_Water": water_material()})
        assign_kit(kit_instances(kit_master(), report), report)
        report["shop_glass"] = SHOP_GLASS_MODE + (" -> opaque fallback" if "shop_glass_fallback" in report else "")
        report["shop_night"] = {"screen": SHOP_NIGHT_SCREEN, "specular": SHOP_NIGHT_SPECULAR, "day_glow": SHOP_DAY_GLOW}
        report["kit_roughness"] = {k: KIT_SLOTS[k][3] for k in ("MI_Ironwork_Painted", "MI_Persienne")}
        report["surfaces"] = surfaces().report()
        facade_check(report)
    except StopIteration:
        pass
    except Exception as e:  # report, never leave the editor hanging on a dialog
        report["errors"].append(repr(e))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "materials_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    log("report: " + json.dumps(report))


main()
