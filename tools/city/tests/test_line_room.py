import contextlib
import io
import json
import math
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import flight_circuit as fc  # noqa: E402
import line_room as lr  # noqa: E402
from test_flight_circuit import LAP, ROWS  # noqa: E402


def box(x0, y0, x1, y1):
    """A rectangle as the processed data writes it: local metres, x east, y NORTH."""
    return [[x0, y0], [x1, y0], [x1, y1], [x0, y1], [x0, y0]]


def write_town(folder, buildings=(), landmarks=(), roads=()):
    with open(os.path.join(folder, "buildings.geojson"), "w", encoding="utf-8") as f:
        json.dump({"type": "FeatureCollection", "features": [
            {"type": "Feature", "properties": {"id": name}, "geometry": {"type": "Polygon", "coordinates": [ring]}}
            for name, ring in buildings]}, f)
    with open(os.path.join(folder, "landmarks.json"), "w", encoding="utf-8") as f:
        json.dump({"landmarks": [{"key": key, "kind": kind, "polygon_xy": [ring]} for key, kind, ring in landmarks]}, f)
    if roads:
        with open(os.path.join(folder, "carriageway.geojson"), "w", encoding="utf-8") as f:
            json.dump({"type": "FeatureCollection", "features": [
                {"type": "Feature", "properties": {"road_id": "r%d" % i, "half_width_m": [3.5]},
                 "geometry": {"type": "Polygon", "coordinates": [ring]}} for i, ring in enumerate(roads)]}, f)


def circle(cx, cy, radius, count=16):
    return [[cx + radius * math.cos(2.0 * math.pi * i / count), cy + radius * math.sin(2.0 * math.pi * i / count)] for i in range(count)] \
        + [[cx + radius, cy]]


def corner(step=3.0):
    """East along y = 0 for 60 m, then south (y grows) for 60 m: a right-hand corner as sharp as the lanes give it."""
    return [[i * step, 0.0] for i in range(20)] + [[60.0, i * step] for i in range(21)]


class LineRoomTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()

    def tearDown(self):
        for name in os.listdir(self.dir):
            os.remove(os.path.join(self.dir, name))
        os.rmdir(self.dir)

    def test_the_town_is_read_in_the_lanes_frame(self):
        write_town(self.dir, buildings=[("b1", box(10, 20, 30, 40))],
                   landmarks=[("merry", "carousel", circle(100, -50, 6)), ("place", "square", box(0, 0, 500, 500))])
        walls = lr.read_town(self.dir)
        self.assertEqual(sorted(w["name"] for w in walls), ["b1", "merry"])     # a square is ground, not a wall
        b1 = next(w for w in walls if w["name"] == "b1")
        self.assertEqual(b1["box"], (10.0, -40.0, 30.0, -20.0))                 # y north in the data, south for the lanes
        self.assertEqual([w["name"] for w in lr.near(walls, (20.0, -45.0), 6.0)], ["b1"])
        self.assertEqual(lr.near(walls, (20.0, -60.0), 6.0), [])
        self.assertAlmostEqual(lr.to_wall((20.0, -43.0), b1)[0], 3.0)
        self.assertAlmostEqual(lr.to_wall((20.0, -38.0), b1)[0], -2.0)          # inside
        self.assertTrue(lr.has_town(self.dir))
        self.assertFalse(lr.has_town(os.path.join(self.dir, "none")))

    def test_room_and_least_radius_of_a_line(self):
        write_town(self.dir, buildings=[("b1", box(20, -4.6, 30, -1.6))])        # 1.6 m south of the line y = 0
        walls = lr.read_town(self.dir)
        line = [[i * 3.0, 0.0] for i in range(20)]
        least, at, name = lr.room(line, walls)
        self.assertAlmostEqual(least, 1.6)
        self.assertEqual(name, "b1")
        self.assertEqual(lr.least_radius(line)[0], float("inf"))
        self.assertLess(lr.least_radius(corner())[0], 5.0)

    def test_a_line_goes_round_what_stands_on_it(self):
        # A carrousel on the line, a wall 5 m to its left (north of a line driven east): the way round is to the right.
        write_town(self.dir, buildings=[("north", box(0, 11, 120, 30))], landmarks=[("merry", "carousel", circle(60, 2, 6))])
        walls = lr.read_town(self.dir)
        line = [[i * 3.0, 0.0] for i in range(41)]
        done = lr.go_round(line, walls, closed=False)
        self.assertEqual([(d[0], d[1]) for d in done], [("merry", "right")])
        merry = next(w for w in walls if w["name"] == "merry")
        self.assertGreaterEqual(min(lr.to_wall(p, merry)[0] for p in line), lr.CLEAR_ROUND_M - 0.3)
        self.assertEqual(line[0], [0.0, 0.0])
        self.assertEqual(line[-1], [120.0, 0.0])
        # Nothing on the line: nothing done.
        self.assertEqual(lr.go_round([[i * 3.0, 40.0] for i in range(41)], walls, closed=False), [])

    def test_a_corner_is_eased_as_wide_as_the_room_allows(self):
        walls = lr.Town([])
        given = corner()
        eased, by = lr.ease(given, walls, 3.0, closed=False, tail=1)
        self.assertEqual(len(eased), len(given))
        self.assertEqual(eased[0], given[0])
        self.assertEqual(eased[-1], given[-1])
        self.assertGreater(lr.least_radius(eased)[0], 12.0)
        self.assertLess(lr.least_radius(given)[0], 5.0)
        # Within the slack of the line given, and on it along the straights.
        for p, k in zip(eased, by):
            self.assertLessEqual(math.dist(p, given[k]), lr.SLACK_CORNER_M + 1.6)
        self.assertTrue(all(abs(p[1]) <= lr.SLACK_STRAIGHT_M + 0.05 for p in eased if p[0] < 20.0))
        self.assertEqual(by, sorted(by))
        # Entered from its outside, cut at its inside: the corner is at (60, 0), its inside to the south-west.
        self.assertTrue(any(p[1] < -1.0 for p in eased))
        self.assertTrue(any(p[0] < 59.0 and p[1] > 1.0 for p in eased))

    def test_an_eased_line_keeps_from_the_walls_and_on_the_carriageway(self):
        # The inside of the corner is built 3 m from the line, and a wall stands 1.6 m from the first straight.
        write_town(self.dir, buildings=[("inside", box(30, -40, 57, -3)), ("close", box(10, 1.6, 20, 9))],
                   roads=[box(-10, -5, 65, 5), box(55, -70, 65, 5)])
        walls, roads = lr.read_town(self.dir), lr.read_roads(self.dir)
        given = corner()
        self.assertLess(lr.room(given, walls)[0], 1.7)
        eased, _ = lr.ease(given, walls, 3.0, closed=False, tail=1, roads=roads)
        self.assertGreaterEqual(lr.room(eased, walls)[0], lr.CLEAR_WALL_M - 0.3)
        self.assertGreater(lr.least_radius(eased)[0], lr.least_radius(given)[0] + 2.0)
        # On the carriageway, KERB_M from its kerbs: x and y within 5 m of the two axes, less the kerb.
        for x, y in eased:
            self.assertTrue((abs(y) <= 5.0 - lr.KERB_M + 0.3 and x <= 65.0) or (55.0 + lr.KERB_M - 0.3 <= x <= 65.0 - lr.KERB_M + 0.3), (x, y))


