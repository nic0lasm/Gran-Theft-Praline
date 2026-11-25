"""Fill a footprint with boxes: what stops a walker inside the outline of a building whose own
walls do not (pure Python, used by unreal/backdrop_collision.py and tested without the engine).

A landmark that stands for a whole building (landmarks.py, kind "building") is a LiDAR hull:
walls from the ground to the roof edge, left out where the roof edge reads at street level, and
wound either way; which of the two lets a walker in is not known. Gate 101, 157 to 203 s: the player on foot walked through the north wall of
the cinema at the corner of rue de la Republique, stood a metre inside it, could not come out,
and the picture stayed black for 47 s. The hull keeps its look; boxes that are not drawn fill
its outline.
"""

import math

STEP_CM = 50.0         # width of a strip
INSET_CM = 10.0        # the boxes stop this far inside the walls
MIN_SIDE_CM = 30.0     # nothing thinner
SAMPLES = 5            # lines read across each strip


def spans(ring, x):
    """[(y0, y1)] where the line of abscissa x lies inside the ring (even-odd)."""
    ys = []
    for i in range(len(ring)):
        (xa, ya), (xb, yb) = ring[i], ring[(i + 1) % len(ring)]
        if (xa <= x) != (xb <= x):
            ys.append(ya + (yb - ya) * (x - xa) / (xb - xa))
    ys.sort()
    return [(ys[i], ys[i + 1]) for i in range(0, len(ys) - 1, 2)]


def common(a, b):
    """The part two lists of spans share."""
    out = []
    for a0, a1 in a:
        for b0, b1 in b:
            lo, hi = max(a0, b0), min(a1, b1)
            if hi > lo:
                out.append((lo, hi))
    return out


def inside(ring, x, y):
    return any(y0 <= y <= y1 for y0, y1 in spans(ring, x))


def strips(ring, step=STEP_CM, inset=INSET_CM, min_side=MIN_SIDE_CM):
    """Boxes (x0, y0, x1, y1) that lie inside the ring and cover it, one strip of `step` after
    the other along x. A box holds only what every line read across its strip finds inside, so
    none passes a wall, straight or slanted; the walls keep a stair of at most a strip."""
    if len(ring) < 3:
        return []
    x_lo, x_hi = min(p[0] for p in ring), max(p[0] for p in ring)
    out = []
    for i in range(int(math.ceil((x_hi - x_lo) / step))):
        xa, xb = x_lo + i * step, min(x_hi, x_lo + (i + 1) * step)
        if xb - xa < min_side:
            continue
        eps = 0.01 * (xb - xa)
        xs = [xa + eps + (xb - xa - 2.0 * eps) * k / (SAMPLES - 1) for k in range(SAMPLES)]
        # a corner of the outline inside the strip is read too: the outline turns there
        xs += [p[0] for p in ring if xa + eps < p[0] < xb - eps]
        found = spans(ring, xs[0])
        for x in xs[1:]:
            found = common(found, spans(ring, x))
        for y0, y1 in found:
            if y1 - y0 - 2.0 * inset >= min_side:
                out.append((round(xa, 1), round(y0 + inset, 1), round(xb, 1), round(y1 - inset, 1)))
    return out


def area(boxes):
    return sum((x1 - x0) * (y1 - y0) for x0, y0, x1, y1 in boxes)
