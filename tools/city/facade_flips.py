"""Facade kit modules turned the wrong way, in plain Python (no numpy), shared by
tools/city/unreal/fix_facade_flips.py (in the editor) and the tests.

Gate 75 video, 70-95 s: the stolen car stood 25 s against a white block in rue de la Bourse. The
PC's probe (captures/portes/75-19f6004/probe-bloc-blanc.json) names it: the module
SM_Facade_Haussmann_ShopClosed_B at (26250, -14825), yaw -90, on a facade line whose every other
module (ground windows, entrance, windows above) has yaw 90. A shop module carries its room
behind the facade plane: turned round, the room stands in the street, its plain back walls
towards the traffic.

Rule: a module's pivot is the bottom centre of its bay on the facade plane (facade_kit/kit.py),
its bay runs along its local X. Its neighbours are the modules within NEAR_CM whose pivot lies on
its facade line (within LINE_CM of it) and within DZ_CM in height. A module with at least
MIN_VOTES neighbours facing the opposite way (yaw 180 +- TOL_DEG off) and none facing its own way
is turned round (yaw + 180, same pivot). Corners (90 degrees) and the far side of a thin
building (another line) are not neighbours.
"""

import math

NEAR_CM = 600.0
LINE_CM = 40.0
DZ_CM = 450.0
TOL_DEG = 10.0
MIN_VOTES = 3            # prep 78: eight Blank modules flipped on 2 votes each, odd yaws, most likely
#                         the short faces of a notch in a footprint, which do face the other way
CELL_CM = 600.0


def _wrap(d):
    return (d + 180.0) % 360.0 - 180.0


def votes(instances):
    """instances: [(key, x, y, z, yaw_deg)]. Returns [(same, opposite)] per instance."""
    grid = {}
    for i, (_, x, y, _, _) in enumerate(instances):
        grid.setdefault((int(x // CELL_CM), int(y // CELL_CM)), []).append(i)
    out = []
    for i, (_, x, y, z, yaw) in enumerate(instances):
        a = math.radians(yaw)
        nx, ny = -math.sin(a), math.cos(a)          # facade normal: the bay runs along local X
        same = opposite = 0
        cx, cy = int(x // CELL_CM), int(y // CELL_CM)
        for gx in (cx - 1, cx, cx + 1):
            for gy in (cy - 1, cy, cy + 1):
                for j in grid.get((gx, gy), ()):
                    if j == i:
                        continue
                    _, bx, by, bz, byaw = instances[j]
                    dx, dy = bx - x, by - y
                    if abs(bz - z) > DZ_CM or dx * dx + dy * dy > NEAR_CM * NEAR_CM:
                        continue
                    if abs(dx * nx + dy * ny) > LINE_CM:
                        continue
                    d = abs(_wrap(byaw - yaw))
                    if d <= TOL_DEG:
                        same += 1
                    elif d >= 180.0 - TOL_DEG:
                        opposite += 1
        out.append((same, opposite))
    return out


def flipped(instances, min_votes=None):
    """[(key, votes_opposite)] of the instances to turn round."""
    need = MIN_VOTES if min_votes is None else min_votes
    return [(inst[0], o) for inst, (s, o) in zip(instances, votes(instances)) if o >= need and s == 0]
