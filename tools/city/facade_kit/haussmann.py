"""Haussmann bays of the facade kit, v1: relief that reads at street level on the PC (Nanite).

Same modules, names, nominal sizes and frame as kit.py v0 (one bay x one storey, pivot at the
bottom centre on the facade plane y = 0, outside -Y), so placement and the CSV do not change.
What v1 adds, after the rue de la Republique (1855-1865, Second Empire limestone):
  - coursed ashlar with real joints on the upper storeys, chamfered horizontal rustication
    (refends) on the ground floor and the entresol;
  - profiled mouldings (fillet, ovolo, cyma, cavetto) instead of stacked boxes: string courses,
    sills, lintel cornices, the main cornice with dentils and scrolled modillions;
  - architraves with ears (crossettes) round the windows, deeper reveals, folded metal
    shutters (persiennes) in the reveals, windows set back 0.25 m;
  - balconies on scrolled consoles with a moulded slab edge, cast-iron railings with rings;
  - every stone and joinery arris chamfered (Builder.box bevel), so the low sun draws lines.
Triangles: about 5 to 20 k per module (v0: 0.3 to 1 k); fine for Nanite, the M1 keeps the
generated LODs (place_facades.py) or the v0 kit with GTP_KIT_V=0.
"""

import math

import kit

STONE = "MI_Facade_LimestoneCream"
PLINTH = "MI_Facade_LimestoneGreyPlinth"
IRON = "MI_Ironwork_Painted"
JOIN = "MI_Window_PaintedWood"
PERS = "MI_Persienne"
BEVEL = 0.012
FRAME_Y = 0.25          # window frame set back from the facade plane


# --------------------------------------------------------------------------- profiles
def profile(parts, z0=0.0, y0=0.0, back=0.04, seg=5):
    """Moulding cross-section [(y, z)] from bottom to top: parts [(kind, height, projection)],
    kind step (out to the projection, then straight up), ovolo (convex quarter), cavetto
    (concave quarter), cyma (S curve), chamfer (straight slope). Projection is outward (-Y).
    Returns (points, top z)."""
    out, z = 0.0, z0
    pts = [(0.0, z0)]
    for kind, dz, p in parts:
        if kind == "step":
            pts.append((p, z))
            out = p
            z += dz
            pts.append((out, z))
            continue
        p0 = out
        for k in range(1, seg + 1):
            t = k / seg
            if kind == "ovolo":
                th = t * math.pi / 2
                o, zz = p0 + (p - p0) * math.sin(th), z + dz * (1 - math.cos(th))
            elif kind == "cavetto":
                th = t * math.pi / 2
                o, zz = p0 + (p - p0) * (1 - math.cos(th)), z + dz * math.sin(th)
            elif kind == "cyma":
                o, zz = p0 + (p - p0) * (0.5 - 0.5 * math.cos(math.pi * t)), z + dz * t
            else:
                o, zz = p0 + (p - p0) * t, z + dz * t
            pts.append((o, zz))
        out = p
        z += dz
    pts += [(-back, z), (-back, z0)]
    return [(y0 - o, zz) for o, zz in pts], z


def moulding(b, x0, x1, z0, parts, mat=STONE, y0=0.0):
    pts, top = profile(parts, z0, y0)
    b.sweep(pts, x0, x1, mat)
    return top


def jamb(b, xe, side, z0, z1, mat=STONE, width=0.17):
    """Moulded architrave jamb next to an opening edge xe; side -1 left (grows to -x), +1 right.
    Plan section [(x, y)]: fillet on the reveal, a cyma, a flat band, a bead on the outside."""
    s = side
    pts = [(xe, 0.0), (xe, -0.03), (xe + s * 0.02, -0.05), (xe + s * 0.05, -0.055), (xe + s * 0.07, -0.045),
           (xe + s * 0.12, -0.045), (xe + s * 0.13, -0.06), (xe + s * 0.15, -0.065), (xe + s * width, -0.05),
           (xe + s * width, 0.0), (xe + s * width, 0.04), (xe, 0.04)]
    if s < 0:
        pts = list(reversed(pts))
    b.sweep(pts, z0, z1, mat, axis="z")


