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


def points(ax, ay, bx, by, step=4.0):
    """A straight lane from a to b (m) as the lanes file writes it (cm), a point every step m."""
    count = max(int(round(math.hypot(bx - ax, by - ay) / step)), 1)
    return ";".join("%.0f %.0f 0" % ((ax + (bx - ax) * i / count) * 100.0, (ay + (by - ay) * i / count) * 100.0)
                    for i in range(count + 1))


# A block driven clockwise (Y is south): 1 east, 2 south, 3 west, 4 north; 5 and 6 lead to it from the west, 7 from
# the north; 8 leaves it and ends; 9 stands apart. Each lane starts 3 m beside the end of the one before, as the
# lanes of tools/city/traffic.py do.
ROWS = [
    '1,10,"rue A",50,3.5,%s,2 8' % points(0, 0, 100, 0),
    '2,11,"quai B",50,3.5,%s,3' % points(103, 3, 103, 80),
    '3,12,"rue C",50,3.5,%s,4' % points(100, 83, 0, 83),
    '4,13,"quai D",50,3.5,%s,1' % points(-3, 80, -3, 3),
    '5,10,"rue A",50,3.5,%s,6' % points(-200, 0, -100, 0),
    '6,10,"rue A",50,3.5,%s,1' % points(-100, 0, -3, 0),
    '7,14,"rue E",30,3,%s,6' % points(-100, -200, -100, 0),
    '8,10,"rue A",50,3.5,%s,' % points(103, 0, 200, 0),
    '9,15,"rue F",30,3,%s,' % points(300, 0, 400, 0),
    # The other side of the quays: with it their carriageway is 7 m, a main axis.
    '12,11,"quai B",50,3.5,%s,' % points(106.5, 80, 106.5, 3),
    '14,13,"quai D",50,3.5,%s,' % points(-6.5, 3, -6.5, 80),
]
LAP = [1, 2, 3, 4]


class FlightCircuitTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.lanes_path = os.path.join(self.dir, "Map.lanes.csv")
        with open(self.lanes_path, "w", encoding="utf-8") as f:
            f.write("lane_id,road_id,name,speed_kmh,width_m,points_ue_cm,next\n")
            f.write("\n".join(ROWS) + "\n")
        self.lanes = fc.read_lanes(self.lanes_path)
        self.by_id = {lane["id"]: lane for lane in self.lanes}
        self.circuit_path = os.path.join(self.dir, "Map.circuit.csv")

    def tearDown(self):
        for name in os.listdir(self.dir):
            os.remove(os.path.join(self.dir, name))
        os.rmdir(self.dir)

    def run_tool(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = fc.main(list(argv))
        return code, out.getvalue()

    def test_widths_as_the_game_counts_them(self):
        self.assertEqual(self.by_id[2]["kind"], "main")
        self.assertAlmostEqual(self.by_id[2]["carriageway"], 7.0)
        self.assertEqual(self.by_id[1]["kind"], "narrow")
        self.assertEqual(self.by_id[7]["kind"], "narrow")

    def test_closed_ways_are_found_once(self):
        self.assertEqual(fc.closed_ways(self.lanes), [LAP])
        self.assertEqual(fc.closed_ways(self.lanes, avoid={3}), [])

    def test_what_is_not_a_lap(self):
        self.assertEqual(fc.lap_errors(LAP, self.lanes), [])
        self.assertIn("three lanes", fc.lap_errors([1, 2], self.lanes)[0])
        self.assertIn("lane 1 does not lead to lane 3", fc.lap_errors([1, 3, 2, 4], self.lanes))
        self.assertIn("lane 4 does not lead to lane 5", fc.lap_errors([5, 6, 1, 2, 3, 4], self.lanes))
        self.assertTrue(fc.lap_errors([1, 2, 3, 99], self.lanes))
        self.assertTrue(fc.lap_errors([1, 2, 3, 4, 1], self.lanes))

    def test_what_is_not_an_approach(self):
        self.assertEqual(fc.approach_errors([5, 6], LAP, self.lanes), [])
        self.assertEqual(fc.approach_errors([], LAP, self.lanes), [])
        self.assertTrue(fc.approach_errors([5], LAP, self.lanes))        # does not lead to the lap
        self.assertTrue(fc.approach_errors([6, 5], LAP, self.lanes))     # the wrong way round
        self.assertTrue(fc.approach_errors([5, 1], LAP, self.lanes))     # a lane of the lap
        self.assertTrue(fc.approach_errors([8], LAP, self.lanes))        # a dead end
        self.assertTrue(fc.approach_errors([99], LAP, self.lanes))

    def test_the_lap_is_reached_from_the_lanes_that_lead_to_it(self):
        self.assertEqual(fc.reaching(LAP, self.lanes), {1, 2, 3, 4, 5, 6, 7})

    def test_driving_line_is_one_closed_curve_with_two_points_a_lane(self):
        lines, corners = fc.driving_line(LAP, self.by_id)
        self.assertEqual([lane for lane, _ in lines], LAP)
        for lane, line in lines:
            self.assertGreaterEqual(len(line), 2, lane)
        self.assertEqual(fc.line_errors(LAP, dict(lines), self.by_id), [])
        # Four right-hand corners (Y is south: clockwise turns are positive), rounded inside the lanes' corner.
        self.assertEqual(len(corners), 4)
        self.assertTrue(all(angle > 60.0 for _, angle in corners))
        whole = [p for _, line in lines for p in line]
        self.assertTrue(all(math.dist(whole[i], whole[(i + 1) % len(whole)]) < 2.0 * fc.LINE_STEP_M for i in range(len(whole))))
        # A straight stays straight: the middle of lane 1 is still on y = 0.
        middle = [p for p in dict(lines)[1] if 30.0 < p[0] < 70.0]
        self.assertTrue(middle and all(abs(p[1]) < 0.2 for p in middle))

    def test_approach_line_runs_into_the_lap_line(self):
        lap_lines, _ = fc.driving_line(LAP, self.by_id)
        way = fc.approach_line([5, 6], LAP, lap_lines, self.by_id)
        self.assertEqual([lane for lane, _ in way], [5, 6])
        self.assertLess(math.dist(way[0][1][0], (-200.0, 0.0)), 0.01)
        lines = dict(lap_lines)
        lines.update(dict(way))
        self.assertEqual(fc.approach_line_errors([5, 6], LAP, lines, self.by_id), [])
        self.assertLess(math.dist(way[-1][1][-1], dict(lap_lines)[1][0]), 2.0 * fc.LINE_STEP_M)

    def test_write_then_check(self):
        code, out = self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--approach", "5,6", "--out", self.circuit_path)
        self.assertEqual(code, 0, out)
        rows = fc.read_circuit(self.circuit_path)
        self.assertEqual([row["hint"] for row in rows], [5, 6, 1, 2, 3, 4])
        self.assertEqual([row["approach"] for row in rows], [True, True, False, False, False, False])
        self.assertTrue(all(len(row["line"]) >= 2 for row in rows))
        # 43 % of this lap on main axes: under the 70 % wanted, the check says so and fails.
        code, out = self.run_tool("check", self.circuit_path, self.lanes_path)
        self.assertEqual(code, 1)
        self.assertIn("4 lanes, 354 m a lap", out)
        self.assertIn("approach: 2 lanes, 197 m, to lane 1 of the lap: 5,6", out)
        self.assertIn("on main axes by the lanes file, 70 % wanted", out)
        self.assertNotIn("NOT USED", out)
        code, out = self.run_tool("check", self.circuit_path, self.lanes_path, "--min-main", "40")
        self.assertEqual(code, 0, out)

    def test_main_axes_by_the_measured_kerbs(self):
        # The lanes file counts rue A and rue C by their one lane (3.5 m); between their kerbs they are 9 m.
        kerbs = os.path.join(self.dir, "carriageway.geojson")
        with open(kerbs, "w", encoding="utf-8") as f:
            json.dump({"features": [{"properties": {"road_id": road, "centre": [[0, 0], [1, 0], [2, 0]], "half_width_m": [4.4, 4.5, 9.0]}}
                                    for road in ("10", "12")]}, f)
        self.assertEqual(fc.measured_widths(kerbs), {"10": 9.0, "12": 9.0})
        share, measured = fc.measured_main_share(LAP, self.by_id, fc.measured_widths(kerbs))
        self.assertAlmostEqual(share, 1.0)
        self.assertAlmostEqual(measured, 200.0 / 354.0, places=2)
        self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--out", self.circuit_path)
        code, out = self.run_tool("check", self.circuit_path, self.lanes_path, "--carriageway", kerbs)
        self.assertEqual(code, 0, out)
        self.assertIn("100 % of it is 7 m wide or more", out)

    def test_speeds_asked_by_street(self):
        roads = os.path.join(self.dir, "roads.txt")
        code, out = self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--approach", "5,6", "--out", self.circuit_path,
                                  "--speeds", "rue a=60,QUAI B=100", "--roads-out", roads)
        self.assertEqual(code, 0, out)
        rows = fc.read_circuit(self.circuit_path)
        self.assertEqual([row["kmh"] for row in rows], [60.0, 60.0, 60.0, 100.0, 0.0, 0.0])
        code, out = self.run_tool("check", self.circuit_path, self.lanes_path, "--min-main", "40")
        self.assertEqual(code, 0, out)
        self.assertIn("speeds asked: 100 km/h quai B; 60 km/h rue A", out)
        # The roads of the circuit, once each, approach first: what tools/city/traffic.py keeps whole.
        with open(roads, encoding="utf-8") as f:
            listed = [line.split("#")[0].strip() for line in f if line.split("#")[0].strip()]
        self.assertEqual(listed, ["10", "11", "12", "13"])
        # A street that is not of the circuit: a typing mistake, refused.
        code, out = self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--out", self.circuit_path, "--speeds", "rue Z=60")
        self.assertEqual(code, 1)
        self.assertIn("no lane of the circuit is of: RUE Z", out)

    def test_the_lanes_the_flight_ends_on(self):
        code, out = self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--approach", "5,6", "--out", self.circuit_path, "--end", "3")
        self.assertEqual(code, 0, out)
        rows = fc.read_circuit(self.circuit_path)
        self.assertEqual([row["hint"] for row in rows if row["end_place"]], [3])
        code, out = self.run_tool("check", self.circuit_path, self.lanes_path, "--min-main", "40")
        self.assertEqual(code, 0, out)
        self.assertIn("the filmed flight ends on: 3 rue C (100 m)", out)
        # Not on the approach, nor on a lane off the circuit.
        for wrong in ("5", "8"):
            code, out = self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--approach", "5,6", "--out", self.circuit_path, "--end", wrong)
            self.assertEqual(code, 1)
            self.assertIn("only end on lanes of the lap", out)

    def test_a_line_far_from_its_lanes_ends_is_refused_as_the_game_refuses_it(self):
        lines, _ = fc.driving_line(LAP, self.by_id)
        lines = dict(lines)
        self.assertEqual(fc.line_ends_errors(1, lines[1], self.by_id), [])
        cut = [p for p in lines[1] if p[0] > fc.LINE_ENDS_M + 3.0]
        self.assertIn("the game would not use it", fc.line_ends_errors(1, cut, self.by_id)[0])

    def test_write_refuses_what_is_not_a_lap_or_an_approach(self):
        code, out = self.run_tool("write", self.lanes_path, "--lanes", "1,3,2,4", "--out", self.circuit_path)
        self.assertEqual(code, 1)
        self.assertIn("not a lap", out)
        code, out = self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--approach", "8", "--out", self.circuit_path)
        self.assertEqual(code, 1)
        self.assertIn("not an approach", out)
        self.assertFalse(os.path.exists(self.circuit_path))

    def test_rows_find_their_lanes_under_other_numbers(self):
        self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--out", self.circuit_path)
        rows = fc.read_circuit(self.circuit_path)
        renamed = [dict(lane, id=lane["id"] + 100, next=[n + 100 for n in lane["next"]]) for lane in self.lanes]
        found, renumbered = fc.resolve(rows, renamed)
        self.assertEqual(found, [101, 102, 103, 104])
        self.assertEqual(renumbered, 4)
        # A road that is no longer there.
        with self.assertRaises(ValueError):
            fc.resolve(rows, [lane for lane in renamed if lane["id"] != 103])

    def test_objects_left_standing_on_the_circuit_fail_the_check(self):
        report = os.path.join(self.dir, "lane_clear_report.json")
        with open(report, "w", encoding="utf-8") as f:
            json.dump({"lanes": 11, "clear_cm": 150.0,
                       "removed_instances": [{"lane": "2 quai B", "label": "bench"}],
                       "reported_only": [{"lane": "6 rue A", "label": "facade module", "actor": "A", "xyz": [0, 0, 0], "gap_cm": 40},
                                         {"lane": "9 rue F", "label": "sign", "actor": "B", "xyz": [0, 0, 0], "gap_cm": 10}]}, f)
        kept, removed, _ = fc.report_blockers(report, LAP + [5, 6])
        self.assertEqual([item["label"] for item in kept], ["facade module"])
        self.assertEqual(len(removed), 1)
        self.assertEqual(fc.report_blockers(report, LAP)[0], [])
        self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--approach", "5,6", "--out", self.circuit_path)
        code, out = self.run_tool("check", self.circuit_path, self.lanes_path, "--report", report)
        self.assertEqual(code, 1)
        self.assertIn("STANDS on lane 6 rue A", out)

    def test_a_landmark_by_the_circuit_is_told_not_failed(self):
        report = os.path.join(self.dir, "lane_clear_report.json")
        with open(report, "w", encoding="utf-8") as f:
            json.dump({"lanes": 11, "clear_cm": 150.0, "removed_instances": [],
                       "reported_only": [{"lane": "6 rue A", "label": "carrousel", "actor": "C", "folder": "City/Landmarks",
                                          "index": -1, "xyz": [0, 0, 0], "gap_cm": 0}]}, f)
        self.run_tool("write", self.lanes_path, "--lanes", "1,2,3,4", "--approach", "5,6", "--out", self.circuit_path)
        code, out = self.run_tool("check", self.circuit_path, self.lanes_path, "--report", report, "--min-main", "0")
        self.assertEqual(code, 0)
        self.assertIn("landmark by lane 6 rue A", out)
        self.assertNotIn("STANDS", out)


if __name__ == "__main__":
    unittest.main()
