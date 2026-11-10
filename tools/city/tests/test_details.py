import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import details  # noqa: E402


class DetailsTests(unittest.TestCase):
    def test_hash_is_stable_and_in_range(self):
        a = details.h01("BATIMENT0001", 3, "Rue de la République")
        self.assertEqual(a, details.h01("BATIMENT0001", 3, "Rue de la République"))
        self.assertNotEqual(a, details.h01("BATIMENT0001", 4, "Rue de la République"))
        self.assertTrue(0.0 <= a < 1.0)

    def test_size_classes_follow_the_budget(self):
        self.assertEqual(details.size_class({"size_m": 0.3}), "small")
        self.assertEqual(details.size_class({"size_m": 1.8}), "medium")
        self.assertEqual(details.size_class({"size_m": 6.0}), "large")

    def test_signs_map_to_catalogue_ids(self):
        for code, cid in details.SIGNS.items():
            self.assertRegex(cid, r"^W(1|8)-\d\d$", code)

    def test_manifests_cover_every_placed_id(self):
        root = os.path.join(os.path.dirname(__file__), "..", "..", "..", "assets", "manifests")
        items = details.load_manifests(root)
        for cid in list(details.PRIORITY) + list(details.SIGNS.values()):
            self.assertIn(cid, items, cid)

    def test_group_switches(self):
        rows = [{"catalog_id": "W14-42" if i % 2 else "W14-03", "family": "Wall", "x_m": str(i), "y_m": "0",
                 "z_m": "0", "cull_m": "40"} for i in range(400)]
        kept, sw = details.apply_switches([dict(r) for r in rows], {})
        self.assertEqual(len(kept), 400)
        self.assertEqual(sw["removed"], 0)
        kept, _ = details.apply_switches([dict(r) for r in rows], {"GTP_DETAILS_DROP": "W14-03"})
        self.assertTrue(all(r["catalog_id"] == "W14-42" for r in kept))
        kept, _ = details.apply_switches([dict(r) for r in rows], {"GTP_DETAILS_ONLY": "W14-03"})
        self.assertEqual(len(kept), 200)
        thin = {"GTP_DETAILS_THIN": "W14-42:0.5"}
        a, _ = details.apply_switches([dict(r) for r in rows], thin)
        b, _ = details.apply_switches([dict(r) for r in rows], thin)
        self.assertEqual(a, b)                                     # same instances every run
        self.assertTrue(250 <= len(a) <= 350)
        kept, _ = details.apply_switches([dict(r) for r in rows], {"GTP_DETAILS_CULL": "W14-42:25,*:0.5"})
        self.assertEqual({r["cull_m"] for r in kept if r["catalog_id"] == "W14-42"}, {25})
        self.assertEqual({r["cull_m"] for r in kept if r["catalog_id"] == "W14-03"}, {20})


if __name__ == "__main__":
    unittest.main()
