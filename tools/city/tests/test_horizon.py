import math
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import horizon  # noqa: E402


class HorizonTests(unittest.TestCase):
    def setUp(self):
        self.plan = horizon.layout()

    def test_the_same_seed_gives_the_same_far_view(self):
        self.assertEqual(self.plan, horizon.layout())
        self.assertNotEqual(self.plan["blocks"], horizon.layout(seed=8)["blocks"])

    def test_nothing_stands_where_the_real_buildings_are(self):
        # backdrop.py fills 600 m around the axis with the BD TOPO volumes
        for b in self.plan["blocks"]:
            reach = 0.5 * math.hypot(b["width"], b["depth"])
            self.assertGreater(horizon.axis_distance(b["x"], b["y"]) - reach, horizon.BACKDROP_M, b)
        for h in self.plan["hills"]:
            for t in range(360):
                x = h["x"] + h["half_x"] * math.cos(math.radians(t))
                y = h["y"] + h["half_y"] * math.sin(math.radians(t))
                self.assertGreater(horizon.axis_distance(x, y), horizon.BACKDROP_M, h["name"])

    def test_the_actor_count_stays_small(self):
        self.assertTrue(150 <= len(self.plan["blocks"]) <= 320, len(self.plan["blocks"]))

    def test_every_side_of_the_map_has_a_skyline(self):
        sides = {"east": 0, "west": 0, "north": 0, "south": 0}
        for b in self.plan["blocks"]:
            if abs(b["x"]) > abs(b["y"]) - horizon.AXIS_HALF_M:
                sides["east" if b["x"] > 0 else "west"] += 1
            else:
                sides["south" if b["y"] > 0 else "north"] += 1
        for side, n in sides.items():
            self.assertGreaterEqual(n, 12, side)

    def test_blocks_are_town_buildings_and_stand_on_the_hills(self):
        on_hills = 0
        for b in self.plan["blocks"]:
            self.assertLess(b["colour"], horizon.COLOURS)
            if b["kind"] == "block":
                self.assertTrue(18.0 <= b["top"] - b["ground"] <= 34.0, b)
                self.assertTrue(40.0 <= b["width"] <= 170.0, b)
            ground, _ = horizon.hill_at(b["x"], b["y"])
            self.assertAlmostEqual(b["ground"], ground, delta=1.0)    # the position is rounded to 10 cm
            on_hills += ground > 0.0
        self.assertGreater(on_hills, 10)

    def test_the_part_dieu_towers_rise_east_above_everything(self):
        towers = [b for b in self.plan["blocks"] if b["kind"] == "tower"]
        self.assertEqual(len(towers), len(horizon.TOWERS))
        self.assertTrue(all(t["x"] > 1300.0 for t in towers))
        self.assertGreater(max(t["top"] for t in towers), 150.0)

    def test_fourviere_stands_on_the_skyline_seen_from_bellecour(self):
        parts = self.plan["monuments"]
        basilica = [m for m in parts if m["name"].startswith("Basilica")]
        tower = [m for m in parts if m["name"].startswith("IronTower")]
        self.assertEqual(len(tower), 1)
        self.assertEqual(sum(1 for m in basilica if abs(m["top"] - m["ground"] - 48.0) < 0.2), 4)    # the four towers
        for m in parts:
            self.assertIn(m["shape"], ("Cube", "Cylinder", "Cone"))
            self.assertIn(m["material"], ("stone", "iron"))
            self.assertGreater(horizon.axis_distance(m["x"], m["y"]), horizon.BACKDROP_M)
            self.assertTrue(105.0 <= m["ground"] <= 130.0, m)        # the terrace is about 115 m over the town
        self.assertGreater(tower[0]["top"], max(m["top"] for m in basilica) + 40.0)
        # from the east side of place Bellecour, over a roof of 25 m standing 250 m away
        eye_x, eye_y, roof = -20.0, 530.0, 25.0 / 250.0
        nave = basilica[0]
        away = math.hypot(nave["x"] - eye_x, nave["y"] - eye_y)
        self.assertGreater(nave["ground"] / away, roof)
        # and no far block hides it: none stands higher between the square and the basilica
        for b in self.plan["blocks"]:
            if math.hypot(b["x"] - nave["x"], b["y"] - nave["y"]) < 150.0:
                self.assertLess(b["top"], nave["top"], b)

    def test_the_ground_reaches_the_real_horizon(self):
        # eyes 2 m above the street: the horizon is 5 km away; from a roof at 30 m, 20 km
        self.assertGreaterEqual(self.plan["skirt"]["half"], 20000.0)


if __name__ == "__main__":
    unittest.main()
