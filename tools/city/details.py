"""Fine street detail placement (pass 2 of the street dressing), from real anchors.

props.py places the surveyed street furniture. This pass adds what makes a street read
as lived in, always anchored on something real, never scattered at random over a plane:

  facades (facade_modules.csv)  downpipes on party walls, cellar grilles, meter cabinets,
                                house-number plaques, mailboxes, wall lamps by carriage
                                doors, awnings over cafés and food shops, ATMs on banks,
                                ashtrays at bar and tobacconist doors, doormats, window
                                boxes and the odd satellite dish or air conditioner,
                                display shelves in the lit rooms behind the shop glass,
                                keystone mascarons over Haussmann carriage doors
  corners (facade_modules.csv)  street-name plates where a building turns a street corner
  OpenStreetMap (ODbL)          bollards, pedestrian and vehicle traffic lights on the
                                kerbs of signalled crossings, traffic signs by their French
                                code, street cabinets, advertising columns, ATMs
  Grand Lyon (Licence Ouverte)  fallen plane leaves around the tree grates (autumn is the
                                default season of the art bible), kerb lines used to move
                                traffic lights and signs from the road centre to the kerb
  roofs (buildings.csv)         chimney stacks on the party walls, at roof height
  life                          bins and bags by some entrances, a bicycle against a wall,
                                bicycles on the surveyed hoops, shared kick scooters,
                                motorbikes on the OSM two-wheeler parkings,
                                pigeons near benches, litter near the bins

The choice of which bay gets a detail is a stable hash of its building id and bay, so
every run gives the same street. Same columns as props_placement.csv plus: snap ("ground"
rows sit on the terrain tiles, terrain.surface_z, and may still be traced in the editor;
"keep" rows, wall and roof items, keep their height), scale_x/y/z (awnings stretched to the bay width) and
collision (false for small items). Cull distances and shadows come from the mesh size in
place_props.py (details set). The total is capped (MAX_INSTANCES) by priority, the most
visible first.

  python3 tools/city/details.py --name presqu-ile --focus rue-de-la-republique [--buffer 45]
Output (regenerated, not committed, ODbL-derived): gen/<focus>/details_placement.csv and
details_report.json. Placed by place_props.py with GTP_PROPS_SET=details (folder
City/Details), which run_city_import.sh runs when the table exists.
"""

import argparse
import csv
import glob
import hashlib
import json
import math
import os
import sys
import time
import xml.etree.ElementTree as ET

import numpy as np
from shapely.geometry import LineString, Point, box
from shapely.ops import nearest_points, unary_union
from shapely.strtree import STRtree

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import citylib as cl  # noqa: E402
import generate  # noqa: E402
import grandlyon  # noqa: E402
import raster  # noqa: E402
import terrain  # noqa: E402

sys.path.insert(0, os.path.join(HERE, "facade_kit"))
from kit_layout import module_for  # noqa: E402

SHOP_BACK_M = 0.45 + 2.2 - 0.25   # kit.py WALL_DEPTH + shop_interior depth, minus half a shelf

SHADOW_KEEP = {"W1-45", "W1-46", "W13-19"}   # awnings and chimneys throw shadows a player sees
# Draw distances (m) of the perf thread's budget: 20-40 m for details, a little more for what
# draws the street's silhouette (chimneys, awnings, signals).
CULL_M = {"small": 20, "medium": 30, "large": 40, "W13-19": 60, "W1-45": 40, "W1-46": 40, "W8-01": 60,
          "W8-02": 50, "W14-42": 40}
MAX_INSTANCES = 3000       # agreed with the optimisation pass; above it, ask first
MIN_GAP_M = 0.35           # wall items sit close together (plaque beside an intercom)
FOOD_TRADES = {"restaurant", "cafe", "bar", "pub", "fast_food", "ice_cream", "bakery", "pastry",
               "confectionery", "chocolate", "greengrocer", "deli", "cheese", "wine", "tea", "coffee"}
