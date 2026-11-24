"""Closed pits of the terrain, and flat lids for them, in plain Python (no numpy), so the same code
runs inside the Unreal Editor (tools/city/unreal/cover_pits.py) and in the tests.

Gate 76 video, 92-115 s: the player on foot ended in a granite pit several metres under the
street, walls of the ground material, and could not climb out. The terrain is the IGN LiDAR model
(terrain.py): the ramp of an underground car park or the mouth of an underpass is a hole in it,
with nothing at the bottom in the game. A pit here is a closed depression of the sampled ground:
the ground "filled" so that every cell drains to the edge of the grid (priority flood), minus the
ground itself, deeper than min_depth over a connected area between min_area and max_area. The
river and its low quays drain to the edge of the grid and are not pits.

Grid: rows of heights (cm) or None (no ground), step in cm, origin = centre of cell (0, 0).
"""

import heapq

NEIGHBOURS = ((1, 0), (-1, 0), (0, 1), (0, -1))


def fill(z):
    """Priority flood: the lowest surface >= z with no closed basin (cells drain to the border or
    to a cell without ground). Returns the filled grid (None stays None)."""
    rows, cols = len(z), len(z[0])
    out = [[None] * cols for _ in range(rows)]
    seen = [[False] * cols for _ in range(rows)]
    heap = []
    for r in range(rows):
        for c in range(cols):
            if z[r][c] is None:
                seen[r][c] = True
                continue
            edge = r in (0, rows - 1) or c in (0, cols - 1) or any(
                0 <= r + dr < rows and 0 <= c + dc < cols and z[r + dr][c + dc] is None for dr, dc in NEIGHBOURS)
            if edge:
                seen[r][c] = True
                out[r][c] = z[r][c]
                heapq.heappush(heap, (z[r][c], r, c))
    while heap:
        h, r, c = heapq.heappop(heap)
        for dr, dc in NEIGHBOURS:
            rr, cc = r + dr, c + dc
            if 0 <= rr < rows and 0 <= cc < cols and not seen[rr][cc]:
                seen[rr][cc] = True
                level = max(h, z[rr][cc])
                out[rr][cc] = level
                heapq.heappush(heap, (level, rr, cc))
    return out


def pits(z, step_cm, min_depth_cm=100.0, min_area_m2=6.0, max_area_m2=3000.0, wet_cm=10.0):
    """[{cells [(r, c)], spill_cm, depth_cm, area_m2, centre_rc}] of the closed depressions: cells
    at least wet_cm under the filled surface, connected, whose deepest cell is min_depth_cm deep."""
    f = fill(z)
    rows, cols = len(z), len(z[0])
    done = [[False] * cols for _ in range(rows)]
    cell_m2 = (step_cm / 100.0) ** 2
    out = []
    for r0 in range(rows):
        for c0 in range(cols):
            if done[r0][c0] or z[r0][c0] is None or f[r0][c0] - z[r0][c0] < wet_cm:
                continue
            stack, cells = [(r0, c0)], []
            done[r0][c0] = True
            while stack:
                r, c = stack.pop()
                cells.append((r, c))
                for dr, dc in NEIGHBOURS:
                    rr, cc = r + dr, c + dc
                    if 0 <= rr < rows and 0 <= cc < cols and not done[rr][cc] and z[rr][cc] is not None \
                            and f[rr][cc] - z[rr][cc] >= wet_cm:
                        done[rr][cc] = True
                        stack.append((rr, cc))
            depth = max(f[r][c] - z[r][c] for r, c in cells)
            area = len(cells) * cell_m2
            if depth < min_depth_cm or not (min_area_m2 <= area <= max_area_m2):
                continue
            spill = max(f[r][c] for r, c in cells)
            out.append({"cells": cells, "spill_cm": spill, "depth_cm": depth, "area_m2": area,
                        "centre_rc": (sum(r for r, _ in cells) / len(cells), sum(c for _, c in cells) / len(cells))})
    return out


def lid_runs(cells):
    """Row runs of a pit's cells: [(row, first col, last col)], one lid box each."""
    by_row = {}
    for r, c in cells:
        by_row.setdefault(r, []).append(c)
    runs = []
    for r in sorted(by_row):
        cs = sorted(by_row[r])
        start = prev = cs[0]
        for c in cs[1:]:
            if c != prev + 1:
                runs.append((r, start, prev))
                start = c
            prev = c
        runs.append((r, start, prev))
    return runs
