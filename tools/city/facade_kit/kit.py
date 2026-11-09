"""Lyon facade kit v0: one bay x one storey modules, built procedurally in Blender.

Two families from the art bible (section 3.1):
  Haussmann  Second Empire limestone (rue de la Republique, Edouard-Herriot...):
             ashlar, grey hard-stone plinth, entresol, French windows with
             stone surrounds and railings, continuous balconies on consoles,
             moulded cornice.
  Lyonnais   older rendered buildings (side streets, towards the Terreaux):
             coloured render, pierre doree surrounds, louvred shutters,
             protruding sills, wooden shopfronts, genoise eaves.

Module frame (metres): x across the bay, centred (-W/2 .. W/2); the facade
plane is y = 0, the outside is -Y, the wall goes into +Y; z from 0 (storey
floor) to H. Pivot = bottom centre of the bay on the facade plane, which is
where facade_modules.csv puts each module. Placement scales x by
width / nominal width and z by height / nominal height.

UVs are planar, 1 UV unit = 1 m, for tiling textures (art bible 4.5);
material slot names follow the bible (MI_*). Needs Blender 4.2+ (bpy).
"""

import math

import bmesh
import bpy

from kit_layout import FAMILY_OF_TYPOLOGY, KIND_TO_MODULE  # noqa: F401  (re-exported)

from kit_layout import HAUSSMANN, LYONNAIS  # noqa: E402
WALL_DEPTH = 0.45

MATERIALS = {
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
}