SMOKER_TRADES = {"bar", "pub", "cafe", "tobacco", "restaurant"}
# French sign code (OSM traffic_sign) -> catalogue id. Signs we have no mesh for are skipped.
SIGNS = {"B1": "W8-07", "AB3a": "W8-06", "AB4": "W8-05", "B6a1": "W8-09", "B6d": "W8-09",
         "B14": "W8-08", "B30": "W8-08", "C115": "W8-08", "B54": "W1-57", "B52": "W1-57",
         "B53": "W1-57", "B55": "W1-57", "A13a": "W8-16", "A13b": "W8-11", "C20a": "W8-11",
         "C12": "W8-10", "B22a": "W8-14", "C113": "W8-14", "B21-1": "W8-15", "C1a": "W8-17"}
# Priority: lower first when the cap is reached (eye level and the street's own first).
PRIORITY = {"W1-46": 0, "W1-45": 0, "W1-57": 0, "W8-02": 0, "W8-01": 0, "W1-56": 0, "W14-42": 1,
            "W1-09": 1, "W8-07": 1, "W8-06": 1, "W8-05": 1, "W8-09": 1, "W8-08": 1, "W8-10": 1,
            "W8-11": 1, "W8-14": 1, "W8-15": 1, "W8-16": 1, "W8-17": 1, "W1-59": 1, "W13-19": 2,
            "W14-45": 2, "W14-46": 2, "W14-41": 2, "W14-62": 2, "W13-07": 2, "W14-32": 1, "W1-30": 2, "W1-19": 2,
            "W14-43": 3, "W14-36": 3, "W14-55": 3, "W14-34": 3, "W14-03": 3, "W1-32": 3, "W1-60": 3,
            "W14-53": 3, "W14-69": 3, "W1-14": 2, "W1-54": 2, "W1-55": 3, "W14-65": 4, "W14-66": 4, "W14-48": 4, "W1-47": 4, "W14-72": 4}


def _group_map(text, cast):
    """'W14-42:0.5,Shopfront:0.8' -> {'W14-42': 0.5, 'Shopfront': 0.8}; a bare key maps to None."""
    out = {}
    for item in filter(None, (t.strip() for t in (text or "").split(","))):
        key, _, val = item.partition(":")
        out[key.strip()] = cast(val) if val else None
    return out


def apply_switches(rows, env):
    """Per-group switches for the perf passes, each keyed by catalogue id or family:

      GTP_DETAILS_ONLY=W14-42,Shopfront   keep these groups only
      GTP_DETAILS_DROP=W14-03             remove these groups
      GTP_DETAILS_THIN=W14-42:0.5         keep a stable share of a group (same instances every run)
      GTP_DETAILS_CULL=W14-42:25,*:0.8    draw distance in metres, or ``*:<factor>`` for all

    All empty by default: the full dressing. The report lists what was applied."""
    only = _group_map(env.get("GTP_DETAILS_ONLY"), str)
    drop = _group_map(env.get("GTP_DETAILS_DROP"), str)
    thin = _group_map(env.get("GTP_DETAILS_THIN"), float)
    cull = _group_map(env.get("GTP_DETAILS_CULL"), float)

    def pick(table, r):
        for k in (r["catalog_id"], r["family"]):
            if k in table:
                return k
        return None

    kept = []
    for r in rows:
        if only and pick(only, r) is None or pick(drop, r) is not None:
            continue
        k = pick(thin, r)
        if k is not None and h01("thin", r["catalog_id"], r["x_m"], r["y_m"], r["z_m"]) >= thin[k]:
            continue
        k = pick(cull, r)
        if k is not None:
            r["cull_m"] = round(cull[k])
        elif cull.get("*") is not None:
            r["cull_m"] = round(float(r["cull_m"]) * cull["*"])
        kept.append(r)
    switches = {name: env[name] for name in ("GTP_DETAILS_ONLY", "GTP_DETAILS_DROP", "GTP_DETAILS_THIN",
                                             "GTP_DETAILS_CULL") if env.get(name)}
    switches["removed"] = len(rows) - len(kept)
    return kept, switches


