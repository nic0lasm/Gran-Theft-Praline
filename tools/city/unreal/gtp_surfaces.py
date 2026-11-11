"""Photographed surfaces (Megascans) layered into the city materials (Unreal Editor Python).

apply_city_materials.py builds the city materials procedurally (palette colours, joints,
weathering, no texture). When tools/city/unreal/import_surfaces.py has resolved the scanned
surfaces of tools/city/surfaces.json, it writes Saved/CityImport/surfaces.json, and these hooks,
called by the material generators before gtp_weather, put the textures in:

  ground(mat, mask)    M_City_Ground: one layer per ground mask channel (sidewalk, pedestrian
                       paving G, carriageway R, kerb B off the road); road markings and any
                       layer without a surface keep the procedural colour
  shell(mat)           M_City_Shell: roofs (zinc, or terracotta tiles where the vertex colour
                       is red), stone detail on the walls
  kit(mat)             M_City_Kit: stone or render detail on the slots that take the building
                       colour (UseBuildingColour), the building hue kept
  kit_params(mic, name) per slot: SurfaceRender 1 on the rendered facade slot

Without the manifest, or with GTP_SURFACES=0, every hook does nothing: the procedural look
stays as it is, so the generators run the same on a project without the scans.

M1 budget (no Nanite there, every fetch is paid per pixel): textures capped at 2K by the import,
shared wrap samplers, one BaseColor + Normal + roughness map per layer: ground mask + 4 layers =
13 fetches, kit 4 (one is the 1x1 mip used to keep the building hue), shell 6. GTP_SURFACES_LITE=1
keeps only the colour maps (ground 5, kit 2, shell 2) if the perf pass asks for it.
"""

import json
import os

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

ENABLED = os.environ.get("GTP_SURFACES", "1") != "0"
LITE = os.environ.get("GTP_SURFACES_LITE", "0") == "1"
MANIFEST = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                        "CityImport", "surfaces.json")

# Which catalogue surface feeds which layer, best first (ids of tools/city/surfaces.json).
GROUND_LAYERS = {
    "sidewalk": ("limestone_slab", "concrete_sidewalk", "granite_slab_light"),
    "paving": ("granite_slab_light", "granite_setts"),
    "road": ("asphalt_dry",),
    "kerb": ("granite_kerb",),
}
ROOF = ("zinc_roof", "slate_roof")
ROOF_TILE = ("terracotta_tile",)
STONE = ("limestone_ashlar",)
RENDER = ("render_ochre",)

_used = {}


def _manifest():
    if not ENABLED or not os.path.isfile(MANIFEST):
        return {}
    with open(MANIFEST, encoding="utf-8") as f:
        return json.load(f).get("surfaces", {})


def _pick(manifest, ids):
    for i in ids:
        if i in manifest:
            return i, manifest[i]
    return None, None


def report():
    """What the hooks used, for materials_report.json."""
    return {"enabled": ENABLED and os.path.isfile(MANIFEST), "lite": LITE, "used": dict(_used)}


class _G:
    def __init__(self, mat, x=-3000):
        self.mat, self.x, self.y = mat, x, 0

    def node(self, cls, **props):
        e = mel.create_material_expression(self.mat, cls, self.x, self.y)
        self.y += 130
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, a_out, b, b_in):
        for name in ((b_in,) if b_in else ("", "Input", "X")):
            if mel.connect_material_expressions(a, a_out, b, name):
                return
        raise RuntimeError("gtp_surfaces: cannot connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))

    def const(self, v):
        if isinstance(v, (int, float)):
            return self.node(unreal.MaterialExpressionConstant, r=float(v))
        if len(v) == 2:
            return self.node(unreal.MaterialExpressionConstant2Vector, r=float(v[0]), g=float(v[1]))
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(*v, 1.0))

    def op(self, cls, a, b, a_out="", b_out=""):
        e = self.node(cls)
        self.link(a, a_out, e, "A")
        self.link(b, b_out, e, "B")
        return e

    def mul(self, a, b, a_out="", b_out=""):
        return self.op(unreal.MaterialExpressionMultiply, a, b, a_out, b_out)

    def div(self, a, b, a_out="", b_out=""):
        return self.op(unreal.MaterialExpressionDivide, a, b, a_out, b_out)

    def sub(self, a, b, a_out="", b_out=""):
        return self.op(unreal.MaterialExpressionSubtract, a, b, a_out, b_out)

    def lerp(self, a, b, alpha, a_out="", b_out="", alpha_out=""):
        e = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, a_out, e, "A")
        self.link(b, b_out, e, "B")
        self.link(alpha, alpha_out, e, "Alpha")
        return e

    def unary(self, cls, src, src_out="", **props):
        # "src", not "a": ComponentMask has a property named "a" (alpha channel).
        e = self.node(cls, **props)
        self.link(src, src_out, e, "")
        return e

    def mask(self, a, r=False, g=False, b=False):
        return self.unary(unreal.MaterialExpressionComponentMask, a, r=r, g=g, b=b, a=False)

    def world_xy(self):
        wp = self.node(unreal.MaterialExpressionWorldPosition)
        cls = getattr(unreal, "MaterialExpressionTruncateLWC", None)
        if cls:
            wp = self.unary(cls, wp)
        return self.mask(wp, r=True, g=True)

    def sample(self, path, uv, kind, mip=None):
        """kind: color | normal | masks. Shared wrap sampler: no 16-sampler limit, one state."""
        tex = eal.load_asset(path)
        if tex is None:
            raise RuntimeError("gtp_surfaces: texture missing " + path)
        stype = {"color": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                 "normal": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                 "masks": unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind]
        e = self.node(unreal.MaterialExpressionTextureSample, texture=tex, sampler_type=stype)
        try:
            e.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        except Exception:  # noqa: BLE001  older property name: keep the texture's own sampler
            pass
        if mip is not None:
            e.set_editor_property("mip_value_mode", unreal.TextureMipValueMode.TMVM_MIP_LEVEL)
            e.set_editor_property("const_mip_value", int(mip))
        self.link(uv, "", e, "UVs")
        return e

    def layer(self, surf, uv):
        """(colour rgb, normal or None, roughness or None) of one manifest surface."""
        col = self.mask(self.sample(surf["basecolor"], uv, "color"), r=True, g=True, b=True)
        if LITE:
            return col, None, None
        nrm = self.sample(surf["normal"], uv, "normal") if surf.get("normal") else None
        rough = None
        if surf.get("rough"):
            ch = surf.get("rough_channel", "G").upper()
            rough = self.mask(self.sample(surf["rough"], uv, "masks"), r=ch == "R", g=ch == "G", b=ch == "B")
        return col, nrm, rough


