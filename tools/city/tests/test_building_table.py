import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import building_table as bt  # noqa: E402


def box_house(w=10.0, d=8.0, eave=12.0, ridge=16.0):
    """A gabled house as CityGML-like faces: ground, 4 walls (2 gables), 2 roof slopes."""
    g = [[0, 0, 0], [w, 0, 0], [w, d, 0], [0, d, 0]]
    faces = [g[::-1],
             [[0, 0, 0], [w, 0, 0], [w, 0, eave], [0, 0, eave]],
             [[w, d, 0], [0, d, 0], [0, d, eave], [w, d, eave]],
             [[w, 0, 0], [w, d, 0], [w, d, eave], [w, d / 2, ridge], [w, 0, eave]],
             [[0, d, 0], [0, 0, 0], [0, 0, eave], [0, d / 2, ridge], [0, d, eave]],
             [[0, 0, eave], [w, 0, eave], [w, d / 2, ridge], [0, d / 2, ridge]],
             [[w, d, eave], [0, d, eave], [0, d / 2, ridge], [w, d / 2, ridge]]]
    v, polys, nv = [], [], 0
    for f in faces:
        polys.append((0, nv, len(f), -1))
        v += f
        nv += len(f)
    return {"v": np.array(v, np.float32), "polys": np.array(polys), "first": np.array([0, len(polys)])}


class RoofOf(unittest.TestCase):
    def test_gabled_house(self):
        r = bt.roof_of(box_house(), 0)
        self.assertEqual(r["roof_type"], "pitched")
        self.assertAlmostEqual(r["eave_z"], 12.0, places=3)
        self.assertAlmostEqual(r["ridge_z"], 16.0, places=3)
        self.assertAlmostEqual(r["roof_pitch_deg"], 45.0, delta=0.5)   # rise 4 over 4 m

    def test_flat_roof(self):
        r = bt.roof_of(box_house(eave=20.0, ridge=20.0), 0)
        self.assertEqual(r["roof_type"], "flat")


class Labels(unittest.TestCase):
    def test_era_and_style(self):
        self.assertEqual(bt.era_of(1860), "1853-1914")
        self.assertIsNone(bt.era_of(None))
        self.assertEqual(bt.style_of(1860, False, None, 18.0, False, True)[0], "haussmann")
        self.assertEqual(bt.style_of(1860, False, None, 18.0, False, False)[0], "xixe")
        self.assertEqual(bt.style_of(1700, False, None, 12.0, False, False)[0], "ancien")
        self.assertEqual(bt.style_of(None, True, None, 30.0, False, False)[0], "monument")
        self.assertEqual(bt.style_of(1700, False, "haussmann", 12.0, False, False), ("haussmann", "carte-de-la-rue"))

    def test_storeys(self):
        # rue de la Republique: ground 4.2 + entresol 2.6 + 4 x 3.3 = 20 m, plus a mansard
        self.assertEqual(bt.storeys_from("haussmann", 20.0, True), 7)
        self.assertEqual(bt.storeys_from("ancien", 3.0, False), 1)


if __name__ == "__main__":
    unittest.main()
