"""What stands beyond the generated city, so that no view ends on the edge of the world.

The street map holds the street, its facade kit and the BD TOPO volumes within 600 m of the
axis (backdrop.py). Past them there was nothing: from the quay the far bank ended on a dark
blue band under the sky, like a sea (gate 76 video, 60 s), and the ends of the long streets
opened on the same band. This module lays out, without any geo data, what a town shows at
that distance:

  skirt    one flat ground far below the lowest terrain, to the real horizon: it takes the
           haze of the sky like any ground, so the horizon is a pale line and not a band
  blocks   rows of plain building volumes around the map, 720 m to 1.5 km from the street
           axis, facing it, five to eight storeys with a street gap between them; they take
           the procedural Lyon facade of the backdrop (apply_city_materials)
  roofs    on each of those blocks a tiled roof or a zinc mansard, stepped on the wide ones, and
           chimney stacks: from the street a far block is its roof line
  towers   the three towers of the Part-Dieu, east, as plain shafts: the skyline every view
           of Lyon across the Rhone has
  hills    Fourviere (west, wooded, 150 m) and the Croix-Rousse (north, built, 85 m) as two
           flattened domes; the blocks that fall on them stand on their slopes

  monuments  what a Lyonnais looks for on the skyline, as a few plain shapes: the basilica of
           Fourviere (nave, four towers, apse, the old chapel's tower) and the metal tower
           beside it, on the edge of the hill, where they stand over place Bellecour

  square setting  where a square runs past the end of the map's ground (place Bellecour, 150 m
           past it): the ground carried on, the fronts round the square, two rows of trees

Positions are in metres on the Unreal axes of the street map (x east, y SOUTH, origin of the
generated zone); heights are above the street. tools/city/unreal/place_horizon.py places it.
Everything is approximate on purpose: this is a far view, never a place to go.
"""

import math
import random

AXIS_HALF_M = 520.0            # the street axis runs about 520 m each side of the origin, along y
ROWS_M = (720.0, 900.0, 1150.0, 1500.0)    # distance of each row from the axis
BACKDROP_M = 600.0             # backdrop.py stops there: nothing of ours any nearer
SKIRT_HALF_M = 30000.0
# name, centre x, centre y, half-size x, half-size y, top above the street, wooded
HILLS = (
    # 150 m at the top so that the terrace of the basilica, on its east edge, is at its true 115 m
    ("Fourviere", -1250.0, 50.0, 620.0, 1350.0, 150.0, True),
    ("CroixRousse", -50.0, -2150.0, 1150.0, 820.0, 85.0, False),
)
# x, y, width, depth, top: Part-Dieu, about 1.6 km east of the rue de la Republique
TOWERS = (
    (1650.0, 60.0, 32.0, 32.0, 165.0),
    (1520.0, -170.0, 38.0, 28.0, 200.0),
    (1780.0, 260.0, 40.0, 30.0, 170.0),
    (1600.0, 420.0, 45.0, 30.0, 82.0),
    (1450.0, 180.0, 50.0, 26.0, 64.0),
)
# Fourviere, from the coordinates of the two monuments (WGS84 through Lambert-93, against the
# origin of the zone): within some 20 m, on a view from 800 m and more.
BASILICA_XY = (-845.0, 97.0)
BASILICA_YAW = 12.0            # the nave is turned a little from east; the apse looks over the town
IRON_TOWER_XY = (-887.0, -58.0)
# shape, along the nave, across it, width, depth, height, material (metres; a cylinder's width is its diameter)
BASILICA_PARTS = (
    ("Cube", 0.0, 0.0, 78.0, 30.0, 31.0, "stone"),          # nave
    ("Cylinder", 39.0, 0.0, 24.0, 24.0, 27.0, "stone"),     # apse, town side
    ("Cylinder", 33.0, -17.0, 9.0, 9.0, 48.0, "stone"),     # the four towers
    ("Cylinder", 33.0, 17.0, 9.0, 9.0, 48.0, "stone"),
    ("Cylinder", -35.0, -17.0, 9.0, 9.0, 48.0, "stone"),
    ("Cylinder", -35.0, 17.0, 9.0, 9.0, 48.0, "stone"),
    ("Cube", 22.0, 40.0, 8.0, 8.0, 33.0, "stone"),          # tower of the old chapel, south
)
IRON_TOWER = ("Cone", 0.0, 0.0, 15.0, 15.0, 86.0, "iron")
MONUMENT_FOOT_M = 8.0          # how far a monument goes into the hill under it
COLOURS = 4                    # block colours built by apply_city_materials (horizon_colours)
TOWER_COLOUR = 3               # the grey of the modern typology
WOODED_KEEP = 0.35             # share of the blocks kept on a wooded hill
# Gate 99, 76 s: from place Bellecour, Fourviere carried slabs of seven storeys a hundred metres
# long, grey against the sky, where the hill has houses among its trees and, on its crest, the
# basilica and the metal tower. On a wooded hill a block is a large house, and none stands on
# the crest.
WOODED_WIDTH_M = (18.0, 34.0)
WOODED_STOREYS = (3, 4)
WOODED_CREST = 0.8             # no block above this share of the hill's height


