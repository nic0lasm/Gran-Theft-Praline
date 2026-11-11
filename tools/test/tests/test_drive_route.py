"""Unit tests for tools/test/drive_route.py (python3 -m unittest discover -s tools/test/tests)."""

import csv
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import drive_route as dr  # noqa: E402


def write_lanes(rows):
    """rows: (points [(x_cm, y_cm)], next ids, speed)."""
    fd, path = tempfile.mkstemp(suffix=".lanes.csv")
    with os.fdopen(fd, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["lane_id", "road_id", "name", "speed_kmh", "width_m", "points_ue_cm", "next", "from_junction", "to_junction"])
        for i, (pts, nxt, speed) in enumerate(rows):
            w.writerow([i, "r", "road, test", speed, 3, ";".join(f"{x} {y}" for x, y in pts), " ".join(map(str, nxt)), -1, -1])
    return path


def straight(x0, y0, x1, y1, n=11):
    return [(x0 + (x1 - x0) * i / (n - 1), y0 + (y1 - y0) * i / (n - 1)) for i in range(n)]


class DriveRouteTest(unittest.TestCase):
    def setUp(self):
        # 0: east along y=0 (0 -> 100 m), then 1: north (100 m), then 2: west; 3: one-way back south (unlinked from 0).
        self.path = write_lanes([
            (straight(0, 0, 10000, 0), [1], 30),
            (straight(10000, 0, 10000, 10000), [2], 50),
            (straight(10000, 10000, 0, 10000), [], 30),
            (straight(0, 10000, 0, 0), [0], 30),
        ])
        self.lanes = dr.load_lanes(self.path)

    def tearDown(self):
        os.remove(self.path)

    def test_quoted_name_with_comma(self):
        self.assertEqual(len(self.lanes), 4)
        self.assertEqual(self.lanes[0]["next"], [1])

    def test_route_follows_links(self):
        path = dr.build_path(self.lanes, [(0, 0), (100, 100)])
        self.assertAlmostEqual(path[0][0], 0)
        self.assertAlmostEqual(path[-1][0], 10000)
        self.assertAlmostEqual(path[-1][1], 10000)
        self.assertTrue(all(abs(path[i][0] - path[i + 1][0]) + abs(path[i][1] - path[i + 1][1]) > 99 for i in range(len(path) - 1)))

    def test_one_way_is_refused(self):
        with self.assertRaises(dr.RouteError):
            dr.build_path(self.lanes, [(100, 100), (100, 0)])  # only the dead-end lane 2 leaves (100, 100)

    def test_off_lane_waypoint(self):
        with self.assertRaises(dr.RouteError):
            dr.build_path(self.lanes, [(0, 0), (50, 50)])

    def test_speed_cap_and_stops(self):
        rows = dr.timed(dr.build_path(self.lanes, [(0, 0), (100, 100)], max_kmh=40))
        self.assertEqual(rows[0][3], 0.0)
        self.assertEqual(rows[-1][3], 0.0)
        self.assertLessEqual(max(r[3] for r in rows), 40.0 + 1e-6)
        self.assertTrue(all(rows[i][0] < rows[i + 1][0] for i in range(len(rows) - 1)))

    def test_bend_is_slower(self):
        rows = dr.timed(dr.build_path(self.lanes, [(0, 0), (100, 100)]))
        corner = min(rows, key=lambda r: abs(r[1] - 10000) + abs(r[2]))
        self.assertLess(corner[3], 30.0)

    def test_loop_on_ring(self):
        lanes = dr.load_lanes(write_lanes([
            (straight(0, 0, 10000, 0), [1], 30),
            (straight(10000, 0, 10000, 10000), [2], 30),
            (straight(10000, 10000, 0, 10000), [3], 30),
            (straight(0, 10000, 0, 0), [0], 30),
        ]))
        path = dr.build_path(lanes, [(0, 0), (100, 100)], loop=True)
        self.assertAlmostEqual(path[-1][0], 0)
        self.assertAlmostEqual(path[-1][1], 0)

    def test_longest_and_cli(self):
        wps = dr.longest_route(self.lanes)
        self.assertGreaterEqual(len(wps), 2)
        out = self.path + ".path.csv"
        self.assertEqual(dr.main(["--lanes", self.path, "--longest", "--out", out]), 0)
        with open(out, encoding="utf-8") as f:
            self.assertEqual(f.readline().strip(), "t_s,x_cm,y_cm,speed_kmh")
        os.remove(out)

    def test_shipped_scenarios_parse(self):
        drives = dr.load_scenarios()
        self.assertTrue(drives)
        for s in drives.values():
            self.assertGreaterEqual(len(s["waypoints_m"]), 2)


if __name__ == "__main__":
    unittest.main()
