"""Room for the driving line of the flight circuit: what stands in the town, and a line eased among it.

flight_circuit.py makes the line a flight drives from the lanes of traffic.py: road centre lines pushed aside,
straightened and rounded. Three things it could not know (gate 86, long take):
- right-hand corners came out with a radius of 2 to 4 m (rounded by 0.9 m, "the inside is the kerb"): no car turns
  that tight, the stolen car ran wide by 11 m at the corner of the quay, or went round in circles at full lock;
- the line ran 1.6 m from a building on the place de l'Hopital: the car stopped against it;
- the line ran through the carrousel of the place de la Republique, a stand-in put where the lanes cross the place.

Here: the walls of the town read from the processed data of the zone (buildings.geojson, landmarks.json,
water.geojson; local metres, x east, y north), given in the lanes' own frame (Unreal's: x east, y SOUTH, metres);
a line taken round what stands on it (go_round); and a line eased (ease): as little bend as the room allows,
within a few metres of the line it was given at the corners, almost on it along the straights, on the carriageway
(carriageway.geojson) and never nearer a wall than asked. numpy and shapely only to ease: the check of a line
needs neither.
"""

import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

CLEAR_WALL_M = 2.8      # the line keeps this far from a wall (the prep step "lanes" clears 2.5 m round the line)
CLEAR_ROUND_M = 2.6     # ... and this far from something it goes round
ROUND_MAX_M = 12.0      # something the line can go round fits in a circle of this radius
ROUND_HALF_M = 36.0     # the way round begins and ends this far before and after it, along the line
CORNER_BASE_M = 12.0    # a bend is told by the headings this far before and after a point
CORNER_DEG = 25.0       # ... when they differ by this much
SLACK_CORNER_M = 4.0    # at a bend the eased line may be this far from the line given
SLACK_STRAIGHT_M = 0.3  # ... and along a straight this far
CORNER_FULL_M = 18.0    # the whole slack this far from a bend
CORNER_FADE_M = 30.0    # ... none of it from here on
WALL_SPREAD_M = 24.0    # a line too near a wall leaves it and comes back over this length, before and after
WALL_MARGIN_M = 0.4     # ... with this much more slack than the distance it lacks
KERB_M = 1.2            # the eased line keeps this far inside the kerbs, where the line given is on the carriageway
KERB_FAST_M = 4.25      # where the flight is asked FAST_KMH or more: a lane of 3 m between the car and the kerb
FAST_KMH = 90.0
KERB_REACH_M = 10.0     # the kerbs are looked for this far across the line
KERB_SPREAD_M = 8.0     # the line moves away from a kerb, and back, over about three times this
CELL_M = 25.0
THROUGH_M = 0.5         # a line this near a wall, or inside it, runs through it
NORMAL_BASE_M = 9.0     # the normal of the line given is told over this length before and after a point
SCAN_M = 0.25           # the room across the line is looked for by steps of this
STAY = 0.004            # what a metre off the line given weighs against bending
EASE_PASSES = 400


def _ring(points):
    ring = [(float(p[0]), -float(p[1])) for p in points]
    if len(ring) > 2 and ring[0] != ring[-1]:
        ring.append(ring[0])
    return ring


def _wall(name, ring, kind):
    xs = [p[0] for p in ring]
    ys = [p[1] for p in ring]
    cx, cy = sum(xs[:-1]) / max(len(xs) - 1, 1), sum(ys[:-1]) / max(len(ys) - 1, 1)
    return {"name": name, "kind": kind, "ring": ring, "box": (min(xs), min(ys), max(xs), max(ys)),
            "centre": (cx, cy), "radius": max(math.hypot(x - cx, y - cy) for x, y in ring)}


class Town(list):
    """The walls of a zone, and where they are on a grid of CELL_M: near() looks only at the cells round a point."""

    def __init__(self, walls=()):
        super().__init__(walls)
        self.cells = {}
        for wall in self:
            box = wall["box"]
            for i in range(int(math.floor(box[0] / CELL_M)), int(math.floor(box[2] / CELL_M)) + 1):
                for j in range(int(math.floor(box[1] / CELL_M)), int(math.floor(box[3] / CELL_M)) + 1):
                    self.cells.setdefault((i, j), []).append(wall)


