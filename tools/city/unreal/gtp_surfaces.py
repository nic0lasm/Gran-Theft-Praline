"""Photographed surfaces (Megascans) layered into the city materials (Unreal Editor Python).

apply_city_materials.py builds the city materials procedurally (palette colours, joints,
weathering, no texture). When tools/city/unreal/import_surfaces.py has resolved the scanned
surfaces of tools/city/surfaces.json, it writes Saved/CityImport/surfaces.json, and these hooks,
called by the material generators before gtp_weather, put the textures in:

  ground(mat, mask)    M_City_Ground (with mud and oil stains as texture layers): one layer per mask channel (sidewalk, pedestrian
                       paving G, carriageway R, kerb B off the road), then a large grime
                       texture over all of it (decal_dirt); road markings and any layer
                       without a surface keep the procedural colour
  shell(mat)           M_City_Shell and M_City_Backdrop: roofs (zinc, or terracotta tiles where
                       the vertex colour is red); on the walls stone detail, or render detail
                       where the building colour is a coloured render (facade_tone.render_share)
  kit(mat)             M_City_Kit: stone or render detail on the slots that take the building
                       colour (UseBuildingColour), the building hue kept
  kit_params(mic, name) per slot: SurfaceRender 1 on the rendered facade slot

Without the manifest, or with GTP_SURFACES=0, every hook does nothing: the procedural look
stays as it is, so the generators run the same on a project without the scans.

M1 budget (no Nanite there, every fetch is paid per pixel): textures capped at 2K by the import,
shared wrap samplers, one BaseColor + Normal + roughness map per layer: ground mask + 4 layers =
up to 21 fetches with the stains (mud, oil, cracks, gum: colour + opacity each), plus the 1x1 mip of each
ground surface for its albedo cap, kit 4 (one is the 1x1 mip used to keep the building hue), shell 6
(10 with the render detail). GTP_SURFACES_LITE=1
keeps only the colour maps (ground 5, kit 2, shell 2) if the perf pass asks for it.
"""

