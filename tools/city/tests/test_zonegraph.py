import math
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import zonegraph  # noqa: E402


def lane(lid, pts, nxt=(), fj=-1, tj=-1):
    return {"id": lid, "points": pts, "speed_kmh": 30.0, "next": list(nxt), "from_junction": fj, "to_junction": tj}


class ZoneGraphTests(unittest.TestCase):
    def test_lane_is_cut_short_at_a_junction(self):
        shapes, counts = zonegraph.build([lane(0, [(0, 0), (5000, 0)], tj=7)], [])
        car = shapes[0]
        self.assertEqual(car["profile"], "Car")
        self.assertEqual(car["points"][-1], [4400.0, 0.0])
        self.assertEqual(counts["junctions"], 0)  # one end only: no polygon

    def test_junction_polygon_gathers_the_cut_ends_in_order(self):
        # A cross: four lanes in towards (0,0), four out.
        lanes = []
        for i, (dx, dy) in enumerate([(1, 0), (0, 1), (-1, 0), (0, -1)]):
            lanes.append(lane(2 * i, [(dx * 4000, dy * 4000 + 150 * dx), (0, 150 * dx)], tj=1))
            lanes.append(lane(2 * i + 1, [(0, -150 * dx), (dx * 4000, dy * 4000 - 150 * dx)], fj=1))
        shapes, counts = zonegraph.build(lanes, [])
        self.assertEqual(counts["junctions"], 1)
        poly = [s for s in shapes if s["type"] == "polygon"][0]
        self.assertEqual(len(poly["points"]), 8)
        self.assertEqual(sorted(poly["roles"]), ["in"] * 4 + ["out"] * 4)
        self.assertIn("Intersection", poly["tags"])
        cx, cy = poly["centre"]
        angles = [math.atan2(y - cy, x - cx) for x, y in poly["points"]]
        self.assertEqual(angles, sorted(angles))
        # Every cut end stands about JUNCTION_TRIM_M from the node.
        for x, y in poly["points"]:
            self.assertAlmostEqual(math.hypot(x, y), 600.0, delta=30.0)

    def test_short_lane_between_junctions_keeps_its_middle(self):
        shapes, _ = zonegraph.build([lane(0, [(0, 0), (800, 0)], fj=1, tj=2)], [])
        pts = shapes[0]["points"]
        self.assertAlmostEqual(pts[-1][0] - pts[0][0], 200.0, delta=1.0)

    def test_plain_continuation_is_not_cut(self):
        shapes, _ = zonegraph.build([lane(0, [(0, 0), (1000, 0)], nxt=[1]), lane(1, [(1000, 0), (2000, 0)])], [])
        self.assertEqual(shapes[0]["points"][-1], shapes[1]["points"][0])

    def test_walkways_two_way_and_crossings_tagged(self):
        walks = [{"id": "a", "kind": 0, "width_m": 3.0, "points": [(0, 0), (100, 0)]},
                 {"id": "b", "kind": 2, "width_m": 4.0, "points": [(100, 0), (100, 900)]}]
        shapes, counts = zonegraph.build([], walks)
        self.assertEqual(counts["crossings"], 1)
        self.assertEqual(shapes[0]["profile"], "Pedestrian")
        self.assertEqual(shapes[1]["tags"], ["Pedestrian", "Crossing"])
        self.assertEqual(shapes[1]["width_cm"], 400.0)
        directions = [lane_["direction"] for lane_ in zonegraph.PROFILES["Pedestrian"]["lanes"]]
        self.assertEqual(sorted(directions), ["backward", "forward"])

    def test_points_parse_like_traffic_csv(self):
        self.assertEqual(zonegraph.parse_points("100 -200;300 -400"), [(100.0, -200.0), (300.0, -400.0)])


if __name__ == "__main__":
    unittest.main()
