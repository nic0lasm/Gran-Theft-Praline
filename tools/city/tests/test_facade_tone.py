import colorsys
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import facade_tone as ft  # noqa: E402
import procgen  # noqa: E402

# Art bible 4.2, albedo as a grey level in sRGB 8 bits.
LIMESTONE = (150, 210)
RENDER = (110, 200)


def grey(rgb):
    return 255.0 * ft.linear_to_srgb(ft.luminance(rgb))


def saturation(rgb):
    return colorsys.rgb_to_hsv(*(ft.linear_to_srgb(c) for c in rgb))[1]


class FacadeToneTests(unittest.TestCase):
    def test_stone_hue_keeps_the_luminance(self):
        self.assertAlmostEqual(ft.luminance(ft.stone_hue()), 1.0, places=6)

    def test_every_palette_colour_lands_in_the_art_bible_range(self):
        for typology, colours in procgen.PALETTE.items():
            lo, hi = RENDER if typology == "ancien" else LIMESTONE
            for colour in colours:
                g = grey(ft.tone(ft.hex_linear(colour)))
                self.assertTrue(lo <= g <= hi, (typology, colour, round(g)))

    def test_no_wall_keeps_a_toy_colour(self):
        # the loudest generator colours (brick orange, salmon, straw yellow) end as muted renders
        for colours in procgen.PALETTE.values():
            for colour in colours:
                before, after = ft.hex_linear(colour), ft.tone(ft.hex_linear(colour))
                self.assertLessEqual(saturation(after), 0.42, colour)
                if saturation(before) > 0.30:
                    self.assertLessEqual(saturation(after), 0.75 * saturation(before), colour)
                # the greys of the modern blocks take a little of the stone, no more
                self.assertLessEqual(saturation(after), saturation(before) + 0.12, colour)

    def test_buildings_still_differ(self):
        toned = [ft.tone(ft.hex_linear(c)) for c in procgen.PALETTE["ancien"]]
        greys = sorted(grey(c) for c in toned)
        self.assertGreater(greys[-1] - greys[0], 25.0)      # light and dark renders
        warm = [c[0] / c[2] for c in toned]
        self.assertGreater(max(warm) - min(warm), 0.15)     # pinker and yellower ones

    def test_order_of_lightness_is_kept(self):
        colours = [ft.hex_linear(c) for cs in procgen.PALETTE.values() for c in cs]
        colours.sort(key=ft.luminance)
        toned = [ft.luminance(ft.tone(c)) for c in colours]
        self.assertEqual(toned, sorted(toned))

    def test_dark_colours_are_not_lifted(self):
        glass = (0.16, 0.20, 0.24)
        self.assertAlmostEqual(ft.luminance(ft.tone(glass)), ft.luminance(glass), places=6)

    def test_raw_bytes_read_as_linear_stay_in_range_too(self):
        # whichever way the importer hands the vertex colours to the material (decoded or not),
        # no wall leaves the range of light limestone
        for colours in procgen.PALETTE.values():
            for colour in colours:
                h = colour.lstrip("#")
                raw = tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))
                self.assertLessEqual(grey(ft.tone(raw)), LIMESTONE[1], colour)


if __name__ == "__main__":
    unittest.main()
