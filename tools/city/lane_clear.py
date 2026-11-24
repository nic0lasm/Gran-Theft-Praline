"""Lanes of the game and the street objects standing on them, in plain Python (no shapely), so the
same code runs inside the Unreal Editor (tools/city/unreal/clear_lane_instances.py) and in the
tests.

Gate 75: four placed objects stood on the lanes the flight drives (a white block on lane 13 rue
de la Bourse, 0.5 m from its centre line; others quai Jean Moulin -> rue Gentil, rue Claudia, rue
Antoine Salles), and the police saw furniture within 1 m of lanes 42, 62, 94 and 153. The tables
are cleared when they are written (clear_lanes.py), but objects placed from other tables, or
before that rule, stay in the map. This finds them from the map itself, against the lanes file
the game reads (Content/City/Traffic/<Map>.lanes.csv), with clear_lanes.py's rule: nothing a car
can hit within CLEAR_CM of a lane centre line, nor of the straight link from a lane's end to the
start of the next.

Unreal units: cm, X east, Y south. A footprint is a polygon [(x, y), ...] (an instance's mesh box
turned by its yaw) or a disc ((x, y), r) for a tree trunk.
"""

import csv
import math

CLEAR_CM = 150.0           # clear_lanes.CLEAR_M: half a car and what a driver strays from the line
CELL_CM = 2000.0
TRUNK_RADIUS_CM = 40.0     # a tree's bounds are its crown: only the trunk is in the way


def read_lanes(path):
    """[{id, name, width_cm, points [(x, y, z)], next [ids]}] of a lanes.csv (traffic.py)."""
    lanes = []
    with open(path, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            pts = []
            for p in r["points_ue_cm"].split(";"):
                v = [float(c) for c in p.split()]
                pts.append((v[0], v[1], v[2] if len(v) > 2 else 0.0))
            lanes.append({"id": int(r["lane_id"]), "name": r.get("name", ""),
                          "width_cm": float(r.get("width_m") or 3.0) * 100.0, "points": pts,
                          "next": [int(n) for n in (r.get("next") or "").split()]})
    return lanes


END_TREE_CLEAR_CM = 800.0   # where the filmed flight ends, no tree trunk this close to the driven line


def read_end_lines(path):
    """The driven lines [[(x, y), ...]] of the rows marked "end" in a flight circuit file
    (Content/Test/Circuits/<Map>.circuit.csv): where the filmed flight stops and the arrest is
    played, the chase camera turning round the car 6.5 m away and up to 3 m above it. Leaves have
    no collision: gate 78, 80 s, the camera sat inside a tree. [] without the file or the column."""
    out, header = [], None
    try:
        with open(path, encoding="utf-8") as f:
            rows = f.read().splitlines()
    except OSError:
        return out
    for row in rows:
        if row.startswith("#") or not row.strip():
            continue
        cells = row.split(",")
        if cells[0] == "lane_id":
            header = cells
            continue
        if header is None or "mark" not in header or "line_ue_cm" not in header or len(cells) < len(header):
            continue
        if cells[header.index("mark")].strip() != "end":
            continue
        pts = []
        for pair in cells[header.index("line_ue_cm")].split(";"):
            xy = pair.split()
            if len(xy) == 2:
                pts.append((float(xy[0]), float(xy[1])))
        if not pts:        # no line written: the lane's own ends
            pts = [(float(cells[header.index("start_x_cm")]), float(cells[header.index("start_y_cm")])),
                   (float(cells[header.index("end_x_cm")]), float(cells[header.index("end_y_cm")]))]
        out.append(pts)
    return out


def end_gap(x, y, lines):
    """Distance (cm) from a point to the nearest of those lines (their points joined), inf without any."""
    best = float("inf")
    for pts in lines:
        if len(pts) == 1:
            best = min(best, math.hypot(x - pts[0][0], y - pts[0][1]))
        for a, b in zip(pts, pts[1:]):
            best = min(best, _seg_point((x, y), a, b))
    return best


class LaneIndex:
    """Lane centre lines and lane-to-lane links as segments, in a uniform grid."""

    def __init__(self, lanes, clear_cm=CLEAR_CM):
        self.clear = clear_cm
        self.segs = []          # (label, (x0, y0, z0), (x1, y1, z1))
        by_id = {lane["id"]: lane for lane in lanes}
        for lane in lanes:
            p = lane["points"]
            for a, b in zip(p, p[1:]):
                self.segs.append((str(lane["id"]), a, b))
            for n in lane["next"]:
                nxt = by_id.get(n)
                if nxt and p and nxt["points"] and p[-1][:2] != nxt["points"][0][:2]:
                    self.segs.append(("%d>%d" % (lane["id"], n), p[-1], nxt["points"][0]))
        self.grid = {}
        for i, (_, a, b) in enumerate(self.segs):
            for key in self._cells(min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1]), self.clear):
                self.grid.setdefault(key, []).append(i)

    @staticmethod
    def _cells(x0, y0, x1, y1, pad):
        for cx in range(int(math.floor((x0 - pad) / CELL_CM)), int(math.floor((x1 + pad) / CELL_CM)) + 1):
            for cy in range(int(math.floor((y0 - pad) / CELL_CM)), int(math.floor((y1 + pad) / CELL_CM)) + 1):
                yield cx, cy

    def nearest(self, shape, z_low=None, reach_cm=250.0):
        """(lane label, gap in cm) of the nearest lane segment within the clearance, else None.
        z_low: the object's lowest point; a lane more than reach_cm under it is not in its way
        (a bridge deck over a quay road)."""
        x0, y0, x1, y1 = bounds(shape)
        best = None
        seen = set()
        for key in self._cells(x0, y0, x1, y1, self.clear):
            for i in self.grid.get(key, ()):
                if i in seen:
                    continue
                seen.add(i)
                label, a, b = self.segs[i]
                if z_low is not None and z_low - max(a[2], b[2]) > reach_cm:
                    continue
                d = gap(shape, a, b)
                if d < self.clear and (best is None or d < best[1]):
                    best = (label, d)
        return best