def _moved(wall):
    """The landmark where the map has it: the prep step "lanes" moves the actor of a landmark the frozen base map holds
    in the wrong place (lane_clear.LANDMARK_MOVES, by the label of its actor: SM_Landmark_ and its key in capitals),
    and data made before the move still have it where it was."""
    import lane_clear

    label = "SM_Landmark_" + "".join(part.capitalize() for part in wall["name"].split("-"))
    box = wall["box"]
    move = lane_clear.landmark_move(label, ((box[0] + box[2]) * 50.0, (box[1] + box[3]) * 50.0))
    if move is None:
        return wall
    return _wall(wall["name"], [(x + move[0] / 100.0, y + move[1] / 100.0) for x, y in wall["ring"]], wall["kind"])


def read_town(folder):
    """The walls of the zone: [{name, kind (building, landmark, water), ring [(x, y)] closed, box, centre, radius}]."""
    walls = []

    def rings(geometry):
        if geometry["type"] == "Polygon":
            return [geometry["coordinates"][0]]
        if geometry["type"] == "MultiPolygon":
            return [polygon[0] for polygon in geometry["coordinates"]]
        return []

    for name, kind in (("buildings.geojson", "building"), ("water.geojson", "water")):
        path = os.path.join(folder, name)
        if not os.path.exists(path):
            continue
        with open(path, encoding="utf-8") as f:
            for index, feature in enumerate(json.load(f)["features"]):
                for ring in rings(feature["geometry"]):
                    if len(ring) >= 4:
                        walls.append(_wall(str(feature.get("properties", {}).get("id", "%s %d" % (kind, index))), _ring(ring), kind))
    path = os.path.join(folder, "landmarks.json")
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            for landmark in json.load(f).get("landmarks", []):
                if landmark.get("kind") == "square":
                    continue    # a place is ground, not a wall
                for ring in landmark.get("polygon_xy") or []:
                    if len(ring) >= 4:
                        walls.append(_moved(_wall(landmark.get("key", "landmark"), _ring(ring), "landmark")))
    return Town(walls)


def has_town(folder):
    return bool(folder) and os.path.exists(os.path.join(folder, "buildings.geojson"))


def to_segment(p, a, b):
    """(distance, nearest point) from p to the segment a b."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    l2 = dx * dx + dy * dy
    t = min(max(((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l2, 0.0), 1.0) if l2 > 0.0 else 0.0
    q = (a[0] + dx * t, a[1] + dy * t)
    return math.hypot(p[0] - q[0], p[1] - q[1]), q


def inside(p, ring):
    x, y = p
    within = False
    for i in range(len(ring) - 1):
        x1, y1 = ring[i]
        x2, y2 = ring[i + 1]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            within = not within
    return within


def to_wall(p, wall):
    """(distance to the wall's outline, negative inside it; nearest point of the outline)."""
    best, at = float("inf"), None
    ring = wall["ring"]
    for i in range(len(ring) - 1):
        d, q = to_segment(p, ring[i], ring[i + 1])
        if d < best:
            best, at = d, q
    return (-best if inside(p, ring) else best), at


def near(walls, p, reach):
    """The walls whose box is within reach of p."""
    if isinstance(walls, Town):
        seen, found = set(), []
        for i in range(int(math.floor((p[0] - reach) / CELL_M)), int(math.floor((p[0] + reach) / CELL_M)) + 1):
            for j in range(int(math.floor((p[1] - reach) / CELL_M)), int(math.floor((p[1] + reach) / CELL_M)) + 1):
                for w in walls.cells.get((i, j), ()):
                    if id(w) not in seen:
                        seen.add(id(w))
                        if w["box"][0] - reach <= p[0] <= w["box"][2] + reach and w["box"][1] - reach <= p[1] <= w["box"][3] + reach:
                            found.append(w)
        return found
    return [w for w in walls if w["box"][0] - reach <= p[0] <= w["box"][2] + reach and w["box"][1] - reach <= p[1] <= w["box"][3] + reach]


def room(points, walls):
    """(least distance from the points and the middles of their segments to a wall, the point, the wall's name)."""
    best = (float("inf"), None, "")
    for i, p in enumerate(points):
        probes = [p]
        if i + 1 < len(points):
            q = points[i + 1]
            probes.append(((p[0] + q[0]) / 2.0, (p[1] + q[1]) / 2.0))
        for probe in probes:
            for wall in near(walls, probe, 12.0):
                d, _ = to_wall(probe, wall)
                if d < best[0]:
                    best = (d, probe, wall["name"])
    return best


def radius_at(a, b, c):
    area = abs((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1])) / 2.0
    if area < 1e-9:
        return float("inf")
    return math.dist(a, b) * math.dist(b, c) * math.dist(a, c) / (4.0 * area)


