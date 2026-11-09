import os
import sys
import unittest

HERE = os.path.dirname(__file__)
sys.path.insert(0, os.path.join(HERE, ".."))
import brands  # noqa: E402
import signs  # noqa: E402


class SignTests(unittest.TestCase):
    def test_contrast_ratio(self):
        self.assertAlmostEqual(signs.contrast((255, 255, 255), (0, 0, 0)), 21.0, places=1)
        self.assertAlmostEqual(signs.contrast((120, 120, 120), (120, 120, 120)), 1.0)

    def test_every_trade_colour_has_readable_lettering(self):
        for trade, colour in list(signs.TRADE_COLOUR.items()) + [("", signs.DEFAULT_COLOUR)]:
            bg = signs.hex_rgb(colour)
            best = max(signs.contrast(bg, (250, 248, 242)), signs.contrast(bg, (20, 20, 22)))
            self.assertGreaterEqual(best, 4.5, trade)  # WCAG AA for text

    def test_texture_matches_the_fascia_aspect(self):
        w, h = signs.SIZE
        self.assertEqual(w, 2048)
        self.assertAlmostEqual(w / h, signs.FASCIA_M[0] / signs.FASCIA_M[1], delta=0.1)

    def test_logo_colour_ignores_greys(self):
        from PIL import Image
        grey = Image.new("RGBA", (64, 64), (128, 128, 128, 255))
        self.assertIsNone(signs.logo_colour(grey))
        red = Image.new("RGBA", (64, 64), (200, 20, 30, 255))
        c = signs.logo_colour(red)
        self.assertTrue(c[0] > 150 and c[1] < 60)

    def test_commons_thumbnail_url(self):
        thumb, orig = brands.commons_urls("Example logo.svg")
        self.assertTrue(thumb.startswith(brands.UPLOAD + "thumb/"))
        self.assertTrue(thumb.endswith("/960px-Example_logo.svg.png"))
        self.assertTrue(orig.endswith("/Example_logo.svg"))


if __name__ == "__main__":
    unittest.main()
