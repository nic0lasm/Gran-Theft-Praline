import json
import os
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..")))
import ground_mask  # noqa: E402
import meshkit  # noqa: E402
import squares  # noqa: E402

STEP_M = 4.0
SIZE_M = 256.0          # one terrain tile: x 0 to 256 m east, y 0 to -256 m north (Unreal y 0 to 256 m)
RIVER_X_M = (160.0, 230.0)
LEVEL_M = -5.5


def height(x, _y):
    return LEVEL_M if RIVER_X_M[0] <= x <= RIVER_X_M[1] else 0.3


def write_zone(base):
    """A quay road along a river, as the pipeline stores a zone: terrain tile, roads, water."""
    os.makedirs(os.path.join(base, "terrain"))
    m = meshkit.Mesh()
    n = int(SIZE_M / STEP_M)
    for i in range(n):
        for j in range(n):
            xa, ya = j * STEP_M, -SIZE_M + i * STEP_M
            m.quad(*[(x, y, height(x, y)) for x, y in ((xa, ya), (xa + STEP_M, ya), (xa + STEP_M, ya + STEP_M),
                                                       (xa, ya + STEP_M))], (0.4, 0.4, 0.4))
    meshkit.write_glb(os.path.join(base, "terrain", "terrain.glb"), [("SM_Terrain_0", m, (0.0, -SIZE_M, 0.0))])

    def road(cleabs, pts, **props):
        p = {"cleabs": cleabs, "nature": "Route à 1 chaussée", "largeur_de_chaussee": 6.0, "nombre_de_voies": 2,
             "sens_de_circulation": "Double sens", "vitesse_moyenne_vl": 30, "position_par_rapport_au_sol": "0"}
        p.update(props)
        return {"type": "Feature", "properties": p, "geometry": {"type": "MultiLineString", "coordinates": [pts]}}

    roads = [road("QUAY", [[120.0, -10.0], [120.0, -246.0]]),
             road("PATH", [[40.0, -128.0], [100.0, -128.0]], nature="Sentier"),
             road("BRIDGE", [[120.0, -60.0], [250.0, -60.0]], position_par_rapport_au_sol="1")]
    with open(os.path.join(base, "roads.geojson"), "w", encoding="utf-8") as f:
        json.dump({"type": "FeatureCollection", "features": roads}, f)
    wavy = [[RIVER_X_M[0] + (1.0 if k % 2 else 0.0), -4.0 * k] for k in range(-10, 80)]      # a bank of many points
    ring = wavy + [[RIVER_X_M[1], -320.0], [RIVER_X_M[1], 40.0], wavy[0]]
    water = [{"type": "Feature", "properties": {"cleabs": "RIVER", "nature": "Ecoulement naturel"},
              "geometry": {"type": "MultiPolygon", "coordinates": [[ring]]}},
             {"type": "Feature", "properties": {"cleabs": "BASIN", "nature": "Réservoir-bassin"},
              "geometry": {"type": "MultiPolygon", "coordinates": [[[[10, -10], [30, -10], [30, -30], [10, -10]]]]}}]
    with open(os.path.join(base, "water.geojson"), "w", encoding="utf-8") as f:
        json.dump({"type": "FeatureCollection", "features": water}, f)


class GroundFarTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from PIL import Image
        cls.dir = tempfile.TemporaryDirectory()
        write_zone(cls.dir.name)
        cls.meta = ground_mask.build("zone", "focus", far=True, processed=cls.dir.name, far_texels=512)
        cls.out = os.path.join(cls.dir.name, "ground_far")
        cls.img = Image.open(os.path.join(cls.out, "T_GroundMaskFar.png"))
        cls.img.load()

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def texel(self, x_m, y_south_m):
        x0, y0, x1, y1 = self.meta["extent_ue_cm"]
        w, h = self.img.size
        return self.img.getpixel((int((x_m * 100.0 - x0) / (x1 - x0) * w), int((y_south_m * 100.0 - y0) / (y1 - y0) * h)))

    def test_the_mask_covers_the_terrain_and_has_no_alpha(self):
        self.assertEqual(self.meta["extent_ue_cm"], [0.0, 0.0, 25600.0, 25600.0])
        with open(os.path.join(self.out, "T_GroundMaskFar.png"), "rb") as f:
            head = f.read(26)
        self.assertEqual(struct.unpack(">II", head[16:24]), (512, 512))
        self.assertEqual((head[24], head[25]), (8, 2))        # 8 bits, RGB: no alpha for the importer to fill
        with open(os.path.join(self.out, "ground_far.json"), encoding="utf-8") as f:
            self.assertEqual(json.load(f)["size_px"], [512, 512])

    def test_carriageway_kerb_sidewalk_and_path_are_where_the_roads_are(self):
        r, g, b = self.texel(121.5, 128.0)                    # on the quay road, off its centre line
        self.assertGreater(r, 240)
        self.assertEqual(g, 0)
        self.assertGreater(self.texel(120.0, 20.0)[2] + self.texel(120.0, 21.5)[2] + self.texel(120.0, 23.0)[2]
                           + self.texel(120.0, 24.5)[2] + self.texel(120.0, 26.0)[2], 60)     # dashes on its axis
        kerb = max(self.texel(120.0 + 3.0 + d, 128.0)[2] for d in (0.0, 0.25, 0.5))
        self.assertGreater(kerb, 60)                           # a kerb one texel wide survives the averaging
        self.assertEqual(self.texel(140.0, 128.0), (0, 0, 0))  # sidewalk between the road and the bank
        self.assertGreater(self.texel(70.0, 128.0)[1], 240)    # the footpath is paving
        self.assertEqual(self.texel(200.0, 60.0)[0], 0)        # no asphalt under a bridge: the river stays a river

    def test_the_river_comes_with_few_corners_and_its_level(self):
        self.assertEqual([r["name"] for r in self.meta["rivers"]], ["RIVER"])      # the basin is not a river
        river = self.meta["rivers"][0]
        self.assertLessEqual(len(river["ring"]), 8)            # a wavy bank of 90 points
        self.assertAlmostEqual(river["level_cm"], LEVEL_M * 100.0, delta=1.0)
        self.assertLess(river["level_spread_cm"], 5.0)
        ring = [tuple(p) for p in river["ring"]]
        self.assertLess(squares.distance([ring], (19500.0, 12800.0)), -3000.0)     # mid river
        self.assertGreater(squares.distance([ring], (12000.0, 12800.0)), 3000.0)   # the quay road
        self.assertEqual(squares.hlsl([ring]).count("if (p.x > "), 1)


if __name__ == "__main__":
    unittest.main()