# --------------------------------------------------------------------------- builder
class Builder:
    """Accumulates boxes and cylinders in one bmesh with material indices."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.slots = []

    def _mat(self, name):
        if name not in self.slots:
            self.slots.append(name)
        return self.slots.index(name)

    def box(self, x0, x1, y0, y1, z0, z1, mat):
        if x1 - x0 < 1e-4 or y1 - y0 < 1e-4 or z1 - z0 < 1e-4:
            return
        v = [self.bm.verts.new((x, y, z)) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]
        faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        mi = self._mat(mat)
        for f in faces:
            self.bm.faces.new([v[i] for i in f]).material_index = mi

    def cyl(self, x, y, z0, z1, r, mat, seg=6, axis="z", length=None):
        """Cylinder along z (from z0 to z1) or along x (z0..z1 then mean x from x to x+length)."""
        mi = self._mat(mat)
        ring0, ring1 = [], []
        for k in range(seg):
            a = 2 * math.pi * k / seg
            c, s = r * math.cos(a), r * math.sin(a)
            if axis == "z":
                ring0.append(self.bm.verts.new((x + c, y + s, z0)))
                ring1.append(self.bm.verts.new((x + c, y + s, z1)))
            else:  # along x, at height z0, from x to x + length
                ring0.append(self.bm.verts.new((x, y + c, z0 + s)))
                ring1.append(self.bm.verts.new((x + length, y + c, z0 + s)))
        for k in range(seg):
            n = (k + 1) % seg
            self.bm.faces.new((ring0[k], ring0[n], ring1[n], ring1[k])).material_index = mi
        self.bm.faces.new(list(reversed(ring0))).material_index = mi
        self.bm.faces.new(ring1).material_index = mi

    # ----- composite parts
    def wall(self, x0, x1, z0, z1, mat, openings=(), depth=WALL_DEPTH, y=0.0):
        """Wall slab with rectangular openings [(ox0, ox1, oz0, oz1)], reveals included."""
        xs = sorted({x0, x1, *[o[0] for o in openings], *[o[1] for o in openings]})
        zs = sorted({z0, z1, *[o[2] for o in openings], *[o[3] for o in openings]})
        for i in range(len(xs) - 1):
            for j in range(len(zs) - 1):
                cx, cz = (xs[i] + xs[i + 1]) / 2, (zs[j] + zs[j + 1]) / 2
                if any(o[0] < cx < o[1] and o[2] < cz < o[3] for o in openings):
                    continue
                self.box(xs[i], xs[i + 1], y, y + depth, zs[j], zs[j + 1], mat)

    def casement(self, ox0, ox1, oz0, oz1, y=0.16, frame=0.06, mat="MI_Window_PaintedWood", transom=True):
        """Window frame, two leaves, glass behind."""
        self.box(ox0, ox1, y + 0.05, y + 0.07, oz0, oz1, "MI_Glass_Window")
        f, d0, d1 = frame, y, y + 0.06
        self.box(ox0, ox1, d0, d1, oz0, oz0 + f, mat)
        self.box(ox0, ox1, d0, d1, oz1 - f, oz1, mat)
        self.box(ox0, ox0 + f, d0, d1, oz0 + f, oz1 - f, mat)
        self.box(ox1 - f, ox1, d0, d1, oz0 + f, oz1 - f, mat)
        mx = (ox0 + ox1) / 2
        tz = oz0 + (oz1 - oz0) * 0.78
        self.box(mx - f / 2, mx + f / 2, d0, d1, oz0 + f, (tz - f / 2) if transom else oz1 - f, mat)
        if transom:
            self.box(ox0 + f, ox1 - f, d0, d1, tz - f / 2, tz + f / 2, mat)
        # glazing bars, three panes per leaf (set back so they never touch the frame faces)
        top = tz - f / 2 if transom else oz1 - f
        for k in (1, 2):
            gz = oz0 + f + (top - oz0 - f) * k / 3
            for a, c in ((ox0 + f, mx - f / 2), (mx + f / 2, ox1 - f)):
                self.box(a, c, d0 + 0.015, d1 - 0.015, gz - 0.015, gz + 0.015, mat)

    def railing(self, x0, x1, z0, height, y, spacing=0.12, mat="MI_Ironwork_Painted"):
        self.box(x0, x1, y - 0.025, y + 0.025, z0 + height - 0.04, z0 + height, mat)
        self.box(x0, x1, y - 0.015, y + 0.015, z0 + 0.06, z0 + 0.09, mat)
        n = max(2, int((x1 - x0) / spacing))
        for k in range(n + 1):
            self.cyl(x0 + (x1 - x0) * k / n, y, z0, z0 + height, 0.011, mat, seg=6)
        # a simple scroll motif: two horizontal bars near the top
        self.box(x0, x1, y - 0.012, y + 0.012, z0 + height - 0.22, z0 + height - 0.19, mat)

    def moulding(self, x0, x1, z0, steps, mat, y0=0.0):
        """Stepped cornice: steps = [(height, projection)] from bottom to top."""
        z = z0
        for h, p in steps:
            self.box(x0, x1, y0 - p, y0 + 0.05, z, z + h, mat)
            z += h
        return z

    def finish(self, collection):
        bm = self.bm
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        for face in bm.faces:  # planar UVs in metres along the dominant axis
            n = face.normal
            ax = max(range(3), key=lambda i: abs(n[i]))
            for loop in face.loops:
                co = loop.vert.co
                loop[self.uv].uv = {0: (co.y, co.z), 1: (co.x, co.z), 2: (co.x, co.y)}[ax]
        mesh = bpy.data.meshes.new(self.name)
        bm.to_mesh(mesh)
        bm.free()
        for s in self.slots:
            mesh.materials.append(material(s))
        obj = bpy.data.objects.new(self.name, mesh)
        collection.objects.link(obj)
        return obj


def material(name):
    m = bpy.data.materials.get(name)
    if m is None:
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get("Principled BSDF")
        r, g, b, rough, metal = MATERIALS[name]
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Roughness"].default_value = rough
        bsdf.inputs["Metallic"].default_value = metal
    return m


# --------------------------------------------------------------------------- Haussmann
def haus_band(b, W, z, mat="MI_Facade_LimestoneCream"):
    b.moulding(-W / 2, W / 2, z, [(0.06, 0.04), (0.14, 0.07), (0.05, 0.04)], mat)


def haus_window(b, W, H, balcony=False, small=False, court=False):
    ow, sill = 1.22, 0.12
    oh = min(H - 0.75, 2.45) if not small else 1.5
    ox0, ox1, oz0, oz1 = -ow / 2, ow / 2, sill, sill + oh
    b.wall(-W / 2, W / 2, 0, H, "MI_Facade_LimestoneCream", [(ox0, ox1, oz0, oz1)])
    b.casement(ox0, ox1, oz0, oz1)
    if court:
        return
    s = 0.16  # stone surround
    b.box(ox0 - s, ox0, -0.04, 0.0, oz0, oz1, "MI_Facade_LimestoneCream")
    b.box(ox1, ox1 + s, -0.04, 0.0, oz0, oz1, "MI_Facade_LimestoneCream")
    b.box(ox0 - s, ox1 + s, -0.04, 0.0, oz1, oz1 + s, "MI_Facade_LimestoneCream")
    # entablature over the window
    b.moulding(ox0 - s - 0.08, ox1 + s + 0.08, oz1 + s, [(0.12, 0.06), (0.06, 0.12)], "MI_Facade_LimestoneCream")
    haus_band(b, W, 0.0)
    if balcony:
        # continuous balcony: slab on consoles, wrought-iron railing
        b.box(-W / 2, W / 2, -0.85, 0.0, 0.0, 0.17, "MI_Facade_LimestoneCream")
        b.box(-W / 2, W / 2, -0.88, -0.80, 0.17, 0.2, "MI_Facade_LimestoneCream")
        for cx in (-W / 2 + 0.35, W / 2 - 0.35):
            for k in range(3):  # stepped console
                b.box(cx - 0.09, cx + 0.09, -0.7 + k * 0.22, -0.01, -0.36 + k * 0.12, -0.24 + k * 0.12,
                      "MI_Facade_LimestoneCream")
        b.railing(-W / 2, W / 2, 0.2, 1.0, -0.8)
    else:
        b.railing(ox0, ox1, sill, 0.95, -0.06, spacing=0.1)  # balconette in the window bay


def haus_entresol(b, W, H):
    ow, oz0 = 1.22, 0.35
    oz1 = min(H - 0.35, oz0 + 1.45)
    b.wall(-W / 2, W / 2, 0, H, "MI_Facade_LimestoneCream", [(-ow / 2, ow / 2, oz0, oz1)])
    b.casement(-ow / 2, ow / 2, oz0, oz1, transom=False)
    for z in (0.5, 1.15, 1.8):  # rustication grooves read as projecting courses
        if z < H - 0.1:
            b.box(-W / 2, -ow / 2 - 0.12, -0.03, 0, z, z + 0.55, "MI_Facade_LimestoneCream")
            b.box(ow / 2 + 0.12, W / 2, -0.03, 0, z, z + 0.55, "MI_Facade_LimestoneCream")
    haus_band(b, W, 0.0)


def haus_shop(b, W, H, closed=False):
    pil = 0.32
    oz1 = H - 0.95
    b.wall(-W / 2, W / 2, 0, H, "MI_Facade_LimestoneCream", [(-W / 2 + pil, W / 2 - pil, 0.0, oz1)])
    # grey hard-stone plinth on the piers
    b.box(-W / 2, -W / 2 + pil, -0.05, 0.0, 0, 0.6, "MI_Facade_LimestoneGreyPlinth")
    b.box(W / 2 - pil, W / 2, -0.05, 0.0, 0, 0.6, "MI_Facade_LimestoneGreyPlinth")
    x0, x1 = -W / 2 + pil, W / 2 - pil
    # aluminium shopfront: frame, glass, door leaf, shutter box
    b.box(x0, x1, 0.15, 0.17, 0.12, oz1 - 0.35, "MI_Glass_Window")
    for x in (x0, x0 + (x1 - x0) * 0.62, x1 - 0.06):
        b.box(x, x + 0.06, 0.1, 0.18, 0.12, oz1 - 0.35, "MI_Shopfront_Aluminium")
    b.box(x0, x1, 0.1, 0.18, 0.0, 0.12, "MI_Shopfront_Aluminium")
    b.box(x0, x1, 0.02, 0.2, oz1 - 0.35, oz1, "MI_Shopfront_Aluminium" if closed else "MI_Sign_Fascia")
    b.moulding(-W / 2, W / 2, oz1 + 0.1, [(0.08, 0.05), (0.12, 0.1), (0.06, 0.05)], "MI_Facade_LimestoneCream")


def haus_entrance(b, W, H):
    dw, dh = min(W - 0.9, 1.9), min(H - 0.45, 3.5)
    b.wall(-W / 2, W / 2, 0, H, "MI_Facade_LimestoneCream", [(-dw / 2, dw / 2, 0.0, dh)])
    b.box(-W / 2, -dw / 2, -0.07, 0.0, 0, 0.6, "MI_Facade_LimestoneGreyPlinth")
    b.box(dw / 2, W / 2, -0.07, 0.0, 0, 0.6, "MI_Facade_LimestoneGreyPlinth")
    # carriage door: two panelled leaves, fanlight with ironwork
    fan = dh - 0.9
    b.box(-dw / 2, dw / 2, 0.2, 0.26, 0.0, fan, "MI_Door_Wood")
    for x in (-dw / 2 + 0.12, 0.04):
        for z0 in (0.25, 1.35):
            b.box(x, x + dw / 2 - 0.16, 0.16, 0.2, z0, z0 + 0.9, "MI_Door_Wood")
    b.box(-0.02, 0.02, 0.14, 0.2, 0.0, fan, "MI_Door_Wood")
    b.box(-dw / 2, dw / 2, 0.22, 0.24, fan, dh, "MI_Glass_Window")
    b.railing(-dw / 2, dw / 2, fan, dh - fan, 0.2, spacing=0.15)
    s = 0.25
    b.box(-dw / 2 - s, -dw / 2, -0.06, 0, 0.6, dh, "MI_Facade_LimestoneCream")
    b.box(dw / 2, dw / 2 + s, -0.06, 0, 0.6, dh, "MI_Facade_LimestoneCream")
    b.box(-dw / 2 - s, -0.18, -0.06, 0, dh, dh + s, "MI_Facade_LimestoneCream")
    b.box(0.18, dw / 2 + s, -0.06, 0, dh, dh + s, "MI_Facade_LimestoneCream")
    b.box(-0.18, 0.18, -0.1, 0, dh - 0.05, dh + s + 0.05, "MI_Facade_LimestoneCream")  # keystone


def haus_cornice(b, W):
    b.moulding(-W / 2, W / 2, 0.0, [(0.12, 0.08), (0.08, 0.16), (0.22, 0.26), (0.1, 0.42), (0.08, 0.5)],
               "MI_Facade_LimestoneCream")
    for k in range(int(W / 0.55)):  # modillions under the corona
        x = -W / 2 + 0.275 + k * 0.55
        b.box(x - 0.07, x + 0.07, -0.36, -0.08, 0.2, 0.42, "MI_Facade_LimestoneCream")


def blank(b, W, H, mat):
    b.wall(-W / 2, W / 2, 0, H, mat)


# --------------------------------------------------------------------------- Lyonnais
def lyon_window(b, W, H, small=False, court=False, shutters=True, bars=False, sill_z=0.9):
    ow = 1.0
    oh = min(H - sill_z - 0.35, 0.95 if small else 1.75)
    ox0, ox1, oz0, oz1 = -ow / 2, ow / 2, sill_z, sill_z + oh
    b.wall(-W / 2, W / 2, 0, H, "MI_Facade_Render", [(ox0, ox1, oz0, oz1)])
    b.casement(ox0, ox1, oz0, oz1, transom=not small)
    if court:
        return
    s = 0.14
    b.box(ox0 - s, ox0, -0.025, 0, oz0, oz1, "MI_Facade_PierreDoree")
    b.box(ox1, ox1 + s, -0.025, 0, oz0, oz1, "MI_Facade_PierreDoree")
    b.box(ox0 - s, ox1 + s, -0.025, 0, oz1, oz1 + s, "MI_Facade_PierreDoree")
    b.box(ox0 - s - 0.04, ox1 + s + 0.04, -0.09, 0, oz0 - 0.08, oz0, "MI_Facade_PierreDoree")  # sill
    if bars:
        for k in range(1, 6):
            b.cyl(ox0 + ow * k / 6, -0.05, oz0, oz1, 0.012, "MI_Ironwork_Painted")
    if shutters:  # louvred shutters folded back against the wall
        for side in (-1, 1):
            xa = ox0 - s - 0.52 if side < 0 else ox1 + s
            xb = xa + 0.52
            b.box(xa, xb, -0.06, -0.03, oz0, oz1, "MI_Shutter_Painted")
            for z in [oz0 + 0.06 + k * 0.07 for k in range(int((oz1 - oz0 - 0.12) / 0.07))]:
                b.box(xa + 0.04, xb - 0.04, -0.085, -0.06, z, z + 0.035, "MI_Shutter_Painted")


def lyon_shop(b, W, H):
    x0, x1, oz1 = -W / 2 + 0.25, W / 2 - 0.25, H - 0.9
    b.wall(-W / 2, W / 2, 0, H, "MI_Facade_Render", [(x0, x1, 0.0, oz1)])
    # wooden shopfront fixed in front of the wall (devanture en applique)
    b.box(x0, x1, 0.1, 0.12, 0.6, oz1 - 0.5, "MI_Glass_Window")
    b.box(x0 + 0.05, x1 - 0.05, -0.12, 0.0, 0.0, 0.6, "MI_Shopfront_Wood")
    b.box(x0 - 0.15, x0 + 0.05, -0.14, 0.0, 0.0, oz1 - 0.5, "MI_Shopfront_Wood")
    b.box(x1 - 0.05, x1 + 0.15, -0.14, 0.0, 0.0, oz1 - 0.5, "MI_Shopfront_Wood")
    b.box(x0 - 0.15, x1 + 0.15, -0.16, 0.0, oz1 - 0.5, oz1 + 0.15, "MI_Sign_Fascia")
    b.moulding(x0 - 0.2, x1 + 0.2, oz1 + 0.15, [(0.06, 0.2), (0.05, 0.24)], "MI_Shopfront_Wood")
    b.box(-0.05, 0.05, 0.05, 0.12, 0.6, oz1 - 0.5, "MI_Shopfront_Wood")


def lyon_entrance(b, W, H):
    dw, dh = min(W - 0.8, 1.2), min(H - 0.5, 2.6)
    b.wall(-W / 2, W / 2, 0, H, "MI_Facade_Render", [(-dw / 2, dw / 2, 0.0, dh)])
    b.box(-dw / 2, dw / 2, 0.2, 0.26, 0.0, dh - 0.55, "MI_Door_Wood")
    b.box(-dw / 2, dw / 2, 0.22, 0.24, dh - 0.55, dh, "MI_Glass_Window")
    for z0 in (0.2, 1.1):
        b.box(-dw / 2 + 0.1, dw / 2 - 0.1, 0.16, 0.2, z0, z0 + 0.75, "MI_Door_Wood")
    s = 0.2
    b.box(-dw / 2 - s, -dw / 2, -0.04, 0, 0, dh, "MI_Facade_PierreDoree")
    b.box(dw / 2, dw / 2 + s, -0.04, 0, 0, dh, "MI_Facade_PierreDoree")
    b.box(-dw / 2 - s, dw / 2 + s, -0.04, 0, dh, dh + s, "MI_Facade_PierreDoree")


def lyon_genoise(b, W):
    """Two rows of Roman tiles under the roof edge, typical of Lyon."""
    for row, (z, p) in enumerate(((0.0, 0.18), (0.14, 0.34))):
        n = int(W / 0.2)
        for k in range(n):
            x = -W / 2 + 0.1 + k * 0.2 + (0.1 if row else 0)
            if x > W / 2 - 0.08:
                continue
            b.box(x - 0.08, x + 0.08, -p, 0.0, z, z + 0.14, "MI_Genoise_Tile")
        b.box(-W / 2, W / 2, -p - 0.02, 0.0, z + 0.12, z + 0.14, "MI_Facade_Render")


# --------------------------------------------------------------------------- catalogue
# module name -> (family nominal sizes, nominal height key, builder)
def _defs():
    H, L = HAUSSMANN, LYONNAIS
    return {
        "SM_Facade_Haussmann_Window": (H, "upper", lambda b: haus_window(b, H["bay"], H["upper"])),
        "SM_Facade_Haussmann_WindowBalcony": (H, "upper", lambda b: haus_window(b, H["bay"], H["upper"], True)),
        "SM_Facade_Haussmann_WindowAttic": (H, "upper", lambda b: haus_window(b, H["bay"], H["upper"], small=True)),
        "SM_Facade_Haussmann_CourtWindow": (H, "upper", lambda b: haus_window(b, H["bay"], H["upper"], court=True)),
        "SM_Facade_Haussmann_Entresol": (H, "entresol", lambda b: haus_entresol(b, H["bay"], H["entresol"])),
        "SM_Facade_Haussmann_Shop": (H, "gf", lambda b: haus_shop(b, H["bay"], H["gf"])),
        "SM_Facade_Haussmann_ShopClosed": (H, "gf", lambda b: haus_shop(b, H["bay"], H["gf"], closed=True)),
        "SM_Facade_Haussmann_Entrance": (H, "gf", lambda b: haus_entrance(b, H["bay"], H["gf"])),
        "SM_Facade_Haussmann_Blank": (H, "upper", lambda b: blank(b, H["bay"], H["upper"], "MI_Facade_LimestoneCream")),
        "SM_Facade_Haussmann_Cornice": (H, None, lambda b: haus_cornice(b, H["bay"])),
        "SM_Facade_Lyonnais_Window": (L, "upper", lambda b: lyon_window(b, L["bay"], L["upper"])),
        "SM_Facade_Lyonnais_WindowSmall": (L, "upper", lambda b: lyon_window(b, L["bay"], L["upper"], small=True)),
        "SM_Facade_Lyonnais_CourtWindow": (L, "upper", lambda b: lyon_window(b, L["bay"], L["upper"], court=True)),
        "SM_Facade_Lyonnais_GroundWindow": (L, "gf", lambda b: lyon_window(b, L["bay"], L["gf"], shutters=False,
                                                                           bars=True, sill_z=1.2)),
        "SM_Facade_Lyonnais_Shop": (L, "gf", lambda b: lyon_shop(b, L["bay"], L["gf"])),
        "SM_Facade_Lyonnais_Entrance": (L, "gf", lambda b: lyon_entrance(b, L["bay"], L["gf"])),
        "SM_Facade_Lyonnais_Blank": (L, "upper", lambda b: blank(b, L["bay"], L["upper"], "MI_Facade_Render")),
        "SM_Facade_Lyonnais_Genoise": (L, None, lambda b: lyon_genoise(b, L["bay"])),
    }


def build_kit(collection):
    """Create every module object in `collection`. Returns {name: (object, nominal_w, nominal_h)}."""
    out = {}
    for name, (fam, hkey, fn) in _defs().items():
        b = Builder(name)
        fn(b)
        obj = b.finish(collection)
        out[name] = (obj, fam["bay"], fam[hkey] if hkey else None)
    return out
