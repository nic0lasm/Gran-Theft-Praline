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

    def test_stone_and_render_are_told_apart_by_the_colour(self):
        # the shells carry only the building colour: dressed stone keeps its joints, a strongly
        # coloured render gets the trowelled detail, in both readings of the vertex colour
        def readings(colour):
            h = colour.lstrip("#")
            return (tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)), ft.hex_linear(colour))

        for typology in ("haussmann", "monument", "moderne"):
            for colour in procgen.PALETTE[typology]:
                raw, linear = readings(colour)
                self.assertEqual(ft.render_share(raw), 0.0, colour)
                self.assertLess(ft.render_share(linear), 0.85, colour)
        strong = 0
        for colour in procgen.PALETTE["ancien"]:
            raw, linear = readings(colour)
            self.assertEqual(ft.render_share(linear), 1.0, colour)
            self.assertGreater(ft.render_share(raw), 0.0, colour)
            strong += ft.render_share(raw) >= 0.5
        self.assertGreaterEqual(strong, 5)

    def test_roof_colours_on_walls_are_known_and_no_wall_colour_is_taken_for_one(self):
        import meshkit
        self.assertEqual(set(ft.ROOF_ON_WALLS), {meshkit.ROOF_COLOURS["tile"], meshkit.ROOF_COLOURS["zinc"]})
        for colour in ft.roof_colour_readings():
            self.assertGreater(ft.roof_share(colour), 0.99)
        for colours in procgen.PALETTE.values():
            for colour in colours:
                h = colour.lstrip("#")
                raw = tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))
                self.assertEqual(ft.roof_share(raw), 0.0, colour)
                self.assertEqual(ft.roof_share(ft.hex_linear(colour)), 0.0, colour)
        # the stone a roof-coloured wall becomes lands in the bible's limestone range
        self.assertTrue(LIMESTONE[0] <= grey(ft.tone(ft.ROOF_AS_WALL)) <= LIMESTONE[1])

    def test_roofs_take_a_tone_darker_than_any_wall(self):
        import meshkit
        luma = lambda c: 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]
        tile, zinc = ft.ROOF_TRUE
        for kind, true in (("tile", tile), ("zinc", zinc)):
            raw = meshkit.ROOF_COLOURS[kind]
            byte = tuple(round(c * 255.0) / 255.0 for c in raw)
            decoded = tuple(ft.srgb_to_linear(c) for c in byte)
            for reading in (byte, decoded):
                got = ft.roof_true(reading)
                for a, b in zip(got, true):
                    self.assertAlmostEqual(a, b, places=3)
        self.assertGreater(tile[0], 2.0 * tile[2])                 # the tiles are red
        self.assertLess(abs(zinc[0] - zinc[2]), 0.05)              # the zinc is grey
        # every toned wall of the palette is lighter than either roof, the darkest render by 15 %
        for colours in procgen.PALETTE.values():
            for colour in colours:
                wall = ft.tone(ft.hex_linear(colour))
                self.assertEqual(ft.roof_true(ft.hex_linear(colour)), ft.hex_linear(colour))
                self.assertGreater(luma(wall), 1.15 * max(luma(tile), luma(zinc)), colour)


if __name__ == "__main__":
    unittest.main()
