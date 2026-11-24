"""The ground of the squares, drawn by the ground material from their outline (plain Python).

landmarks.py records the squares of the quarter as data only: outline and surface. Place
Bellecour is "red_gravel": its red ground is what a Lyonnais sees first, and the ground material
laid the same pale slabs there as on any sidewalk, or, past the edge of the ground mask, the dark
tone of the far view. This module turns each outline into the text of a signed distance in the
material (a Custom node of apply_city_materials.py): negative inside, in Unreal cm, so the
material can lay the surface, a stone border and nothing outside, with no texture and whatever
the extent of the mask.

  load(path)        the squares of a landmarks.json with a drawn surface, rings in Unreal cm
  simplify(ring)    fewer corners (Douglas-Peucker on the closed outline): each one is paid for
                    on every pixel of the ground
  distance(...)     the reference the shader text is tested against
  hlsl(rings)       the shader text

Axes: landmarks.json is in local metres, x east, y north; Unreal is x east, y south, cm.
"""

import json
import math
import os

SURFACES = ("red_gravel",)      # the surfaces the ground material knows how to draw
TOLERANCE_CM = 150.0            # a corner cut by less than this is not worth its cost
MAX_CORNERS = 12


def to_ue(ring_xy_m):
    """Closed or open ring in local metres -> open ring in Unreal cm."""
    pts = [(float(x) * 100.0, -float(y) * 100.0) for x, y in ring_xy_m]
    if len(pts) > 1 and pts[0] == pts[-1]:
        pts.pop()
    return pts


def load(path, surfaces=SURFACES):
    """[{"key", "name", "surface", "area_m2", "ring"}] for the squares whose surface is drawn; [] when
    the file is not there (a map generated without landmarks)."""
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        meta = json.load(f)
    out = []
    for r in meta.get("landmarks", []):
        if r.get("kind") != "square" or r.get("surface") not in surfaces or not r.get("polygon_xy"):
            continue
        ring = max((to_ue(p) for p in r["polygon_xy"]), key=area)    # the largest piece is the square
        if len(ring) >= 3:
            out.append({"key": r["key"], "name": r.get("name", r["key"]), "surface": r["surface"],
                        "area_m2": round(area(ring) / 1.0e4), "ring": ring})
    return out


def area(ring):
    return 0.5 * abs(sum(ring[i][0] * ring[(i + 1) % len(ring)][1] - ring[(i + 1) % len(ring)][0] * ring[i][1]
                         for i in range(len(ring))))


def _segment_distance(p, a, b):
    ex, ey, wx, wy = b[0] - a[0], b[1] - a[1], p[0] - a[0], p[1] - a[1]
    ee = ex * ex + ey * ey
    t = max(0.0, min(1.0, (wx * ex + wy * ey) / ee)) if ee > 0.0 else 0.0
    return math.hypot(wx - ex * t, wy - ey * t)


def _peucker(pts, tol):
    """Douglas-Peucker on an open line, ends kept."""
    if len(pts) < 3:
        return list(pts)
    far, worst = 0, -1.0
    for i in range(1, len(pts) - 1):
        d = _segment_distance(pts[i], pts[0], pts[-1])
        if d > worst:
            far, worst = i, d
    if worst <= tol:
        return [pts[0], pts[-1]]
    return _peucker(pts[:far + 1], tol)[:-1] + _peucker(pts[far:], tol)


def simplify(ring, tolerance_cm=TOLERANCE_CM, max_corners=MAX_CORNERS):
    """The outline with fewer corners: cut at its two farthest points, each half simplified, the
    tolerance doubled until no more than max_corners are left."""
    if len(ring) <= 4:
        return list(ring)
    a = max(range(len(ring)), key=lambda i: math.hypot(ring[i][0] - ring[0][0], ring[i][1] - ring[0][1]))
    b = max(range(len(ring)), key=lambda i: math.hypot(ring[i][0] - ring[a][0], ring[i][1] - ring[a][1]))
    a, b = min(a, b), max(a, b)
    one, two = ring[a:b + 1], ring[b:] + ring[:a + 1]
    tol = tolerance_cm
    while True:
        out = _peucker(one, tol)[:-1] + _peucker(two, tol)[:-1]
        if len(out) <= max_corners or tol > 1.0e5:
            return out
        tol *= 2.0


def distance(rings, p):
    """Signed distance in cm from p to the nearest outline: negative inside any of them."""
    best = float("inf")
    for ring in rings:
        d, inside = float("inf"), False
        for i, a in enumerate(ring):
            b = ring[(i + 1) % len(ring)]
            d = min(d, _segment_distance(p, a, b))
            if (a[1] > p[1]) != (b[1] > p[1]) and p[0] < a[0] + (p[1] - a[1]) * (b[0] - a[0]) / (b[1] - a[1]):
                inside = not inside
        best = min(best, -d if inside else d)
    return best


def bounds(rings, pad_cm=0.0):
    xs = [p[0] for r in rings for p in r]
    ys = [p[1] for r in rings for p in r]
    return min(xs) - pad_cm, min(ys) - pad_cm, max(xs) + pad_cm, max(ys) + pad_cm


FAR_CM = 100000.0       # what the shader answers away from every square


def hlsl(rings, pad_cm=1000.0):
    """Body of a Custom node with one float2 input, XY (world position, cm): the signed distance
    to the nearest outline, FAR_CM beyond pad_cm of them all. Plain scalar code, one block an
    edge, no array and no loop: the same text compiles as C++ in the tests."""
    x0, y0, x1, y1 = bounds(rings, pad_cm)
    lines = ["float2 p = XY;",
             "float best = %.1f;" % FAR_CM,
             "if (p.x > %.1f && p.x < %.1f && p.y > %.1f && p.y < %.1f)" % (x0, x1, y0, y1),
             "{",
             "\tfloat2 a = float2(0.0, 0.0);", "\tfloat2 b = a;", "\tfloat2 e = a;", "\tfloat2 w = a;",
             "\tfloat2 q = a;", "\tfloat d = 0.0;", "\tfloat s = 1.0;"]
    for ring in rings:
        lines += ["\td = 1.0e12;", "\ts = 1.0;"]
        for i, a in enumerate(ring):
            b = ring[(i + 1) % len(ring)]
            lines += ["\ta = float2(%.1f, %.1f); b = float2(%.1f, %.1f);" % (a[0], a[1], b[0], b[1]),
                      "\te = b - a; w = p - a;",
                      "\tq = w - e * saturate(dot(w, e) / max(dot(e, e), 0.0001));",
                      "\td = min(d, dot(q, q));",
                      "\tif ((a.y > p.y) != (b.y > p.y)) { if (p.x < a.x + (p.y - a.y) * e.x / e.y) { s = -s; } }"]
        lines += ["\tbest = min(best, s * sqrt(d));"]
    lines += ["}", "return best;"]
    return "\n".join(lines)