def head(b, x0, x1, z0, mat=STONE, height=0.17):
    """Architrave head across the opening (same section as the jambs, along x)."""
    pts = [(0.0, z0), (-0.05, z0), (-0.055, z0 + 0.05), (-0.045, z0 + 0.07), (-0.045, z0 + 0.12),
           (-0.06, z0 + 0.13), (-0.065, z0 + 0.15), (-0.05, z0 + height), (0.04, z0 + height), (0.04, z0)]
    b.sweep(pts, x0, x1, mat)
    return z0 + height


def console(b, x, w, z_top, out, height, mat=STONE, eye=True):
    """Scrolled console (bracket) under a slab or a lintel cornice, centred on x, w wide."""
    pts = [(0.03, z_top), (-out, z_top), (-out, z_top - 0.08)]
    for k in range(1, 9):   # S-shaped underside from the front down to the wall
        t = k / 8
        o = out - (out - 0.06) * (0.5 - 0.5 * math.cos(math.pi * t))
        pts.append((-o, z_top - 0.08 - (height - 0.14) * t))
    pts += [(-0.06, z_top - height), (0.03, z_top - height)]
    b.sweep(pts, x - w / 2, x + w / 2, mat)
    if eye:   # the volute's eye at the foot, a short drum across the console
        b.cyl(x - w / 2 - 0.01, -0.1, z_top - height + 0.09, 0, 0.075, mat, seg=10, axis="x", length=w + 0.02)
        b.cyl(x - w / 2 - 0.01, -out + 0.07, z_top - 0.1, 0, 0.06, mat, seg=10, axis="x", length=w + 0.02)
    # acanthus leaf hinted by a tapered front panel
    b.box(x - w / 2 + 0.03, x + w / 2 - 0.03, -out + 0.01, -out + 0.04, z_top - 0.08 - (height - 0.14) * 0.45,
          z_top - 0.08, mat, bevel=0.01)


# --------------------------------------------------------------------------- walls
def rect_minus(r, holes):
    """Rectangle (x0, x1, z0, z1) minus holes: rectangles merged along x in each band."""
    x0, x1, z0, z1 = r
    hs = [h for h in holes if h[0] < x1 - 1e-6 and h[1] > x0 + 1e-6 and h[2] < z1 - 1e-6 and h[3] > z0 + 1e-6]
    if not hs:
        return [r]
    xs = sorted({x0, x1, *[min(max(h[i], x0), x1) for h in hs for i in (0, 1)]})
    zs = sorted({z0, z1, *[min(max(h[i], z0), z1) for h in hs for i in (2, 3)]})
    out = []
    for j in range(len(zs) - 1):
        run = None
        for i in range(len(xs) - 1):
            cx, cz = (xs[i] + xs[i + 1]) / 2, (zs[j] + zs[j + 1]) / 2
            inside = any(h[0] < cx < h[1] and h[2] < cz < h[3] for h in hs)
            if inside:
                if run:
                    out.append((run[0], xs[i], zs[j], zs[j + 1]))
                run = None
            elif run is None:
                run = (xs[i],)
        if run:
            out.append((run[0], xs[-1], zs[j], zs[j + 1]))
    return out


