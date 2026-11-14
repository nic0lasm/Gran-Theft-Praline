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
        # Junctions 1 and 2 (nodes merged by traffic.py) are too wide to be one: over 45 m together.
        lanes = [lane(0, [(0, 0), (800, 0)], fj=1, tj=2), lane(1, [(-5000, 1000), (-2500, 1000)], tj=1),
                 lane(2, [(3000, 0), (6000, 0)], fj=2)]
        shapes, counts = zonegraph.build(lanes, [])
        self.assertEqual(counts["junctions_merged"], 0)
        pts = [s for s in shapes if s["name"] == "Car_0"][0]["points"]
        self.assertAlmostEqual(pts[-1][0] - pts[0][0], 200.0, delta=1.0)

    def test_short_link_makes_one_junction(self):
        # Two nodes 15 m apart: MassTraffic would find no room on the lane between them.
        lanes = [lane(0, [(-5000, 0), (0, 0)], tj=1), lane(1, [(0, 0), (1500, 0)], fj=1, tj=2),
                 lane(2, [(1500, 0), (6500, 0)], fj=2), lane(3, [(0, 5000), (0, 0)], tj=1),
                 lane(4, [(1500, 0), (1500, 5000)], fj=2)]
        shapes, counts = zonegraph.build(lanes, [])
        self.assertEqual((counts["junctions"], counts["junctions_merged"], counts["junction_lanes_dropped"]), (1, 1, 1))
        self.assertFalse(any(s["name"] == "Car_1" for s in shapes))
        poly = [s for s in shapes if s["type"] == "polygon"][0]
        self.assertEqual(sorted(poly["lanes"]), [0, 2, 3, 4])
        self.assertEqual(counts["short_after_junction"], 0)

    def test_short_lane_back_into_its_junction_goes_a_long_one_stays(self):
        lanes = [lane(0, [(-5000, 0), (0, 0)], tj=1), lane(1, [(0, 0), (900, 0)], fj=1, tj=1),
                 lane(2, [(0, 0), (0, 3000), (3000, 3000), (0, 0)], fj=1, tj=1), lane(3, [(0, 0), (0, -5000)], fj=1)]
        shapes, counts = zonegraph.build(lanes, [])
        names = {s["name"] for s in shapes}
        self.assertNotIn("Car_1", names)
        self.assertIn("Car_2", names)
        self.assertEqual(counts["junction_lanes_dropped"], 1)

    def test_plain_pieces_become_one_lane(self):
        # Three BD TOPO pieces of one street, no junction between them: one lane, as a road of Epic's.
        lanes = [lane(0, [(0, 0), (1000, 0)], nxt=[1], fj=4), lane(1, [(1000, 0), (1400, 0)], nxt=[2]),
                 lane(2, [(1400, 0), (3000, 0)], nxt=[9], tj=5)]
        lanes[1]["speed_kmh"] = 20.0
        out, absorbed = zonegraph.chain_plain_lanes(lanes)
        self.assertEqual((len(out), absorbed), (1, 2))
        one = out[0]
        self.assertEqual((one["id"], one["from_junction"], one["to_junction"], one["next"]), (0, 4, 5, [9]))
        self.assertEqual(one["points"], [(0, 0), (1000.0, 0.0), (1400.0, 0.0), (3000, 0)])
        self.assertEqual(one["speed_kmh"], 20.0)
        self.assertEqual(len(lanes[0]["points"]), 2)  # the input is left as it was

    def test_pieces_stay_apart_at_a_split_or_a_merge(self):
        lanes = [lane(0, [(0, 0), (1000, 0)], nxt=[1, 2]), lane(1, [(1000, 0), (2000, 0)]),
                 lane(2, [(1000, 0), (1000, 1000)]), lane(3, [(0, 500), (1000, 500)], nxt=[4]),
                 lane(5, [(500, 900), (1000, 500)], nxt=[4]), lane(4, [(1000, 500), (2000, 500)])]
        out, absorbed = zonegraph.chain_plain_lanes(lanes)
        self.assertEqual((len(out), absorbed), (6, 0))

    def test_plain_continuation_is_not_cut(self):
        shapes, _ = zonegraph.build([lane(0, [(0, 0), (1000, 0)], nxt=[1]), lane(1, [(1000, 0), (2000, 0)])], [])
        self.assertEqual(shapes[0]["name"], "Car_0")
        self.assertEqual(shapes[0]["points"][0], [0.0, 0.0])
        self.assertEqual(shapes[0]["points"][-1], [2000.0, 0.0])

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

    def test_two_way_dead_end_turns_back(self):
        # A two-way road leaving the map: lane 0 runs out east, lane 1 comes back west, 3.2 m apart.
        lanes = [lane(0, [(0, 0), (5000, 0)]), lane(1, [(5000, 320), (0, 320)])]
        shapes, counts = zonegraph.build(lanes, [])
        # Both ends of the road turn back: east onto lane 1, west onto lane 0.
        self.assertEqual(counts["dead_ends_closed"], 2)
        self.assertEqual(counts["dead_ends_left"], 0)
        uturn = [s for s in shapes if s["name"] == "UTurn_0"]
        self.assertEqual(len(uturn), 1)
        pts = uturn[0]["points"]
        self.assertEqual(uturn[0]["tags"], ["Car"])
        self.assertEqual(pts[0], [5000.0, 0.0])    # starts where lane 0 ends
        self.assertEqual(pts[-1], [5000.0, 320.0])  # ends where lane 1 starts
        self.assertGreater(max(x for x, _ in pts), 5000.0 + 300.0)  # runs on past the end

    def test_no_turnaround_onto_a_fed_lane_or_the_same_way(self):
        # Lane 2 starts near lane 0's end but is fed by lane 3, and lane 4 starts there the same way.
        lanes = [lane(0, [(0, 0), (5000, 0)]), lane(3, [(9000, 320), (5000, 320)], nxt=[2]),
                 lane(2, [(5000, 320), (0, 320)]), lane(4, [(5100, -320), (9000, -320)])]
        shapes, counts = zonegraph.build(lanes, [])
        self.assertFalse(any(s["name"] == "UTurn_0" for s in shapes))

    def test_lane_into_a_junction_is_not_a_dead_end(self):
        lanes = [lane(0, [(-5000, 0), (0, 0)], tj=1), lane(1, [(0, 0), (0, 5000)], fj=1)]
        _, counts = zonegraph.build(lanes, [])
        self.assertEqual(counts["junctions"], 1)
        self.assertEqual(counts["dead_ends_closed"] + counts["dead_ends_left"], 1)  # only lane 1's far end

    def test_next_lane_off_a_junction_is_chained(self):
        # Two roads meet off a junction and their lanes are offset differently: 1.5 m apart.
        lanes = [lane(0, [(0, 0), (5000, 0)], nxt=[1]), lane(1, [(5000, 150), (9000, 150)])]
        shapes, counts = zonegraph.build(lanes, [])
        self.assertEqual((counts["lanes_chained"], counts["dead_ends_joined"]), (1, 0))
        car0 = [s for s in shapes if s["name"] == "Car_0"][0]
        self.assertEqual(car0["points"], [[0.0, 0.0], [5000.0, 75.0], [9000.0, 150.0]])

    def test_next_lane_off_a_junction_is_joined(self):
        # Lane 0 also leads elsewhere (lanes.csv), so it is not chained: its end is drawn onto lane 1.
        lanes = [lane(0, [(0, 0), (5000, 0)], nxt=[1, 2]), lane(1, [(5000, 150), (9000, 150)]),
                 lane(2, [(5000, -900), (5000, -5000)])]
        shapes, counts = zonegraph.build(lanes, [])
        self.assertEqual(counts["dead_ends_joined"], 1)
        self.assertEqual(counts["dead_ends_left"], 2)  # the far ends of lanes 1 and 2
        car0 = [s for s in shapes if s["name"] == "Car_0"][0]
        self.assertEqual(car0["points"][-1], [5000.0, 150.0])  # ends on lane 1's start
        self.assertEqual(car0["points"][-2], [4400.0, 0.0])    # the shift starts 6 m before

    def test_points_parse_like_traffic_csv(self):
        self.assertEqual(zonegraph.parse_points("100 -200;300 -400"), [(100.0, -200.0), (300.0, -400.0)])


if __name__ == "__main__":
    unittest.main()