def hill_at(x, y):
    """(height above the street in metres, hill) of the highest hill over this point, else (0, None)."""
    best, which = 0.0, None
    for hill in HILLS:
        _, cx, cy, rx, ry, top, _ = hill
        r2 = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
        if r2 < 1.0:
            h = top * math.sqrt(1.0 - r2)
            if h > best:
                best, which = h, hill
    return best, which


def axis_distance(x, y):
    """Distance to the street axis, a segment along y."""
    return math.hypot(x, y - max(-AXIS_HALF_M, min(AXIS_HALF_M, y)))


def row_points(distance, pitch):
    """Points every `pitch` metres on the line at `distance` from the axis (a stadium), with the
    direction of the line there (degrees, Unreal yaw)."""
    straight = 2.0 * AXIS_HALF_M
    turn = math.pi * distance
    total = 2.0 * straight + 2.0 * turn
    out = []
    for i in range(max(1, int(total / pitch))):
        s = i * total / max(1, int(total / pitch))
        if s < straight:                                   # east side, going south
            out.append((distance, -AXIS_HALF_M + s, 90.0))
        elif s < straight + turn:                          # south end, turning west
            a = (s - straight) / distance
            out.append((distance * math.cos(a), AXIS_HALF_M + distance * math.sin(a), 90.0 + math.degrees(a)))
        elif s < 2.0 * straight + turn:                    # west side, going north
            out.append((-distance, AXIS_HALF_M - (s - straight - turn), 270.0))
        else:                                              # north end, turning east
            a = (s - 2.0 * straight - turn) / distance
            out.append((-distance * math.cos(a), -AXIS_HALF_M - distance * math.sin(a), 270.0 + math.degrees(a)))
    return out


def monuments():
    """The skyline's monuments as plain shapes: [{"name", "shape", "x", "y", "yaw", "width", "depth",
    "ground", "top", "material"}], ground and top above the street."""
    out = []
    a = math.radians(BASILICA_YAW)
    for (cx, cy), yaw, parts, name in ((BASILICA_XY, BASILICA_YAW, BASILICA_PARTS, "Basilica"),
                                       (IRON_TOWER_XY, 0.0, (IRON_TOWER,), "IronTower")):
        ground, _ = hill_at(cx, cy)               # one level for the whole monument: it stands on a terrace
        for i, (shape, along, across, width, depth, height, material) in enumerate(parts):
            if yaw:
                dx, dy = along * math.cos(a) - across * math.sin(a), along * math.sin(a) + across * math.cos(a)
            else:
                dx, dy = along, across
            out.append({"name": "%s_%d" % (name, i), "shape": shape, "x": round(cx + dx, 1), "y": round(cy + dy, 1),
                        "yaw": yaw, "width": width, "depth": depth, "ground": round(ground, 1),
                        "top": round(ground + height, 1), "material": material})
    return out