def ashlar(b, x0, x1, z0, z1, openings=(), mat=STONE, course=0.47, block=1.1, joint=0.012, jd=0.02,
           rust=False, bevel=0.008, phase=0.0):
    """Stone wall with joints: a back slab with the openings, and the blocks jd in front of it.
    rust: horizontal rustication (refends), one block per course across the bay, joints wide,
    chamfered along x only, so neighbouring bays read as one continuous course."""
    b.wall(x0, x1, z0, z1, mat, openings, depth=kit.WALL_DEPTH - jd, y=jd)
    za, row = z0, 0
    while za < z1 - 1e-3:
        zb = min(z1, za + course)
        if rust:
            xs = [x0, x1]
        else:
            off = ((phase + 0.5 * (row % 2)) * block) % block
            xs = [x0]
            xx = x0 + block - off
            while xx < x1 - 0.2:
                if xx > x0 + 0.2:
                    xs.append(xx)
                xx += block
            xs.append(x1)
        for xa, xb in zip(xs, xs[1:]):
            ja = 0.0 if rust else joint / 2
            r = (xa + ja, xb - ja, za + joint / 2, zb - joint / 2)
            for q in rect_minus(r, openings):
                b.box(q[0], q[1], 0.0, jd + 0.002, q[2], q[3], mat, bevel=bevel, axis="x" if rust else None)
        za, row = zb, row + 1


# --------------------------------------------------------------------------- joinery and ironwork
def casement(b, ox0, ox1, oz0, oz1, transom=True, y=FRAME_Y, persiennes=True):
    """French window: frame, two leaves with three panes, transom light, glass, folded metal
    shutters in the reveals."""
    f, d0, d1 = 0.065, y, y + 0.07
    b.box(ox0, ox1, y + 0.045, y + 0.055, oz0, oz1, "MI_Glass_Window")
    b.box(ox0, ox1, d0, d1, oz0, oz0 + 0.09, JOIN, bevel=0.01)              # sill rail with drip
    b.box(ox0, ox1, d0 - 0.025, d0, oz0 + 0.06, oz0 + 0.09, JOIN, bevel=0.008)
    b.box(ox0, ox1, d0, d1, oz1 - f, oz1, JOIN, bevel=0.01)
    b.box(ox0, ox0 + f, d0, d1, oz0, oz1, JOIN, bevel=0.01)
    b.box(ox1 - f, ox1, d0, d1, oz0, oz1, JOIN, bevel=0.01)
    mx = (ox0 + ox1) / 2
    tz = oz0 + (oz1 - oz0) * 0.8 if transom else oz1 - f
    if transom:
        b.box(ox0 + f, ox1 - f, d0 - 0.01, d1, tz - 0.035, tz + 0.035, JOIN, bevel=0.01)
    # leaves: stiles meeting at the centre (with the espagnolette rod), rails
    for a, c in ((ox0 + f, mx), (mx, ox1 - f)):
        b.box(a, a + 0.05, d0 + 0.005, d1 - 0.005, oz0 + 0.09, tz, JOIN, bevel=0.008)
        b.box(c - 0.05, c, d0 + 0.005, d1 - 0.005, oz0 + 0.09, tz, JOIN, bevel=0.008)
        b.box(a, c, d0 + 0.005, d1 - 0.005, oz0 + 0.09, oz0 + 0.22, JOIN, bevel=0.008)   # bottom rail
        b.box(a, c, d0 + 0.005, d1 - 0.005, tz - 0.08, tz - 0.035, JOIN, bevel=0.008)
        top = tz - 0.08
        for k in (1, 2):
            gz = oz0 + 0.22 + (top - oz0 - 0.22) * k / 3
            b.box(a + 0.05, c - 0.05, d0 + 0.02, d1 - 0.02, gz - 0.014, gz + 0.014, JOIN)
    b.cyl(mx, d0 - 0.012, oz0 + 0.25, tz - 0.1, 0.008, "MI_Ironwork_Painted", seg=6)
    if transom:
        b.box(mx - 0.015, mx + 0.015, d0 + 0.02, d1 - 0.02, tz + 0.035, oz1 - f, JOIN)
    if persiennes:   # folded metal shutters, a stack of four leaves against each reveal
        for side in (-1, 1):
            xe = ox0 if side < 0 else ox1
            xa, xb = (xe, xe + 0.075) if side < 0 else (xe - 0.075, xe)
            b.box(xa, xb, 0.03, y - 0.005, oz0 + 0.02, oz1 - 0.02, PERS, bevel=0.006)
            for k in range(1, 4):
                yy = 0.03 + (y - 0.035) * k / 4
                b.box(xa - 0.004, xb + 0.004, yy - 0.004, yy + 0.004, oz0 + 0.02, oz1 - 0.02, PERS)