import json
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import facade_tone  # noqa: E402  pure Python, shared with the tests

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
GROUND_GRIME = ("decal_dirt",)
# Dry street ground, whatever the source (gate 68): ambientCG's granites are polished worktops,
# roughness 0.02-0.03 and albedo 0.35-0.46 (measured on Granite002A/003A), so the pavement was a
# mirror of the sky, white at grazing angles and burnt in the sun. Each layer's colour is scaled
# down to at most its cap of mean linear albedo (the texture's 1x1 mip, one cheap fetch per
# surface), and the roughness never goes below the floor (wet weather is added after this, by
# gtp_weather, and still lowers it when it rains). GTP_GROUND_ROUGH_MIN=0 turns the floor off.
# Gate 75: at 0.28 the sunlit quai still read white next to 0.05 asphalt (measured on the video:
# 4.6 times the road's brightness, 190/255): 0.20, weathered concrete or grey granite.
GROUND_ALBEDO_CAP = {"sidewalk": 0.20, "paving": 0.20, "kerb": 0.22, "road": 0.10}
GROUND_ROUGH_MIN = float(os.environ.get("GTP_GROUND_ROUGH_MIN", "0.6"))
# Gate 86, long take, 13 s and 120 s: driving towards the sun the carriageway of the quay is a
# pale sheet, nearly white, its grain gone, while the same asphalt is dark seen across the sun.
# Dry asphalt is among the roughest grounds of a street: its floor is higher than the stone's.
ROAD_ROUGH_MIN = float(os.environ.get("GTP_ROAD_ROUGH_MIN", "0.85"))
LUMA = (0.2126, 0.7152, 0.0722)
# The stains are laid after the layer caps: a light mud scan made blotches of several metres on
# the asphalt, up to about 0.2 of albedo against 0.05 (video of gate 69, 47 s, "camouflage").
# Each stain's colour is capped too, dark like wet dirt, and the mud is laid at a lower strength.
STAIN_ALBEDO_CAP = 0.08
MUD_STRENGTH = 0.35
GRIME_SCALE = 3.0        # the grime repeats every 3 tiles of its own size (about 6 m)
GRIME_STRENGTH = 0.5
ROOF_TONES = os.environ.get("GTP_ROOF_TONES", "1") != "0"      # apply_city_materials.roof_tones
KIT_DETAIL = float(os.environ.get("GTP_SURFACES_DETAIL", "0.6"))   # share of the stone detail kept
ROOF = ("zinc_roof", "slate_roof")
ROOF_TILE = ("terracotta_tile",)
# Dressed stone first: the "Roman limestone wall" scan behind limestone_ashlar is rough rubble,
# and on a sunlit wall of the flight it read as a fortress (gate 76 video, 43 s).
STONE = ("dressed_stone", "limestone_ashlar")
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

    def tint(self, col, surf):
        """Surfaces with a tint (a red painted roof standing in for zinc): grey, then the tint
        times twice the grey, so the scan's light and dark patches stay."""
        if surf.get("gain"):
            col = self.mul(col, self.const(float(surf["gain"])))
        if not surf.get("tint"):
            return col
        grey = self.node(unreal.MaterialExpressionDesaturation)
        self.link(col, "", grey, "")       # first input: its pin has no name on 5.8 (PC 12/11)
        grey.set_editor_property("luminance_factors", unreal.LinearColor(0.3, 0.59, 0.11, 0.0))
        return self.mul(self.mul(grey, self.const(tuple(surf["tint"]))), self.const(2.0))

    def colour(self, surf, uv):
        return self.tint(self.mask(self.sample(surf["basecolor"], uv, "color"), r=True, g=True, b=True), surf)

    def layer(self, surf, uv):
        """(colour rgb, normal or None, roughness or None) of one manifest surface."""
        col = self.mask(self.sample(surf["basecolor"], uv, "color"), r=True, g=True, b=True)
        col = self.tint(col, surf)
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
    tex, by_id, luma = {}, {}, {}
    for k, (sid, s) in layers.items():
        if s:
            if sid not in by_id:          # one surface on two layers: sampled once
                uv = g.mul(xy, g.const(1.0 / float(s.get("tile_cm", 200.0))))
                by_id[sid] = g.layer(s, uv)
                avg = g.tint(g.mask(g.sample(s["basecolor"], uv, "color", mip=12), r=True, g=True, b=True), s)
                luma[sid] = g.op(unreal.MaterialExpressionMax, g.op(unreal.MaterialExpressionDotProduct, avg,
                                                                    g.const(LUMA)), g.const(0.01))
            col, nrm, rough = by_id[sid]
            cap = GROUND_ALBEDO_CAP.get(k)
            if cap:
                # colour x min(1, cap / mean albedo): a darker scan is left as it is
                scale = g.op(unreal.MaterialExpressionMin, g.div(g.const(cap), luma[sid]), g.const(1.0))
                col = g.mul(col, scale)
                _used["ground.albedo_cap." + k] = cap
            tex[k] = (col, nrm, rough)
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
        for k, alpha, alpha_out in (("paving", g.mask(mask, g=True), ""), ("kerb", kerb, ""), ("road", on_road, "")):
            b, bo = pick(k, i, fallback, fallback_out)
            v, vo = g.lerp(v, b, alpha, vo, bo, alpha_out), ""
        return v, vo

    col, _ = blend(0, col0, col0_out)
    # Stains as texture layers, not decal actors (no draw call, no DBuffer pass on the M1): the
    # decal's colour where its opacity map says so, scattered by two slow sines so the repeat
    # does not read as a grid. Mud over all the ground, oil on the carriageway only.
    off_road = g.sub(g.const(1.0), on_road)
    for sid_list, region, region_out, period, strength in ((GROUND_GRIME, None, "", 450.0, MUD_STRENGTH),
                                                          (("decal_oil",), on_road, "", 700.0, 0.7),
                                                          (("decal_cracks",), on_road, "", 900.0, 0.6),
                                                          (("decal_gum",), off_road, "", 260.0, 0.8)):
        sid, st = _pick(m, sid_list)
        if not st or not st.get("opacity"):
            continue
        uv = g.mul(xy, g.const(1.0 / period))
        a = g.mask(g.sample(st["opacity"], uv, "masks"), r=True)
        x, y = g.mask(xy, r=True), g.mask(xy, g=True)
        s1 = g.unary(unreal.MaterialExpressionSine, g.mul(x, g.const(1.0 / 730.0)))
        s2 = g.unary(unreal.MaterialExpressionSine, g.mul(y, g.const(1.0 / 1130.0)))
        scatter = g.unary(unreal.MaterialExpressionSaturate, g.mul(g.mul(s1, s2), g.const(3.0)))
        a = g.mul(g.mul(a, scatter), g.const(strength))
        if region is not None:
            a = g.mul(a, region, b_out=region_out)
        stain = g.colour(st, uv)
        avg = g.tint(g.mask(g.sample(st["basecolor"], uv, "color", mip=12), r=True, g=True, b=True), st)
        lum = g.op(unreal.MaterialExpressionMax, g.op(unreal.MaterialExpressionDotProduct, avg, g.const(LUMA)),
                   g.const(0.01))
        stain = g.mul(stain, g.op(unreal.MaterialExpressionMin, g.div(g.const(STAIN_ALBEDO_CAP), lum), g.const(1.0)))
        col = g.lerp(col, stain, a)
        _used["ground.stain." + sid] = sid
        _used["ground.stain_albedo_cap"] = STAIN_ALBEDO_CAP
    gid, grime = _pick(m, GROUND_GRIME)
    if grime and not grime.get("opacity"):
        # Grime as a texture over the whole ground, not as decal actors: one fetch (plus its
        # 1x1 mip), no draw call, no DBuffer pass on the M1. Large repeat, half strength.
        uv = g.mul(xy, g.const(1.0 / float(grime.get("tile_cm", 200.0) * GRIME_SCALE)))
        det = g.lerp(g.const((1.0, 1.0, 1.0)), _detail(g, grime, uv), g.const(GRIME_STRENGTH))
        col = g.mul(col, det)
        _used["ground.grime"] = gid
    col = g.lerp(col, col0, mark, b_out=col0_out)         # markings stay procedural
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if not LITE:
        nrm, _ = blend(1, _flat(g))
        nrm = g.lerp(nrm, _flat(g), mark)
        mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
        if r0 is not None:
            rough, _ = blend(2, r0, r0_out)
            if GROUND_ROUGH_MIN > 0:
                rough = g.op(unreal.MaterialExpressionMax, rough, g.const(GROUND_ROUGH_MIN))
                _used["ground.rough_min"] = GROUND_ROUGH_MIN
            if ROAD_ROUGH_MIN > GROUND_ROUGH_MIN:
                rough = g.op(unreal.MaterialExpressionMax, rough, g.mul(on_road, g.const(ROAD_ROUGH_MIN)))
                _used["ground.road_rough_min"] = ROAD_ROUGH_MIN
            mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    eal.set_metadata_tag(mat, "GTPSurfaces", "1")
    return True


