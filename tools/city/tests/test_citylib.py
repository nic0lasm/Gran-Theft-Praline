import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import citylib as cl  # noqa: E402


class ProjectionTests(unittest.TestCase):
    def test_projection_origin(self):
        x, y = cl.lonlat_to_l93(3.0, 46.5)
        self.assertAlmostEqual(x, 700000.0, places=3)
        self.assertAlmostEqual(y, 6600000.0, places=3)

    def test_matches_ign_reference(self):
        # Station Bellecour as served by the IGN WFS in both EPSG:4326 and EPSG:2154.
        x, y = cl.lonlat_to_l93(4.83371681, 45.75785624)
        self.assertAlmostEqual(x, 842528.40, delta=0.05)
        self.assertAlmostEqual(y, 6519238.70, delta=0.05)

    def test_unreal_axes(self):
        self.assertEqual(cl.to_unreal(10, 20, 3), (1000.0, -2000.0, 300.0))


class GeometryTests(unittest.TestCase):
    SQUARE = [(0, 0), (10, 0), (10, 10), (0, 10)]

    def test_area_and_centroid(self):
        self.assertAlmostEqual(cl.ring_area(self.SQUARE), 100.0)
        self.assertEqual(cl.ring_centroid(self.SQUARE), (5.0, 5.0))
        self.assertAlmostEqual(cl.ring_area(self.SQUARE[::-1]), -100.0)

    def test_point_in_ring(self):
        self.assertTrue(cl.point_in_ring((5, 5), self.SQUARE))
        self.assertFalse(cl.point_in_ring((15, 5), self.SQUARE))

    def test_distance_to_ring(self):
        self.assertAlmostEqual(cl.dist_point_ring((13, 5), self.SQUARE), 3.0)


if __name__ == "__main__":
    unittest.main()