def railing(b, x0, x1, z0, height, y, posts=True):
    """Cast-iron railing: handrail, two lower rails, square bars, a frieze of rings at the top
    and the bottom, heavier posts about every 1.1 m."""
    top = z0 + height
    b.box(x0, x1, y - 0.035, y + 0.035, top - 0.045, top, IRON, bevel=0.012)        # handrail
    b.box(x0, x1, y - 0.012, y + 0.012, top - 0.2, top - 0.18, IRON)
    b.box(x0, x1, y - 0.012, y + 0.012, z0 + 0.13, z0 + 0.15, IRON)
    b.box(x0, x1, y - 0.02, y + 0.02, z0, z0 + 0.04, IRON)
    n = max(2, int(round((x1 - x0) / 0.11)))
    for k in range(n + 1):
        x = x0 + (x1 - x0) * k / n
        b.bar((x, y, z0 + 0.04), (x, y, top - 0.045), 0.009, IRON)
    m = max(1, int(round((x1 - x0) / 0.22)))
    for k in range(m):
        cx = x0 + (x1 - x0) * (k + 0.5) / m
        b.ring(cx, y, top - 0.11, 0.065, 0.007, IRON, n=10)
        b.ring(cx, y, z0 + 0.095, 0.045, 0.006, IRON, n=8)
    if posts:
        p = max(1, int(round((x1 - x0) / 1.1)))
        for k in range(p + 1):
            x = x0 + (x1 - x0) * k / p
            b.box(x - 0.018, x + 0.018, y - 0.018, y + 0.018, z0, top, IRON, bevel=0.006)
            b.box(x - 0.03, x + 0.03, y - 0.03, y + 0.03, top, top + 0.05, IRON, bevel=0.01)


# --------------------------------------------------------------------------- modules
def string_course(b, W, z=0.0):
    return moulding(b, -W / 2, W / 2, z, [("step", 0.04, 0.03), ("ovolo", 0.05, 0.07), ("step", 0.1, 0.075),
                                         ("cyma", 0.06, 0.11), ("step", 0.03, 0.115), ("chamfer", 0.03, 0.0)])


