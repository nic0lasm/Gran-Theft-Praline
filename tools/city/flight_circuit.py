"""The circuit of the flight: one closed way on the lanes of a map, chosen here, kept in Git.

Until gate 78 the filmed flight of -GTARun was free on the 235 lanes of L_City_RueDeLaRepublique
and the script put the car back on a lane 4 to 8 times in four minutes. From now on it drives one
lap, again and again: GranTheftPraline/Content/Test/Circuits/<Map>.circuit.csv, read by the game
(GTPFlightCircuit.h). A row names a lane by where it starts and ends (Unreal cm); its number in
the lanes file is only a hint, the numbers move when tools/city/traffic.py makes the file again.

Plain Python, no dependency (Pillow only for --png; numpy, scipy and shapely only for write --town).

  find   <lanes.csv> [--avoid 42,4] [--top 12]
         every closed way without a lane taken twice, the longest first: metres, share of main
         axes (carriageway of 7 m and more, as GTPRoadWidth.h) and of narrow streets (under 5 m)
  write  <lanes.csv> --lanes 214,7,95,... [--approach 188,215,...] --out <Map>.circuit.csv
         [--speeds "QU JULES COURMONT=100,R STELLA=60"] [--roads-out through_roads/<focus>.txt]
         [--end 373,379,381] [--town <processed data of the zone>]
         the circuit file of that lap, checked first, each lane with the line driven on it
         (driving_line: straights straightened, corners rounded). The approach: the lanes driven
         from where the flight starts to the lap, in order; without one the game takes the
         shortest way to the lap. --speeds: the speed the flight asks street by street (column
         kmh; without one the game goes by the width the lanes file gives). --roads-out: the
         BD TOPO roads of the circuit, which tools/city/traffic.py then keeps whole. --end: the
         lanes of the lap the filmed flight is to end on (column mark): -GTARun holds the end
         of its pursuit until the car is there, 20 s at most. --town: the folder of the zone's
         processed data (buildings.geojson, landmarks.json, water.geojson, carriageway.geojson):
         the line is then eased among the walls of the town (line_room.py): taken round what
         stands on it, kept 2.8 m from the walls and on the carriageway, each bend entered from
         its outside and cut at its inside, as wide as the room allows. A row whose line goes
         further than 8 m from its lane says so (column mark: round)
  check  <circuit.csv> <lanes.csv> [--report lane_clear_report.json] [--png lap.png]
         [--carriageway carriageway.geojson] [--min-main 70] [--town <processed data>]
         the lap against the lanes file the game reads: every row finds its lane, each lane
         leads to the next, the last to the first; metres, widths, turns; the driving lines stay
         within 8 m of their lanes (16 m on a row marked round) and follow one another. With
         the town (--town, or the files next to --carriageway): how near the line comes to a
         wall and its tightest bend; a line through a wall fails. With the report of the
         prep step "lanes" (tools/city/unreal/clear_lane_instances.py): nothing the step only
         reported (facade modules, signs: it never takes those out) may stand on a lane of the
         lap. Main axes are 70 % of the lap or more (--min-main): by the lanes file, or with
         --carriageway by the kerbs measured on the ground (tools/city/carriageway.py), which is
         what the rule means. Exit 0 when the lap holds, 1 when it does not.

The lap of L_City_RueDeLaRepublique (2026-11-24) goes through the place Bellecour, the shop window
of the town: 1793 m, 37 lanes. South on the quai Jules Courmont (346 m), the rue Childebert, the
place de la Republique and its basin, the rue Marcel-Gabriel Riviere, the place de l'Hopital, the
rue Bellecordiere, the rue de la Barre, the north-east corner of the place Bellecour, north on
the rue du President Edouard Herriot past the place des Jacobins, the rue Jean de Tournes, the
rue Stella, the rue Grolee, the rue du President Carnot and right onto the quay again. The theft
of -GTARun is on the quai Jean Moulin, northbound, 400 m north of the lap: the approach turns
back through the junction of the pont Lafayette (397, 161, 148) and runs down the quay into the
rue Childebert (488 m).
Two things kept the lanes file from holding that lap until gate 80, both in tools/city/traffic.py:
a road segment of 2.7 m of the rue Herriot at the place des Jacobins was dropped as too short and
cut the street; and lanes were only made within 60 m of a generated building, which cut 610 m
out of this lap. The roads of the circuit are now listed in tools/city/through_roads/<focus>.txt
(write --roads-out) and kept whole. The lap before, 817 m round the place des Cordeliers, did not
show Bellecour.
By the lanes file 41 % of the lap is on main axes: it counts a one-way street by its one lane of
3 m. By the kerbs measured on the ground (carriageway.geojson) 77 % of it is 7 m wide or more.
Since gate 86 the line is written with --town. Its long take showed what the line of the lanes
alone could not hold: five right-hand corners of 2 to 4 m of radius, which no car turns (the car
ran wide or went round at full lock, and was put back on a lane six times); a building 1.6 m from
the line on the place de l'Hopital; and the carrousel of the place de la Republique, a stand-in
whose estimated place was on the carriageway of the rue Jean de Tournes. At gate 90 the line went
round the carrousel by the north, over the place. Since gate 91 the carrousel itself stands north
of the street, on the place (lane_clear.LANDMARK_MOVES; line_room.py reads it there whatever the data
say), and the line runs straight down the street again. It comes no nearer than 2.8 m to a wall,
and its tightest bend has a radius of 8.8 m (the turn back of the approach; 9.3 m on the lap).
"""

import argparse
import csv
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import line_room  # noqa: E402

MAIN_M = 7.0            # GTPRoadWidth::MainMetres
NARROW_M = 5.0          # GTPRoadWidth::NarrowMetres
SAME_ROAD_M = 20.0      # GTPRoadWidth::SameRoadMetres
MATCH_M = 3.0           # GTPFlightCircuit::MatchMetres
MIN_MAIN_SHARE = 0.70   # the roadmap's rule for the flight circuit
HEADER = ["lane_id", "start_x_cm", "start_y_cm", "end_x_cm", "end_y_cm", "name", "line_ue_cm", "kmh", "mark"]
LINE_STEP_M = 3.0       # spacing of the driving line's points (the lanes file has one every 4 m)
CORNER_BASE_M = 12.0    # a corner is told by the headings this far before and after a point
CORNER_DEG = 40.0       # ... when they differ by this much
CORNER_ZONE_M = 12.0    # around a corner the line is only rounded, not straightened
SIGMA_STRAIGHT_M = 10.0  # how far a jog of the lanes is spread along a straight
SIGMA_RIGHT_M = 1.6     # rounding of a right-hand corner: its inside is the kerb (about 0.9 m cut at 90 degrees)
SIGMA_LEFT_M = 4.5      # rounding of a left-hand corner: its inside is the middle of the junction (about 2.5 m)
MAX_LINE_GAP_M = 8.0    # the line may leave its lane by this much (a corner taken from the outside to the inside)
LINE_ENDS_M = 16.0      # GTPFlightCircuit::LineEndsMetres: a lane's line starts and ends this near the lane's ends
ROUND_MARK = "round"    # mark of a row whose line goes round something that stands on its lane: up to LINE_ENDS_M from it


