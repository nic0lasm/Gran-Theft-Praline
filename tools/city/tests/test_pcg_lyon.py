import os
import re
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import pcg_footprints  # noqa: E402
import pcg_lyon  # noqa: E402


def row(**kw):
    base = {"id": "69382AK57", "style": "haussmann", "roof_type": "mansard", "height_eave_m": 20.5,
            "ground_floor_commercial": True, "remarkable": False}
    base.update(kw)
    return base


class StoreyPlan(unittest.TestCase):
    def test_plan_fills_the_eave_height(self):
        for style, h in (("haussmann", 20.5), ("ancien", 17.0), ("moderne", 24.0), ("xixe", 12.3)):
            fam = pcg_lyon.family_of(row(style=style))
            plan = pcg_lyon.storey_plan(row(style=style, height_eave_m=h), fam)
            self.assertAlmostEqual(sum(z for _, z in plan), h, delta=0.05, msg=style)

    def test_haussmann_storeys(self):
        plan = pcg_lyon.storey_plan(row(height_eave_m=20.5), "haussmann")
        self.assertEqual([r for r, _ in plan], ["RDC", "ENT", "NOB", "ETG", "ETG", "BAL"])

    def test_low_building_is_one_storey(self):
        self.assertEqual(pcg_lyon.storey_plan(row(height_eave_m=4.0), "lyonnais"), [("RDC", 4.0)])


class Grammar(unittest.TestCase):
    def test_families(self):
        self.assertEqual(pcg_lyon.family_of(row(style="ancien")), "lyonnais")
        self.assertEqual(pcg_lyon.family_of(row(style="inconnu", roof_type="flat")), "moderne")
        self.assertEqual(pcg_lyon.family_of(row(remarkable=True)), "none")

    def test_every_symbol_is_a_module(self):
        for style in ("haussmann", "ancien", "xixe", "moderne", "entre-deux-guerres", "inconnu"):
            for shops in (True, False):
                g = pcg_lyon.grammar_for(row(style=style, ground_floor_commercial=shops))
                for text in g["rows"].values():
                    for sym in re.findall(r"[A-Z]+", text):
                        self.assertIn(sym, g["modules"], (style, text))
                self.assertTrue(any(m["scalable"] for m in g["modules"].values()))

    def test_mansard_only_on_haussmann(self):
        self.assertEqual(pcg_lyon.grammar_for(row())["roof"], "mansard")
        self.assertEqual(pcg_lyon.grammar_for(row(style="ancien"))["roof"], "genoise")

    def test_one_variant_per_building(self):
        mesh = lambda i: pcg_lyon.grammar_for(row(id=i, style="ancien"))["modules"]["W"]["mesh"]
        self.assertEqual(mesh("A1"), mesh("A1"))
        self.assertGreater(len({mesh("B%d" % k) for k in range(40)}), 1)


class Footprints(unittest.TestCase):
    def test_ring_is_anticlockwise_and_open(self):
        sq = {"type": "Polygon", "coordinates": [[(0, 0), (0, 10), (10, 10), (10, 0), (0, 0)]]}
        pts, area = pcg_footprints.outer_ring(sq)
        self.assertEqual(len(pts), 4)
        self.assertAlmostEqual(area, 100.0)
        s = sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]))
        self.assertGreater(s, 0)

    def test_record_in_unreal_cm(self):
        f = {"type": "Feature", "geometry": {"type": "Polygon", "coordinates": [[(0, 0), (10, 0), (10, 10), (0, 10), (0, 0)]]},
             "properties": dict(row(), source="maquette-2018", ground_z_m=1.0, height_ridge_m=24.0)}
        r = pcg_footprints.building_record(f)
        self.assertEqual(r["max_height_cm"], 2050)
        self.assertEqual(r["ground_z_cm"], 100.0)
        self.assertIn([1000.0, -1000.0, 100.0], r["points_cm"])


if __name__ == "__main__":
    unittest.main()
