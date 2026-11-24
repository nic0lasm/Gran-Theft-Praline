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
  towers   the three towers of the Part-Dieu, east, as plain shafts: the skyline every view
           of Lyon across the Rhone has
  hills    Fourviere (west, wooded, 150 m) and the Croix-Rousse (north, built, 85 m) as two
           flattened domes; the blocks that fall on them stand on their slopes

  monuments  what a Lyonnais looks for on the skyline, as a few plain shapes: the basilica of
           Fourviere (nave, four towers, apse, the old chapel's tower) and the metal tower
           beside it, on the edge of the hill, where they stand over place Bellecour

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
# Fourviere, from the coordinates of the two monuments (WGS84) against the place de la Republique
# of the map: within some 30 m, on a view from 800 m and more.
BASILICA_XY = (-853.0, 29.0)
BASILICA_YAW = 12.0            # the nave is turned a little from east; the apse looks over the town
IRON_TOWER_XY = (-892.0, -138.0)
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
            if hill is not None and hill[6] and keep > WOODED_KEEP:
                continue
            blocks.append({"x": round(x, 1), "y": round(y, 1), "yaw": round(yaw % 360.0, 1),
                           "width": round(width, 1), "depth": round(depth, 1), "ground": round(ground, 1),
                           "top": round(ground + top, 1), "colour": colour, "kind": "block"})
    for x, y, width, depth, top in TOWERS:
        blocks.append({"x": x, "y": y, "yaw": 0.0, "width": width, "depth": depth, "ground": 0.0,
                       "top": top, "colour": TOWER_COLOUR, "kind": "tower"})
    hills = [{"name": name, "x": cx, "y": cy, "half_x": rx, "half_y": ry, "top": top, "wooded": wooded}
             for name, cx, cy, rx, ry, top, wooded in HILLS]
    return {"skirt": {"half": SKIRT_HALF_M}, "hills": hills, "blocks": blocks, "monuments": monuments()}