# --- the setting of a square that runs past the end of the map -------------------------------
# The ground of the map is a rectangle; place Bellecour runs 150 m past its south edge. Seen from
# the street along its north side the square was 30 to 65 m of ground, then the edge of the world.
SQUARE_STREET_M = 16.0             # between the outline of the square and the fronts round it
SQUARE_FRONT_DEPTH_M = 14.0
SQUARE_FRONT_CLEAR_M = 4.0         # a front stands at least this far from the map's ground and buildings
SQUARE_FRONT_M = (21.0, 24.5)      # eave of the fronts of a great square: five storeys and an attic
SQUARE_APRON_PAD_M = 140.0         # ground laid this far round the square, past the map
SQUARE_APRON_LAP_M = 2.0           # and this far under the edge of the map's own ground
SQUARE_TREE_ROWS_M = (9.0, 19.0)   # rows along the far long side, this far inside the outline
SQUARE_TREE_PITCH_M = 9.0
SQUARE_TREE_END_M = 14.0           # no tree this close to the ends of the rows


def convex_hull(points):
    """Andrew's monotone chain; counter-clockwise, no repeated point."""
    pts = sorted(set((float(x), float(y)) for x, y in points))
    if len(pts) < 3:
        return pts

    def half(seq):
        out = []
        for p in seq:
            while len(out) >= 2 and (out[-1][0] - out[-2][0]) * (p[1] - out[-2][1]) - \
                    (out[-1][1] - out[-2][1]) * (p[0] - out[-2][0]) <= 0.0:
                out.pop()
            out.append(p)
        return out[:-1]

    return half(pts) + half(reversed(pts))


def oriented_box(ring):
    """Smallest rectangle round an outline: (cx, cy, ux, uy, half_u, half_v), u its long side."""
    hull = convex_hull(ring)
    best = None
    for i, a in enumerate(hull):
        b = hull[(i + 1) % len(hull)]
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        if length <= 0.0:
            continue
        ux, uy = (b[0] - a[0]) / length, (b[1] - a[1]) / length
        us = [p[0] * ux + p[1] * uy for p in hull]
        vs = [-p[0] * uy + p[1] * ux for p in hull]
        du, dv = max(us) - min(us), max(vs) - min(vs)
        if best is None or du * dv < best[0]:
            cu, cv = 0.5 * (max(us) + min(us)), 0.5 * (max(vs) + min(vs))
            best = (du * dv, cu * ux - cv * uy, cu * uy + cv * ux, ux, uy, 0.5 * du, 0.5 * dv)
    _, cx, cy, ux, uy, hu, hv = best
    if hv > hu:                                    # u is the long side
        ux, uy, hu, hv = -uy, ux, hv, hu
    return cx, cy, ux, uy, hu, hv


def _inside(box, x, y, pad=0.0):
    return box[0] - pad <= x <= box[2] + pad and box[1] - pad <= y <= box[3] + pad