def window(b, W, H, balcony=False, small=False, court=False, style="A"):
    """Upper storey bay. style A: frieze and cornice over the window; B: triangular pediment;
    C: lintel cornice on two scrolled consoles, keystone, moulded apron."""
    ow = 1.0 if small else 1.22
    oz0 = 0.22 if balcony else 0.14
    oh = 1.55 if small else min(H - 0.9, 2.35)
    ox0, ox1, oz1 = -ow / 2, ow / 2, oz0 + oh
    if court:
        b.wall(-W / 2, W / 2, 0, H, STONE, [(ox0, ox1, oz0, oz1)])
        kit.Builder.casement(b, ox0, ox1, oz0, oz1)
        return
    op = [(ox0, ox1, oz0, oz1)]
    ashlar(b, -W / 2, W / 2, 0.0, H, op, phase=0.25)
    casement(b, ox0, ox1, oz0, oz1, transom=not small)
    w = 0.17
    jamb(b, ox0, -1, oz0, oz1)
    jamb(b, ox1, 1, oz0, oz1)
    top = head(b, ox0 - w - 0.09, ox1 + w + 0.09, oz1)
    for x in (ox0 - w - 0.09, ox1 + w):   # crossettes: the head's ears drop past the jambs
        b.box(x, x + 0.09, -0.05, 0.03, oz1 - 0.13, oz1, STONE, bevel=0.008)
    if style == "B":
        top = moulding(b, ox0 - w - 0.14, ox1 + w + 0.14, top, [("step", 0.06, 0.03), ("cyma", 0.06, 0.1),
                                                               ("step", 0.05, 0.13)])
        half = ow / 2 + w + 0.16
        peak = min(0.45, H - top - 0.1)
        b.prism([(-half, top), (half, top), (0.0, top + peak)], -0.13, 0.0, STONE)
        b.prism([(-half + 0.14, top + 0.02), (half - 0.14, top + 0.02), (0.0, top + peak - 0.1)], -0.15, -0.12,
                STONE)   # tympanum set back behind the raking cornice
    elif style == "C":
        for cx in (ox0 - w / 2, ox1 + w / 2):
            console(b, cx, 0.15, top + 0.02, 0.2, 0.42, eye=False)
        b.box(-0.13, 0.13, -0.1, 0.0, oz1 - 0.06, top + 0.06, STONE, bevel=0.015)   # keystone
        moulding(b, ox0 - w - 0.2, ox1 + w + 0.2, top + 0.02,
                 [("step", 0.04, 0.05), ("ovolo", 0.06, 0.12), ("step", 0.07, 0.24), ("cyma", 0.06, 0.29),
                  ("step", 0.03, 0.3)])
        if not balcony:   # moulded apron under the sill
            b.box(ox0 + 0.08, ox1 - 0.08, -0.03, 0.0, 0.02, oz0 - 0.03, STONE, bevel=0.01)
    else:
        b.box(ox0 - w, ox1 + w, -0.02, 0.0, top, top + 0.2, STONE, bevel=0.006)   # frieze
        moulding(b, ox0 - w - 0.12, ox1 + w + 0.12, top + 0.2,
                 [("step", 0.04, 0.04), ("ovolo", 0.05, 0.09), ("step", 0.06, 0.17), ("cyma", 0.05, 0.2)])
    string_course(b, W, 0.0)
    if balcony:
        slab = [("cavetto", 0.07, 0.72), ("step", 0.08, 0.86), ("ovolo", 0.05, 0.9), ("step", 0.02, 0.88)]
        top_slab = moulding(b, -W / 2, W / 2, 0.02, slab)
        for cx in (-W / 2 + 0.42, W / 2 - 0.42):
            console(b, cx, 0.22, 0.02, 0.72, 0.62)
        railing(b, -W / 2, W / 2, top_slab, 1.0, -0.8)
    else:
        sill = moulding(b, ox0 - 0.08, ox1 + 0.08, oz0 - 0.08, [("step", 0.03, 0.04), ("ovolo", 0.03, 0.08),
                                                              ("step", 0.02, 0.09)])
        railing(b, ox0 + 0.02, ox1 - 0.02, sill, 0.95, -0.07, posts=False)


def entresol(b, W, H):
    ow, oz0 = 1.22, 0.4
    oz1 = min(H - 0.45, oz0 + 1.5)
    op = [(-ow / 2, ow / 2, oz0, oz1)]
    ashlar(b, -W / 2, W / 2, 0.0, H, op, rust=True, course=0.43, joint=0.035, jd=0.05, bevel=0.03)
    casement(b, -ow / 2, ow / 2, oz0, oz1, transom=False, persiennes=False)
    b.box(-0.14, 0.14, -0.09, 0.0, oz1 - 0.05, oz1 + 0.34, STONE, bevel=0.015)   # keystone
    for s in (-1, 1):   # voussoirs either side of the keystone, a flat arch
        b.box(s * 0.16 if s > 0 else -0.5, 0.5 if s > 0 else -0.16, -0.06, 0.0, oz1, oz1 + 0.28, STONE, bevel=0.012)
    moulding(b, -ow / 2 - 0.1, ow / 2 + 0.1, oz0 - 0.09, [("step", 0.03, 0.05), ("ovolo", 0.04, 0.1),
                                                          ("step", 0.02, 0.11)])


