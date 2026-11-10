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


if __name__ == "__main__":
    unittest.main()
