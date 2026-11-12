"""Ground-floor facade modules left out in front of an enterable interior.

An interior level (tools/interiors/<id>.json, built by tools/interiors/unreal/build_interior_level.py)
streams in behind a city facade. Its anchor gives the interior frame in Unreal centimetres:
origin = shop_ue_cm + origin_offset_m (local metres, x east, y north, so north is Unreal -Y),
x along front_direction_m, y inward, frame width x depth. Only the glass front (frame line y = 0)
opens: the ground-storey modules, street or court side, whose pivot lies within FRONT_BAND_M of
that line over the frame width widened by MARGIN_M are not placed; the other sides of the interior
are blind walls and keep the city facade. The interior's
own glass front and door take their place. The building shell has no wall where the facade
modules are (meshkit.building_mesh with_modules=False), so the opening needs nothing else.
Plain Python, used by unreal/place_facades.py and the tests.
"""

import glob
import json
import math
import os

MARGIN_M = 0.5
FRONT_BAND_M = 1.0
GROUND_STOREY = 0


def load_frames(interiors_dir):
    """[(id, origin_ue_cm, unit_front, unit_inward, width_cm, depth_cm)] for every interior with an anchor."""
    frames = []
    for path in sorted(glob.glob(os.path.join(interiors_dir, "*.json"))):
        try:
            with open(path, encoding="utf-8") as fh:
                spec = json.load(fh)
        except (OSError, ValueError):
            continue
        if not isinstance(spec, dict):
            continue
        anchor, frame = spec.get("anchor"), spec.get("frame")
        if not anchor or not frame:
            continue
        sx, sy, _sz = anchor["shop_ue_cm"]
        ox, oy = anchor["origin_offset_m"]
        dx, dy = anchor["front_direction_m"]
        n = math.hypot(dx, dy)
        front = (dx / n, -dy / n)        # local north flips to Unreal -Y
        inward = (front[1], -front[0])   # local +90 deg (inward, north of the front), mirrored
        frames.append((spec.get("id", os.path.basename(path)), (sx + ox * 100.0, sy - oy * 100.0), front, inward,
                       frame["width"] * 100.0, frame["depth"] * 100.0))
    return frames


def on_front(frame, x, y, margin_cm=MARGIN_M * 100.0, band_cm=FRONT_BAND_M * 100.0):
    _id, (ox, oy), (fx, fy), (ix, iy), width, _depth = frame
    u = (x - ox) * fx + (y - oy) * fy
    v = (x - ox) * ix + (y - oy) * iy
    return -margin_cm <= u <= width + margin_cm and abs(v) <= band_cm


def cut_rows(rows, frames):
    """Rows of facade_modules.csv without the ground modules on an interior's glass front; and {id: count}."""
    kept, cut = [], {}
    for r in rows:
        if int(r["storey"]) == GROUND_STOREY:
            hit = next((f for f in frames if on_front(f, float(r["ue_x"]), float(r["ue_y"]))), None)
            if hit:
                cut[hit[0]] = cut.get(hit[0], 0) + 1
                continue
        kept.append(r)
    return kept, cut
