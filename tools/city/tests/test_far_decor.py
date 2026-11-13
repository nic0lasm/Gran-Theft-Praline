import io
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import far_decor  # noqa: E402


class Cc46ToLambert93(unittest.TestCase):
    def test_reference_points(self):
        # Values from pyproj (EPSG:3946 -> EPSG:2154), checked on 2026-11-13.
        x, y = far_decor.cc46_to_l93(np.array([1842000.0, 1840000.0]), np.array([5174000.0, 5177000.0]))
        np.testing.assert_allclose(x, [841888.444, 839889.424], atol=0.01)
        np.testing.assert_allclose(y, [6518508.895, 6521506.141], atol=0.01)


class Triangulate(unittest.TestCase):
    def area(self, p, tris):
        cross = lambda u, v: u[0] * v[1] - u[1] * v[0]  # noqa: E731
        return sum(abs(cross(p[b] - p[a], p[c] - p[a])) / 2 for a, b, c in tris)

    def test_concave_l_shape_both_windings(self):
        p = np.array([[0, 0], [4, 0], [4, 1], [1, 1], [1, 3], [0, 3]], float)
        for poly in (p, p[::-1]):
            tris = far_decor.triangulate(poly)
            self.assertEqual(len(tris), 4)
            self.assertAlmostEqual(self.area(poly, tris), 6.0)


class AtlasPacking(unittest.TestCase):
    def test_uv_inside_slot_and_full_atlas(self):
        from PIL import Image
        at = far_decor.Atlas(64)
        r = at.add_patch(Image.new("RGB", (8, 8), (255, 0, 0)), 32)
        uv = at.uv(r, np.array([[0, 0], [1, 1]], np.float32))
        x0, y0, s = r
        self.assertTrue(np.all(uv[:, 0] >= x0 / 64) and np.all(uv[:, 0] <= (x0 + s) / 64))
        self.assertAlmostEqual(float(uv[0, 1]), (y0 + s - 1) / 64)    # CityGML v up -> glTF v down
        grey = at.uv(r, np.full((3, 2), -1.0, np.float32))
        self.assertTrue(np.allclose(grey, grey[0]))
        self.assertIsNotNone(at.add_patch(None, 32))
        self.assertIsNone(at.add_patch(None, 64))                    # no room left


class AtlasCrop(unittest.TestCase):
    def test_crop_to_used_power_of_two(self):
        at = far_decor.Atlas(1024)
        r = at.add_patch(None, 100)
        img, (su, sv) = at.cropped()
        self.assertEqual(img.size, (128, 128))                     # one row of 16 + 100 px
        uv = at.uv(r, np.array([[1.0, 0.0]], np.float32)) * [su, sv]
        self.assertTrue(np.all(uv <= 1.0))


class TerrainTiles(unittest.TestCase):
    def test_shared_edges_hole_and_skirts(self):
        import json
        import tempfile
        from shapely.geometry import LineString
        with tempfile.TemporaryDirectory() as d:
            n, res = 30, 100.0                                     # 3 km square, 3 x 3 tiles
            yy, xx = np.mgrid[0:n, 0:n]
            (xx * 1.0 + yy * 2.0 + 170).astype("<f4").tofile(os.path.join(d, "mnt.f32"))
            with open(os.path.join(d, "far.json"), "w") as f:
                json.dump({"envelope_l93": [0, 0, 3000, 3000], "res_m": res, "width": n, "height": n,
                           "mnt": "mnt.f32"}, f)
            axis = LineString([(1500, 1500), (1500, 1600)])
            tiles = list(far_decor.terrain_mesh(d, (0, 0), 0.0, [1100, 1100, 1900, 1900], axis,
                                                far_decor.terrain_steps("300:1,inf:2")))
            self.assertEqual(len(tiles), 9)
            centre = [t for t in tiles if t[0] == (1000, 1000)][0]
            self.assertEqual(centre[5], 1)
            v, tris = centre[1], centre[3]
            cx = v[tris].mean(1)
            self.assertFalse(np.any((cx[:, 0] > 1200) & (cx[:, 0] < 1800) & (cx[:, 1] > 1200) & (cx[:, 1] < 1800)
                                    & (cx[:, 2] > -1)), "hole over the zone envelope")
            corner = [t for t in tiles if t[0] == (0, 0)][0]
            self.assertEqual(corner[5], 2)
            cv = corner[1]
            top = {}
            for x, y, z in cv:                                       # surface = highest vertex per x, y
                k = (round(float(x), 3), round(float(y), 3))
                top[k] = max(top.get(k, z), z)
            drops = [top[(round(float(x), 3), round(float(y), 3))] - z for x, y, z in cv
                     if (round(float(x), 3), round(float(y), 3)) in top and top[(round(float(x), 3), round(float(y), 3))] > z]
            self.assertTrue(drops and np.allclose(drops, 6.0))       # skirts hang 6 m under the edges
            east = [t for t in tiles if t[0] == (1000, 0)][0][1]
            xs_edge = sorted(set(np.round(corner[1][:, 0], 3)) & set(np.round(east[:, 0], 3)))
            self.assertEqual(len(xs_edge), 1)                      # the two tiles share one sample column


class GlbWriter(unittest.TestCase):
    def test_embedded_texture(self):
        from PIL import Image
        w = far_decor.GlbWriter()
        m = w.material("T_Test", far_decor.jpeg(Image.new("RGB", (4, 4))))
        v = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], np.float32)
        w.mesh("SM_Test", v, np.tile([0, 0, 1], (3, 1)).astype(np.float32), np.zeros((3, 2), np.float32),
               np.array([[0, 1, 2]], np.uint32), m)
        path = os.path.join(os.path.dirname(__file__), "_far_decor_test.glb")
        try:
            w.write(path, {})
            data = open(path, "rb").read()
            self.assertEqual(data[:4], b"glTF")
            self.assertIn(b"TEXCOORD_0", data)
            self.assertIn(b"image/jpeg", data)
        finally:
            if os.path.exists(path):
                os.remove(path)


if __name__ == "__main__":
    unittest.main()