def shop_walls(b, W, H, x0, x1, oz1):
    """Rusticated piers and lintel around a shop opening, grey hard-stone plinth."""
    ashlar(b, -W / 2, W / 2, 0.0, H, [(x0, x1, 0.0, oz1)], rust=True, course=0.45, joint=0.035, jd=0.05,
           bevel=0.03)
    for xa, xb in ((-W / 2, x0), (x1, W / 2)):
        b.box(xa, xb, -0.06, 0.05, 0.0, 0.62, PLINTH, bevel=0.015)
        moulding(b, xa, xb, 0.62, [("chamfer", 0.05, 0.03), ("step", 0.0, 0.0)], PLINTH, y0=-0.03)


def entrance(b, W, H):
    dw, dh = min(W - 0.9, 1.9), min(H - 0.45, 3.4)
    op = [(-dw / 2, dw / 2, 0.0, dh)]
    ashlar(b, -W / 2, W / 2, 0.0, H, op, rust=True, course=0.45, joint=0.035, jd=0.05, bevel=0.03)
    for xa, xb in ((-W / 2, -dw / 2), (dw / 2, W / 2)):
        b.box(xa, xb, -0.06, 0.05, 0.0, 0.62, PLINTH, bevel=0.015)
    # carriage door: two panelled leaves with raised panels, fanlight with an iron grille
    fan = dh - 0.95
    b.box(-dw / 2, dw / 2, 0.24, 0.3, 0.0, fan, "MI_Door_Wood")
    for x in (-dw / 2 + 0.1, 0.05):
        for z0, z1 in ((0.2, 0.95), (1.1, fan - 0.15)):
            b.box(x, x + dw / 2 - 0.15, 0.2, 0.24, z0, z1, "MI_Door_Wood", bevel=0.02)
    b.box(-0.025, 0.025, 0.18, 0.24, 0.0, fan, "MI_Door_Wood", bevel=0.008)
    b.box(-dw / 2, dw / 2, 0.17, 0.24, fan - 0.12, fan, "MI_Door_Wood", bevel=0.015)
    b.box(-dw / 2, dw / 2, 0.26, 0.27, fan, dh, "MI_Glass_Window")
    n = 7
    for k in range(n + 1):
        x = -dw / 2 + dw * k / n
        b.bar((x, 0.24, fan), (x, 0.24, dh), 0.012, IRON)
    b.ring(0.0, 0.235, fan + (dh - fan) / 2, 0.3, 0.012, IRON, n=16)
    jamb(b, -dw / 2, -1, 0.62, dh, width=0.22)
    jamb(b, dw / 2, 1, 0.62, dh, width=0.22)
    head(b, -dw / 2 - 0.22, dw / 2 + 0.22, dh, height=0.2)
    b.box(-0.18, 0.18, -0.12, 0.0, dh - 0.12, dh + 0.42, STONE, bevel=0.02)   # carved keystone
    for cx in (-dw / 2 - 0.11, dw / 2 + 0.11):
        console(b, cx, 0.2, dh + 0.55, 0.32, 0.5)
    moulding(b, -dw / 2 - 0.35, dw / 2 + 0.35, dh + 0.55, [("step", 0.04, 0.06), ("ovolo", 0.06, 0.14),
                                                           ("step", 0.07, 0.36), ("cyma", 0.06, 0.4)])


def blank(b, W, H):
    ashlar(b, -W / 2, W / 2, 0.0, H, phase=0.25)


