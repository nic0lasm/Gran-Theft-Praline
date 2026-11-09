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
         "window_small", "court_window", "monument_bay", "blank"}


class KitLayoutTests(unittest.TestCase):
    def test_every_kind_has_a_module(self):
        for typ in procgen.TYPOLOGY:
            for kind in KINDS:
                self.assertTrue(kit_layout.module_for(typ, kind).startswith("SM_Facade_"))

    def test_kinds_match_procgen(self):
        src = open(os.path.join(HERE, "..", "procgen.py"), encoding="utf-8").read()
        emitted = set(re.findall(r'mk = "([a-z_]+)"', src)) | {"blank"}
        self.assertEqual(emitted, KINDS)

    def test_modules_are_built_by_the_kit(self):
        src = open(os.path.join(HERE, "..", "facade_kit", "kit.py"), encoding="utf-8").read()
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


if __name__ == "__main__":
    unittest.main()
