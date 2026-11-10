"""Replace the greybox material of the generated city with art-bible ground and walls.

Run after import_city.py (and place_facades.py), same way, no UI:
  UnrealEditor GranTheftPraline.uproject -unattended -nosplash -NSAppSleepDisabled YES \
      -ExecutePythonScript=/tmp/apply_city_materials.py
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
    "MI_Ironwork_Painted": (0.025, 0.025, 0.028, 0.55, 0.0),
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
    "MI_Glass_Shop": (0.05, 0.06, 0.065, 0.03, 0.0),   # opaque here: reflective dark glass
}
KIT_EMISSIVE = {"MI_Shop_Interior": (1.0, 0.92, 0.8, 1.2)}
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


def new_material(name):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return asset_tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())


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
    col = g.lerp(col, g.const(ASPHALT), s, "R")
    col = g.lerp(col, g.const(MARKING), mark)
    col = g.mul(col, tone)
    rough = g.lerp(g.lerp(g.const(0.78), g.const(0.92), s, "R"), g.const(0.6), mark)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
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
    rough = g.lerp(g.const(0.55), g.const(0.88), wall)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    g.done()
    return mat


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
    rough = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Roughness", default_value=0.8)
    metal = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Metallic", default_value=0.0)
    emis = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="Emissive",
                  default_value=unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(g.mask(emis, r=True, g=True, b=True), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    g.done()
    return mat


def kit_instances(master):
    out = {}
    for name, (r, gr, b, rough, metal) in KIT_SLOTS.items():
        path = MAT_DIR + "/" + name.replace("MI_", "MI_City_", 1)
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        mic = asset_tools.create_asset(path.rsplit("/", 1)[1], MAT_DIR, unreal.MaterialInstanceConstant,
                                       unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mic, master)
        mel.set_material_instance_vector_parameter_value(mic, "BaseColor", unreal.LinearColor(r, gr, b, 1.0))
        mel.set_material_instance_scalar_parameter_value(mic, "Roughness", rough)
        mel.set_material_instance_scalar_parameter_value(mic, "Metallic", metal)
        mel.set_material_instance_scalar_parameter_value(mic, "UseBuildingColour",
                                                         1.0 if name in KIT_BUILDING_COLOUR else 0.0)
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


def assign(folder, mat):
    n = 0
    for p in eal.list_assets(folder, recursive=True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            for i in range(len(a.get_editor_property("static_materials"))):
                a.set_material(i, mat)
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
    if done:
        level_sub.save_current_level()
    report["exposure_locked_ev100"] = ev if done else None
    report["post_process_volumes"] = done


def main():
    t0 = time.time()
    report = {"map": MAP, "ground_dir": GROUND_DIR, "errors": []}
    try:
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
        assign_kit(kit_instances(kit_master()), report)
        facade_check(report)
    except Exception as e:  # report, never leave the editor hanging on a dialog
        report["errors"].append(repr(e))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "materials_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    log("report: " + json.dumps(report))


main()