def _detail(g, s, uv):
    """Texture colour divided by its own average (1x1 mip): a detail around 1 that keeps the
    generator's colour, so each building keeps its hue."""
    col = g.tint(g.mask(g.sample(s["basecolor"], uv, "color"), r=True, g=True, b=True), s)
    avg = g.tint(g.mask(g.sample(s["basecolor"], uv, "color", mip=12), r=True, g=True, b=True), s)
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
    # Half to two thirds of the scan's contrast: full strength read blotchy from 30 m (PC 12/11).
    det = g.lerp(g.const((1.0, 1.0, 1.0)), det, g.mul(use, g.const(KIT_DETAIL)))
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
    canal tiles); walls: stone detail, and render detail on the coloured buildings (gate 69: a
    peach wall cut in ashlar blocks). World projection (the shells have no tiling UVs)."""
    m = _manifest()
    rid, roof = _pick(m, ROOF)
    tid, tile = _pick(m, ROOF_TILE)
    sid, stone = _pick(m, STONE)
    pid, render = _pick(m, RENDER)
    if not (roof or tile or stone or render):
        return False
    g = _G(mat)
    vc = g.node(unreal.MaterialExpressionVertexColor)
    nz = g.unary(unreal.MaterialExpressionAbs, g.mask(g.node(unreal.MaterialExpressionVertexNormalWS), b=True))
    roof_a = g.unary(unreal.MaterialExpressionSaturate, g.mul(g.sub(nz, g.const(0.55)), g.const(4.0)))
    xy = g.world_xy()
    col0, col0_out = _input(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    col = (col0, col0_out)
    roof_col = None
    red = g.unary(unreal.MaterialExpressionSaturate,
                  g.mul(g.sub(g.mask(vc, r=True), g.mask(vc, b=True)), g.const(4.0)))

    def patches(surf, uv):
        """The scan's light and dark patches around 1: its grey over its own mean, 0.6 to 1.5."""
        grey = g.op(unreal.MaterialExpressionDotProduct,
                    g.mask(g.sample(surf["basecolor"], uv, "color"), r=True, g=True, b=True), g.const(LUMA))
        mean = g.op(unreal.MaterialExpressionMax, g.op(
            unreal.MaterialExpressionDotProduct,
            g.mask(g.sample(surf["basecolor"], uv, "color", mip=12), r=True, g=True, b=True), g.const(LUMA)),
            g.const(0.01))
        return g.op(unreal.MaterialExpressionMin,
                    g.op(unreal.MaterialExpressionMax, g.div(grey, mean), g.const(0.6)), g.const(1.5))

    zinc_true, tile_true = facade_tone.ROOF_TRUE[1], facade_tone.ROOF_TRUE[0]
    if roof:
        uv = g.mul(xy, g.const(1.0 / float(roof.get("tile_cm", 200.0))))
        roof_col = g.colour(roof, uv)
        _used["shell.roof"] = rid
        try:
            toned = ROOF_TONES and (roof.get("tint") or not tile)
            relief = patches(roof, uv) if toned else None
            if ROOF_TONES and roof.get("tint"):
                # a scan standing in for zinc (a painted metal roof, tinted): its patches on the
                # real tone of zinc, whatever the scan's own lightness (gate 88: roofs as light
                # as the walls, the far bank read as grey blocks)
                roof_col = g.mul(relief, g.const(tuple(zinc_true)))
                _used["shell.roof_tone"] = list(zinc_true)
            if ROOF_TONES and not tile:
                # no scan of tiles on this machine: the roofs the generator painted as tiles
                # stood in zinc like the others. The same patches, on weathered Roman tiles.
                roof_col = g.lerp(roof_col, g.mul(relief, g.const(tuple(tile_true))), red)
                _used["shell.tile"] = "tone on " + rid
                _used["shell.tile_tone"] = list(tile_true)
        except Exception as e:  # noqa: BLE001  the roofs keep the scan as it was
            roof_col = g.colour(roof, uv)
            _used["shell.roof_tone_error"] = repr(e)
    if tile:
        tcol = g.colour(tile, g.mul(xy, g.const(1.0 / float(tile.get("tile_cm", 150.0)))))
        roof_col = g.lerp(roof_col, tcol, red) if roof_col is not None else tcol
        _used["shell.tile"] = tid
    if roof_col is not None:
        col = (g.lerp(col0, roof_col, roof_a, col0_out), "")
    if stone or render:
        # Walls: a detail projected on X/Z and Y/Z, the dominant one by the normal.
        wp = g.node(unreal.MaterialExpressionWorldPosition)
        cls = getattr(unreal, "MaterialExpressionTruncateLWC", None)
        if cls:
            wp = g.unary(cls, wp)
        n = g.node(unreal.MaterialExpressionVertexNormalWS)
        nx = g.unary(unreal.MaterialExpressionAbs, g.mask(n, r=True))
        ny = g.unary(unreal.MaterialExpressionAbs, g.mask(n, g=True))
        side = g.unary(unreal.MaterialExpressionSaturate, g.mul(g.sub(nx, ny), g.const(8.0)))

        def wall_detail(s):
            k = g.const(1.0 / float(s.get("tile_cm", 200.0)))
            xz = g.mul(g.op(unreal.MaterialExpressionAppendVector, g.mask(wp, r=True), g.mask(wp, b=True)), k)
            yz = g.mul(g.op(unreal.MaterialExpressionAppendVector, g.mask(wp, g=True), g.mask(wp, b=True)), k)
            return g.lerp(_detail(g, s, xz), _detail(g, s, yz), side)

        if stone and render:
            # Stone or render from the saturation of the building colour (facade_tone.render_share).
            r, gr, b = g.mask(vc, r=True), g.mask(vc, g=True), g.mask(vc, b=True)
            top = g.op(unreal.MaterialExpressionMax, g.op(unreal.MaterialExpressionMax, r, gr), b)
            low = g.op(unreal.MaterialExpressionMin, g.op(unreal.MaterialExpressionMin, r, gr), b)
            sat = g.div(g.sub(top, low), g.op(unreal.MaterialExpressionMax, top, g.const(0.001)))
            lo, hi = facade_tone.RENDER_SATURATION
            share = g.unary(unreal.MaterialExpressionSaturate, g.div(g.sub(sat, g.const(lo)), g.const(hi - lo)))
            det = g.lerp(wall_detail(stone), wall_detail(render), share)
        else:
            det = wall_detail(stone or render)
        det = g.lerp(g.const((1.0, 1.0, 1.0)), det, g.const(KIT_DETAIL))
        det = g.lerp(det, g.const((1.0, 1.0, 1.0)), roof_a)
        col = (g.mul(col[0], det, col[1]), "")
        if stone:
            _used["shell.stone"] = sid
        if render:
            _used["shell.render"] = pid
    mel.connect_material_property(col[0], col[1], unreal.MaterialProperty.MP_BASE_COLOR)
    eal.set_metadata_tag(mat, "GTPSurfaces", "1")
    return True
