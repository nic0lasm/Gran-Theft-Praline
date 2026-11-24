import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import lane_clear as lc  # noqa: E402


def lanes_csv(rows):
    fd, path = tempfile.mkstemp(suffix=".lanes.csv")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write("lane_id,road_id,name,speed_kmh,width_m,points_ue_cm,next\n")
        for r in rows:
            f.write(r + "\n")
    return path


class LaneClearTests(unittest.TestCase):
    def setUp(self):
        # lane 1 east along y=0 from x=0 to 10 m, lane 2 starts 4 m further (a link 10 m -> 14 m)
        self.path = lanes_csv(['1,1,"rue de la Bourse",30,3,0 0 0;1000 0 0,2',
                               '2,1,"rue de la Bourse",30,3,1400 0 0;3000 0 0,'])
        self.index = lc.LaneIndex(lc.read_lanes(self.path))

    def tearDown(self):
        os.remove(self.path)

    def test_block_on_the_lane_is_found(self):
        box = lc.footprint_box(500, 50, 0, (-100, -100), (100, 100))   # 2 m block, 0.5 m off the line
        lane, d = self.index.nearest(box)
        self.assertEqual(lane, "1")
        self.assertEqual(d, 0.0)

    def test_kerbside_bench_is_left(self):
        bench = lc.footprint_box(500, 300, 0, (-90, -35), (90, 35))  # 2.65 m from the line
        self.assertIsNone(self.index.nearest(bench))

    def test_link_between_lanes_counts(self):
        lane, _ = self.index.nearest(((1200, 60), 35.0))
        self.assertEqual(lane, "1>2")

    def test_yaw_turns_the_box(self):
        long_thing = lc.footprint_box(500, 260, 90, (-200, -20), (200, 20))   # 4 m long, turned across
        self.assertIsNotNone(self.index.nearest(long_thing))
        self.assertIsNone(self.index.nearest(lc.footprint_box(500, 260, 0, (-200, -20), (200, 20))))

    def test_tree_trunk_not_crown(self):
        self.assertIsNone(self.index.nearest(((500, 250), lc.TRUNK_RADIUS_CM)))

    def test_lane_far_below_is_ignored(self):
        box = lc.footprint_box(500, 0, 0, (-50, -50), (50, 50))
        self.assertIsNone(self.index.nearest(box, z_low=900.0))
        self.assertIsNotNone(self.index.nearest(box, z_low=100.0))


if __name__ == "__main__":
    unittest.main()