def footprint_box(x, y, yaw_deg, lo, hi, scale=(1.0, 1.0)):
    """Polygon of a mesh box (local lo/hi corners, cm) at (x, y), turned by yaw (degrees)."""
    c, s = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
    out = []
    for lx, ly in ((lo[0], lo[1]), (hi[0], lo[1]), (hi[0], hi[1]), (lo[0], hi[1])):
        lx, ly = lx * scale[0], ly * scale[1]
        out.append((x + lx * c - ly * s, y + lx * s + ly * c))
    return out


def bounds(shape):
    if isinstance(shape, tuple):                     # disc
        (x, y), r = shape
        return x - r, y - r, x + r, y + r
    xs, ys = [p[0] for p in shape], [p[1] for p in shape]
    return min(xs), min(ys), max(xs), max(ys)


def _seg_point(p, a, b):
    ax, ay, bx, by = a[0], a[1], b[0], b[1]
    dx, dy = bx - ax, by - ay
    t = ((p[0] - ax) * dx + (p[1] - ay) * dy) / (dx * dx + dy * dy or 1.0)
    t = max(0.0, min(1.0, t))
    return math.hypot(p[0] - (ax + t * dx), p[1] - (ay + t * dy))


def _cross(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


def _segments_cross(p1, p2, q1, q2):
    d1, d2 = _cross(q1, q2, p1), _cross(q1, q2, p2)
    d3, d4 = _cross(p1, p2, q1), _cross(p1, p2, q2)
    return (d1 > 0) != (d2 > 0) and (d3 > 0) != (d4 > 0)


def _inside(p, poly):
    n, inside = len(poly), False
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        if (a[1] > p[1]) != (b[1] > p[1]) and p[0] < a[0] + (p[1] - a[1]) * (b[0] - a[0]) / (b[1] - a[1]):
            inside = not inside
    return inside


def gap(shape, a, b):
    """Distance (cm) from a footprint to the segment a-b in plan, 0 when they overlap."""
    if isinstance(shape, tuple):
        (x, y), r = shape
        return max(0.0, _seg_point((x, y), a, b) - r)
    if _inside(a, shape) or _inside(b, shape):
        return 0.0
    best = float("inf")
    n = len(shape)
    for i in range(n):
        p, q = shape[i], shape[(i + 1) % n]
        if _segments_cross(p, q, a, b):
            return 0.0
        best = min(best, _seg_point(p, a, b), _seg_point(q, a, b), _seg_point(a, p, q), _seg_point(b, p, q))
    return best