def h01(*parts):
    """Stable pseudo-random number in [0, 1) from any key (no global seed, run-independent)."""
    d = hashlib.blake2b("|".join(str(p) for p in parts).encode(), digest_size=8).digest()
    return int.from_bytes(d, "big") / 2.0 ** 64


def load_manifests(root):
    out = {}
    for path in sorted(glob.glob(os.path.join(root, "wave-*.json"))):
        with open(path, encoding="utf-8") as f:
            for it in json.load(f)["items"]:
                out[it["id"]] = it
    return out


def size_class(item):
    s = float(item.get("size_m", 1.0))
    return "small" if s < 0.5 else "medium" if s <= 2.0 else "large"


def osm_nodes(raw):
    """Every tagged OSM node of the raw tiles: id -> (lon, lat, tags)."""
    out = {}
    for path in sorted(glob.glob(os.path.join(raw, "osm_*.osm"))):
        for el in ET.parse(path).getroot().iter("node"):
            tags = {t.get("k"): t.get("v") for t in el.iter("tag")}
            if tags:
                out[el.get("id")] = (float(el.get("lon")), float(el.get("lat")), tags)
    return out


def main():  # noqa: C901, PLR0912, PLR0915
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--street", default="Rue de la République")
    ap.add_argument("--buffer", type=float, default=45.0)
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    ap.add_argument("--manifests", default=os.path.join(REPO, "assets", "manifests"))
    ap.add_argument("--max", type=int, default=MAX_INSTANCES)
    ap.add_argument("--cell", help="x0,y0,x1,y1 (m): with --focus <cell>, a cell of the streamed quarter "
                                   "(cells.py): the street zone clipped to the cell box, so no detail is "
                                   "placed twice at a seam (ADR-0010)")
    args = ap.parse_args()
    t0 = time.time()
    raw = os.path.join(args.geo, "raw", args.name)
    proc = os.path.join(args.geo, "processed", args.name)
    gen = os.path.join(proc, "gen", args.focus)
    with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
        man = json.load(f)
    origin, origin_alt = man["origin_lambert93_m"], man["origin_altitude_m"]
    mnt = raster.Raster.load(raw, "mnt")
    items = load_manifests(args.manifests)

    roads = generate.load_roads(raw, origin)
    street = [r[0] for r in roads if r[2] == args.street]
    if not street:
        sys.exit("no road named %r" % args.street)
    zone = unary_union(street).buffer(args.buffer)
    if args.cell:
        zone = zone.intersection(box(*[float(v) for v in args.cell.split(",")]))
    if zone.is_empty:   # the street does not cross this cell: an empty table, nothing placed
        write_tables(gen, [], 0, args, {}, t0)
        return
    buildings = [b for b in generate.load_buildings(proc) if b["poly"].intersects(zone)]
    footprints = [b["poly"] for b in buildings]
    fp_tree = STRtree(footprints)
    with open(os.path.join(gen, "facade_modules.csv"), encoding="utf-8") as f:
        all_mods = [r for r in csv.DictReader(f) if r["side"] == "street"]
    for m in all_mods:
        m["x"], m["y"], m["z"] = float(m["x_m"]), float(m["y_m"]), float(m["z_m"])
        m["yaw"], m["w"], m["h"] = float(m["ue_yaw"]), float(m["width_m"]), float(m["height_m"])
    ground = [m for m in all_mods if m["storey"] == "0"]
    if not ground:   # no street-level facade of this cell in the zone: nothing to anchor on
        write_tables(gen, [], 0, args, {}, t0)
        return
    gxy = np.array([[m["x"], m["y"]] for m in ground])
    bgen = {}
    with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
        for r in csv.DictReader(f):
            bgen[r["building_id"]] = r
    with open(os.path.join(proc, "shops.csv"), encoding="utf-8") as f:
        shops = {r["osm_id"]: r for r in csv.DictReader(f)}

    kerbs = []
    for ft in grandlyon.load(raw, "kerbs"):
        g = ft["geometry"]
        parts = [g["coordinates"]] if g["type"] == "LineString" else g.get("coordinates", [])
        for coords in parts:
            pts = [(c[0] - origin[0], c[1] - origin[1]) for c in coords]
            if len(pts) >= 2:
                ls = LineString(pts)
                if ls.intersects(zone):
                    kerbs.append(ls)
    kerb_tree = STRtree(kerbs) if kerbs else None

    def ground_z(x, y):
        """Fallback ground height (local metres): the nearest street-level facade module."""
        k = int(np.argmin(np.hypot(gxy[:, 0] - x, gxy[:, 1] - y)))
        return ground[k]["z"]

    def facing(x, y):
        k = int(np.argmin(np.hypot(gxy[:, 0] - x, gxy[:, 1] - y)))
        return ground[k]["yaw"]

    rows, placed = [], []

    def add(cid, x, y, z, yaw, source, ref="", snap="ground", scale=(1.0, 1.0, 1.0), inside_ok=False):
        it = items.get(cid)
        if it is None:
            return False
        p = Point(x, y)
        if not zone.contains(p):
            return False
        if not inside_ok and snap == "ground" and any(footprints[i].contains(p) for i in fp_tree.query(p)):
            return False
        if any(abs(x - a) < MIN_GAP_M and abs(y - b) < MIN_GAP_M and abs(z - c) < MIN_GAP_M for a, b, c in placed):
            return False
        if snap == "ground":   # on the terrain tiles the game collides with (a headless editor cannot trace)
            gz = float(terrain.surface_z(mnt, origin, origin_alt, [x], [y])[0])
            z = z if np.isnan(gz) else gz
        cls = size_class(it)
        ux, uy, uz = cl.to_unreal(x, y, z)
        rows.append({"catalog_id": cid, "asset": it["asset"], "family": it["family"],
                     "asset_path": "/Game/Environment/Props/%s/%s/SM_%s_A" % (it["family"], it["asset"], it["asset"]),
                     "x_m": round(x, 2), "y_m": round(y, 2), "z_m": round(z, 2), "ue_x": ux, "ue_y": uy, "ue_z": uz,
                     "ue_yaw": round(yaw % 360.0, 1), "source": source, "ref": ref, "snap": snap,
                     "scale_x": round(scale[0], 3), "scale_y": round(scale[1], 3), "scale_z": round(scale[2], 3),
                     "collision": "true" if cls != "small" and snap == "ground" else "false",
                     # wall-flush items, shelves behind glass and litter: no dynamic shadow (GPU budget)
                     "shadow": "true" if cid in SHADOW_KEEP or (snap == "ground" and cls != "small") else "false",
                     "cull_m": CULL_M.get(cid, CULL_M[cls])})
        placed.append((x, y, z))
        return True

    def frame(m):
        """Outward normal and along-facade tangent of a module, local metres."""
        nx, ny = math.cos(math.radians(m["yaw"])), -math.sin(math.radians(m["yaw"]))
        return nx, ny, ny, -nx

    def on_wall(cid, m, along, height, out=0.03, ref_tag="", yaw_off=0.0, scale=(1.0, 1.0, 1.0)):
        nx, ny, tx, ty = frame(m)
        x = m["x"] + nx * out + tx * along
        y = m["y"] + ny * out + ty * along
        return add(cid, x, y, m["z"] + height, m["yaw"] + yaw_off, "facade:" + m["kind"],
                   ref_tag or m["building_id"], snap="keep", scale=scale)

    # --- 1. Ground-floor facades --------------------------------------------------------
    by_building = {}
    for m in ground:
        by_building.setdefault(m["building_id"], []).append(m)
    for bid, mods in by_building.items():
        mods.sort(key=lambda m: (m["street"], int(m["bay"])))
        hauss = mods[0]["typology"] == "haussmann"
        storeys = 1 + max((int(r["storey"]) for r in all_mods if r["building_id"] == bid), default=0)
        wall_h = sum(m["h"] for m in mods[:1]) + 3.1 * (storeys - 1)
        # one downpipe per building front, on the party wall side, full height (6 m pieces)
        m = mods[-1]
        if len(mods) > 1:
            for k in range(max(1, int(round(wall_h / 6.0)))):
                on_wall("W14-42", m, m["w"] / 2 - 0.12, 3.0 + 6.0 * k, out=0.06)
        meter_done = False
        for m in mods:
            u = h01(bid, m["bay"], m["street"])
            kind = m["kind"]
            if kind == "entrance":
                # door opening and stone surround of the kit (kit.py haus_entrance / lyon_entrance)
                dw, dh, sur = ((min(m["w"] - 0.9, 1.9), min(m["h"] - 0.45, 3.5), 0.25) if hauss
                               else (min(m["w"] - 0.8, 1.2), min(m["h"] - 0.5, 2.6), 0.2))
                pier = (dw / 2 + sur + m["w"] / 2) / 2                              # middle of the side pier
                on_wall("W14-41", m, pier, min(2.4, dh - 0.2), out=0.02)           # house number
                if hauss:
                    on_wall("W13-07", m, 0.0, dh + sur / 2, out=0.07)             # keystone over the door
                if not meter_done:
                    on_wall("W14-46", m, -pier, 0.75, out=0.02)                   # gas/electricity meters
                    meter_done = True
                nx, ny, tx, ty = frame(m)
                if u < 0.12:
                    add("W1-32", m["x"] + nx * 0.5 + tx * (m["w"] / 2 + 0.4), m["y"] + ny * 0.5 + ty * (m["w"] / 2 + 0.4),
                        m["z"], m["yaw"], "entrance", bid)
                elif u < 0.18:
                    add("W1-60", m["x"] + nx * 0.45 + tx * (m["w"] / 2 + 0.5), m["y"] + ny * 0.45 + ty * (m["w"] / 2 + 0.5),
                        m["z"], m["yaw"] + 360 * u, "entrance", bid)
                elif u < 0.23:
                    add("W14-72", m["x"] + nx * 0.35, m["y"] + ny * 0.35, m["z"], m["yaw"] + 20, "entrance", bid)
            elif kind in ("gf_window", "blank", "shopfront_closed"):
                if hauss and kind != "shopfront_closed" and u < 0.6:
                    on_wall("W14-45", m, 0.0, 0.25, out=0.02)                          # cellar grille
                if kind == "blank" and u > 0.9:
                    nx, ny, tx, ty = frame(m)
                    add("W14-53", m["x"] + nx * 0.35, m["y"] + ny * 0.35, m["z"], m["yaw"] + 90, "wall", bid)
            elif kind == "shop" and m["sign_continued"] == "False":
                trade = shops.get(m["shop_osm_id"], {}).get("type", "")
                top = m["h"] - 0.95                                                    # under the 60 cm sign band
                if trade in FOOD_TRADES:
                    on_wall("W1-46", m, 0.0, top, out=0.05, scale=(1.0, m["w"] / 4.0, 1.0))
                elif u < 0.5:
                    on_wall("W1-45", m, 0.0, top, out=0.05, scale=(1.0, m["w"] / 4.0, 1.0))
                if trade == "bank":
                    on_wall("W1-59", m, m["w"] / 2 - 0.6, 1.2, out=0.0)
                if trade in SMOKER_TRADES:
                    on_wall("W14-55", m, -(m["w"] / 2 - 0.15), 1.1)
                nx, ny, tx, ty = frame(m)
                if u < 0.5:
                    add("W14-34", m["x"] + nx * 0.4, m["y"] + ny * 0.4, m["z"], m["yaw"], "shop", m["shop_osm_id"])
                if trade == "greengrocer":
                    add("W14-73", m["x"] + nx * 0.7 + tx * 1.0, m["y"] + ny * 0.7 + ty * 1.0, m["z"], m["yaw"],
                        "shop", m["shop_osm_id"])
                if trade in ("bar", "pub") and u < 0.5:
                    add("W14-75", m["x"] + nx * 0.5 + tx * (m["w"] / 2 + 0.3), m["y"] + ny * 0.5 + ty * (m["w"] / 2 + 0.3),
                        m["z"], m["yaw"], "shop", m["shop_osm_id"])

    # --- 1b. Something to see behind the shop glass: a display shelf against the back wall,
    # only with see-through glass (the default opaque interior-mapped glass fakes its own room)
    for m in (ground if os.environ.get("GTP_SHOP_GLASS") == "translucent" else []):
        if m["kind"] not in ("shop", "shopfront_closed") or m["w"] < 2.0 or m["street"] != args.street:
            continue
        if module_for(m["typology"], m["kind"], m["building_id"]).endswith("Shutter"):
            continue                                                               # shutter down
        if shops.get(m["shop_osm_id"], {}).get("type", "") in ("clothes", "shoes", "boutique"):
            continue                                                               # mannequins (props.py)
        nx, ny, tx, ty = frame(m)
        off = (h01(m["building_id"], m["bay"], "shelf") - 0.5) * 0.6
        add("W14-32", m["x"] - nx * SHOP_BACK_M + tx * off, m["y"] - ny * SHOP_BACK_M + ty * off, m["z"], m["yaw"],
            "shop_window", m["shop_osm_id"] or m["building_id"], snap="keep", inside_ok=True)

    # --- 2. Upper storeys: window boxes, the odd dish or air conditioner ----------------
    for m in all_mods:
        if m["storey"] in ("0",) or m["kind"] not in ("window", "window_small", "entresol_window"):
            continue
        u = h01(m["building_id"], m["storey"], m["bay"], "up")
        if u < 0.08 and m["typology"] == "ancien":   # on the protruding sill of the Lyonnais window
            on_wall("W14-03", m, 0.0, 1.02, out=0.18)
        elif u > 0.99 and m["typology"] == "ancien":
            on_wall("W1-47", m, m["w"] / 2 - 0.35, 0.6, out=0.0)
        elif u > 0.985 and m["typology"] == "ancien":
            on_wall("W14-48", m, -(m["w"] / 2 - 0.35), 1.8, out=0.0)

    # --- 3. Street-name plates on corners ------------------------------------------------
    by_bs = {}
    for m in ground:
        if m["street"]:
            by_bs.setdefault((m["building_id"], m["street"]), []).append(m)
    for bid in by_building:
        names = [s for (b, s) in by_bs if b == bid]
        if len(names) < 2:
            continue
        for s in names:
            others = [m for t in names if t != s for m in by_bs[(bid, t)]]
            mods = by_bs[(bid, s)]
            best = min(mods, key=lambda m: min(math.hypot(m["x"] - o["x"], m["y"] - o["y"]) for o in others))
            near_end = min(others, key=lambda o: math.hypot(best["x"] - o["x"], best["y"] - o["y"]))
            nx, ny, tx, ty = frame(best)
            side = 1 if (near_end["x"] - best["x"]) * tx + (near_end["y"] - best["y"]) * ty > 0 else -1
            on_wall("W1-56", best, side * (best["w"] / 2 - 0.35), 3.3, ref_tag=s)

    # --- 4. OpenStreetMap: bollards, lights, signs, cabinets, columns, ATMs ---------------
    def to_kerb(x, y, back=0.45):
        """Move a point on the carriageway onto the nearest kerb, ``back`` metres onto the pavement."""
        if kerb_tree is None:
            return x, y
        p = Point(x, y)
        cand = [kerbs[i] for i in kerb_tree.query(p.buffer(12.0))]
        if not cand:
            return x, y
        k = min(cand, key=lambda ls: ls.distance(p))
        q = nearest_points(k, p)[0]
        dx, dy = q.x - x, q.y - y
        d = math.hypot(dx, dy) or 1.0
        return q.x + dx / d * back, q.y + dy / d * back

    def road_dir(x, y):
        p = Point(x, y)
        r = min(roads, key=lambda r: r[0].distance(p))
        ls = r[0]
        s = ls.project(p)
        a, b = ls.interpolate(max(0.0, s - 1.0)), ls.interpolate(min(ls.length, s + 1.0))
        return math.atan2(b.y - a.y, b.x - a.x), r[1]

    for oid, (lon, lat, tags) in osm_nodes(raw).items():
        ex, ey = cl.lonlat_to_l93(lon, lat)
        x, y = ex - origin[0], ey - origin[1]
        if not zone.contains(Point(x, y)):
            continue
        ref = "node/" + oid
        if tags.get("barrier") == "bollard":
            add("W1-09", x, y, ground_z(x, y), facing(x, y), "osm:bollard", ref)
        elif tags.get("highway") == "crossing" and tags.get("crossing") == "traffic_signals":
            ang, half = road_dir(x, y)
            px, py = -math.sin(ang), math.cos(ang)                    # across the road
            for s in (-1, 1):
                lx, ly = x + px * s * (half + 0.6), y + py * s * (half + 0.6)
                face = math.degrees(math.atan2(-(-py * s), -px * s))   # UE yaw: looks across the road
                add("W8-02", lx, ly, ground_z(lx, ly), face, "osm:crossing_signals", ref)
        elif tags.get("highway") == "traffic_signals":
            ang, half = road_dir(x, y)
            px, py = -math.sin(ang), math.cos(ang)
            lx, ly = x + px * (half + 0.6), y + py * (half + 0.6)       # right-hand kerb
            add("W8-01", lx, ly, ground_z(lx, ly), math.degrees(math.atan2(math.sin(ang), -math.cos(ang))),
                "osm:traffic_signals", ref)
        elif "traffic_sign" in tags and tags["traffic_sign"].startswith("FR:"):
            codes = [c.split("(")[0].strip() for c in tags["traffic_sign"][3:].replace(";", ",").split(",")]
            cid = next((SIGNS[c] for c in codes if c in SIGNS), None)
            if cid:
                sx, sy = to_kerb(x, y)
                add(cid, sx, sy, ground_z(sx, sy), facing(sx, sy), "osm:traffic_sign", ref + " " + codes[0])
        elif tags.get("man_made") == "street_cabinet":
            add("W1-30", x, y, ground_z(x, y), facing(x, y), "osm:street_cabinet", ref)
        elif tags.get("man_made") == "advertising" and tags.get("advertising") == "column":
            add("W1-19", x, y, ground_z(x, y), 0.0, "osm:advertising_column", ref)
        elif tags.get("amenity") == "motorcycle_parking":   # two-wheelers parked side by side
            yaw = facing(x, y)
            a = math.radians(yaw + 90.0)
            for k in range(2 + int(h01("moto", oid) * 3)):
                off = (k - 1.5) * 0.9
                add("W1-54", x + math.cos(a) * off, y - math.sin(a) * off, ground_z(x, y), yaw + 90.0,
                    "osm:motorcycle_parking", ref)

    # --- 5. Grand Lyon trees: autumn leaves; litter and pigeons near benches and bins ----
    for ft in grandlyon.load(raw, "trees"):
        c = ft["geometry"]["coordinates"]
        x, y = c[0] - origin[0], c[1] - origin[1]
        u = h01("leaves", ft["properties"].get("identifiant", ""), x, y)
        if u < 0.55:
            a = 2 * math.pi * u / 0.55
            lx, ly = x + math.cos(a) * 1.1, y + math.sin(a) * 1.1
            add("W14-62", lx, ly, ground_z(lx, ly), math.degrees(a), "grandlyon:trees", "leaves")
    for ft in grandlyon.load(raw, "bins"):
        c = ft["geometry"]["coordinates"]
        x, y = c[0] - origin[0], c[1] - origin[1]
        u = h01("litter", x, y)
        if u < 0.3:
            add("W14-65", x + 0.7, y + 0.3, ground_z(x, y), 360 * u, "grandlyon:bins", "litter")
        elif u < 0.4:
            add("W14-66", x - 0.6, y + 0.5, ground_z(x, y), 360 * u, "grandlyon:bins", "newspaper")
    benches = [r for r in csv.DictReader(open(os.path.join(proc, "furniture.csv"), encoding="utf-8"))
               if r["type"] == "amenity=bench"]
    for r in benches:
        x, y = float(r["x_m"]), float(r["y_m"])
        u = h01("pigeons", r["osm_id"])
        if u < 0.2:
            add("W14-69", x + 1.6 * math.cos(6.28 * u), y + 1.6 * math.sin(6.28 * u), ground_z(x, y), 360 * u,
                "osm:bench", "pigeons")

    # --- 5b. Bicycles locked to the surveyed hoops of props.py, shared kick scooters nearby
    props_csv = os.path.join(gen, "props_placement.csv")
    if os.path.exists(props_csv):
        with open(props_csv, encoding="utf-8") as f:
            hoops = [r for r in csv.DictReader(f) if r["catalog_id"] == "W1-12"]
        for r in hoops:
            x, y, yaw = float(r["x_m"]), float(r["y_m"]), float(r["ue_yaw"])
            u = h01("bike", r["x_m"], r["y_m"])
            a = math.radians(yaw + 90.0)                       # along the facade, beside the hoop
            if u < 0.3:
                add("W1-14", x + math.cos(a) * 0.18, y - math.sin(a) * 0.18, ground_z(x, y), yaw,
                    "props:bike_hoop", r["ref"])
            elif u > 0.93:
                add("W1-55", x + math.cos(a) * 0.6, y - math.sin(a) * 0.6, ground_z(x, y), yaw + 70.0 * (u - 0.9),
                    "props:bike_hoop", "kick scooter")

    # --- 6. Roofs: chimney stacks on the party walls -------------------------------------
    for b in buildings:
        g = bgen.get(b["key"])
        if g is None:
            continue
        top = float(g["top_z"])
        ring = list(b["poly"].exterior.coords)
        n = 0
        for (ax, ay), (bx, by) in zip(ring, ring[1:]):
            seg = LineString([(ax, ay), (bx, by)])
            if seg.length < 4.0:
                continue
            mid = seg.interpolate(0.5, normalized=True)
            shared = any(footprints[i] is not b["poly"] and footprints[i].distance(mid) < 0.6
                         for i in fp_tree.query(mid.buffer(0.6)))
            if not shared or n >= 2:
                continue
            ix, iy = b["poly"].centroid.x - mid.x, b["poly"].centroid.y - mid.y
            d = math.hypot(ix, iy) or 1.0
            cx, cy = mid.x + ix / d * 0.7, mid.y + iy / d * 0.7       # just inside the party wall
            yaw = math.degrees(math.atan2(-(by - ay), bx - ax))
            if add("W13-19", cx, cy, top - 0.4, yaw, "roof:party_wall", b["key"], snap="keep", inside_ok=True):
                n += 1

    # --- per-group switches (perf passes), then cap by priority, write -----------------------
    rows, switches = apply_switches(rows, os.environ)
    total = len(rows)
    rows.sort(key=lambda r: PRIORITY.get(r["catalog_id"], 5))
    rows = rows[:args.max]
    write_tables(gen, rows, total, args, switches, t0)


def write_tables(gen, rows, total, args, switches, t0):
    fields = ["catalog_id", "asset", "family", "asset_path", "x_m", "y_m", "z_m", "ue_x", "ue_y", "ue_z", "ue_yaw",
              "source", "ref", "snap", "scale_x", "scale_y", "scale_z", "collision", "shadow", "cull_m"]
    with open(os.path.join(gen, "details_placement.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fields)
        w.writeheader()
        w.writerows(rows)
    by = {}
    for r in rows:
        k = r["catalog_id"] + " " + r["asset"]
        by[k] = by.get(k, 0) + 1
    report = {"details": len(rows), "before_cap": total, "cap": args.max, "asset_types": len(by),
              "switches": switches,
              "by_asset": dict(sorted(by.items())), "seconds": round(time.time() - t0, 1)}
    with open(os.path.join(gen, "details_report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    print(json.dumps(report, indent=1))


if __name__ == "__main__":
    main()
