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
  hills    Fourviere (west, wooded, 125 m) and the Croix-Rousse (north, built, 85 m) as two
           flattened domes; the blocks that fall on them stand on their slopes

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
    ("Fourviere", -1250.0, 50.0, 560.0, 1350.0, 125.0, True),
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


def layout(seed=7):
    """The whole far view: {"skirt": {...}, "hills": [...], "blocks": [...]}, the same for a seed."""
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
    return {"skirt": {"half": SKIRT_HALF_M}, "hills": hills, "blocks": blocks}