def square_setting(ring, ground_box, seed=7, built=()):
    """What a square needs where it runs past the ground of the map (ring: its outline, ground_box:
    (x0, y0, x1, y1) of the map's ground, built: the boxes of the real building volumes that stand
    past that ground, metres on the Unreal axes):

      aprons  flat ground past the edge of the map, round the square: axis-aligned rectangles
              {"x", "y", "width", "depth"} that lap under the map's ground; they take the ground
              material, which draws the square's own surface in world space
      fronts  the buildings round the square, a street back from its outline, as plain blocks in
              the format of layout()'s (kind "front"; "top" is above the square); only those that
              stand wholly past the map's ground and clear of the real volumes: no front on a
              building, a street or a lane of the map
      trees   two rows along its far long side: {"x", "y", "yaw", "scale"}

    A square that lies wholly on the map's ground gets none of them."""
    rng = random.Random(seed)
    cx, cy, ux, uy, hu, hv = oriented_box(ring)
    vx, vy = -uy, ux
    reach = SQUARE_STREET_M + SQUARE_FRONT_DEPTH_M
    corners = [(cx + su * (hu + reach) * ux + sv * (hv + reach) * vx, cy + su * (hu + reach) * uy + sv * (hv + reach) * vy)
               for su in (-1.0, 1.0) for sv in (-1.0, 1.0)]
    if all(_inside(ground_box, x, y) for x, y in corners):
        return {"aprons": [], "fronts": [], "trees": [], "box": [round(v, 2) for v in (cx, cy, ux, uy, hu, hv)]}

    # ground: the box round the square and its fronts, padded, less the ground of the map
    x0 = min(p[0] for p in corners) - SQUARE_APRON_PAD_M
    x1 = max(p[0] for p in corners) + SQUARE_APRON_PAD_M
    y0 = min(p[1] for p in corners) - SQUARE_APRON_PAD_M
    y1 = max(p[1] for p in corners) + SQUARE_APRON_PAD_M
    gx0, gy0, gx1, gy1 = ground_box
    lap = SQUARE_APRON_LAP_M
    pieces = [(x0, y0, x1, min(y1, gy0 + lap)),                                   # north of the map
              (x0, max(y0, gy1 - lap), x1, y1),                                   # south of it
              (x0, max(y0, gy0 + lap), min(x1, gx0 + lap), min(y1, gy1 - lap)),   # west, between the two
              (max(x0, gx1 - lap), max(y0, gy0 + lap), x1, min(y1, gy1 - lap))]   # east
    aprons = [{"x": round(0.5 * (a + c), 2), "y": round(0.5 * (b + d), 2), "width": round(c - a, 2),
               "depth": round(d - b, 2)} for a, b, c, d in pieces if c - a > 1.0 and d - b > 1.0]

    # fronts: a row along each side, a street back from the outline, only where the map has no ground
    fronts = []
    for nx, ny, tx, ty, half_side, half_out in ((vx, vy, ux, uy, hu, hv), (-vx, -vy, ux, uy, hu, hv),
                                                (ux, uy, vx, vy, hv, hu), (-ux, -uy, vx, vy, hv, hu)):
        back = half_out + SQUARE_STREET_M + 0.5 * SQUARE_FRONT_DEPTH_M
        along, end = -(half_side + SQUARE_STREET_M), half_side + SQUARE_STREET_M
        since_street = 0.0
        while along < end - 12.0:
            width = min(rng.uniform(26.0, 44.0), end - along)
            top = rng.uniform(*SQUARE_FRONT_M)
            colour = rng.randrange(2)                      # the two stone colours
            mid = along + 0.5 * width
            x, y = cx + nx * back + tx * mid, cy + ny * back + ty * mid
            hw, hd = 0.5 * width, 0.5 * SQUARE_FRONT_DEPTH_M
            footprint = [(x + a * hw * tx + b * hd * nx, y + a * hw * ty + b * hd * ny)
                         for a in (-1.0, 1.0) for b in (-1.0, 1.0)]
            fx0, fx1 = min(p[0] for p in footprint), max(p[0] for p in footprint)
            fy0, fy1 = min(p[1] for p in footprint), max(p[1] for p in footprint)
            # wholly past the map's ground (its streets and lanes) and clear of every real building
            if not any(fx0 < b[2] + SQUARE_FRONT_CLEAR_M and fx1 > b[0] - SQUARE_FRONT_CLEAR_M and
                       fy0 < b[3] + SQUARE_FRONT_CLEAR_M and fy1 > b[1] - SQUARE_FRONT_CLEAR_M
                       for b in [ground_box] + list(built)):
                fronts.append({"x": round(x, 1), "y": round(y, 1),
                               "yaw": round(math.degrees(math.atan2(ty, tx)) % 360.0, 1),
                               "width": round(width, 1), "depth": SQUARE_FRONT_DEPTH_M, "ground": 0.0,
                               "top": round(top, 1), "colour": colour, "kind": "front"})
            along += width
            since_street += width
            if since_street > 85.0:                        # a street opens on the square
                along += rng.uniform(9.0, 13.0)
                since_street = 0.0

    # trees: along the long side farther from the map (the side the camera looks at across the square)
    far = 1.0 if _far_from(ground_box, cx + vx * hv, cy + vy * hv) >= _far_from(ground_box, cx - vx * hv, cy - vy * hv) \
        else -1.0
    trees = []
    count = int((2.0 * hu - 2.0 * SQUARE_TREE_END_M) / SQUARE_TREE_PITCH_M)
    for row, inset in enumerate(SQUARE_TREE_ROWS_M):
        for i in range(count + 1):
            u = -hu + SQUARE_TREE_END_M + i * SQUARE_TREE_PITCH_M + (0.5 * SQUARE_TREE_PITCH_M if row % 2 else 0.0)
            if u > hu - SQUARE_TREE_END_M:
                continue
            v = far * (hv - inset)
            trees.append({"x": round(cx + ux * u + vx * v + rng.uniform(-0.4, 0.4), 2),
                          "y": round(cy + uy * u + vy * v + rng.uniform(-0.4, 0.4), 2),
                          "yaw": round(rng.uniform(0.0, 360.0), 1), "scale": round(rng.uniform(0.95, 1.25), 2)})
    return {"aprons": aprons, "fronts": fronts, "trees": trees,
            "box": [round(v, 2) for v in (cx, cy, ux, uy, hu, hv)]}