def read_lanes(path):
    """[{id, road, name, width, points [(x, y) m], next [ids], length m, carriageway m, kind}]."""
    lanes = []
    with open(path, encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            pts = []
            for p in r["points_ue_cm"].split(";"):
                v = p.split()
                if len(v) >= 2:
                    pts.append((float(v[0]) / 100.0, float(v[1]) / 100.0))
            lanes.append({"id": int(r["lane_id"]), "road": (r.get("road_id") or "").strip(),
                          "name": (r.get("name") or "").strip(),
                          "width": max(float(r.get("width_m") or 0.0), 0.0), "points": pts,
                          "next": [int(n) for n in (r.get("next") or "").split()]})
    by_id = {lane["id"]: lane for lane in lanes}
    for lane in lanes:
        lane["next"] = [n for n in lane["next"] if n in by_id]
        pts = lane["points"]
        lane["length"] = sum(math.dist(pts[i], pts[i - 1]) for i in range(1, len(pts)))
    carriageways(lanes)
    return lanes


def carriageways(lanes):
    """GTPRoadWidth::Carriageways: a lane's width plus that of the lanes of its road near its middle."""
    by_road = {}
    for lane in lanes:
        lane["carriageway"] = 0.0
        if lane["road"] and lane["width"] > 0.0 and lane["points"]:
            by_road.setdefault(lane["road"], []).append(lane)
    for group in by_road.values():
        for lane in group:
            middle = lane["points"][len(lane["points"]) // 2]
            lane["carriageway"] = sum(
                other["width"] for other in group
                if other is lane or any(math.dist(p, middle) <= SAME_ROAD_M for p in other["points"]))
    for lane in lanes:
        w = lane["carriageway"]
        lane["kind"] = "unknown" if w <= 0.0 else "main" if w >= MAIN_M else "middle" if w >= NARROW_M else "narrow"


def closed_ways(lanes, avoid=()):
    """Every closed way that takes no lane twice, as lists of lane ids starting at their lowest id."""
    by_id = {lane["id"]: lane for lane in lanes}
    out = []

    def walk(start, at, path, seen):
        for nxt in by_id[at]["next"]:
            if nxt == start:
                out.append(list(path))
            elif nxt > start and nxt not in seen and nxt not in avoid:
                seen.add(nxt)
                path.append(nxt)
                walk(start, nxt, path, seen)
                path.pop()
                seen.discard(nxt)

    for start in sorted(by_id):
        if start not in avoid:
            walk(start, start, [start], {start})
    return out


def heading(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def wrap(deg):
    return (deg + 180.0) % 360.0 - 180.0


def driving_line(lap, by_id, walls=None, roads=None, fast=()):
    """The line a flight drives on the lap: [(lane id, [(x, y) m])] in lap order, one closed curve.

    The lanes of tools/city/traffic.py are road centre lines pushed aside: a lane's points wander by
    a metre, a lane starts a metre or two beside or behind the end of the one before, and the
    game's drivers read each of those steps as a bend (GTPDriveLogic.h, a 15 degree step is worth
    42 km/h): three laps driven with the game's decisions on the raw lanes average 39 km/h and
    never pass 73, whatever speed is asked. Here the lap becomes one curve: steps back removed,
    points every LINE_STEP_M, straights straightened (a jog spread over SIGMA_STRAIGHT_M), corners
    kept where they are and rounded a little, more to the left than to the right. On the lanes `fast` (those the
    flight is asked line_room.FAST_KMH or more on) the curve first leaves the kerbs by a lane's width, where the
    carriageway has it (line_room.off_kerbs; roads, as line_room.read_roads gives them).
    """
    # 1. One closed polyline, each point with its lane; no step back at a joint.
    points = []
    for lane_id in lap:
        for p in by_id[lane_id]["points"]:
            points.append((p[0], p[1], lane_id))
    changed = True
    while changed:
        changed = False
        n = len(points)
        for i in range(n):
            a, b, c = points[i - 1], points[i], points[(i + 1) % n]
            if math.dist(a[:2], b[:2]) < 0.05:
                del points[i]
                changed = True
                break
            if math.dist(b[:2], c[:2]) >= 0.05 and abs(wrap(heading(b, c) - heading(a, b))) > 100.0:
                del points[i]
                changed = True
                break
    # 2. A point every LINE_STEP_M along it.
    n = len(points)
    seg = [math.dist(points[i][:2], points[(i + 1) % n][:2]) for i in range(n)]
    total = sum(seg)
    count = max(int(round(total / LINE_STEP_M)), 8)
    step = total / count
    curve, owner = [], []
    at, i = 0.0, 0
    for k in range(count):
        want = k * step
        while at + seg[i] < want and i < n - 1:
            at += seg[i]
            i += 1
        t = (want - at) / seg[i] if seg[i] > 0.0 else 0.0
        a, b = points[i], points[(i + 1) % n]
        curve.append([a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t])
        owner.append(a[2])
    # 3 and 4. Corners kept and rounded, straights straightened.
    corners = smooth(curve, step, closed=True)
    # 4b. With the walls of the town: round what stands on the line, then eased, bends as wide as the room allows.
    corners = [(owner[k], angle) for k, angle in corners]
    if roads is not None and fast:
        curve = line_room.off_kerbs(curve, roads, [line_room.KERB_FAST_M if owner[k] in fast else line_room.KERB_M for k in range(count)],
                                    step, closed=True)[0]
    if walls is not None:
        rounds = line_room.go_round(curve, walls, closed=True)
        curve, by = line_room.ease(curve, walls, step, closed=True, round_names={r[0] for r in rounds}, roads=roads)
        owner = [owner[k] for k in by]
    # 5. Each lane's share of the curve, in lap order (the curve starts on the first lane): the points up to the
    #    first one that belongs to a later lane of the lap. A lane of a few metres gets two points from its
    #    neighbours: the game needs two to drive a lane.
    place_of = {lane_id: place for place, lane_id in enumerate(lap)}
    shares = [[] for _ in lap]
    place = 0
    for k in range(count):
        place = max(place, place_of[owner[k]])
        shares[place].append((curve[k][0], curve[k][1]))
    for _ in range(4 * len(lap)):
        short = [i for i in range(len(lap)) if len(shares[i]) < 2]
        if not short:
            break
        i = short[0]
        before, after = shares[i - 1], shares[(i + 1) % len(lap)]
        if len(before) >= len(after) and len(before) > 2:
            shares[i].insert(0, before.pop())
        elif len(after) > 2:
            shares[i].append(after.pop(0))
        elif len(before) > 2:
            shares[i].insert(0, before.pop())
        else:
            break
    lines = [(lane_id, shares[index]) for index, lane_id in enumerate(lap)]
    return lines, corners


def smooth(curve, step, closed, pinned=()):
    """Straightens the straights and rounds the corners of curve ([[x, y]], a point every step m), in place.

    Corners: where the headings CORNER_BASE_M before and after a point differ by CORNER_DEG or more. Around one the
    curve is only rounded (SIGMA_RIGHT_M, SIGMA_LEFT_M), elsewhere a jog is spread over SIGMA_STRAIGHT_M. The points
    whose index is in pinned do not move; neither do the two ends of an open curve. Returns [(index, degrees)].
    """
    count = len(curve)

    def at(k):
        return k % count if closed else min(max(k, 0), count - 1)

    base = max(int(round(CORNER_BASE_M / step)), 2)
    turn = [wrap(heading(curve[k], curve[at(k + base)]) - heading(curve[at(k - base)], curve[k]))
            if closed or base <= k < count - base else 0.0 for k in range(count)]
    sigma = [SIGMA_STRAIGHT_M] * count
    zone = max(int(round(CORNER_ZONE_M / step)), 1)
    corners = []
    for k in range(count):
        if abs(turn[k]) >= CORNER_DEG and all(abs(turn[k]) >= abs(turn[at(k + d)]) for d in range(-zone, zone + 1)):
            corners.append((k, turn[k]))
    for k, angle in corners:
        tight = SIGMA_RIGHT_M if angle > 0.0 else SIGMA_LEFT_M
        for d in range(-2 * zone, 2 * zone + 1):
            if not closed and not 0 <= k + d < count:
                continue
            # Rounded only within the zone, then more and more straightened up to twice the zone.
            share = min(max((abs(d) - zone) / float(zone), 0.0), 1.0)
            index = at(k + d)
            sigma[index] = min(sigma[index], tight + (SIGMA_STRAIGHT_M - tight) * share)
    # Smoothing by repeated averaging with the two neighbours: n passes are worth a sigma of step * sqrt(n / 2).
    still = set(pinned) | (set() if closed else {0, count - 1})
    passes = [0 if k in still else int(round(2.0 * (sigma[k] / step) ** 2)) for k in range(count)]
    for done in range(max(passes)):
        before = [p[:] for p in curve]
        for k in range(count):
            if passes[k] > done:
                a, b, c = before[at(k - 1)], before[k], before[at(k + 1)]
                curve[k] = [0.25 * a[0] + 0.5 * b[0] + 0.25 * c[0], 0.25 * a[1] + 0.5 * b[1] + 0.25 * c[1]]
    return corners


def approach_line(approach, lap, lap_lines, by_id, walls=None, roads=None, fast=()):
    """The line driven on the approach: [(lane id, [(x, y) m])], from the start of its first lane to the lap's line.

    The approach's lanes as one open curve that runs on into the line of the lap (lap_lines, as driving_line gives
    it) from the lane the approach leads to; smoothed as the lap is, its first point and the lap's line kept where
    they are.
    """
    entry = next(n for n in by_id[approach[-1]]["next"] if n in lap)
    place = lap.index(entry)
    lines = dict(lap_lines)
    tail = []
    for k in range(len(lap)):
        tail += lines[lap[(place + k) % len(lap)]]
        if len(tail) * LINE_STEP_M >= 4.0 * CORNER_ZONE_M:
            break
    points = [(p[0], p[1], lane_id) for lane_id in approach for p in by_id[lane_id]["points"]] + [(p[0], p[1], None) for p in tail]
    # No step back, and none onto the lap's line: the lane's last metres go when the line starts behind them.
    changed = True
    while changed:
        changed = False
        for i in range(1, len(points) - 1):
            a, b, c = points[i - 1], points[i], points[i + 1]
            if b[2] is None:
                continue
            if math.dist(a[:2], b[:2]) < 0.05 or (math.dist(b[:2], c[:2]) >= 0.05 and abs(wrap(heading(b, c) - heading(a, b))) > 100.0):
                del points[i]
                changed = True
                break
    own = [p for p in points if p[2] is not None]
    seg = [math.dist(own[i][:2], (own + [(tail[0][0], tail[0][1], None)])[i + 1][:2]) for i in range(len(own))]
    total = sum(seg)
    count = max(int(round(total / LINE_STEP_M)), 2 * len(approach))
    step = total / count
    curve, owner = [], []
    at, i = 0.0, 0
    ends = own + [(tail[0][0], tail[0][1], None)]
    for k in range(count):
        want = k * step
        while at + seg[i] < want and i < len(seg) - 1:
            at += seg[i]
            i += 1
        t = (want - at) / seg[i] if seg[i] > 0.0 else 0.0
        a, b = ends[i], ends[i + 1]
        curve.append([a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t])
        owner.append(a[2])
    whole = curve + [[p[0], p[1]] for p in tail]
    smooth(whole, step, closed=False, pinned=range(count, len(whole)))
    if roads is not None and fast:
        want = [line_room.KERB_FAST_M if k < count and owner[k] in fast else line_room.KERB_M for k in range(len(whole))]
        whole = line_room.off_kerbs(whole, roads, want, step, closed=False, still={0} | set(range(count, len(whole))))[0]
    if walls is not None:
        whole, by = line_room.ease(whole, walls, step, closed=False, tail=len(tail), roads=roads)
        owner = [owner[k] if k < count else owner[-1] for k in by[:count]]
    place_of = {lane_id: index for index, lane_id in enumerate(approach)}
    shares = [[] for _ in approach]
    index = 0
    for k in range(count):
        index = max(index, place_of[owner[k]])
        shares[index].append((whole[k][0], whole[k][1]))
    for _ in range(4 * len(approach)):
        short = [i for i in range(len(approach)) if len(shares[i]) < 2]
        if not short:
            break
        i = short[0]
        before = shares[i - 1] if i > 0 else []
        after = shares[i + 1] if i + 1 < len(approach) else []
        if len(before) >= len(after) and len(before) > 2:
            shares[i].insert(0, before.pop())
        elif len(after) > 2:
            shares[i].append(after.pop(0))
        elif len(before) > 2:
            shares[i].insert(0, before.pop())
        else:
            break
    return [(lane_id, shares[index]) for index, lane_id in enumerate(approach)]


def point_to_polyline(p, pts):
    best = float("inf")
    for i in range(len(pts) - 1):
        a, b = pts[i], pts[i + 1]
        dx, dy = b[0] - a[0], b[1] - a[1]
        l2 = dx * dx + dy * dy
        t = min(max(((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l2, 0.0), 1.0) if l2 > 0.0 else 0.0
        best = min(best, math.hypot(p[0] - a[0] - dx * t, p[1] - a[1] - dy * t))
    return best


def line_errors(lap, lines_by_lane, by_id, round_lanes=()):
    """What keeps the rows' driving lines from being used (GTPFlightCircuit::LinesFit); empty when they fit.
    round_lanes: the lanes whose row says its line goes round something (ROUND_MARK)."""
    errors = []
    for lane in lap:
        if len(lines_by_lane.get(lane, [])) < 2:
            errors.append("lane %d has no driving line of two points or more" % lane)
    if errors:
        return errors
    for place, lane in enumerate(lap):
        line = lines_by_lane[lane]
        after = lines_by_lane[lap[(place + 1) % len(lap)]]
        away = max(point_to_polyline(p, by_id[lane]["points"]) for p in line)
        allowed = LINE_ENDS_M if lane in round_lanes else MAX_LINE_GAP_M
        if away > allowed:
            errors.append("the driving line of lane %d is %.1f m from the lane (%.1f allowed)" % (lane, away, allowed))
        if math.dist(line[-1], after[0]) > 2.0 * LINE_STEP_M:
            errors.append("the driving line of lane %d ends %.1f m from the next one" % (lane, math.dist(line[-1], after[0])))
        errors += line_ends_errors(lane, line, by_id)
    return errors


def line_ends_errors(lane, line, by_id):
    """The game takes a lane's line only when it starts and ends near the lane's ends (LineEndsMetres)."""
    points = by_id[lane]["points"]
    off = max(math.dist(line[0], points[0]), math.dist(line[-1], points[-1]))
    if off > LINE_ENDS_M:
        return ["the driving line of lane %d starts or ends %.1f m from the lane's ends (%.1f allowed): the game would not use it"
                % (lane, off, LINE_ENDS_M)]
    return []


def approach_errors(approach, lap, lanes):
    """What keeps a list of lane ids from being an approach to the lap (GTPFlightCircuit::SetApproach)."""
    by_id = {lane["id"]: lane for lane in lanes}
    errors = ["lane %d of the approach is of the lap or not of the network" % lane for lane in approach if lane not in by_id or lane in lap]
    if len(set(approach)) != len(approach):
        errors.append("a lane comes twice in the approach")
    if errors:
        return errors
    for place, lane in enumerate(approach):
        if place + 1 < len(approach):
            if approach[place + 1] not in by_id[lane]["next"]:
                errors.append("lane %d of the approach does not lead to lane %d" % (lane, approach[place + 1]))
        elif not any(n in lap for n in by_id[lane]["next"]):
            errors.append("lane %d of the approach does not lead to the lap" % lane)
    return errors


def approach_line_errors(approach, lap, lines_by_lane, by_id, round_lanes=()):
    """What keeps the approach's driving lines from being used (GTPFlightCircuit::SetApproachLines)."""
    errors = ["lane %d of the approach has no driving line of two points or more" % lane
              for lane in approach if len(lines_by_lane.get(lane, [])) < 2]
    entry = next(n for n in by_id[approach[-1]]["next"] if n in lap)
    if len(lines_by_lane.get(entry, [])) < 2:
        errors.append("lane %d of the lap, where the approach ends, has no driving line" % entry)
    if errors:
        return errors
    for place, lane in enumerate(approach):
        line = lines_by_lane[lane]
        after = lines_by_lane[approach[place + 1] if place + 1 < len(approach) else entry]
        away = max(point_to_polyline(p, by_id[lane]["points"]) for p in line)
        allowed = LINE_ENDS_M if lane in round_lanes else MAX_LINE_GAP_M
        if away > allowed:
            errors.append("the driving line of lane %d of the approach is %.1f m from the lane (%.1f allowed)" % (lane, away, allowed))
        if math.dist(line[-1], after[0]) > 2.0 * LINE_STEP_M:
            errors.append("the driving line of lane %d of the approach ends %.1f m from the next one" % (lane, math.dist(line[-1], after[0])))
        errors += line_ends_errors(lane, line, by_id)
    return errors


def measured_widths(path):
    """Kerb to kerb width of each measured road (tools/city/carriageway.py): road id -> median width (m)."""
    with open(path, encoding="utf-8") as f:
        features = json.load(f)["features"]
    out = {}
    for feature in features:
        halves = sorted(float(h) for h in feature["properties"].get("half_width_m") or [])
        if halves:
            out[feature["properties"]["road_id"]] = 2.0 * halves[len(halves) // 2]
    return out


def measured_main_share(lap, by_id, widths):
    """Share of the lap on carriageways of MAIN_M and more by the measured kerbs; a road not measured counts by its lanes."""
    total = sum(by_id[i]["length"] for i in lap)
    main = sum(by_id[i]["length"] for i in lap if widths.get(by_id[i]["road"], by_id[i]["carriageway"]) >= MAIN_M)
    measured = sum(by_id[i]["length"] for i in lap if by_id[i]["road"] in widths)
    return main / total, measured / total


def turn_between(before, after):
    """Change of heading from the end of a lane to the start of the next (degrees, right positive: Y is south)."""
    h1 = heading(before["points"][-2], before["points"][-1])
    h2 = heading(after["points"][0], after["points"][1])
    return (h2 - h1 + 180.0) % 360.0 - 180.0


def lap_numbers(lap, by_id):
    total = sum(by_id[i]["length"] for i in lap)
    share = {kind: sum(by_id[i]["length"] for i in lap if by_id[i]["kind"] == kind) / total
             for kind in ("main", "middle", "narrow", "unknown")}
    return total, share


def read_circuit(path):
    """The rows of a circuit file; those after a line that reads "approach" are the approach's, after "lap" the lap's."""
    rows = []
    in_approach = False
    with open(path, encoding="utf-8-sig") as f:
        for line in f:
            fields = [c.strip() for c in line.strip().split(",")]
            if len(fields) == 1 and fields[0] in ("approach", "lap"):
                in_approach = fields[0] == "approach"
                continue
            if len(fields) < 5 or not fields[0] or fields[0].startswith("#"):
                continue
            try:
                hint = int(fields[0])
            except ValueError:
                continue
            line = []
            for pair in (fields[6].split(";") if len(fields) > 6 else []):
                v = pair.split()
                if len(v) >= 2:
                    line.append((float(v[0]) / 100.0, float(v[1]) / 100.0))
            rows.append({"hint": hint, "start": (float(fields[1]) / 100.0, float(fields[2]) / 100.0),
                         "end": (float(fields[3]) / 100.0, float(fields[4]) / 100.0),
                         "name": fields[5] if len(fields) > 5 else "", "line": line, "approach": in_approach,
                         "kmh": float(fields[7]) if len(fields) > 7 and fields[7] else 0.0,
                         "end_place": len(fields) > 8 and fields[8] == "end",
                         "round": len(fields) > 8 and fields[8] == ROUND_MARK})
    return rows


def resolve(rows, lanes):
    """GTPFlightCircuit::Resolve: (lane ids, rows found under another number), or raises ValueError."""
    by_id = {lane["id"]: lane for lane in lanes}

    def gap(row, lane):
        return max(math.dist(lane["points"][0], row["start"]), math.dist(lane["points"][-1], row["end"]))

    lap = []
    renumbered = 0
    for index, row in enumerate(rows):
        found = None
        hinted = by_id.get(row["hint"])
        if hinted is not None and hinted["points"] and gap(row, hinted) <= MATCH_M:
            found = hinted["id"]
        else:
            best = MATCH_M
            for lane in lanes:
                if lane["points"] and gap(row, lane) <= best:
                    best = gap(row, lane)
                    found = lane["id"]
            renumbered += 1 if found is not None else 0
        if found is None:
            raise ValueError("row %d (lane %d of the file it was made from): no lane of the network starts and ends there"
                             % (index + 1, row["hint"]))
        lap.append(found)
    return lap, renumbered


def lap_errors(lap, lanes):
    """What keeps a list of lane ids from being a lap (GTPFlightCircuit::Build); empty when it is one."""
    by_id = {lane["id"]: lane for lane in lanes}
    errors = []
    if len(lap) < 3:
        errors.append("a lap needs three lanes or more, this one has %d" % len(lap))
    for lane in lap:
        if lane not in by_id:
            errors.append("lane %d is not of the network" % lane)
    if len(set(lap)) != len(lap):
        errors.append("a lane comes twice in the lap")
    if errors:
        return errors
    for place, lane in enumerate(lap):
        after = lap[(place + 1) % len(lap)]
        if after not in by_id[lane]["next"]:
            errors.append("lane %d does not lead to lane %d" % (lane, after))
    return errors


def reaching(lap, lanes):
    """Lanes from which the lap is reached, those of the lap included."""
    reach = set(lap)
    changed = True
    while changed:
        changed = False
        for lane in lanes:
            if lane["id"] not in reach and any(n in reach for n in lane["next"]):
                reach.add(lane["id"])
                changed = True
    return reach


def write_circuit(path, lap, lanes, source, approach=(), speeds=None, end_places=(), walls=None, roads=None):
    by_id = {lane["id"]: lane for lane in lanes}
    total, share = lap_numbers(lap, by_id)
    # Where the flight is asked a quay's speed, its line keeps a lane's width from the kerbs.
    fast = {lane_id for lane_id in list(lap) + list(approach)
            if (speeds or {}).get(by_id[lane_id]["name"].upper(), 0.0) >= line_room.FAST_KMH}
    lap_lines = driving_line(lap, by_id, walls, roads, fast)[0]
    lines = dict(lap_lines)
    if approach:
        lines.update(dict(approach_line(list(approach), lap, lap_lines, by_id, walls, roads, fast)))
    # A row whose line goes round something that stands on its lane says so: it may be further from the lane.
    rounds = {lane_id for lane_id, line in lines.items()
              if max(point_to_polyline(p, by_id[lane_id]["points"]) for p in line) > MAX_LINE_GAP_M}
    if rounds & set(end_places):
        raise ValueError("lane(s) %s: a row has one mark, the film cannot end where the line goes round something" % sorted(rounds & set(end_places)))

    def row(lane_id):
        lane = by_id[lane_id]
        a, b = lane["points"][0], lane["points"][-1]
        kmh = (speeds or {}).get(lane["name"].upper(), 0.0)
        return "%d,%.0f,%.0f,%.0f,%.0f,%s,%s,%s,%s\n" % (lane_id, a[0] * 100.0, a[1] * 100.0, b[0] * 100.0, b[1] * 100.0,
                                                        lane["name"].replace(",", " "),
                                                        ";".join("%.0f %.0f" % (p[0] * 100.0, p[1] * 100.0) for p in lines.get(lane_id, [])),
                                                        ("%.0f" % kmh) if kmh > 0.0 else "",
                                                        "end" if lane_id in end_places else (ROUND_MARK if lane_id in rounds else ""))

    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# The circuit of the flight (GTPFlightCircuit.h), made by tools/city/flight_circuit.py write from %s.\n" % source)
        f.write("# One lap: %d lanes, %.0f m, %.0f %% on main axes, %.0f %% in narrow streets. The last lane leads to the first.\n"
                % (len(lap), total, share["main"] * 100.0, share["narrow"] * 100.0))
        if approach:
            f.write("# Before it the approach: %d lanes, %.0f m, from where the flight starts to the lap.\n"
                    % (len(approach), sum(by_id[i]["length"] for i in approach)))
        f.write("# A row is a lane by where it starts and ends (Unreal cm); lane_id is its number in that lanes file, a hint.\n")
        f.write("# line_ue_cm: the line driven on that lane, one closed curve over the lap: straights straightened, corners rounded%s.\n"
                % ("" if walls is None else ", then eased among the walls of the town (line_room.py)"))
        f.write("# kmh: the speed asked on that lane; empty: by the width the lanes file gives its road (GTPFlightCircuit::CruiseKmh).\n")
        f.write("# mark: end on the lanes of the lap the filmed flight is to end on (-GTARun holds the end of its pursuit until there);\n")
        f.write("#       round where the line goes round something that stands on the lane (the game reads only end).\n")
        f.write(",".join(HEADER) + "\n")
        if approach:
            f.write("approach\n")
            for lane_id in approach:
                f.write(row(lane_id))
            f.write("lap\n")
        for lane_id in lap:
            f.write(row(lane_id))


def room_report(lap, approach, lines_by_lane, walls):
    """(lines to print, failed) on the room the driving line has among the walls of the town: the least distance to a
    wall and the tightest bend. Fails only a line that runs through a wall."""
    lap_points = [p for lane in lap for p in lines_by_lane.get(lane, [])]
    approach_points = [p for lane in approach for p in lines_by_lane.get(lane, [])]
    if not lap_points:
        return [], False
    least, at, name = min(line_room.room(lap_points + lap_points[:1], walls), line_room.room(approach_points, walls) if approach_points else (float("inf"), None, ""))
    radius, where = line_room.least_radius(lap_points, closed=True)
    if approach_points:
        radius, where = min((radius, where), line_room.least_radius(approach_points, closed=False))
    out = ["  room: %d wall(s) of the town; the line is %.1f m from the nearest (%s, at %.0f %.0f cm), %.1f m wanted; its tightest bend has a radius of %.1f m (at %.0f %.0f cm)"
           % (len(walls), least, name or "-", at[0] * 100.0 if at else 0.0, at[1] * 100.0 if at else 0.0, line_room.CLEAR_ROUND_M,
              radius, where[0] * 100.0 if where else 0.0, where[1] * 100.0 if where else 0.0)]
    if least < line_room.THROUGH_M:
        out.append("flight circuit: FAIL the driving line runs through %s" % name)
    return out, least < line_room.THROUGH_M


def is_landmark(item):
    """A landmark of the real city (the carrousel of place de la Republique) is never taken out of the map, and the
    prep step measures a plain actor by its bounding box, which a round building fills only in part: it is told, not failed."""
    return int(item.get("index", 0)) == -1 and str(item.get("folder", "")).startswith("City/Landmarks")


def landmarks_not_moved(report):
    """Labels of lane_clear.LANDMARK_MOVES the prep step did not move: line_room.py draws the line for their new
    place whatever the map says, so a carrousel left on the carriageway would stand in the line's way unseen."""
    import lane_clear  # noqa: PLC0415

    moved = {entry.get("label") for entry in report.get("landmarks_moved", []) if entry.get("moved")}
    return sorted(label for label in lane_clear.LANDMARK_MOVES if label not in moved)


def report_blockers(report_path, lap):
    """Objects the prep step "lanes" only reported (never taken out) on a lane of the lap."""
    with open(report_path, encoding="utf-8-sig") as f:
        report = json.load(f)
    on_lap = {str(lane) for lane in lap}
    kept = [item for item in report.get("reported_only", []) if str(item.get("lane", "")).split(" ")[0] in on_lap]
    removed = [item for item in report.get("removed_instances", []) if str(item.get("lane", "")).split(" ")[0] in on_lap]
    return kept, removed, report


def draw(path, lanes, lap, lines_by_lane=None, approach=()):
    from PIL import Image, ImageDraw  # noqa: PLC0415 (only for the picture)

    xs = [p[0] for lane in lanes for p in lane["points"]]
    ys = [p[1] for lane in lanes for p in lane["points"]]
    x0, x1, y0, y1 = min(xs) - 20, max(xs) + 20, min(ys) - 20, max(ys) + 20
    if lines_by_lane:
        # Only the lap and what is around it, larger.
        lx = [p[0] for lane in lanes if lane["id"] in lap or lane["id"] in approach for p in lane["points"]]
        ly = [p[1] for lane in lanes if lane["id"] in lap or lane["id"] in approach for p in lane["points"]]
        x0, x1, y0, y1 = min(lx) - 30, max(lx) + 30, min(ly) - 30, max(ly) + 30
    scale = 6.0 if lines_by_lane else 2.0
    image = Image.new("RGB", (int((x1 - x0) * scale), int((y1 - y0) * scale)), (250, 250, 247))
    pen = ImageDraw.Draw(image)

    def at(p):
        return ((p[0] - x0) * scale, (p[1] - y0) * scale)

    colours = {"main": (120, 120, 120), "middle": (170, 170, 170), "narrow": (205, 205, 205), "unknown": (205, 205, 205)}
    on_lap = set(lap) | set(approach)
    for lane in lanes:
        if lane["id"] not in on_lap and len(lane["points"]) > 1:
            pen.line([at(p) for p in lane["points"]], fill=colours[lane["kind"]], width=2)
    by_id = {lane["id"]: lane for lane in lanes}
    for place, lane_id in enumerate(lap):
        pts = by_id[lane_id]["points"]
        after = by_id[lap[(place + 1) % len(lap)]]["points"][0]
        pen.line([at(p) for p in pts] + [at(after)], fill=(235, 150, 150) if lines_by_lane else (200, 40, 40), width=3 if lines_by_lane else 5)
        middle = at(pts[len(pts) // 2])
        pen.text((middle[0] + 6, middle[1] - 6), str(lane_id), fill=(120, 20, 20))
    for lane_id in approach:
        pts = by_id[lane_id]["points"]
        pen.line([at(p) for p in pts], fill=(240, 190, 120) if lines_by_lane else (220, 130, 30), width=3 if lines_by_lane else 5)
        middle = at(pts[len(pts) // 2])
        pen.text((middle[0] + 6, middle[1] - 6), str(lane_id), fill=(140, 80, 10))
    if lines_by_lane and approach:
        way = [p for lane_id in approach for p in lines_by_lane.get(lane_id, [])]
        if len(way) > 1:
            pen.line([at(p) for p in way], fill=(20, 130, 60), width=2)
            for p in way:
                q = at(p)
                pen.ellipse([q[0] - 2, q[1] - 2, q[0] + 2, q[1] + 2], fill=(20, 130, 60))
    if lines_by_lane:
        whole = [p for lane_id in lap for p in lines_by_lane.get(lane_id, [])]
        if len(whole) > 1:
            pen.line([at(p) for p in whole] + [at(whole[0])], fill=(20, 60, 160), width=2)
            for p in whole:
                q = at(p)
                pen.ellipse([q[0] - 2, q[1] - 2, q[0] + 2, q[1] + 2], fill=(20, 60, 160))
    start = at(by_id[approach[0] if approach else lap[0]]["points"][0])
    pen.ellipse([start[0] - 7, start[1] - 7, start[0] + 7, start[1] + 7], fill=(30, 90, 200))
    image.save(path)


def cmd_find(args):
    lanes = read_lanes(args.lanes_csv)
    by_id = {lane["id"]: lane for lane in lanes}
    avoid = {int(v) for v in args.avoid.split(",") if v.strip()}
    ways = closed_ways(lanes, avoid)
    ways.sort(key=lambda lap: -lap_numbers(lap, by_id)[0])
    print("%d lanes, %d closed way(s) with no lane taken twice%s" % (len(lanes), len(ways), (", lanes %s left out" % sorted(avoid)) if avoid else ""))
    for lap in ways[:args.top]:
        total, share = lap_numbers(lap, by_id)
        print("%5.0f m  main %3.0f %%  narrow %3.0f %%  %s" % (total, share["main"] * 100.0, share["narrow"] * 100.0, ",".join(str(i) for i in lap)))
    return 0


def cmd_write(args):
    lanes = read_lanes(args.lanes_csv)
    lap = [int(v) for v in args.lanes.split(",") if v.strip()]
    errors = lap_errors(lap, lanes)
    if errors:
        print("not a lap: " + "; ".join(errors))
        return 1
    approach = [int(v) for v in args.approach.split(",") if v.strip()]
    errors = approach_errors(approach, lap, lanes)
    if errors:
        print("not an approach: " + "; ".join(errors))
        return 1
    speeds = {}
    for pair in args.speeds.split(","):
        if "=" in pair:
            name, kmh = pair.rsplit("=", 1)
            speeds[name.strip().upper()] = float(kmh)
    by_id = {lane["id"]: lane for lane in lanes}
    unknown = sorted(set(speeds) - {by_id[i]["name"].upper() for i in lap + approach})
    if unknown:
        print("no lane of the circuit is of: " + ", ".join(unknown))
        return 1
    end_places = [int(v) for v in args.end.split(",") if v.strip()]
    if set(end_places) - set(lap):
        print("the flight can only end on lanes of the lap, not on: " + ", ".join(str(i) for i in sorted(set(end_places) - set(lap))))
        return 1
    walls = roads = None
    if args.town:
        if not line_room.has_town(args.town):
            print("no town in %s: buildings.geojson is wanted there" % args.town)
            return 1
        walls = line_room.read_town(args.town)
        roads = line_room.read_roads(args.town)
    try:
        write_circuit(args.out, lap, lanes, args.source or "the lanes file of the map", approach, speeds, end_places, walls, roads)
    except ValueError as error:
        print("not written: %s" % error)
        return 1
    print("written: %s" % args.out)
    # What was written is read again, as the game and the check read it.
    rows = read_circuit(args.out)
    found = resolve(rows, lanes)[0]
    lines_by_lane = {lane: row["line"] for lane, row in zip(found, rows)}
    round_lanes = {lane for lane, row in zip(found, rows) if row["round"]}
    problems = line_errors(lap, lines_by_lane, by_id, round_lanes)
    if approach and not problems:
        problems = approach_line_errors(approach, lap, lines_by_lane, by_id, round_lanes)
    failed = bool(problems)
    if problems:
        print("the driving line would NOT be used by the game: " + "; ".join(problems))
    if round_lanes:
        print("  the line goes round something on: " + ", ".join("%d %s" % (lane, by_id[lane]["name"] or "-") for lane in found if lane in round_lanes))
    if walls is not None:
        told, through = room_report(lap, approach, lines_by_lane, walls)
        print("\n".join(told))
        failed = failed or through
    if failed:
        return 1
    if args.roads_out:
        # The roads of the circuit, for tools/city/traffic.py: kept whole however far from a generated building.
        seen = []
        with open(args.roads_out, "w", encoding="utf-8", newline="\n") as f:
            f.write("# BD TOPO roads of the flight circuit (tools/city/flight_circuit.py write --roads-out): tools/city/traffic.py\n")
            f.write("# keeps them whole within the extent of the map, however far from a generated building.\n")
            for lane_id in approach + lap:
                road = by_id[lane_id]["road"]
                if road and road not in seen:
                    seen.append(road)
                    f.write("%s  # %s\n" % (road, by_id[lane_id]["name"] or "-"))
        print("written: %s (%d roads)" % (args.roads_out, len(seen)))
    return 0


def cmd_check(args):
    lanes = read_lanes(args.lanes_csv)
    by_id = {lane["id"]: lane for lane in lanes}
    rows = read_circuit(args.circuit_csv)
    try:
        found, renumbered = resolve(rows, lanes)
    except ValueError as error:
        print("flight circuit: FAIL %s" % error)
        return 1
    lap = [lane for lane, row in zip(found, rows) if not row["approach"]]
    approach = [lane for lane, row in zip(found, rows) if row["approach"]]
    errors = lap_errors(lap, lanes)
    errors = errors or approach_errors(approach, lap, lanes)
    if errors:
        print("flight circuit: FAIL " + "; ".join(errors))
        return 1
    total, share = lap_numbers(lap, by_id)
    turns = [turn_between(by_id[lane], by_id[lap[(place + 1) % len(lap)]]) for place, lane in enumerate(lap)]
    sharp = [(lap[place], lap[(place + 1) % len(lap)], turn) for place, turn in enumerate(turns) if abs(turn) >= 60.0]
    print("flight circuit: %d lanes, %.0f m a lap (2 laps %.0f m, 3 laps %.0f m), %.0f %% main axes, %.0f %% middle, %.0f %% narrow; "
          "reached from %d of %d lanes; %d row(s) found under another number"
          % (len(lap), total, total * 2.0, total * 3.0, share["main"] * 100.0, (share["middle"] + share["unknown"]) * 100.0,
             share["narrow"] * 100.0, len(reaching(lap, lanes)), len(lanes), renumbered))
    for lane_id in lap:
        lane = by_id[lane_id]
        print("  lane %3d  %-28s %5.0f m  carriageway %4.1f m  %s" % (lane_id, lane["name"] or "-", lane["length"], lane["carriageway"], lane["kind"]))
    print("  turns of 60 degrees and more: " + (", ".join("%d>%d %+.0f" % t for t in sharp) or "none"))
    if approach:
        print("  approach: %d lanes, %.0f m, to lane %d of the lap: %s" % (len(approach), sum(by_id[i]["length"] for i in approach),
              next(n for n in by_id[approach[-1]]["next"] if n in lap), ",".join(str(i) for i in approach)))
    else:
        print("  approach: none, a flight that starts off the lap takes the shortest way to it")
    failed = False
    main_share, how = share["main"], "by the lanes file"
    if args.carriageway:
        main_share, measured = measured_main_share(lap, by_id, measured_widths(args.carriageway))
        how = "by the measured kerbs"
        print("  kerbs measured on %.0f %% of the lap: %.0f %% of it is %.0f m wide or more" % (measured * 100.0, main_share * 100.0, MAIN_M))
    if main_share * 100.0 < args.min_main:
        print("flight circuit: FAIL %.0f %% on main axes %s, %.0f %% wanted" % (main_share * 100.0, how, args.min_main))
        failed = True
    asked = [(lane, row["kmh"]) for lane, row in zip(found, rows) if row["kmh"] > 0.0]
    if asked:
        by_speed = {}
        for lane, kmh in asked:
            by_speed.setdefault(kmh, []).append(by_id[lane]["name"] or "-")
        print("  speeds asked: " + "; ".join("%.0f km/h %s" % (kmh, ", ".join(sorted(set(names)))) for kmh, names in sorted(by_speed.items(), reverse=True)))
    ends = [lane for lane, row in zip(found, rows) if row["end_place"] and not row["approach"]]
    if ends:
        print("  the filmed flight ends on: %s (%.0f m)" % (", ".join("%d %s" % (lane, by_id[lane]["name"] or "-") for lane in ends),
                                                           sum(by_id[lane]["length"] for lane in ends)))
    if args.report:
        kept, removed, report = report_blockers(args.report, lap + approach)
        print("  prep step \"lanes\" (%s lanes, clear %.0f cm): %d object(s) taken off the circuit's lanes, %d left standing on them"
              % (report.get("lanes", "?"), float(report.get("clear_cm", 0.0)), len(removed), len(kept)))
        landmarks = [item for item in kept if is_landmark(item)]
        kept = [item for item in kept if not is_landmark(item)]
        for item in kept:
            print("    STANDS on lane %s: %s (%s) at %s, %s cm from the line" % (item.get("lane"), item.get("label"), item.get("actor"), item.get("xyz"), item.get("gap_cm")))
        for item in landmarks:
            print("    landmark by lane %s: %s (%s), its bounding box %s cm from the line; not failed, the line has to go round it"
                  % (item.get("lane"), item.get("label"), item.get("actor"), item.get("gap_cm")))
        failed = failed or bool(kept)
        unmoved = landmarks_not_moved(report)
        if unmoved:
            print("flight circuit: FAIL landmark(s) not moved by the prep step \"lanes\" (landmarks_moved): %s; the line is drawn "
                  "for their new place and would go through them" % ", ".join(unmoved))
            failed = True
        if int(report.get("lanes", len(lanes))) != len(lanes):
            print("flight circuit: FAIL the report was made with %s lanes, this lanes file has %d" % (report.get("lanes"), len(lanes)))
            failed = True
    lines_by_lane = {lane: row["line"] for lane, row in zip(found, rows) if row["line"]}
    if lines_by_lane:
        round_lanes = {lane for lane, row in zip(found, rows) if row["round"]}
        problems = line_errors(lap, lines_by_lane, by_id, round_lanes)
        if approach and not problems:
            problems = approach_line_errors(approach, lap, lines_by_lane, by_id, round_lanes)
        count = sum(len(line) for line in lines_by_lane.values())
        away = max(point_to_polyline(p, by_id[lane]["points"]) for lane, line in lines_by_lane.items() for p in line)
        print("  driving line: %d point(s), %.1f m from its lane at most%s" % (count, away, "" if not problems else "; NOT USED by the game: " + "; ".join(problems)))
        failed = failed or bool(problems)
        if round_lanes:
            print("  the line goes round something on: " + ", ".join("%d %s" % (lane, by_id[lane]["name"] or "-") for lane in found if lane in round_lanes))
        town = args.town or (os.path.dirname(args.carriageway) if args.carriageway else "")
        if line_room.has_town(town):
            told, through = room_report(lap, approach, lines_by_lane, line_room.read_town(town))
            print("\n".join(told))
            failed = failed or through
        else:
            print("  room: not measured, no town given (--town, or buildings.geojson next to --carriageway)")
    else:
        print("  driving line: none, the game drives the lanes' own points")
    if args.png:
        draw(args.png, lanes, lap, lines_by_lane, approach)
        print("  picture: %s" % args.png)
    print("flight circuit: %s" % ("FAIL" if failed else "ok"))
    return 1 if failed else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    find = sub.add_parser("find")
    find.add_argument("lanes_csv")
    find.add_argument("--avoid", default="")
    find.add_argument("--top", type=int, default=12)
    find.set_defaults(run=cmd_find)
    write = sub.add_parser("write")
    write.add_argument("lanes_csv")
    write.add_argument("--lanes", required=True)
    write.add_argument("--approach", default="")
    write.add_argument("--speeds", default="", help='speed asked by street, "QU JULES COURMONT=100,R STELLA=60"')
    write.add_argument("--end", default="", help="lanes of the lap the filmed flight is to end on, 373,379,381")
    write.add_argument("--roads-out", default="", help="also write the BD TOPO ids of the circuit's roads (for traffic.py)")
    write.add_argument("--out", required=True)
    write.add_argument("--source", default="")
    write.add_argument("--town", default="", help="processed data of the zone (buildings.geojson, landmarks.json, water.geojson): the line is eased among its walls")
    write.set_defaults(run=cmd_write)
    check = sub.add_parser("check")
    check.add_argument("circuit_csv")
    check.add_argument("lanes_csv")
    check.add_argument("--report", default="")
    check.add_argument("--min-main", type=float, default=MIN_MAIN_SHARE * 100.0, help="share of main axes wanted (%%)")
    check.add_argument("--carriageway", default="", help="count the main axes by the measured kerbs (carriageway.geojson)")
    check.add_argument("--png", default="")
    check.add_argument("--town", default="", help="processed data of the zone, to measure the room of the line (default: next to --carriageway)")
    check.set_defaults(run=cmd_check)
    args = parser.parse_args(argv)
    return args.run(args)


if __name__ == "__main__":
    sys.exit(main())
