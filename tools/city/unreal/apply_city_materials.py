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
     asphalt carriageway, kerb stones, granite paving on streets closed to cars,
     limestone slabs on sidewalks, dashed centre lines; joints and a slow tone
     variation are procedural (no texture fetch beyond the mask)
  3. M_City_Shell: the building colour the generator wrote in the vertex colours,
     stone courses on the walls, rougher walls than roofs
  4. assign them to every terrain tile and building shell mesh, save the meshes
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
SIDEWALK = (0.34, 0.32, 0.29)      # pale limestone slabs
PAVING = (0.27, 0.26, 0.25)        # grey granite setts and slabs
KERB = (0.22, 0.21, 0.20)          # granite kerb
ASPHALT = (0.045, 0.045, 0.05)
MARKING = (0.62, 0.62, 0.60)

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
    wp = g.node(unreal.MaterialExpressionWorldPosition)
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
    col = g.lerp(col, g.const(KERB), s, "A")
    col = g.lerp(col, g.const(ASPHALT), s, "R")
    col = g.lerp(col, g.const(MARKING), s, "B")
    col = g.mul(col, tone)
    rough = g.lerp(g.lerp(g.const(0.78), g.const(0.92), s, "R"), g.const(0.6), s, "B")
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
    wp = g.node(unreal.MaterialExpressionWorldPosition)
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


def main():
    t0 = time.time()
    report = {"map": MAP, "ground_dir": GROUND_DIR, "errors": []}
    try:
        with open(os.path.join(GROUND_DIR, "ground.json")) as f:
            meta = json.load(f)
        if not eal.does_directory_exist(MAT_DIR):
            eal.make_directory(MAT_DIR)
        tex = import_mask()
        ground = ground_material(tex, meta["extent_ue_cm"])
        shell = shell_material()
        report["terrain_meshes"] = assign(ROOT + "/Terrain", ground)
        report["shell_meshes"] = assign(ROOT + "/Buildings/" + CAMEL, shell)
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
