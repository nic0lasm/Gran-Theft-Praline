"""Lyon stone tone of the generated facades (plain Python, no dependency).

The generator gives each building a colour of its typology (procgen.PALETTE): straw yellows, salmon
pinks and brick oranges at full saturation, some lighter than any wall is. Side by side on a quay
they read as a row of painted toys (gate 69 video, 0 to 15 s and 40 s), and the photographs of the
slice show something else: cream limestone on the rue de la Republique and the quays, pale ochre
and old rose renders on the side streets (art bible 3.1, "a capture that contradicts a field
photograph is wrong"). The colour is toned in the materials, where the generated tables and the
map stay as they are:

  hue    pulled towards the cream of the limestone, KEEP of the generator's own hue left, so the
         buildings still differ, as renders of one town do
  light  the luminance above KNEE is compressed by SLOPE: no wall lighter than light limestone
         (art bible 4.2: albedo 150 to 210 in sRGB 8 bits), and the brightest stay the brightest

tone() is the reference; apply_city_materials.py builds the same sums as material nodes
(lyon_tone) from the constants below, and tools/city/tests/test_facade_tone.py holds both to the
art bible's ranges. GTP_FACADE_TONE=0 on apply_city_materials leaves the generator's colours.
"""

LUMA = (0.2126, 0.7152, 0.0722)      # linear Rec. 709
STONE = (1.10, 1.00, 0.80)           # hue of the cream limestone, before normalisation
KEEP = 0.3                           # share of the generator's own hue (0.5 until gate 77: the quay's yellows still read as paint)
KNEE = 0.38                          # linear luminance left untouched below this
SLOPE = 0.5                          # what is kept of the luminance above the knee


def luminance(rgb):
    return sum(c * k for c, k in zip(rgb, LUMA))


def stone_hue():
    """STONE scaled to a luminance of 1: multiplying it by a luminance keeps that luminance."""
    n = luminance(STONE)
    return tuple(c / n for c in STONE)


def tone(rgb):
    """Toned linear RGB of a linear RGB wall colour."""
    lum = luminance(rgb)
    if lum <= 0.0:
        return tuple(rgb)
    hue = stone_hue()
    mixed = tuple(lum * h + (c - lum * h) * KEEP for c, h in zip(rgb, hue))
    scale = (min(lum, KNEE) + max(lum - KNEE, 0.0) * SLOPE) / lum
    return tuple(c * scale for c in mixed)


# Stone or render, read from the generator's colour: the dressed limestone of the Haussmann
# blocks and monuments is a pale cream, the renders of the older streets are coloured. The
# shells carry nothing else than that colour, so the scanned wall detail is chosen by its
# saturation: stone joints under the low bound, trowelled render above the high one, a blend
# between them. The bounds are wide because the vertex colour may reach the material as the
# palette's bytes or as their linear value: no stone turns fully to render in either reading.
RENDER_SATURATION = (0.24, 0.46)


def saturation(rgb):
    top = max(rgb)
    return (top - min(rgb)) / top if top > 0.0 else 0.0


def render_share(rgb):
    """0 for dressed stone, 1 for a coloured render."""
    lo, hi = RENDER_SATURATION
    return min(1.0, max(0.0, (saturation(rgb) - lo) / (hi - lo)))


# Walls painted in a roof colour. The LiDAR-fitted roofs of the backdrop volumes have vertical
# faces (the sides of a roof block, down to a low eave), painted like the roof: whole fronts of
# the quay stood brick red (tiles) or blue-grey (zinc) in gate 76's video. The backdrop material
# knows a roof colour by its value (meshkit.ROOF_COLOURS, as bytes or decoded): on a vertical
# face it becomes stone, on a slope it stays a roof and takes no windows.
ROOF_ON_WALLS = ((0.62, 0.33, 0.24), (0.43, 0.46, 0.49))    # tile, zinc
ROOF_RADIUS = 0.05             # distance in RGB under which a colour is that roof colour
ROOF_AS_WALL = (0.72, 0.66, 0.55)                            # the stone it becomes, before the tone


def roof_colour_readings():
    """Each roof colour as the material may receive it: the bytes as they are, or decoded."""
    out = []
    for colour in ROOF_ON_WALLS:
        out.append(tuple(round(c * 255.0) / 255.0 for c in colour))
        out.append(tuple(srgb_to_linear(round(c * 255.0) / 255.0) for c in colour))
    return out


# What a roof is in the game. The generator's roof colours reach the material as they are
# written, and read as linear they are pastels: salmon for the tiles, pale grey for the zinc.
# With the sun on them the roofs came out as light as the walls, and the far bank of the Rhone
# read as grey blocks without roofs (gate 88, long take, 13 s). Albedo, linear, same order as
# ROOF_ON_WALLS: Roman tiles weathered brown-red, zinc gone dull blue-grey.
ROOF_TRUE = ((0.30, 0.14, 0.09), (0.20, 0.215, 0.235))
ROOF_TRUE_ROUGHNESS = (0.85, 0.5)


def roof_kind_readings():
    """roof_colour_readings() by kind: one list of readings for the tiles, one for the zinc."""
    readings = roof_colour_readings()
    return [readings[2 * k:2 * k + 2] for k in range(len(ROOF_ON_WALLS))]


def roof_true(rgb):
    """The colour a sloped face takes: ROOF_TRUE of its kind when rgb is a roof colour, by the
    share of roof_share(); rgb itself otherwise."""
    out = tuple(rgb)
    for readings, true in zip(roof_kind_readings(), ROOF_TRUE):
        share = 0.0
        for colour in readings:
            d2 = sum((a - b) ** 2 for a, b in zip(rgb, colour))
            share = max(share, 1.0 - d2 / ROOF_RADIUS ** 2)
        share = min(1.0, max(0.0, share))
        out = tuple(o + (t - o) * share for o, t in zip(out, true))
    return out


def roof_share(rgb):
    """1 when rgb is one of the roof colours, 0 beyond ROOF_RADIUS of all of them."""
    best = 0.0
    for colour in roof_colour_readings():
        d2 = sum((a - b) ** 2 for a, b in zip(rgb, colour))
        best = max(best, 1.0 - d2 / ROOF_RADIUS ** 2)
    return max(0.0, best)


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(c):
    c = min(1.0, max(0.0, c))
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1.0 / 2.4) - 0.055


def hex_linear(colour):
    h = colour.lstrip("#")
    return tuple(srgb_to_linear(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4))
