import math
import os
import re
import sys
import unittest

HERE = os.path.dirname(__file__)
sys.path.insert(0, os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(HERE, "..", "facade_kit"))
import generate  # noqa: E402
import kit_layout  # noqa: E402
import procgen  # noqa: E402

KINDS = {"shop", "shopfront_closed", "entrance", "gf_window", "entresol_window", "window", "window_balcony",
         "window_small", "court_window", "monument_bay", "blank", "dormer", "mansard_blank"}


class KitLayoutTests(unittest.TestCase):
    def test_every_kind_has_a_module(self):
        for typ in procgen.TYPOLOGY:
            for kind in KINDS:
                self.assertTrue(kit_layout.module_for(typ, kind).startswith(("SM_Facade_", "SM_Roof_")))

    def test_kinds_match_procgen(self):
        src = open(os.path.join(HERE, "..", "procgen.py"), encoding="utf-8").read()
        emitted = set(re.findall(r'mk = "([a-z_]+)"', src)) | {"blank"}
        emitted |= set(re.findall(r'"(dormer|mansard_blank)"', src))
        self.assertEqual(emitted, KINDS)

    def test_modules_are_built_by_the_kit(self):
        src = open(os.path.join(HERE, "..", "facade_kit", "kit.py"), encoding="utf-8").read()
        src += open(os.path.join(HERE, "..", "facade_kit", "roof_kit.py"), encoding="utf-8").read()
        built = set(re.findall(r'"(SM_[A-Za-z]+_[A-Za-z_]+)": \(', src))
        used = {m for fam in kit_layout.KIND_TO_MODULE.values() for m in fam.values()}
        used |= {v for opts in kit_layout.VARIANTS.values() for v in opts} | set(kit_layout.FASCIA_FRONT_Y)
        self.assertTrue(used <= built, used - built)
        self.assertIn("SM_Sign_Panel", built)

    def test_variants_are_stable_per_building(self):
        a = kit_layout.module_for("haussmann", "shop", "BATIMENT1")
        self.assertEqual(a, kit_layout.module_for("haussmann", "shop", "BATIMENT1"))
        seen = {kit_layout.module_for("haussmann", "shop", "B%d" % k) for k in range(200)}
        self.assertEqual(seen, set(kit_layout.VARIANTS["SM_Facade_Haussmann_Shop"]))
        self.assertEqual(kit_layout.module_for("haussmann", "shop"), "SM_Facade_Haussmann_Shop")

    def test_sign_sits_on_the_fascia(self):
        # nominal Haussmann shop: fascia 0.6 m whose top is 0.95 m under the 4.2 m storey top
        z, out = kit_layout.sign_frame("SM_Facade_Haussmann_Shop", "haussmann", 4.2, 4.2)
        self.assertAlmostEqual(z, 4.2 - 0.95 - 0.3)
        self.assertAlmostEqual(out, -0.01)  # fascia set 2 cm back in the opening
        z, out = kit_layout.sign_frame("SM_Facade_Haussmann_Shop_B", "haussmann", 4.6, 4.2)
        self.assertAlmostEqual(z, (4.2 - 1.25) * 4.6 / 4.2)
        self.assertAlmostEqual(out, 0.21)

    def test_nominal_sizes_match_procgen(self):
        self.assertEqual(kit_layout.HAUSSMANN["bay"], procgen.TYPOLOGY["haussmann"]["bay"])
        self.assertEqual(kit_layout.HAUSSMANN["gf"], procgen.TYPOLOGY["haussmann"]["gf"])
        self.assertEqual(kit_layout.LYONNAIS["upper"], procgen.TYPOLOGY["ancien"]["upper"])

    def test_yaw_turns_module_front_outward(self):
        # assemble.py: Blender yaw = -ue_yaw + 90 deg turns the module's -Y side to (nx, ny)
        for nx, ny in ((1, 0), (0, 1), (-0.6, 0.8), (0.28, -0.96)):
            a = math.radians(-generate.ue_yaw(nx, ny)) + math.pi / 2
            front = (math.sin(a), -math.cos(a))  # rotate (0, -1)
            self.assertAlmostEqual(front[0], nx, places=2)
            self.assertAlmostEqual(front[1], ny, places=2)


class MansardTests(unittest.TestCase):
    def test_tall_haussmann_ends_with_a_mansard(self):
        lv = procgen.levels_for("haussmann", 20.0)
        self.assertEqual(lv[-1][0], "mansard")
        self.assertAlmostEqual(lv[-1][1] + lv[-1][2], 20.0, places=3)
        self.assertNotIn("mansard", [k for k, _, _ in lv[:-1]])

    def test_stone_storeys_stay_near_nominal(self):
        for wall in (19.5, 20.0, 21.0, 22.6, 23.2, 24.0, 26.5):
            ups = [h for k, _, h in procgen.levels_for("haussmann", wall) if k == "upper"]
            self.assertTrue(all(abs(h - 3.3) < 0.35 for h in ups), (wall, ups))

    def test_low_or_other_types_have_no_mansard(self):
        self.assertNotIn("mansard", [k for k, _, _ in procgen.levels_for("haussmann", 15.0)])
        self.assertNotIn("mansard", [k for k, _, _ in procgen.levels_for("ancien", 22.0)])


class InteriorCutoutTests(unittest.TestCase):
    def setUp(self):
        import interior_cutouts
        self.ic = interior_cutouts
        # 19 x 16 m interior whose front runs east from (1000, 2000) cm; inward = local north = Unreal -Y
        self.frame = ("shop", (1000.0, 2000.0), (1.0, 0.0), (0.0, -1.0), 1900.0, 1600.0)

    def row(self, x, y, storey=0, side="street"):
        return {"ue_x": str(x), "ue_y": str(y), "storey": str(storey), "side": side}

    def test_only_the_ground_bays_on_the_glass_front_are_cut(self):
        rows = [self.row(1500, 2000), self.row(1500, 2080), self.row(1500, 2000, side="court"),
                self.row(1500, 2000, storey=1), self.row(3100, 2000), self.row(1500, 1850),
                self.row(1500, 300)]
        kept, cut = self.ic.cut_rows(rows, [self.frame])
        self.assertEqual(cut, {"shop": 3})
        self.assertEqual(len(kept), 4)

    def test_repo_interiors_load(self):
        frames = self.ic.load_frames(os.path.join(HERE, "..", "..", "interiors"))
        for _id, _o, front, inward, w, d in frames:
            self.assertAlmostEqual(math.hypot(*front), 1.0)
            self.assertAlmostEqual(front[0] * inward[0] + front[1] * inward[1], 0.0)
            self.assertGreater(w * d, 0)


if __name__ == "__main__":
    unittest.main()