class CircuitInTownTests(unittest.TestCase):
    """flight_circuit.py write --town: the line of the lap eased among the walls, and what the check says of it."""

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.town = os.path.join(self.dir, "town")
        os.mkdir(self.town)
        self.lanes_path = os.path.join(self.dir, "Map.lanes.csv")
        with open(self.lanes_path, "w", encoding="utf-8") as f:
            f.write("lane_id,road_id,name,speed_kmh,width_m,points_ue_cm,next\n")
            f.write("\n".join(ROWS) + "\n")
        self.circuit_path = os.path.join(self.dir, "Map.circuit.csv")

    def tearDown(self):
        for folder in (self.town, self.dir):
            for name in os.listdir(folder):
                os.remove(os.path.join(folder, name))
            os.rmdir(folder)

    def run_tool(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = fc.main(list(argv))
        return code, out.getvalue()

    def test_write_in_a_town_goes_round_and_keeps_its_room(self):
        # A carrousel 14 m wide on lane 1 (y = 0, driven east) at x = 50: the way round is further from the lane than
        # a line may be without saying so.
        write_town(self.town, landmarks=[("kiosk", "kiosk", circle(50, 0, 7))])
        code, out = self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--approach", "5,6", "--out", self.circuit_path, "--town", self.town)
        self.assertEqual(code, 0, out)
        self.assertIn("the line goes round something on: 1 rue A", out)
        rows = fc.read_circuit(self.circuit_path)
        self.assertEqual([row["round"] for row in rows if not row["approach"]], [True, False, False, False])
        walls = lr.read_town(self.town)
        lap = [p for row in rows if not row["approach"] for p in row["line"]]
        self.assertGreaterEqual(lr.room(lap + lap[:1], walls)[0], lr.CLEAR_ROUND_M - 0.3)
        self.assertGreater(lr.least_radius(lap, closed=True)[0], 8.0)
        code, out = self.run_tool("check", self.circuit_path, self.lanes_path, "--min-main", "0", "--town", self.town)
        self.assertEqual(code, 0, out)
        self.assertIn("room: 1 wall(s) of the town", out)
        self.assertIn("flight circuit: ok", out)

    def test_check_fails_a_line_through_a_wall_and_says_when_it_has_no_town(self):
        code, out = self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--out", self.circuit_path)
        self.assertEqual(code, 0, out)
        code, out = self.run_tool("check", self.circuit_path, self.lanes_path, "--min-main", "0")
        self.assertEqual(code, 0, out)
        self.assertIn("room: not measured", out)
        write_town(self.town, landmarks=[("kiosk", "kiosk", circle(50, -1, 3))])
        code, out = self.run_tool("check", self.circuit_path, self.lanes_path, "--min-main", "0", "--town", self.town)
        self.assertEqual(code, 1, out)
        self.assertIn("FAIL the driving line runs through kiosk", out)

    def test_write_wants_a_town_where_it_is_told_one(self):
        code, out = self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--out", self.circuit_path, "--town", os.path.join(self.dir, "none"))
        self.assertEqual(code, 1)
        self.assertIn("no town", out)


if __name__ == "__main__":
    unittest.main()
