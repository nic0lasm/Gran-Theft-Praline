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

    def test_trees_are_kept_off_the_line_where_the_filmed_flight_ends(self):
        fd, path = tempfile.mkstemp(suffix=".circuit.csv")
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write("# The circuit of the flight\n"
                    "lane_id,start_x_cm,start_y_cm,end_x_cm,end_y_cm,name,line_ue_cm,kmh,mark\n"
                    "approach\n"
                    "7,0,0,0,-3000,QU A,0 -300;0 -600,100,\n"
                    "lap\n"
                    "381,6000,56000,3000,55000,PL BELLECOUR,6000 56000;4500 55500;3000 55000,60,end\n"
                    "373,12000,58000,7000,56500,R DE LA BARRE,12000 58000;7000 56500,,\n"
                    "379,7000,56300,6100,56000,R DE LA BARRE,,75,end\n")
        lines = lc.read_end_lines(path)
        os.remove(path)
        self.assertEqual(len(lines), 2)
        self.assertEqual(lines[0], [(6000.0, 56000.0), (4500.0, 55500.0), (3000.0, 55000.0)])
        self.assertEqual(lines[1], [(7000.0, 56300.0), (6100.0, 56000.0)])       # no line written: the lane's ends
        self.assertAlmostEqual(lc.end_gap(4500.0, 55500.0, lines), 0.0)
        self.assertLess(lc.end_gap(4700.0, 56100.0, lines), lc.END_TREE_CLEAR_CM)     # 5 m off the line
        self.assertGreater(lc.end_gap(4000.0, 57000.0, lines), lc.END_TREE_CLEAR_CM)  # across the street
        self.assertGreater(lc.end_gap(10000.0, 57400.0, lines), lc.END_TREE_CLEAR_CM)  # a lane not marked
        self.assertEqual(lc.read_end_lines(path), [])                             # no file: no rule
        self.assertEqual(lc.end_gap(0.0, 0.0, []), float("inf"))

    def test_the_circuit_of_the_map_names_where_it_ends(self):
        circuit = os.path.join(os.path.dirname(__file__), "..", "..", "..", "GranTheftPraline", "Content", "Test",
                               "Circuits", "L_City_RueDeLaRepublique.circuit.csv")
        if not os.path.exists(circuit):
            self.skipTest("no circuit file in this tree")
        lines = lc.read_end_lines(circuit)
        self.assertGreaterEqual(len(lines), 1)
        length = sum(((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5 for pts in lines for a, b in zip(pts, pts[1:]))
        self.assertTrue(2000.0 <= length <= 30000.0, length)                      # 20 to 300 m of street


if __name__ == "__main__":
    unittest.main()
