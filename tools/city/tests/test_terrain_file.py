import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import meshkit  # noqa: E402
import pits  # noqa: E402
import terrain_file as tf  # noqa: E402

STEP_M = 4.0


def height(x, y):
    """A street sloping gently east, and a car park ramp 3 m deep, closed all round."""
    if 40.0 <= x <= 56.0 and -24.0 <= y <= -16.0:
        return -3.0
    return 0.01 * x


def tile(x0, y0, size_m=64.0):
    """One terrain tile as terrain.tiles_mesh builds it: quads on a 4 m grid, local metres, y north."""
    m = meshkit.Mesh()
    n = int(size_m / STEP_M)
    for i in range(n):
        for j in range(n):
            xa, ya = x0 + j * STEP_M, y0 + i * STEP_M
            p = [(x, y, height(x, y)) for x, y in ((xa, ya), (xa + STEP_M, ya), (xa + STEP_M, ya + STEP_M),
                                                    (xa, ya + STEP_M))]
            m.quad(*p, (0.4, 0.4, 0.4))
    return m, (x0, y0, 0.0)


def write(path):
    items = [("SM_Terrain_%d" % k, *tile(x0, -64.0)) for k, x0 in enumerate((0.0, 64.0))]
    meshkit.write_glb(path, items)


class TerrainFileTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.path = os.path.join(self.dir.name, "terrain.glb")
        write(self.path)
        self.tiles = tf.read_glb(self.path)

    def tearDown(self):
        self.dir.cleanup()

    def test_tiles_come_back_in_unreal_cm_with_south_as_plus_y(self):
        self.assertEqual([n for n, _ in self.tiles], ["SM_Terrain_0", "SM_Terrain_1"])
        x0, y0, x1, y1, z0, z1 = tf.box(self.tiles[1][1])
        for got, want in zip((x0, y0, x1, y1, z0, z1), (6400.0, 0.0, 12800.0, 6400.0, 64.0, 128.0)):
            self.assertAlmostEqual(got, want, delta=0.1)       # north 0 to -64 m is south 0 to 64 m
        self.assertAlmostEqual(tf.box(self.tiles[0][1])[4], -300.0, delta=0.1)

    def test_the_grid_has_one_height_a_corner_and_the_step_of_the_tiles(self):
        z, x0, y0, step = tf.grid(self.tiles)
        self.assertEqual((x0, y0, step), (0.0, 0.0, 400.0))
        self.assertEqual((len(z), len(z[0])), (17, 33))
        self.assertTrue(all(h is not None for row in z for h in row))
        self.assertAlmostEqual(z[5][12], -300.0, delta=0.1)       # 48 m east, 20 m south: in the ramp
        self.assertAlmostEqual(z[5][20], 80.0, delta=0.1)

    def test_the_ramp_is_found_and_its_lids_reach_the_dry_corners(self):
        z, x0, y0, step = tf.grid(self.tiles)
        found = pits.pits(z, step)
        self.assertEqual(len(found), 1)
        p = found[0]
        self.assertEqual(sorted(p["cells"]), [(r, c) for r in (4, 5, 6) for c in range(10, 15)])
        self.assertAlmostEqual(p["spill_cm"], 36.0, delta=0.1)     # the lowest corner of its rim, west side
        boxes = pits.lids(p["cells"], x0, y0, step)
        self.assertEqual(len(boxes), 3)
        self.assertEqual(boxes[0], (3600.0, 1200.0, 6000.0, 2000.0))
        self.assertEqual((min(b[1] for b in boxes), max(b[3] for b in boxes)), (1200.0, 2800.0))

    def test_corners_off_the_lattice_do_not_halve_the_step(self):
        # the tiles of the map at gate 83: 256 corners half a step off made a 2 m grid three
        # quarters empty, where every cell drains and no pit is found
        stray = [(200.0 + 400.0 * i, 200.0, 80.0) for i in range(8)] + [(13000.0, 600.0, 80.0)]
        stats = {}
        z, x0, y0, step = tf.grid(self.tiles + [("stray", stray)], stats=stats)
        self.assertEqual((x0, y0, step), (0.0, 0.0, 400.0))
        self.assertEqual((len(z), len(z[0])), (17, 33))
        self.assertEqual(stats["off_grid"], 9)
        self.assertTrue(all(h is not None for row in z for h in row))
        self.assertEqual(len(pits.pits(z, step)), 1)

    def test_a_file_is_kept_only_if_its_tiles_are_those_of_the_map(self):
        mine = [tf.box(pts) for _, pts in self.tiles]
        self.assertTrue(tf.same_tiles(mine, [tuple(v + 2.0 for v in b) for b in reversed(mine)]))
        self.assertFalse(tf.same_tiles(mine, mine[:1]))
        self.assertFalse(tf.same_tiles(mine, [mine[0], mine[1][:5] + (mine[1][5] + 40.0,)]))
        self.assertFalse(tf.same_tiles([], []))

    def test_no_corner_gives_an_empty_grid(self):
        self.assertEqual(tf.grid([("empty", [])]), ([], 0.0, 0.0, 0.0))


if __name__ == "__main__":
    unittest.main()
