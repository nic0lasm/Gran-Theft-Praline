import os
import sys
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import pits  # noqa: E402


def flat(rows, cols, h=0.0):
    return [[h] * cols for _ in range(rows)]


class PitTests(unittest.TestCase):
    def test_closed_ramp_is_a_pit(self):
        z = flat(20, 20)
        for r in range(8, 12):
            for c in range(5, 15):
                z[r][c] = -300.0 + 20 * (c - 5)          # a ramp down to 3 m, closed all round
        found = pits.pits(z, 200.0)
        self.assertEqual(len(found), 1)
        p = found[0]
        self.assertEqual(p["spill_cm"], 0.0)
        self.assertGreaterEqual(p["depth_cm"], 280.0)
        self.assertEqual(len(p["cells"]), 40)

    def test_river_open_to_the_edge_is_not(self):
        z = flat(20, 20)
        for r in range(20):
            for c in range(0, 4):
                z[r][c] = -500.0                          # low channel along the west edge
        self.assertEqual(pits.pits(z, 200.0), [])

    def test_shallow_dip_is_not(self):
        z = flat(10, 10)
        z[5][5] = -50.0
        self.assertEqual(pits.pits(z, 200.0), [])

    def test_lid_runs_follow_rows(self):
        cells = [(0, 1), (0, 2), (0, 3), (0, 6), (1, 2)]
        self.assertEqual(pits.lid_runs(cells), [(0, 1, 3), (0, 6, 6), (1, 2, 2)])

    def test_no_ground_drains_like_an_edge(self):
        z = flat(10, 10)
        z[5][5] = -400.0
        z[5][6] = None                                    # a hole in the sampling next to it
        self.assertEqual(pits.pits(z, 200.0, min_area_m2=1.0), [])


if __name__ == "__main__":
    unittest.main()
