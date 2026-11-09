import os
import sys
import tempfile
import unittest

import numpy as np
from shapely.geometry import LineString, Polygon

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import meshkit  # noqa: E402
import procgen  # noqa: E402
import raster  # noqa: E402
import render  # noqa: E402
import terrain  # noqa: E402

IDENT = lambda p: (p[0], p[1])  # noqa: E731


def flat_raster(z, env=(-20, -20, 20, 20), res=0.5):
    w, h = int((env[2] - env[0]) / res), int((env[3] - env[1]) / res)
    return raster.Raster(np.full((h, w), z, np.float32), env, res)


class RasterTests(unittest.TestCase):
    def test_sample_and_cells(self):
        r = flat_raster(5.0)
        self.assertAlmostEqual(float(r.sample(np.array([1.0]), np.array([2.0]))[0]), 5.0)
        self.assertTrue(np.isnan(r.sample(np.array([100.0]), np.array([0.0]))[0]))
        x, y, v = r.cells_in_ring([(0, 0), (4, 0), (4, 4), (0, 4)])
        self.assertEqual(len(v), 64)  # 4 m x 4 m of 0.5 m cells

    def test_north_up_rows(self):
        g = np.zeros((4, 4), np.float32)
        g[0, :] = 10.0  # first row is the north edge
        r = raster.Raster(g, (0, 0, 4, 4), 1.0)
        self.assertAlmostEqual(float(r.sample(np.array([1.5]), np.array([3.5]))[0]), 10.0)


class FacadeTests(unittest.TestCase):
    def test_storeys_fill_wall_height(self):
        for typ in procgen.TYPOLOGY:
            for wall in (3.0, 9.5, 18.0, 23.7):
                levels = procgen.storeys(typ, wall)
                self.assertAlmostEqual(levels[-1][1] + levels[-1][2], wall, places=6)

    def test_haussmann_has_entresol(self):
        self.assertEqual([k for k, _, _ in procgen.storeys("haussmann", 21.0)][:2], ["gf", "entresol"])

    def test_material_class(self):
        self.assertEqual(procgen.material_class("30"), "3")
        self.assertEqual(procgen.material_class("00"), "")
        self.assertEqual(procgen.material_class(None), "")

    def test_edge_classification(self):
        a = Polygon([(0, 0), (10, 0), (10, 10), (0, 10)])
        b = Polygon([(10, 0), (20, 0), (20, 10), (10, 10)])
        road = (LineString([(-20, -6), (40, -6)]), 4.0, "Rue de la République")
        ctx = procgen.Context({"a": a, "b": b}, [road])
        kinds = {}
        for ri, ei, p0, p1, n in procgen.edge_frames(a):
            kinds[tuple(np.round(n).astype(int))] = procgen.classify_edge(ctx, "a", p0, p1, n)
        self.assertEqual(kinds[(1, 0)][0], "party")
        self.assertEqual(kinds[(0, -1)], ("street", "Rue de la République"))
        self.assertEqual(kinds[(0, 1)][0], "court")

    def test_layout_places_shop_sign(self):
        a = Polygon([(0, 0), (20, 0), (20, 12), (0, 12)])
        road = (LineString([(-20, -6), (40, -6)]), 4.0, "Rue de la République")
        ctx = procgen.Context({"a": a}, [road])
        roof = {"eave_z": 21.0, "top_z": 24.0, "kind": "flat", "ridges": []}
        shop = {"x": 15.0, "y": 0.5, "name": "Boulangerie", "brand": "", "brand_wikidata": "", "osm_id": "n/1"}
        mods, info = procgen.layout_building(ctx, {"id": "a"}, a, 0.0, roof, [shop])
        self.assertEqual(info["typology"], "haussmann")
        shops = [m for m in mods if m["kind"] == "shop" and not m["sign_continued"]]
        self.assertEqual(len(shops), 1)
        self.assertEqual(shops[0]["sign_name"], "Boulangerie")
        self.assertGreater(shops[0]["x"], 10.0)
        self.assertEqual(sum(1 for m in mods if m["kind"] == "entrance"), 1)


class RoofTests(unittest.TestCase):
    def test_gable_roof_follows_ridge(self):
        poly = Polygon([(0, 0), (10, 0), (10, 8), (0, 8)])
        roof = {"kind": "ridged", "eave_z": 10.0, "top_z": 13.0, "ridges": [[(0.2, 4, 13.0), (9.8, 4, 13.0)]]}
        v, t = procgen.roof_triangles(poly, roof)
        self.assertGreater(len(t), 0)
        self.assertAlmostEqual(v[:, 2].max(), 13.0)
        self.assertAlmostEqual(v[:, 2].min(), 10.0)
        # a sample under the ridge reads the ridge height, one at the eave the eave height
        e = procgen.roof_error(v, t, np.array([5.0, 5.0]), np.array([4.0, 0.01]), np.array([13.0, 10.0]))
        self.assertLess(e.max(), 0.05)

    def test_fit_roof_flat(self):
        poly = Polygon([(0, 0), (10, 0), (10, 10), (0, 10)])
        roof = procgen.fit_roof(poly, flat_raster(18.0), 0.0, 0.0, [], IDENT)
        self.assertEqual(roof["kind"], "flat")
        self.assertAlmostEqual(roof["eave_z"], 18.0, places=3)

    def test_courtyard_is_carved(self):
        mns = flat_raster(20.0)
        mnt = flat_raster(0.0)
        c, r = mns.to_px(np.array([0.0]), np.array([0.0]))
        c, r = int(c[0]), int(r[0])
        mns.grid[r - 6:r + 6, c - 6:c + 6] = 0.3  # a 6 m x 6 m open courtyard in the middle
        poly = Polygon([(-8, -8), (8, -8), (8, 8), (-8, 8)])
        parts, removed = procgen.carve_courtyards(poly, mns, mnt, IDENT, IDENT)
        self.assertTrue(all(h is None for _, h in parts))
        self.assertAlmostEqual(sum(p.area for p, _ in parts), 256 - 36, delta=6)
        self.assertGreater(removed, 0.1)


class OutputTests(unittest.TestCase):
    def test_glb_round_trip(self):
        m = meshkit.Mesh()
        m.quad((0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1), (1.0, 0.5, 0.0))
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "t.glb")
            meshkit.write_glb(path, [("SM_Test", m, (10.0, 20.0, 1.0))])
            V, C, I, gltf = render.read_glb(path)
        self.assertEqual(I.shape, (2, 3))
        self.assertEqual(gltf["nodes"][0]["translation"], [10.0, 1.0, -20.0])
        # vertices come back in local metres, node translation included
        self.assertAlmostEqual(V[:, 0].min(), 0.0, places=5)
        self.assertAlmostEqual(V[:, 2].max(), 1.0, places=5)
        self.assertAlmostEqual(C[0, 1], 128 / 255, places=2)

    def test_heightmap_formula(self):
        g = np.linspace(160, 200, 100 * 100, dtype=np.float32).reshape(100, 100)
        mnt = raster.Raster(g, (0, 0, 100, 100), 1.0)
        values, z, s = terrain.heightmap(mnt, (0, 0), 170.0, max_spacing_m=1.0)
        self.assertEqual(values.shape[0], s["resolution"])
        back = s["location_ue_cm"][2] + (values.astype(float) - 32768) * s["scale_ue"][2] / 128
        self.assertLess(np.abs(back / 100 - (z - 170.0)).max(), 0.01)


if __name__ == "__main__":
    unittest.main()
