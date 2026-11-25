import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import footprint_fill as ff  # noqa: E402
import squares  # noqa: E402

# The cinema at the south end of rue de la Republique, as landmarks.json holds it (local metres).
CINEMA_M = [(73.7, -492.8), (72.5, -496.2), (70.9, -500.6), (82.0, -504.3), (85.5, -495.0), (87.2, -490.5),
            (88.3, -487.6), (86.1, -484.0), (67.2, -480.7), (66.7, -489.2), (73.4, -490.6), (73.1, -491.3),
            (73.2, -492.0), (73.7, -492.8)]


class FootprintFill(unittest.TestCase):
    def setUp(self):
        self.ring = squares.to_ue(CINEMA_M)
        self.boxes = ff.strips(self.ring)

    def test_no_box_passes_a_wall(self):
        self.assertGreater(len(self.boxes), 30)
        for x0, y0, x1, y1 in self.boxes:
            self.assertGreaterEqual(min(x1 - x0, y1 - y0), ff.MIN_SIDE_CM)
            for fx in (0.0, 0.25, 0.5, 0.75, 1.0):
                for fy in (0.0, 0.5, 1.0):
                    x = x0 + (x1 - x0) * (0.001 + 0.998 * fx)
                    self.assertTrue(ff.inside(self.ring, x, y0 + (y1 - y0) * fy), (x0, y0, x1, y1))

    def test_the_outline_is_filled(self):
        self.assertGreater(ff.area(self.boxes), 0.85 * squares.area(self.ring))
        self.assertLess(ff.area(self.boxes), squares.area(self.ring))

    def test_where_the_player_stood_at_gate_101_is_filled(self):
        # the player at (7103, 48240) and the camera at (6908, 48373), both inside the hull
        for x, y in ((7103.0, 48240.0), (6908.0, 48373.0)):
            self.assertTrue(ff.inside(self.ring, x, y))
            self.assertTrue(any(x0 <= x <= x1 and y0 - 100.0 <= y <= y1 for x0, y0, x1, y1 in self.boxes), (x, y))
        # and the pavement he came along is not
        self.assertFalse(any(x0 <= 7044.0 <= x1 and y0 <= 47814.0 <= y1 for x0, y0, x1, y1 in self.boxes))

    def test_a_square_gives_one_box_a_strip(self):
        boxes = ff.strips([(0.0, 0.0), (400.0, 0.0), (400.0, 300.0), (0.0, 300.0)], step=100.0, inset=10.0)
        self.assertEqual(boxes, [(0.0, 10.0, 100.0, 290.0), (100.0, 10.0, 200.0, 290.0),
                                 (200.0, 10.0, 300.0, 290.0), (300.0, 10.0, 400.0, 290.0)])

    def test_a_courtyard_shape_is_left_open(self):
        # a U: the strips of its hollow are two boxes, the hollow between them stays free
        u = [(0.0, 0.0), (600.0, 0.0), (600.0, 600.0), (400.0, 600.0), (400.0, 200.0), (200.0, 200.0),
             (200.0, 600.0), (0.0, 600.0)]
        boxes = ff.strips(u, step=100.0, inset=10.0)
        self.assertFalse(any(x0 < 250.0 < x1 and y0 < 400.0 < y1 for x0, y0, x1, y1 in boxes))
        self.assertTrue(any(x0 < 250.0 < x1 and y0 < 100.0 < y1 for x0, y0, x1, y1 in boxes))

    def test_nothing_from_a_line(self):
        self.assertEqual(ff.strips([(0.0, 0.0), (100.0, 0.0)]), [])


if __name__ == "__main__":
    unittest.main()
