import csv
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import interior_layout as il  # noqa: E402

SPEC = os.path.join(HERE, "..", "fastfood_bellecour.json")


class LayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.spec = il.load_json(SPEC)
        cls.catalog = il.load_json(il.DEFAULT_CATALOG)

    def test_same_seed_same_layout(self):
        first = il.generate(self.spec, self.catalog, 2, seed=5)
        second = il.generate(self.spec, self.catalog, 2, seed=5)
        self.assertEqual(first, second)

    def test_density_only_adds_objects(self):
        counts = [il.report(il.generate(self.spec, self.catalog, d), self.catalog, "m1", self.spec["budgets"])["objects_perceived"]
                  for d in range(4)]
        self.assertEqual(counts, sorted(counts))
        self.assertLess(counts[0], counts[3])

    def test_density_levels_share_one_layout(self):
        # The game switches density by hiding components: objects never move between levels.
        full = il.generate_all(self.spec, self.catalog)
        for density in range(4):
            subset = il.generate(self.spec, self.catalog, density)
            self.assertEqual(subset, [p for p in full if p["min_density"] <= density])
        self.assertEqual({p["min_density"] for p in full}, {0, 1, 2, 3})

    def test_default_density_fits_both_tiers(self):
        for tier in ("m1", "pc"):
            density = self.spec["density"]["default_" + tier]
            result = il.report(il.generate(self.spec, self.catalog, density), self.catalog, tier, self.spec["budgets"])
            self.assertEqual(result["over_budget"], {}, tier)

    def test_massive_density_is_real(self):
        # The point of the reference interior: thousands of objects for a few hundred draw calls.
        result = il.report(il.generate(self.spec, self.catalog, 3), self.catalog, "pc", self.spec["budgets"])
        self.assertGreater(result["objects_perceived"], 8000)
        self.assertLess(result["draw_calls"], 400)

    def test_every_station_item_is_in_the_catalogue(self):
        for station in self.spec["stations"]:
            for cls in station["items"]:
                self.assertIn(cls, self.catalog["classes"], station["name"])

    def test_tills_match_the_cash_model(self):
        tills = sum(s["count"] for s in self.spec["stations"] if s["name"] == "till")
        self.assertEqual(tills, 4)
        placements = il.generate(self.spec, self.catalog, 0)
        self.assertEqual(sum(1 for p in placements if p["class"] == "cash_drawer"), 4)

    def test_objects_stay_inside_their_zone(self):
        zones = il.zone_map(self.spec)
        for placement in il.generate(self.spec, self.catalog, 3):
            x0, y0, x1, y1 = zones[placement["zone"]]["rect"]
            self.assertTrue(x0 - 0.5 <= placement["x"] <= x1 + 0.5 and y0 - 0.5 <= placement["y"] <= y1 + 0.5, placement)

    def test_over_budget_is_reported(self):
        budgets = {"m1": {"rendered_instances": 10}}
        result = il.report(il.generate(self.spec, self.catalog, 0), self.catalog, "m1", budgets)
        self.assertIn("rendered_instances", result["over_budget"])

    def test_csv_in_unreal_units(self):
        placements = il.generate(self.spec, self.catalog, 1)
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "out", "placements.csv")
            il.write_csv(path, placements, self.catalog)
            with open(path, encoding="utf-8") as handle:
                rows = list(csv.DictReader(handle))
        self.assertEqual(len(rows), len(placements))
        self.assertAlmostEqual(float(rows[0]["x_cm"]), placements[0]["x"] * 100, places=0)
        self.assertAlmostEqual(float(rows[0]["y_cm"]), -placements[0]["y"] * 100, places=0)

    def test_bad_density_refused(self):
        with self.assertRaises(ValueError):
            il.generate(self.spec, self.catalog, 4)


if __name__ == "__main__":
    unittest.main()
