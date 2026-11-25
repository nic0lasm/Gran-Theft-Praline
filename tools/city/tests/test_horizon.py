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

    def test_every_far_block_has_a_roof_and_chimneys_above_its_wall(self):
        roofs, chimneys = self.plan["roofs"]["roofs"], self.plan["roofs"]["chimneys"]
        blocks = [b for b in self.plan["blocks"] if b["kind"] == "block"]
        self.assertTrue(len(blocks) <= len(roofs) <= 2 * len(blocks))
        self.assertGreaterEqual(len(chimneys), 2 * len(blocks))
        self.assertEqual({r["kind"] for r in roofs}, {"tile", "zinc"})
        tops = {(b["x"], b["y"]): b for b in blocks}
        for r in roofs:
            near = min(blocks, key=lambda b: math.hypot(b["x"] - r["x"], b["y"] - r["y"]))
            self.assertLess(math.hypot(near["x"] - r["x"], near["y"] - r["y"]), 0.5 * near["width"], r)
            self.assertLess(r["bottom"], near["top"])                    # no gap under the roof
            self.assertTrue(2.0 <= r["top"] - near["top"] <= 7.5, r)
            self.assertLessEqual(r["width"], near["width"] + 1.01)
        for c in chimneys:
            near = min(blocks, key=lambda b: math.hypot(b["x"] - c["x"], b["y"] - c["y"]))
            self.assertLess(math.hypot(near["x"] - c["x"], near["y"] - c["y"]),
                            0.5 * math.hypot(near["width"], near["depth"]), c)
            self.assertTrue(near["top"] + 3.0 < c["top"] < near["top"] + 11.0, c)
            self.assertLess(c["bottom"], c["top"] - 1.0)
        self.assertEqual(len(tops), len(blocks))
        self.assertEqual(self.plan["blocks"], [dict(b) for b in horizon.layout()["blocks"]])

    def test_a_chimney_stands_on_its_block_whatever_the_yaw(self):
        block = {"x": 100.0, "y": 50.0, "yaw": 90.0, "width": 80.0, "depth": 30.0, "ground": 0.0, "top": 20.0,
                 "colour": 0, "kind": "block"}
        out = horizon.roofs([block])
        for c in out["chimneys"]:               # turned a quarter: the width runs along y
            self.assertLess(abs(c["x"] - 100.0), 15.0, c)
            self.assertLess(abs(c["y"] - 50.0), 40.0, c)
        self.assertEqual(len(out["roofs"]), 2)  # wide: two houses
        self.assertEqual(horizon.roofs([dict(block, kind="tower")]), {"roofs": [], "chimneys": []})

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
        on_hills = houses = 0
        for b in self.plan["blocks"]:
            self.assertLess(b["colour"], horizon.COLOURS)
            ground, hill = horizon.hill_at(b["x"], b["y"])
            if b["kind"] == "block" and hill is not None and hill[6]:
                # Fourviere: houses among the trees, none on the crest (gate 99: slabs against the sky)
                self.assertTrue(13.0 <= b["top"] - b["ground"] <= 19.0, b)
                self.assertTrue(18.0 <= b["width"] <= 34.0 and b["depth"] <= b["width"], b)
                self.assertLessEqual(b["ground"], horizon.WOODED_CREST * hill[5] + 0.1, b)
                houses += 1
            elif b["kind"] == "block":
                self.assertTrue(18.0 <= b["top"] - b["ground"] <= 34.0, b)
                self.assertTrue(40.0 <= b["width"] <= 170.0, b)
            self.assertAlmostEqual(b["ground"], ground, delta=1.0)    # the position is rounded to 10 cm
            on_hills += ground > 0.0
        self.assertGreater(on_hills, 10)
        self.assertGreater(houses, 10)

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

    def test_the_horseman_stands_on_place_bellecour(self):
        # the outline of the square as OpenStreetMap draws it, through oriented_box
        box = (-101.37, 607.9, 0.939, 0.343, 132.57, 95.13)
        parts = horizon.square_statues(box)
        self.assertEqual(len(parts), len(horizon.STATUE_PARTS))
        self.assertEqual(horizon.square_statues((500.0, 500.0, 1.0, 0.0, 50.0, 50.0)), [])    # not on another square
        _, sx, sy = horizon.SQUARE_STATUES[0]
        stone = [p for p in parts if p["material"] == "stone"]
        bronze = [p for p in parts if p["material"] == "bronze"]
        self.assertEqual(len(stone) + len(bronze), len(parts))
        for p in parts:
            self.assertIn(p["shape"], ("Cube", "Sphere"))
            self.assertLess(math.hypot(p["x"] - sx, p["y"] - sy), 4.0)
            self.assertGreater(p["top"], p["bottom"])
        # a pedestal of five to six metres, the horse and its rider as much again on top of it
        pedestal = max(p["top"] for p in stone)
        self.assertTrue(5.0 <= pedestal <= 6.0)
        self.assertTrue(5.2 <= max(p["top"] for p in bronze) - pedestal <= 6.0)
        self.assertAlmostEqual(min(p["bottom"] for p in parts), 0.0)
        # nothing in the air: every part stands on the square or on a part under it
        for p in parts:
            self.assertTrue(p["bottom"] == 0.0 or any(q is not p and q["bottom"] < p["bottom"] <= q["top"] + 1e-6
                                                      for q in parts), p)
        # the horse stands across the square and looks south, its head ahead of its tail
        yaw = math.radians(parts[0]["yaw"])
        self.assertAlmostEqual(abs(math.cos(yaw) * box[2] + math.sin(yaw) * box[3]), 0.0, places=2)
        self.assertGreater(math.sin(yaw), 0.0)
        head = max(bronze, key=lambda p: (p["x"] - sx) * math.cos(yaw) + (p["y"] - sy) * math.sin(yaw))
        self.assertGreater(head["top"], pedestal + 4.0)
        # 126 m from the flight's line, and off every tree of the square's rows
        self.assertGreater(math.hypot(sx - 34.48, sy - 544.98), 100.0)

    def test_a_square_carries_its_statue_in_its_setting(self):
        ring = [(120.0 + self.U[0] * a + self.V[0] * b, 610.0 + self.U[1] * a + self.V[1] * b)
                for a, b in ((0.0, 0.0), (300.0, 0.0), (300.0, 190.0), (0.0, 190.0))]
        st = horizon.square_setting(ring, self.GROUND)
        self.assertEqual(st["statues"], horizon.square_statues(horizon.oriented_box(ring)))
        for t in st["trees"]:
            for p in st["statues"]:
                self.assertGreater(math.hypot(t["x"] - p["x"], t["y"] - p["y"]), 8.0)

    # A square like place Bellecour on the map of the street: 300 m by 190 m, turned 18.5 degrees,
    # its north side on the map, the rest past the south edge of the ground (y = 617 m).
    GROUND = (-413.0, -557.0, 413.0, 617.0)
    U, V = (-0.948, -0.317), (-0.317, 0.948)
    NE = (120.0, 610.0)
    RING = [NE, (NE[0] + 300 * U[0], NE[1] + 300 * U[1]),
            (NE[0] + 300 * U[0] + 190 * V[0], NE[1] + 300 * U[1] + 190 * V[1]),
            (NE[0] + 190 * V[0], NE[1] + 190 * V[1])]

    def test_the_box_round_a_square_follows_its_long_side(self):
        cx, cy, ux, uy, hu, hv = horizon.oriented_box(self.RING)
        self.assertAlmostEqual(hu, 150.0, delta=0.5)
        self.assertAlmostEqual(hv, 95.0, delta=0.5)
        self.assertAlmostEqual(abs(ux * self.U[0] + uy * self.U[1]), 1.0, delta=0.001)
        self.assertAlmostEqual(cx, sum(p[0] for p in self.RING) / 4.0, delta=0.5)
        self.assertAlmostEqual(cy, sum(p[1] for p in self.RING) / 4.0, delta=0.5)

    def test_a_square_past_the_end_of_the_map_gets_ground_fronts_and_trees(self):
        st = horizon.square_setting(self.RING, self.GROUND)
        self.assertEqual(st, horizon.square_setting(self.RING, self.GROUND))        # one seed, one setting
        # ground: every point of the square past the map's ground lies on an apron
        def on_apron(x, y):
            return any(abs(x - a["x"]) <= 0.5 * a["width"] and abs(y - a["y"]) <= 0.5 * a["depth"] for a in st["aprons"])
        for i in range(0, 301, 10):
            for j in range(0, 191, 10):
                x = self.NE[0] + i * self.U[0] + j * self.V[0]
                y = self.NE[1] + i * self.U[1] + j * self.V[1]
                if y > self.GROUND[3]:
                    self.assertTrue(on_apron(x, y), (x, y))
        for a in st["aprons"]:                         # and laps under the map's edge without covering it
            self.assertLessEqual(self.GROUND[3] - (a["y"] - 0.5 * a["depth"]), horizon.SQUARE_APRON_LAP_M + 0.01)
        # fronts: none on the map's ground, a row along the whole south side, at a street's distance
        self.assertGreaterEqual(len(st["fronts"]), 10)
        south = 0.0
        for f in st["fronts"]:
            self.assertGreater(f["y"] - 0.5 * math.hypot(f["width"], f["depth"]), self.GROUND[3] - 12.0, f)
            self.assertTrue(21.0 <= f["top"] <= 24.5 and f["colour"] in (0, 1) and f["kind"] == "front", f)
            away = (f["x"] - st["box"][0]) * self.V[0] + (f["y"] - st["box"][1]) * self.V[1]
            if away > 100.0:
                south += f["width"]
                self.assertAlmostEqual(away, 95.0 + horizon.SQUARE_STREET_M + 7.0, delta=0.6)
        self.assertGreater(south, 0.85 * 300.0)
        # trees: two rows inside the square, on its south side, never on a front
        self.assertTrue(50 <= len(st["trees"]) <= 70, len(st["trees"]))
        for t in st["trees"]:
            away = (t["x"] - st["box"][0]) * self.V[0] + (t["y"] - st["box"][1]) * self.V[1]
            self.assertTrue(70.0 <= away <= 90.0, t)

    def test_no_front_stands_on_a_real_building_past_the_ground(self):
        # the real volumes of the west side of the square run on 60 m past the ground of the map
        real = (-300.0, 600.0, -200.0, 680.0)
        free = horizon.square_setting(self.RING, self.GROUND)["fronts"]
        kept = horizon.square_setting(self.RING, self.GROUND, built=[real])["fronts"]
        self.assertLess(len(kept), len(free))
        for f in kept:
            reach = 0.5 * math.hypot(f["width"], f["depth"])
            self.assertFalse(real[0] - 4.0 < f["x"] < real[2] + 4.0 and real[1] - 4.0 < f["y"] < real[3] + 4.0, f)
            self.assertTrue(f["x"] - reach > real[2] or f["x"] + reach < real[0] or f["y"] - reach > real[3] or
                            f["y"] + reach < real[1] or abs(f["x"] - 0.5 * (real[0] + real[2])) > 50.0 or
                            abs(f["y"] - 0.5 * (real[1] + real[3])) > 40.0, f)

    def test_a_square_on_the_map_needs_nothing(self):
        ring = [(0.0, 0.0), (80.0, 0.0), (80.0, 60.0), (0.0, 60.0)]
        st = horizon.square_setting(ring, self.GROUND)
        self.assertEqual((st["aprons"], st["fronts"], st["trees"]), ([], [], []))

    def test_nothing_is_left_on_the_driven_line(self):
        text = "# a circuit\nlane_id,start_x_cm,start_y_cm,end_x_cm,end_y_cm,name,line_ue_cm,kmh\n" \
               "1,0,0,3000,0,R A,0 0;300 0;600 0;900 0;1200 0,60\napproach\n2,0,0,0,900,R B,0 300;0 600;0 900,\n"
        line = horizon.circuit_line(text)
        self.assertEqual(len(line), 8)
        self.assertEqual(line[1], (3.0, 0.0))
        items = [{"x": 4.5, "y": 4.0}, {"x": 9.0, "y": 7.5}, {"x": 40.0, "y": 0.0}, {"x": 2.0, "y": 6.0}]
        kept = horizon.keep_clear(items, line, 5.0)
        self.assertEqual(kept, [{"x": 9.0, "y": 7.5}, {"x": 40.0, "y": 0.0}])
        self.assertEqual(horizon.keep_clear(items, [], 5.0), items)

    def test_the_ground_reaches_the_real_horizon(self):
        # eyes 2 m above the street: the horizon is 5 km away; from a roof at 30 m, 20 km
        self.assertGreaterEqual(self.plan["skirt"]["half"], 20000.0)


if __name__ == "__main__":
    unittest.main()
