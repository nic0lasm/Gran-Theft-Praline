import math
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import parking  # noqa: E402
from test_traffic import road  # noqa: E402


class ParkingTests(unittest.TestCase):
    def test_wide_two_way_parks_on_both_kerbs_facing_the_traffic(self):
        # 100 m east-west street, 6 m half width: lanes at +-1.6 m, kerbs at +-6 m.
        centre = [(x, 0.0) for x in range(0, 101, 2)]
        slots = parking.build_slots([road("W", [(0, 0), (100, 0)])], {"W": (centre, [6.0] * len(centre))}, share=1.0)
        south = [s for s in slots if s[1] < 0]
        north = [s for s in slots if s[1] > 0]
        self.assertTrue(south and north)
        for x, y, heading, _ in south:
            self.assertAlmostEqual(y, -(6.0 - parking.PARK_KERB_M), places=3)
            self.assertAlmostEqual(heading, 0.0, places=3)  # eastbound side (right of travel)
            self.assertTrue(parking.JUNCTION_CLEAR_M <= x <= 100 - parking.JUNCTION_CLEAR_M)
        for _, y, heading, _ in north:
            self.assertAlmostEqual(abs(heading), math.pi, places=3)  # westbound side
        xs = sorted(s[0] for s in south)
        self.assertTrue(all(b - a >= parking.SLOT_M - 1e-6 for a, b in zip(xs, xs[1:])))

    def test_narrow_street_has_no_parking(self):
        centre = [(x, 0.0) for x in range(0, 101, 2)]
        slots = parking.build_slots([road("N", [(0, 0), (100, 0)])], {"N": (centre, [3.0] * len(centre))}, share=1.0)
        self.assertEqual(slots, [])

    def test_one_way_against_drawing_faces_the_traffic(self):
        centre = [(x, 0.0) for x in range(0, 101, 2)]
        f = road("O", [(0, 0), (100, 0)], sens="Sens inverse", voies=1, largeur=8.0)
        slots = parking.build_slots([f], {"O": (centre, [4.0] * len(centre))}, share=1.0)
        self.assertTrue(slots)
        sides = {round(s[1], 2) for s in slots}
        self.assertEqual(sides, {round(4.0 - parking.PARK_KERB_M, 2), round(-(4.0 - parking.PARK_KERB_M), 2)})
        for _, _, heading, _ in slots:
            self.assertAlmostEqual(abs(heading), math.pi, places=3)  # traffic goes west

    def test_share_is_stable(self):
        centre = [(x, 0.0) for x in range(0, 301, 2)]
        cw = {"W": (centre, [6.0] * len(centre))}
        a = parking.build_slots([road("W", [(0, 0), (300, 0)])], cw, share=0.5)
        b = parking.build_slots([road("W", [(0, 0), (300, 0)])], cw, share=0.5)
        full = parking.build_slots([road("W", [(0, 0), (300, 0)])], cw, share=1.0)
        self.assertEqual(a, b)
        self.assertLess(len(a), len(full))


if __name__ == "__main__":
    unittest.main()