def least_radius(points, closed=False):
    """(the least radius of the circle through three points in a row 2 apart, the middle point)."""
    count = len(points)
    best = (float("inf"), None)
    for k in range(count):
        if not closed and not 2 <= k < count - 2:
            continue
        r = radius_at(points[(k - 2) % count], points[k], points[(k + 2) % count])
        if r < best[0]:
            best = (r, points[k])
    return best


def _heading(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def _wrap(angle):
    return (angle + 180.0) % 360.0 - 180.0


def go_round(curve, walls, closed=True):
    """Takes curve ([[x, y]], in place) round each small wall it runs through or by: a hump over ROUND_HALF_M before and after
    it, on the side where nothing else stands. Returns [(wall name, side, height of the hump m, first and last index)]."""
    count = len(curve)
    done = []
    through = {}
    for k in range(count):
        for wall in near(walls, curve[k], THROUGH_M):
            if wall["radius"] <= ROUND_MAX_M and id(wall) not in through and to_wall(curve[k], wall)[0] < THROUGH_M:
                through[id(wall)] = wall
    for wall in through.values():
        cx, cy = wall["centre"]
        keep = wall["radius"] + CLEAR_ROUND_M
        k0 = min(range(count), key=lambda k: math.hypot(curve[k][0] - cx, curve[k][1] - cy))
        a = curve[(k0 - 2) % count] if closed or k0 >= 2 else curve[0]
        b = curve[(k0 + 2) % count] if closed or k0 + 2 < count else curve[-1]
        length = math.dist(a, b)
        ux, uy = (b[0] - a[0]) / length, (b[1] - a[1]) / length
        nx, ny = -uy, ux
        # Where the centre is beside the line, along that normal.
        beside = (cx - curve[k0][0]) * nx + (cy - curve[k0][1]) * ny
        choices = []
        for side in (-1.0 if beside > 0.0 else 1.0, 1.0 if beside > 0.0 else -1.0):
            height = keep + side * beside
            moved = {}
            for k in range(count):
                along = (curve[k][0] - cx) * ux + (curve[k][1] - cy) * uy
                across = (curve[k][0] - cx) * nx + (curve[k][1] - cy) * ny
                if abs(along) < ROUND_HALF_M and abs(across - (-beside)) < 2.0 * ROUND_MAX_M:
                    lift = side * height * 0.5 * (1.0 + math.cos(math.pi * along / ROUND_HALF_M))
                    moved[k] = [curve[k][0] + nx * lift, curve[k][1] + ny * lift]
            free = min([to_wall(p, w)[0] for p in moved.values() for w in near(walls, p, 8.0) if w is not wall] or [float("inf")])
            choices.append((free, side, height, moved))
            if free >= CLEAR_WALL_M:
                break
        free, side, height, moved = max(choices, key=lambda c: c[0]) if choices[0][0] < CLEAR_WALL_M else choices[0]
        for k, p in moved.items():
            curve[k] = p
        done.append((wall["name"], "left" if side < 0.0 else "right", height, min(moved), max(moved), free))
    return done


def slack_along(ref, step, closed, fixed=()):
    """How far from ref each point of the eased line may be: SLACK_CORNER_M round the bends, SLACK_STRAIGHT_M along the
    straights, nothing for the indices in fixed."""
    count = len(ref)

    def at(k):
        return k % count if closed else min(max(k, 0), count - 1)

    base = max(int(round(CORNER_BASE_M / step)), 2)
    bends = [k for k in range(count)
             if abs(_wrap(_heading(ref[k], ref[at(k + base)]) - _heading(ref[at(k - base)], ref[k]))) >= CORNER_DEG
             and (closed or base <= k < count - base)]
    slack = [SLACK_STRAIGHT_M] * count
    full, fade = int(round(CORNER_FULL_M / step)), int(round(CORNER_FADE_M / step))
    for k in bends:
        for d in range(-fade, fade + 1):
            if not closed and not 0 <= k + d < count:
                continue
            share = 1.0 - min(max((abs(d) - full) / float(max(fade - full, 1)), 0.0), 1.0)
            index = at(k + d)
            slack[index] = max(slack[index], SLACK_STRAIGHT_M + (SLACK_CORNER_M - SLACK_STRAIGHT_M) * share)
    for k in fixed:
        slack[k] = 0.0
    return slack


def slack_for_walls(ref, slack, walls, step, closed, clear_of):
    """Where the line given is nearer a wall than it may be, the slack it takes to leave it, spread WALL_SPREAD_M along
    the line so that it leaves and comes back without a kink. clear_of(wall): the distance to keep from that wall."""
    count = len(ref)
    spread = int(round(WALL_SPREAD_M / step))
    out = slack[:]
    for k in range(count):
        if slack[k] <= 0.0:
            continue
        need = max([clear_of(wall) - to_wall(ref[k], wall)[0] for wall in near(walls, ref[k], CLEAR_WALL_M + 1.0)] or [0.0])
        if need <= 0.0:
            continue
        for d in range(-spread, spread + 1):
            if not closed and not 0 <= k + d < count:
                continue
            index = (k + d) % count
            if slack[index] > 0.0:
                out[index] = max(out[index], (need + WALL_MARGIN_M) * 0.5 * (1.0 + math.cos(math.pi * d / float(spread + 1))) + SLACK_STRAIGHT_M)
    return out


class Roads:
    """The carriageways of the zone: contains(point) says a point is on them KERB_M inside the kerbs, on(point) that
    it is on them at all."""

    def __init__(self, whole):
        from shapely.prepared import prep

        self.whole = whole
        self._kerbed = prep(whole.buffer(-KERB_M))
        self._whole = prep(whole)

    def contains(self, point):
        return self._kerbed.contains(point)

    def on(self, point):
        return self._whole.contains(point)


def read_roads(folder):
    """The carriageways of the zone (carriageway.geojson, tools/city/carriageway.py) as one shape in the lanes' frame
    (Roads); None without the file."""
    path = os.path.join(folder, "carriageway.geojson")
    if not os.path.exists(path):
        return None
    from shapely.geometry import shape
    from shapely.ops import unary_union
    from shapely.affinity import scale

    with open(path, encoding="utf-8") as f:
        shapes = [shape(feature["geometry"]).buffer(0.0) for feature in json.load(f)["features"]]
    return Roads(scale(unary_union(shapes), xfact=1.0, yfact=-1.0, origin=(0.0, 0.0)))


def _across(ref, step, closed):
    """The normal of a line at each of its points, told over NORMAL_BASE_M so that it turns slowly through a corner."""
    count = len(ref)
    base = max(int(round(NORMAL_BASE_M / step)), 1)
    out = []
    for k in range(count):
        a = ref[(k - base) % count if closed else max(k - base, 0)]
        b = ref[(k + base) % count if closed else min(k + base, count - 1)]
        length = math.dist(a, b) or 1.0
        out.append((-(b[1] - a[1]) / length, (b[0] - a[0]) / length))
    return out


def off_kerbs(curve, roads, want, step, closed=True, still=()):
    """The line given ([[x, y]] a point every step m) moved across itself so that each point on the carriageway is
    want[k] m from the kerbs (roads, a Roads), where the carriageway is that wide; in its middle where it is not.
    The kerbs are those met across the line within KERB_REACH_M: the mouth of a side street is no kerb. The move is
    spread along the line, and none at the points `still`. Gate 89's long take: on the quay the lanes file puts the
    left lane 1.5 m from the kerb of the promenade, the flight drove it at 98 km/h, and a metre and a half off its
    line it was among the trees and the people. Returns ([[x, y]], the largest move in m)."""
    from shapely.geometry import Point

    count = len(curve)
    ref = [(p[0], p[1]) for p in curve]
    across = _across(ref, step, closed)
    reach = int(round(KERB_REACH_M / SCAN_M))
    move = [0.0] * count
    for k in range(count):
        if want[k] <= KERB_M or k in still or not roads.on(Point(ref[k])):
            continue
        room = []
        for side in (1.0, -1.0):
            i = 1
            while i <= reach and roads.on(Point(ref[k][0] + across[k][0] * side * i * SCAN_M, ref[k][1] + across[k][1] * side * i * SCAN_M)):
                i += 1
            room.append((i - 1) * SCAN_M)
        one, other = room
        if one >= want[k] and other >= want[k]:
            continue
        if one + other >= 2.0 * want[k]:
            move[k] = -(want[k] - one) if one < want[k] else want[k] - other
        else:
            move[k] = 0.5 * (one - other)
    # Spread along the line: a running mean, three times over (about a bell curve), and nothing at the points that stay.
    half = max(int(round(KERB_SPREAD_M / step)), 1)
    for _ in range(3):
        spread = []
        for k in range(count):
            near_by = [move[(k + d) % count] if closed else move[min(max(k + d, 0), count - 1)] for d in range(-half, half + 1)]
            spread.append(sum(near_by) / len(near_by))
        move = spread
    fixed = sorted(still)
    for k in range(count):
        if fixed:
            away = min(abs(k - f) for f in fixed) * step
            move[k] *= min(away / (3.0 * KERB_SPREAD_M), 1.0)
    return [[ref[k][0] + across[k][0] * move[k], ref[k][1] + across[k][1] * move[k]] for k in range(count)], max(abs(m) for m in move)


def _evenly(points, anchors, closed, last):
    """The same curve with its points evenly spaced: the first one stays, and so does the point `last` of an open
    curve (and those after it). The anchors go with the points."""
    count = len(points) if closed else last
    path = [points[k] for k in range(count)] + [points[0] if closed else points[last]]
    marks = [anchors[k] for k in range(count)] + [anchors[0] + len(points) if closed else anchors[last]]
    lengths = [math.dist(path[k], path[k + 1]) for k in range(count)]
    total = sum(lengths)
    out_points, out_anchors = [], []
    done, i = 0.0, 0
    for k in range(count):
        want = total * k / count
        while done + lengths[i] < want and i < count - 1:
            done += lengths[i]
            i += 1
        t = (want - done) / lengths[i] if lengths[i] > 0.0 else 0.0
        out_points.append((path[i][0] + (path[i + 1][0] - path[i][0]) * t, path[i][1] + (path[i + 1][1] - path[i][1]) * t))
        out_anchors.append(marks[i] + (marks[i + 1] - marks[i]) * t)
    return out_points + list(points[count:]), out_anchors + list(anchors[count:])


def _bend(points, closed):
    """For each inner point of the curve (numpy, N x 2), its turn (rad) over the root of the length it is turned on:
    the squares add up to the bending of the curve, whatever the spacing of its points."""
    import numpy

    if closed:
        before, after = points - numpy.roll(points, 1, axis=0), numpy.roll(points, -1, axis=0) - points
    else:
        before, after = points[1:-1] - points[:-2], points[2:] - points[1:-1]
    turn = numpy.arctan2(before[:, 0] * after[:, 1] - before[:, 1] * after[:, 0], before[:, 0] * after[:, 0] + before[:, 1] * after[:, 1])
    return turn / numpy.sqrt(0.5 * (numpy.hypot(before[:, 0], before[:, 1]) + numpy.hypot(after[:, 0], after[:, 1])) + 1e-9)


def ease(curve, walls, step, closed=True, tail=0, round_names=(), roads=None):
    """Eases curve ([[x, y]] a point every step m): the least bending (the turn squared over the length, added along
    the line) of a line whose points move across the line given, each within its slack (SLACK_CORNER_M at the bends,
    SLACK_STRAIGHT_M along the straights, more where a wall asks for it), on the carriageway where the line given is
    on it (roads, as read_roads gives it), and CLEAR_WALL_M from the walls (CLEAR_ROUND_M from those named in
    round_names). A bend is then entered from its outside and cut at its inside, as wide as the room allows. The
    first point of an open curve and its last `tail` points do not move. Returns (the eased points [[x, y]], as many
    and evenly spaced; for each, the index of the point of curve it stands by)."""
    import numpy
    from scipy.optimize import least_squares
    from scipy.sparse import lil_matrix
    from shapely.geometry import Point

    count = len(curve)
    last = count - tail if not closed else count
    ref = [(p[0], p[1]) for p in curve]
    names = set(round_names)

    def clear_of(wall):
        return CLEAR_ROUND_M if wall["name"] in names else CLEAR_WALL_M

    def on_road(p):
        return roads is None or roads.contains(Point(p[0], p[1]))

    def at(k):
        return k % count if closed else min(max(k, 0), count - 1)

    still = set() if closed else {0} | set(range(last, count))
    slack = slack_for_walls(ref, slack_along(ref, step, closed, still), walls, step, closed, clear_of)
    across = _across(ref, step, closed)
    # How far each point may go, to one side and to the other: the stretch of allowed places nearest to where it is.
    low, high = [0.0] * count, [0.0] * count
    for k in range(count):
        if k in still or slack[k] <= 0.0:
            continue
        mine = [(wall, clear_of(wall)) for wall in near(walls, ref[k], slack[k] + CLEAR_WALL_M + 1.0)]
        kerbs = roads is not None and on_road(ref[k])
        steps = int(math.floor(slack[k] / SCAN_M))
        places = [(ref[k][0] + across[k][0] * i * SCAN_M, ref[k][1] + across[k][1] * i * SCAN_M) for i in range(-steps, steps + 1)]
        clear = [all(to_wall(p, wall)[0] >= keep for wall, keep in mine) for p in places]
        allowed = [clear[i] and (not kerbs or i == steps or on_road(places[i])) for i in range(len(places))]
        if not any(allowed):
            allowed = clear     # a wall comes before a kerb
        if not any(allowed):
            continue
        start = min((i for i in range(len(allowed)) if allowed[i]), key=lambda i: abs(i - steps))
        first = end = start
        while first > 0 and allowed[first - 1]:
            first -= 1
        while end + 1 < len(allowed) and allowed[end + 1]:
            end += 1
        low[k], high[k] = (first - steps) * SCAN_M, (end - steps) * SCAN_M
    free = [k for k in range(count) if high[k] - low[k] > 1e-6]
    pinned = numpy.array([min(max(0.0, low[k]), high[k]) for k in range(count)])
    origin = numpy.array(ref, dtype=float)
    normal = numpy.array(across, dtype=float)
    if free:
        index = numpy.array(free)

        def placed(values):
            offset = pinned.copy()
            offset[index] = values
            return origin + normal * offset[:, None]

        def residuals(values):
            return numpy.concatenate([_bend(placed(values), closed), STAY * values])

        bends = count if closed else count - 2
        pattern = lil_matrix((bends + len(free), len(free)), dtype=int)
        for column, k in enumerate(free):
            for d in (-1, 0, 1):
                row = (k + d) % count if closed else k + d - 1
                if 0 <= row < bends:
                    pattern[row, column] = 1
            pattern[bends + column, column] = 1
        lows, highs = numpy.array([low[k] for k in free]), numpy.array([high[k] for k in free])
        found = least_squares(residuals, numpy.clip(numpy.zeros(len(free)), lows, highs), bounds=(lows, highs), jac_sparsity=pattern,
                              x_scale=1.0, max_nfev=EASE_PASSES)
        points = [(float(p[0]), float(p[1])) for p in placed(found.x)]
    else:
        points = list(ref)
    points, anchors = _evenly(points, [float(k) for k in range(count)], closed, last)
    return [[p[0], p[1]] for p in points], [min(max(int(round(a)), 0), count - 1) for a in anchors]
