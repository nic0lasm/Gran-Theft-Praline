import csv
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import clear_lanes  # noqa: E402


def row(cid, x, y, yaw=0.0, collision=None, ref=""):
    r = {"catalog_id": cid, "asset": "", "x_m": str(x), "y_m": str(y), "ue_yaw": str(yaw), "ref": ref}
    if collision is not None:
        r["collision"] = collision
    return r


# One lane going east along y = 0, then a second one going north from (60, 10): the link between
# them crosses the corner.
LANES = [{"id": 7, "name": "R A", "points": [(0.0, 0.0), (50.0, 0.0)], "next": [8]},
         {"id": 8, "name": "R B", "points": [(60.0, 10.0), (60.0, 60.0)], "next": []}]


class ClearLanesTests(unittest.TestCase):
    def test_what_stands_on_a_lane_is_taken_out(self):
        rows = [row("W1-09", 20.0, 0.4, ref="on"), row("W1-09", 20.0, 1.7, ref="edge"),
                row("W1-09", 20.0, 2.0, ref="pavement")]
        kept, out = clear_lanes.split(rows, LANES)
        self.assertEqual([r["ref"] for r in kept], ["pavement"])
        self.assertEqual([(r["ref"], lane) for r, lane, _ in out], [("on", "7 R A"), ("edge", "7 R A")])

    def test_what_a_car_cannot_hit_stays(self):
        rows = [row("W1-46", 20.0, 0.0, collision="false"), row("W1-54", 20.0, 0.0, collision="true")]
        kept, out = clear_lanes.split(rows, LANES)
        self.assertEqual(kept, [rows[0]])
        self.assertEqual(len(out), 1)

    def test_the_link_between_two_lanes_is_cleared_too(self):
        kept, out = clear_lanes.split([row("W1-09", 55.0, 5.0)], LANES)
        self.assertEqual(kept, [])
        self.assertEqual(out[0][1], "7>8 R A")

    def test_a_long_object_counts_along_its_length_only(self):
        # A bus shelter 3 m from the centre line: clear when it lies along the lane (yaw 90: its
        # front faces the lane), in the way when it lies across it.
        along, across = row("W1-21", 20.0, 3.4, yaw=90.0), row("W1-21", 20.0, 3.4, yaw=0.0)
        kept, out = clear_lanes.split([along, across], LANES)
        self.assertEqual(kept, [along])
        self.assertEqual([r for r, _, _ in out], [across])

    def test_summary_counts_by_asset_and_lane(self):
        _, out = clear_lanes.split([row("W1-09", 20.0, 0.0), row("W1-09", 30.0, 0.0), row("W1-01", 60.0, 30.0)], LANES)
        s = clear_lanes.summary(out)
        self.assertEqual(s["removed"], 3)
        self.assertEqual(s["by_asset"], {"W1-01": 1, "W1-09": 2})
        self.assertEqual(s["by_lane"], {"7 R A": 2, "8 R B": 1})

    def test_lanes_csv_is_read_in_local_metres(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "lanes.csv")
            with open(path, "w", newline="", encoding="utf-8") as f:
                w = csv.writer(f)
                w.writerow(["lane_id", "road_id", "name", "speed_kmh", "width_m", "points_ue_cm", "next",
                            "from_junction", "to_junction"])
                w.writerow([3, "T1", "R C", 30, 3.0, "0 0;5000 -100", "4 5", -1, 2])
            lanes = clear_lanes.lanes_from_csv(path)
        self.assertEqual(lanes, [{"id": 3, "name": "R C", "points": [(0.0, 0.0), (50.0, 1.0)], "next": [4, 5]}])

    def test_a_footprint_beside_a_lane_is_cut_back(self):
        from shapely.geometry import LineString, box
        wall = {"id": "B1", "poly": box(10.0, 0.9, 30.0, 12.0)}        # 0.9 m from the centre line
        far = {"id": "B2", "poly": box(10.0, -12.0, 30.0, -2.0)}       # 2 m away: untouched
        before = far["poly"]
        kept, report = clear_lanes.trim_footprints([wall, far], LANES)
        self.assertEqual([b["id"] for b in kept], ["B1", "B2"])
        self.assertIs(far["poly"], before)
        self.assertAlmostEqual(wall["poly"].bounds[1], clear_lanes.CLEAR_M, places=2)
        self.assertGreaterEqual(LineString(LANES[0]["points"]).distance(wall["poly"]), clear_lanes.CLEAR_M - 0.01)
        self.assertEqual([(e["id"], e["cut_m2"], e["lanes"]) for e in report["trimmed"]], [("B1", 12.0, ["7 R A"])])
        self.assertEqual(report["left_out"], [])

    def test_a_building_on_the_road_is_left_out(self):
        from shapely.geometry import box
        on_road = {"id": "B3", "poly": box(20.0, -4.0, 28.0, 4.0)}     # the lane runs through it
        kept, report = clear_lanes.trim_footprints([on_road], LANES)
        self.assertEqual(kept, [])
        self.assertEqual([e["id"] for e in report["left_out"]], ["B3"])

    def test_no_lanes_keeps_everything(self):
        rows = [row("W1-09", 20.0, 0.0)]
        self.assertEqual(clear_lanes.split(rows, []), (rows, []))


if __name__ == "__main__":
    unittest.main()
