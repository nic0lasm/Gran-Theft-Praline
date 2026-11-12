import csv
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import credits_from_manifest as cm  # noqa: E402


class CreditsTest(unittest.TestCase):
    def rows(self):
        return [
            {"fiche": "Cash register", "auteur": "Wnoz", "url": "https://www.fab.com/listings/x", "licence": "CC-BY", "etat": "exporte"},
            {"fiche": "Hamburger", "auteur": "", "url": "", "licence": "CC0", "etat": "exporte"},
            {"fiche": "Burger", "auteur": "", "url": "", "licence": "FSL", "etat": "exporte"},
            {"fiche": "No author", "auteur": "", "url": "", "licence": "CC-BY", "etat": "exporte"},
            {"fiche": "Kitchen pack", "auteur": "", "url": "", "licence": "", "etat": "ajoute_au_projet"},
        ]

    def test_build(self):
        credits, sources, blocked, count = cm.build(self.rows(), "2026-11-12")
        self.assertEqual(count, 1)
        self.assertIn('"Cash register" by Wnoz (https://www.fab.com/listings/x), CC BY 4.0', credits)
        self.assertIn('"Hamburger", CC0', credits)
        self.assertIn("Fab Standard License", credits)
        self.assertIn("No author: CC BY without an author", blocked)
        self.assertIn("Kitchen pack: licence not recorded", blocked)
        self.assertEqual(sources.count("\n"), 3)

    def test_only_downloaded(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "m.csv")
            with open(path, "w", encoding="utf-8", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=["fiche", "auteur", "url", "licence", "etat"])
                writer.writeheader()
                writer.writerow({"fiche": "A", "auteur": "x", "url": "", "licence": "CC-BY", "etat": "exporte"})
                writer.writerow({"fiche": "B", "auteur": "y", "url": "", "licence": "CC-BY", "etat": "introuvable"})
            self.assertEqual([r["fiche"] for r in cm.load(path)], ["A"])


if __name__ == "__main__":
    unittest.main()