def cornice(b, W):
    """Main cornice: architrave band, frieze, bed mouldings, dentils, corona on scrolled
    modillions, cymatium. About 1.05 m high, 0.66 m projection."""
    z = moulding(b, -W / 2, W / 2, 0.0, [("step", 0.08, 0.03), ("cyma", 0.04, 0.06), ("step", 0.03, 0.065)])
    b.box(-W / 2, W / 2, -0.02, 0.0, z, z + 0.28, STONE, bevel=0.006)                    # frieze
    z = moulding(b, -W / 2, W / 2, z + 0.28, [("step", 0.03, 0.03), ("ovolo", 0.05, 0.09),
                                              ("step", 0.11, 0.12)])                      # dentil band
    n = int(W / 0.12)
    for k in range(n):
        x = -W / 2 + (k + 0.5) * W / n
        b.box(x - 0.035, x + 0.035, -0.18, -0.1, z - 0.11, z - 0.02, STONE, bevel=0.006)
    zs = z + 0.2                                                                         # corona soffit
    b.box(-W / 2, W / 2, -0.15, 0.0, z, zs, STONE)
    m = max(1, int(round(W / 0.55)))
    for k in range(m):
        console(b, -W / 2 + (k + 0.5) * W / m, 0.12, zs, 0.56, 0.2, eye=False)
    moulding(b, -W / 2, W / 2, zs, [("step", 0.0, 0.62), ("step", 0.17, 0.62), ("step", 0.02, 0.64),
                                    ("cyma", 0.13, 0.7), ("step", 0.03, 0.7)])


def shop(b, W, H, closed=False, style="A"):
    """Shop bays: v0 shopfronts (kit.haus_shop) inside rusticated piers."""
    pil = 0.32
    oz1 = H - 0.95
    x0, x1 = -W / 2 + pil, W / 2 - pil
    # draw the v0 shopfront without its flat wall, then the v1 walls round it
    b.wall = lambda *a, **k: None
    try:
        kit.haus_shop(b, W, H, closed, style)
    finally:
        del b.wall
    shop_walls(b, W, H, x0, x1, oz1)


def defs():
    H = kit.HAUSSMANN
    W = H["bay"]
    return {
        "SM_Facade_Haussmann_Window": (H, "upper", lambda b: window(b, W, H["upper"])),
        "SM_Facade_Haussmann_Window_B": (H, "upper", lambda b: window(b, W, H["upper"], style="B")),
        "SM_Facade_Haussmann_Window_C": (H, "upper", lambda b: window(b, W, H["upper"], style="C")),
        "SM_Facade_Haussmann_WindowBalcony": (H, "upper", lambda b: window(b, W, H["upper"], True)),
        "SM_Facade_Haussmann_WindowBalcony_B": (H, "upper", lambda b: window(b, W, H["upper"], True, style="B")),
        "SM_Facade_Haussmann_WindowBalcony_C": (H, "upper", lambda b: window(b, W, H["upper"], True, style="C")),
        "SM_Facade_Haussmann_WindowAttic": (H, "upper", lambda b: window(b, W, H["upper"], small=True)),
        "SM_Facade_Haussmann_CourtWindow": (H, "upper", lambda b: window(b, W, H["upper"], court=True)),
        "SM_Facade_Haussmann_Entresol": (H, "entresol", lambda b: entresol(b, W, H["entresol"])),
        "SM_Facade_Haussmann_Shop": (H, "gf", lambda b: shop(b, W, H["gf"])),
        "SM_Facade_Haussmann_Shop_B": (H, "gf", lambda b: shop(b, W, H["gf"], style="B")),
        "SM_Facade_Haussmann_Shop_C": (H, "gf", lambda b: shop(b, W, H["gf"], style="C")),
        "SM_Facade_Haussmann_ShopClosed": (H, "gf", lambda b: shop(b, W, H["gf"], closed=True)),
        "SM_Facade_Haussmann_ShopClosed_B": (H, "gf", lambda b: shop(b, W, H["gf"], True, "B")),
        "SM_Facade_Haussmann_ShopClosed_C": (H, "gf", lambda b: shop(b, W, H["gf"], True, "C")),
        "SM_Facade_Haussmann_ShopShutter": (H, "gf", lambda b: shop(b, W, H["gf"], True, "S")),
        "SM_Facade_Haussmann_Entrance": (H, "gf", lambda b: entrance(b, W, H["gf"])),
        "SM_Facade_Haussmann_Blank": (H, "upper", lambda b: blank(b, W, H["upper"])),
        "SM_Facade_Haussmann_Cornice": (H, None, lambda b: cornice(b, W)),
    }
