"""Mansard roof modules of the facade kit (Second Empire blocks, rue de la Republique).

procgen.levels_for() ends a Haussmann wall taller than MANSARD_MIN_WALL_M with a mansard
level: the stone storeys stop at the cornice, then one module per bay of this file:
  SM_Roof_Haussmann_Dormer        zinc brisis with a stone dormer (lucarne): window with
                                  casement, moulded surround, segmental pediment, zinc cheeks
  SM_Roof_Haussmann_MansardBlank  zinc brisis only (court side, narrow bays)
Same frame as kit.py: x across the bay, centred; facade plane y = 0, outside -Y, z from the
cornice top (0) to the eave (H). The brisis leans back by INSET at the eave, so the terrasson
of the generated roof meets it there. Built with kit.Builder (Blender); this module itself
only needs the Builder passed in, so it imports without bpy.

Material slots added to the kit: MI_Roof_Zinc (brisis, cheeks, dormer roof, ridge rolls).
"""

ROOF = {"bay": 3.3, "mansard": 3.2}
INSET = 0.8            # metres the brisis top stands behind the facade plane (procgen.MANSARD_INSET_M)
SLAB = 0.12            # brisis thickness
MATERIALS = {"MI_Roof_Zinc": (0.40, 0.42, 0.44, 0.45, 0.6)}
STONE = "MI_Facade_LimestoneCream"
ZINC = "MI_Roof_Zinc"


def slab(b, pts_yz, x0, x1, mat):
    """Convex polygon in the yz plane, extruded along x from x0 to x1."""
    mi = b._mat(mat)
    left = [b.bm.verts.new((x0, y, z)) for y, z in pts_yz]
    right = [b.bm.verts.new((x1, y, z)) for y, z in pts_yz]
    b.bm.faces.new(list(reversed(left))).material_index = mi
    b.bm.faces.new(right).material_index = mi
    n = len(pts_yz)
    for k in range(n):
        j = (k + 1) % n
        b.bm.faces.new((left[k], left[j], right[j], right[k])).material_index = mi


def brisis_y(z, H):
    """Facade-side y of the brisis surface at height z."""
    return INSET * z / H


def brisis(b, W, H, x0=None, x1=None):
    x0 = -W / 2 if x0 is None else x0
    x1 = W / 2 if x1 is None else x1
    slab(b, [(0.0, 0.0), (SLAB, 0.0), (INSET + SLAB, H), (INSET, H)], x0, x1, ZINC)
    # ridge roll at the break of slope, where the terrasson starts
    b.cyl(x0, INSET - 0.02, H, None, 0.035, ZINC, seg=6, axis="x", length=x1 - x0)


def mansard_blank(b, W, H):
    brisis(b, W, H)


def dormer(b, W, H):
    """Stone lucarne standing on the cornice line, its front just behind the cornice."""
    dw, front = 1.36, 0.18                 # dormer width, front face y
    z0, zw0, zw1 = 0.30, 0.55, 2.05        # dormer base, window bottom and top
    ztop = 2.45                            # top of the side walls, pediment above
    ow = 0.82
    hx = dw / 2
    # brisis on both sides of the dormer
    brisis(b, W, H, -W / 2, -hx)
    brisis(b, W, H, hx, W / 2)
    # brisis under and above the dormer
    slab(b, [(0.0, 0.0), (SLAB, 0.0), (brisis_y(z0, H) + SLAB, z0), (brisis_y(z0, H), z0)], -hx, hx, ZINC)
    zb = ztop + 0.45
    slab(b, [(brisis_y(zb, H), zb), (brisis_y(zb, H) + SLAB, zb), (INSET + SLAB, H), (INSET, H)], -hx, hx, ZINC)
    b.cyl(-hx, INSET - 0.02, H, None, 0.035, ZINC, seg=6, axis="x", length=dw)
    # front wall with the window opening, stone surround and sill
    depth = 0.22
    b.wall(-hx, hx, z0, ztop, STONE, openings=[(-ow / 2, ow / 2, zw0, zw1)], depth=depth, y=front)
    b.casement(-ow / 2, ow / 2, zw0, zw1, y=front + 0.08, frame=0.05, transom=False)
    b.box(-ow / 2 - 0.12, ow / 2 + 0.12, front - 0.06, front + 0.02, zw0 - 0.08, zw0, STONE)   # sill
    for x in (-ow / 2 - 0.12, ow / 2):                                                       # jambs
        b.box(x, x + 0.12, front - 0.04, front, zw0, zw1, STONE)
    b.box(-ow / 2 - 0.12, ow / 2 + 0.12, front - 0.05, front, zw1, zw1 + 0.14, STONE)         # lintel
    # segmental pediment: a low arch approximated by a convex polygon, with a moulded edge
    arch = [(-hx - 0.05, ztop), (hx + 0.05, ztop), (hx * 0.7, ztop + 0.30), (0.0, ztop + 0.42),
            (-hx * 0.7, ztop + 0.30)]
    b.prism(arch, front - 0.06, front + depth, STONE)
    b.box(-hx - 0.08, hx + 0.08, front - 0.09, front + 0.02, ztop - 0.06, ztop, STONE)        # cornice band
    # zinc cheeks from the front wall back to the brisis, and the dormer roof
    # (the brisis meets the back of the front wall at z_meet; below, the wall stands in front of it)
    back = front + depth
    z_meet = H * (back - SLAB) / INSET
    zt2 = ztop + 0.30
    back_top = brisis_y(zt2, H) + SLAB
    for x0, x1 in ((-hx, -hx + 0.06), (hx - 0.06, hx)):
        slab(b, [(back, z_meet), (back_top, zt2), (back, zt2)], x0, x1, ZINC)
    slab(b, [(back - 0.02, zt2), (back_top + 0.1, zt2), (back_top + 0.1, zt2 + 0.12), (back - 0.02, zt2 + 0.12)],
         -hx - 0.02, hx + 0.02, ZINC)


def defs(materials):
    """Kit definitions {name: (family sizes, height key, builder fn)}; adds the roof materials."""
    materials.update(MATERIALS)
    W, H = ROOF["bay"], ROOF["mansard"]
    return {
        "SM_Roof_Haussmann_Dormer": (ROOF, "mansard", lambda b: dormer(b, W, H)),
        "SM_Roof_Haussmann_MansardBlank": (ROOF, "mansard", lambda b: mansard_blank(b, W, H)),
    }