def _far_from(box, x, y):
    """How far outside a box a point is (0 inside)."""
    return math.hypot(max(box[0] - x, 0.0, x - box[2]), max(box[1] - y, 0.0, y - box[3]))


def keep_clear(items, line, clear_m):
    """The items ({"x", "y"}) at least clear_m from every point of a driven line [(x, y)], metres.
    The line's points are a few metres apart: the half of that step is added to the distance."""
    if not line:
        return list(items)
    gaps = sorted(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(line, line[1:]))
    step = gaps[len(gaps) // 2] if gaps else 0.0          # the usual step: lanes follow one another in the list
    need = clear_m + 0.5 * step
    cell = max(need, 1.0)
    grid = {}
    for x, y in line:
        grid.setdefault((int(x // cell), int(y // cell)), []).append((x, y))
    out = []
    for it in items:
        gx, gy = int(it["x"] // cell), int(it["y"] // cell)
        near = (p for i in (gx - 1, gx, gx + 1) for j in (gy - 1, gy, gy + 1) for p in grid.get((i, j), ()))
        if all(math.hypot(px - it["x"], py - it["y"]) >= need for px, py in near):
            out.append(it)
    return out


def circuit_line(text):
    """Every point of the driven line of a flight circuit file (its line_ue_cm column), in metres."""
    out = []
    for row in text.splitlines():
        if row.startswith("#") or ";" not in row:
            continue
        for cell in row.split(","):
            if ";" in cell:
                for pair in cell.split(";"):
                    xy = pair.split()
                    if len(xy) == 2:
                        try:
                            out.append((float(xy[0]) / 100.0, float(xy[1]) / 100.0))
                        except ValueError:
                            pass
    return out


ROOF_TILE_SHARE = 0.6           # Lyon from afar is tile; the rest zinc, the mansards of the centre
ROOF_SEED = 1009                # the roofs have their own draw: the blocks stay where they were


def roofs(blocks, seed=7):
    """What tops the far blocks, as boxes: {"roofs": [...], "chimneys": [...]}, each {x, y, yaw,
    width, depth, bottom, top} in metres, a roof with its "kind" ("tile" or "zinc").

    Gate 86, long take, 120 s: at the end of the quay the far blocks are flat grey boxes under
    one straight line. A town's skyline is its roofs: each block gets a tiled roof (a low slab
    that overhangs the wall, the eave line) or a zinc mansard (a storey set back), a wide block
    one that steps up over part of its length, and chimney stacks on its party walls. Towers
    keep their flat top."""
    rng = random.Random(seed * ROOF_SEED)
    out = {"roofs": [], "chimneys": []}
    for b in blocks:
        if b["kind"] != "block":
            continue
        kind = "tile" if rng.random() < ROOF_TILE_SHARE else "zinc"
        grow, tall = (0.5, rng.uniform(2.2, 3.0)) if kind == "tile" else (-0.9, rng.uniform(3.0, 3.8))
        cos, sin = math.cos(math.radians(b["yaw"])), math.sin(math.radians(b["yaw"]))

        def at(lx, ly, b=b, cos=cos, sin=sin):       # along the block, across it -> the map's x, y
            return round(b["x"] + lx * cos - ly * sin, 1), round(b["y"] + lx * sin + ly * cos, 1)

        # one roof, or two when the block is wide: two houses of different heights side by side
        cut = rng.uniform(0.35, 0.65) if b["width"] > 60.0 else 1.0
        step = rng.choice((1.6, 3.2)) if cut < 1.0 else 0.0
        spans = [(-0.5, cut - 0.5, 0.0)] + ([(cut - 0.5, 0.5, step)] if cut < 1.0 else [])
        if rng.random() < 0.5:
            spans = [(-hi, -lo, up) for lo, hi, up in spans]
        ridge = []
        for lo, hi, up in spans:
            x, y = at(0.5 * (lo + hi) * b["width"], 0.0)
            top = b["top"] + up + tall
            out["roofs"].append({"x": x, "y": y, "yaw": b["yaw"], "kind": kind,
                                 "width": round((hi - lo) * b["width"] + 2.0 * grow, 1),
                                 "depth": round(b["depth"] + 2.0 * grow, 1),
                                 "bottom": b["top"] - 0.2, "top": round(top, 1)})
            ridge.append((lo, hi, top))
        for k in range(max(2, int(round(b["width"] / 13.0)))):
            f = (k + rng.uniform(0.25, 0.75)) / max(2, int(round(b["width"] / 13.0))) - 0.5
            top = next(t for lo, hi, t in ridge if lo <= f <= hi)
            x, y = at(f * b["width"], rng.choice((-0.22, 0.22)) * b["depth"])
            out["chimneys"].append({"x": x, "y": y, "yaw": b["yaw"], "width": round(rng.uniform(1.2, 1.8), 1),
                                    "depth": round(rng.uniform(2.4, 3.6), 1), "bottom": round(top - 0.5, 1),
                                    "top": round(top + rng.uniform(2.0, 3.4), 1)})
    return out


def layout(seed=7):
    """The whole far view: {"skirt": {...}, "hills": [...], "blocks": [...], "monuments": [...]}, the
    same for a seed."""
    rng = random.Random(seed)
    blocks = []
    for row, distance in enumerate(ROWS_M):
        pitch = 95.0 + 25.0 * row                          # coarser as it gets farther
        for x, y, yaw in row_points(distance, pitch):
            width = pitch * rng.uniform(0.55, 0.92)        # the rest is the street between two blocks
            depth = rng.uniform(22.0, 44.0)
            storeys = rng.choice((5, 5, 6, 6, 6, 7, 7, 8))
            top = 4.5 + 3.2 * storeys + rng.uniform(0.0, 2.5)
            colour = rng.randrange(COLOURS)
            x += rng.uniform(-12.0, 12.0)
            y += rng.uniform(-12.0, 12.0)
            ground, hill = hill_at(x, y)
            keep = rng.random()                            # drawn for every block: one seed, one layout
            if hill is not None and hill[6]:
                if keep > WOODED_KEEP or ground > WOODED_CREST * hill[5]:
                    continue
                lo, hi = WOODED_WIDTH_M                      # no new draw: the other blocks stay put
                width = lo + (hi - lo) * keep / WOODED_KEEP
                depth = min(depth, 0.8 * width)
                top = 4.0 + 3.0 * WOODED_STOREYS[storeys % 2] + (top - 4.5 - 3.2 * storeys)
            blocks.append({"x": round(x, 1), "y": round(y, 1), "yaw": round(yaw % 360.0, 1),
                           "width": round(width, 1), "depth": round(depth, 1), "ground": round(ground, 1),
                           "top": round(ground + top, 1), "colour": colour, "kind": "block"})
    for x, y, width, depth, top in TOWERS:
        blocks.append({"x": x, "y": y, "yaw": 0.0, "width": width, "depth": depth, "ground": 0.0,
                       "top": top, "colour": TOWER_COLOUR, "kind": "tower"})
    hills = [{"name": name, "x": cx, "y": cy, "half_x": rx, "half_y": ry, "top": top, "wooded": wooded}
             for name, cx, cy, rx, ry, top, wooded in HILLS]
    return {"skirt": {"half": SKIRT_HALF_M}, "hills": hills, "blocks": blocks, "monuments": monuments(),
            "roofs": roofs(blocks, seed)}