def _input(mat, prop):
    node = mel.get_material_property_input_node(mat, prop)
    if node is None:
        return None, ""
    return node, mel.get_material_property_input_node_output_name(mat, prop) or ""


def _flat(g):
    return g.const((0.0, 0.0, 1.0))


def ground(mat, mask):
    """mask: the T_GroundMask sample node of M_City_Ground (R road, G paving, B kerb or marking)."""
    m = _manifest()
    layers = {k: _pick(m, ids) for k, ids in GROUND_LAYERS.items()}
    if not any(s for _, s in layers.values()):
        return False
    g = _G(mat)
    xy = g.world_xy()
    col0, col0_out = _input(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    r0, r0_out = _input(mat, unreal.MaterialProperty.MP_ROUGHNESS)
    tex, by_id = {}, {}
    for k, (sid, s) in layers.items():
        if s:
            if sid not in by_id:          # one surface on two layers: sampled once
                uv = g.mul(xy, g.const(1.0 / float(s.get("tile_cm", 200.0))))
                by_id[sid] = g.layer(s, uv)
            tex[k] = by_id[sid]
            _used["ground." + k] = sid
    if not LITE:
        # The terrain is flat and its tangents are not the world X/Y the textures follow:
        # the normal is given in world space (Z up), which is what these maps are on the ground.
        mat.set_editor_property("tangent_space_normal", False)
    on_road = g.mask(mask, r=True)
    kerb = g.mul(g.mask(mask, b=True), g.sub(g.const(1.0), on_road))
    mark = g.mul(g.mask(mask, b=True), on_road)

    def pick(k, i, fallback, fallback_out=""):
        t = tex.get(k)
        return (t[i], "") if t and t[i] is not None else (fallback, fallback_out)

    def blend(i, fallback, fallback_out=""):
        v, vo = pick("sidewalk", i, fallback, fallback_out)
        for k, alpha, alpha_out in (("paving", mask, "G"), ("kerb", kerb, ""), ("road", mask, "R")):
            b, bo = pick(k, i, fallback, fallback_out)
            v, vo = g.lerp(v, b, alpha, vo, bo, alpha_out), ""
        return v, vo

    col, _ = blend(0, col0, col0_out)
    col = g.lerp(col, col0, mark, b_out=col0_out)         # markings stay procedural
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if not LITE:
        nrm, _ = blend(1, _flat(g))
        nrm = g.lerp(nrm, _flat(g), mark)
        mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
        if r0 is not None:
            rough, _ = blend(2, r0, r0_out)
            mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    eal.set_metadata_tag(mat, "GTPSurfaces", "1")
    return True


def _detail(g, s, uv):
    """Texture colour divided by its own average (1x1 mip): a detail around 1 that keeps the
    generator's colour, so each building keeps its hue."""
    col = g.mask(g.sample(s["basecolor"], uv, "color"), r=True, g=True, b=True)
    avg = g.mask(g.sample(s["basecolor"], uv, "color", mip=12), r=True, g=True, b=True)
    return g.div(col, g.op(unreal.MaterialExpressionMax, avg, g.const(0.02)))


def kit(mat):
    m = _manifest()
    sid, stone = _pick(m, STONE)
    rid, render = _pick(m, RENDER)
    if not (stone or render):
        return False
    g = _G(mat)
    use = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="UseBuildingColour", default_value=0.0)
    is_render = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="SurfaceRender", default_value=0.0)
    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate)     # kit UVs: 1 unit = 1 m (kit.py)
    picks = []
    for s, sid_ in ((stone, sid), (render, rid)):
        if s:
            uv = g.mul(uv0, g.const(100.0 / float(s.get("tile_cm", 200.0))))
            picks.append((s, uv))
            _used["kit." + ("stone" if s is stone else "render")] = sid_
    if len(picks) == 2:
        det = g.lerp(_detail(g, *picks[0]), _detail(g, *picks[1]), is_render)
    else:
        det = _detail(g, *picks[0])
    det = g.lerp(g.const((1.0, 1.0, 1.0)), det, use)
    col0, col0_out = _input(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(g.mul(col0, det, col0_out), "", unreal.MaterialProperty.MP_BASE_COLOR)
    if not LITE:
        s, uv = picks[0] if not render or not stone else (None, None)
        nrm = None
        if stone and render and stone.get("normal") and render.get("normal"):
            nrm = g.lerp(g.sample(stone["normal"], picks[0][1], "normal"),
                         g.sample(render["normal"], picks[1][1], "normal"), is_render)
        elif s and s.get("normal"):
            nrm = g.sample(s["normal"], uv, "normal")
        if nrm is not None:
            mel.connect_material_property(g.lerp(_flat(g), nrm, use), "", unreal.MaterialProperty.MP_NORMAL)
    eal.set_metadata_tag(mat, "GTPSurfaces", "1")
    return True


def kit_params(mic, name):
    if name == "MI_Facade_Render" and ENABLED:
        mel.set_material_instance_scalar_parameter_value(mic, "SurfaceRender", 1.0)


def shell(mat):
    """Roofs: zinc or slate, terracotta where the generator's vertex colour is red (Lyon's
    canal tiles); walls: stone detail. World projection (the shells have no tiling UVs)."""
    m = _manifest()
    rid, roof = _pick(m, ROOF)
    tid, tile = _pick(m, ROOF_TILE)
    sid, stone = _pick(m, STONE)
    if not (roof or tile or stone):
        return False
    g = _G(mat)
    vc = g.node(unreal.MaterialExpressionVertexColor)
    nz = g.unary(unreal.MaterialExpressionAbs, g.mask(g.node(unreal.MaterialExpressionVertexNormalWS), b=True))
    roof_a = g.unary(unreal.MaterialExpressionSaturate, g.mul(g.sub(nz, g.const(0.55)), g.const(4.0)))
    xy = g.world_xy()
    col0, col0_out = _input(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    col = (col0, col0_out)
    roof_col = None
    if roof:
        roof_col = g.layer(roof, g.mul(xy, g.const(1.0 / float(roof.get("tile_cm", 200.0)))))[0]
        _used["shell.roof"] = rid
    if tile:
        tcol = g.layer(tile, g.mul(xy, g.const(1.0 / float(tile.get("tile_cm", 150.0)))))[0]
        red = g.unary(unreal.MaterialExpressionSaturate,
                      g.mul(g.sub(g.mask(vc, r=True), g.mask(vc, b=True)), g.const(4.0)))
        roof_col = g.lerp(roof_col, tcol, red) if roof_col is not None else tcol
        _used["shell.tile"] = tid
    if roof_col is not None:
        col = (g.lerp(col0, roof_col, roof_a, col0_out), "")
    if stone:
        # Walls: a stone detail projected on X/Z and Y/Z, the dominant one by the normal.
        wp = g.node(unreal.MaterialExpressionWorldPosition)
        cls = getattr(unreal, "MaterialExpressionTruncateLWC", None)
        if cls:
            wp = g.unary(cls, wp)
        k = g.const(1.0 / float(stone.get("tile_cm", 200.0)))
        xz = g.mul(g.op(unreal.MaterialExpressionAppendVector, g.mask(wp, r=True), g.mask(wp, b=True)), k)
        yz = g.mul(g.op(unreal.MaterialExpressionAppendVector, g.mask(wp, g=True), g.mask(wp, b=True)), k)
        n = g.node(unreal.MaterialExpressionVertexNormalWS)
        nx = g.unary(unreal.MaterialExpressionAbs, g.mask(n, r=True))
        ny = g.unary(unreal.MaterialExpressionAbs, g.mask(n, g=True))
        side = g.unary(unreal.MaterialExpressionSaturate, g.mul(g.sub(nx, ny), g.const(8.0)))
        det = g.lerp(_detail(g, stone, xz), _detail(g, stone, yz), side)
        det = g.lerp(det, g.const((1.0, 1.0, 1.0)), roof_a)
        col = (g.mul(col[0], det, col[1]), "")
        _used["shell.stone"] = sid
    mel.connect_material_property(col[0], col[1], unreal.MaterialProperty.MP_BASE_COLOR)
    eal.set_metadata_tag(mat, "GTPSurfaces", "1")
    return True
