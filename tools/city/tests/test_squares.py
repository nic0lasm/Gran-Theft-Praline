import json
import os
import random
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import squares  # noqa: E402

HARNESS = os.path.join(os.path.dirname(__file__), "..", "facade_preview")
# A square like place Bellecour: about 300 m by 200 m, turned, with a notch, in Unreal cm.
RING = [(-27000.0, 45000.0), (3000.0, 43000.0), (4500.0, 62000.0), (-9000.0, 63000.0),
        (-9500.0, 60500.0), (-25500.0, 61500.0)]


def dense(ring, step_cm=500.0):
    """The same outline with a point every few metres, slightly off the line: what OSM gives."""
    rng, out = random.Random(3), []
    for i, a in enumerate(ring):
        b = ring[(i + 1) % len(ring)]
        n = max(1, int(((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5 / step_cm))
        for k in range(n):
            wobble = 0.0 if k == 0 else rng.uniform(-40.0, 40.0)
            out.append((a[0] + (b[0] - a[0]) * k / n + wobble, a[1] + (b[1] - a[1]) * k / n + wobble))
    return out


class SquaresTests(unittest.TestCase):
    def test_squares_are_read_in_unreal_cm_and_others_left_out(self):
        meta = {"landmarks": [
            {"key": "place-bellecour", "kind": "square", "surface": "red_gravel",
             "polygon_xy": [[[0, 0], [100, 0], [100, -50], [0, -50], [0, 0]]]},
            {"key": "place-des-jacobins", "kind": "square", "surface": "paving_stones",
             "polygon_xy": [[[0, 0], [10, 0], [10, 10], [0, 0]]]},
            {"key": "opera", "kind": "building", "polygon_xy": [[[0, 0], [10, 0], [10, 10], [0, 0]]]}]}
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "landmarks.json")
            with open(path, "w", encoding="utf-8") as f:
                json.dump(meta, f)
            got = squares.load(path)
            self.assertEqual(squares.load(os.path.join(tmp, "none.json")), [])
        self.assertEqual([s["key"] for s in got], ["place-bellecour"])
        self.assertEqual(got[0]["ring"], [(0.0, 0.0), (10000.0, 0.0), (10000.0, 5000.0), (0.0, 5000.0)])  # y north -> south
        self.assertEqual(got[0]["area_m2"], 5000)

    def test_an_outline_of_many_points_keeps_its_shape_in_a_few_corners(self):
        many = dense(RING)
        self.assertGreater(len(many), 100)
        few = squares.simplify(many)
        self.assertLessEqual(len(few), squares.MAX_CORNERS)
        self.assertAlmostEqual(squares.area(few) / squares.area(RING), 1.0, delta=0.01)
        for corner in RING:                        # every real corner is still a corner, within the tolerance
            self.assertLess(abs(squares.distance([few], corner)), squares.TOLERANCE_CM + 60.0)

    def test_a_ring_with_too_many_real_corners_is_still_cut_down(self):
        import math
        circle = [(10000.0 * math.cos(i * math.pi / 60.0), 10000.0 * math.sin(i * math.pi / 60.0)) for i in range(120)]
        few = squares.simplify(circle)
        self.assertLessEqual(len(few), squares.MAX_CORNERS)
        self.assertGreater(squares.area(few), 0.85 * squares.area(circle))

    def test_distance_is_negative_inside_and_measured_to_the_edge(self):
        box = [(0.0, 0.0), (1000.0, 0.0), (1000.0, 500.0), (0.0, 500.0)]
        self.assertAlmostEqual(squares.distance([box], (500.0, 250.0)), -250.0)
        self.assertAlmostEqual(squares.distance([box], (500.0, -30.0)), 30.0)
        self.assertAlmostEqual(squares.distance([box], (1300.0, 900.0)), 500.0)
        self.assertLess(squares.distance([RING], (-9200.0, 61500.0)), 0.0)      # inside, beside the notch
        self.assertGreater(squares.distance([RING], (-15000.0, 62500.0)), 0.0)  # in the notch: outside

    def test_the_level_of_a_square_is_read_on_what_stands_round_it(self):
        box = [(0.0, 0.0), (10000.0, 0.0), (10000.0, 5000.0), (0.0, 5000.0)]
        points = [(5000.0, -800.0, 212.0), (9000.0, -900.0, 208.0), (-2000.0, 2000.0, 215.0),   # streets round it
                  (5000.0, 2500.0, 210.0),                                                    # across it
                  (5000.0, -9000.0, 480.0), (30000.0, 0.0, -350.0),                           # other streets
                  (5000.0, -500.0)]                                                           # no height written
        self.assertEqual(squares.heights_near(box, points, 3000.0), [208.0, 210.0, 212.0, 215.0])
        self.assertEqual(squares.heights_near(box, points, 100.0), [210.0])

    @unittest.skipUnless(shutil.which(os.environ.get("CXX", "g++")), "no C++ compiler")
    def test_the_shader_text_compiles_and_agrees_with_the_reference(self):
        rings = [squares.simplify(dense(RING)), [(30000.0, 10000.0), (34000.0, 10000.0), (32000.0, 13000.0)]]
        rng = random.Random(11)
        x0, y0, x1, y1 = squares.bounds(rings, 3000.0)
        pts = [(rng.uniform(x0, x1), rng.uniform(y0, y1)) for _ in range(4000)]
        pts += [(-500000.0, 0.0), (0.0, 900000.0)]                 # far from everything
        with tempfile.TemporaryDirectory() as tmp:
            with open(os.path.join(tmp, "square_body.inc"), "w", encoding="utf-8") as f:
                f.write(squares.hlsl(rings))
            exe = os.path.join(tmp, "square_distance")
            subprocess.run([os.environ.get("CXX", "g++"), "-O2", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-I", tmp, "-I", HARNESS, os.path.join(HARNESS, "square_distance.cpp"), "-o", exe],
                           check=True)
            got = subprocess.run([exe], input="".join("%f %f\n" % p for p in pts), check=True,
                                 capture_output=True, text=True).stdout.split()
        self.assertEqual(len(got), len(pts))
        inside = 0
        for p, g in zip(pts, got):
            want = squares.distance(rings, p)
            if want > 900.0:                                       # past the pad the shader only says "far"
                self.assertGreater(float(g), 900.0)
            else:
                self.assertAlmostEqual(float(g), want, delta=2.0 + 1.0e-4 * abs(want))
                inside += want < 0.0
        self.assertGreater(inside, 500)

    @unittest.skipUnless(shutil.which(os.environ.get("CXX", "g++")), "no C++ compiler")
    def test_the_grain_of_the_sand_compiles_and_is_no_regular_pattern(self):
        step, n = 10.0, 400                                        # a line of 40 m, a point every 10 cm
        lines = [[(-9000.0 + i * step, 61000.0 + 137.0 * row) for i in range(n)] for row in range(6)]
        with tempfile.TemporaryDirectory() as tmp:
            with open(os.path.join(tmp, "square_body.inc"), "w", encoding="utf-8") as f:
                f.write(squares.grain_hlsl())
            exe = os.path.join(tmp, "grain")
            subprocess.run([os.environ.get("CXX", "g++"), "-O2", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-I", tmp, "-I", HARNESS, os.path.join(HARNESS, "square_distance.cpp"), "-o", exe],
                           check=True)
            got = subprocess.run([exe], input="".join("%f %f\n" % p for line in lines for p in line), check=True,
                                 capture_output=True, text=True).stdout.split()
        values = [float(v) for v in got]
        self.assertEqual(len(values), 6 * n)
        half = 0.5 * sum(w for _, w in squares.GRAIN_OCTAVES)
        self.assertTrue(all(1.0 - half - 1e-3 <= v <= 1.0 + half + 1e-3 for v in values))
        mean = sum(values) / len(values)
        self.assertAlmostEqual(mean, 1.0, delta=0.03)
        self.assertGreater(max(values) - min(values), 0.12)        # it shows
        # no jump from a point to the next (continuous), and no period: a line never comes back on itself
        row = values[:n]
        self.assertLess(max(abs(a - b) for a, b in zip(row, row[1:])), 0.05)
        dev = [v - sum(row) / n for v in row]
        power = sum(d * d for d in dev)
        for lag in range(20, 200):
            auto = sum(dev[i] * dev[i + lag] for i in range(n - lag)) / power
            self.assertLess(auto, 0.75, lag)


if __name__ == "__main__":
    unittest.main()
