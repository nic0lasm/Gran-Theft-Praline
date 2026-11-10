import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import traffic  # noqa: E402


def road(cid, coords, sens="Double sens", voies=2, largeur=6.0, vitesse=30, nature="Route à 1 chaussée"):
    return {"properties": {"cleabs": cid, "nature": nature, "nombre_de_voies": voies,
                           "largeur_de_chaussee": largeur, "sens_de_circulation": sens,
                           "position_par_rapport_au_sol": "0", "vitesse_moyenne_vl": vitesse},
            "geometry": {"type": "MultiLineString", "coordinates": [coords]}}


class TrafficTests(unittest.TestCase):
    def test_two_way_drives_on_the_right(self):
        lanes = traffic.build_lanes([road("A", [[0, 0], [0, 100]])])
        self.assertEqual(len(lanes), 2)
        north = next(lane for lane in lanes if lane["points"][-1][1] > lane["points"][0][1])
        south = next(lane for lane in lanes if lane is not north)
        # Northbound keeps to the east (right), southbound to the west.
        self.assertAlmostEqual(north["points"][0][0], 1.5, places=3)
        self.assertAlmostEqual(south["points"][0][0], -1.5, places=3)

    def test_one_way_and_closed_roads(self):
        lanes = traffic.build_lanes([
            road("A", [[0, 0], [100, 0]], sens="Sens inverse", voies=1, largeur=3.5),
            road("P", [[0, 0], [0, 50]], vitesse=0),
            road("S", [[0, 0], [0, -50]], nature="Escalier"),
        ])
        self.assertEqual(len(lanes), 1)
        self.assertGreater(lanes[0]["points"][0][0], lanes[0]["points"][-1][0])  # drawn east, driven west
        self.assertAlmostEqual(lanes[0]["points"][0][1], 0.0, places=3)

    def test_junction_links_without_u_turn(self):
        lanes = traffic.build_lanes([
            road("A", [[0, 0], [0, 100]]),
            road("B", [[0, 100], [100, 100]]),
            road("C", [[0, 100], [0, 200]], sens="Sens direct", voies=1, largeur=3.5),
        ])
        by_road = {}
        for lane in lanes:
            by_road.setdefault((lane["road"], lane["dir"]), lane)
        a_north = by_road[("A", 1)]
        targets = {(lanes[i]["road"], lanes[i]["dir"]) for i in a_north["next"]}
        self.assertEqual(targets, {("B", 1), ("C", 1)})
        # Coming back down B, the only ways on are A south (C is one-way north) or C.
        b_west = by_road[("B", -1)]
        targets = {(lanes[i]["road"], lanes[i]["dir"]) for i in b_west["next"]}
        self.assertEqual(targets, {("A", -1), ("C", 1)})

    def test_dead_end_has_no_u_turn(self):
        lanes = traffic.build_lanes([road("A", [[0, 0], [0, 100]])])
        for lane in lanes:
            self.assertEqual(lane["next"], [])

    def test_clip_to_extent(self):
        lanes = traffic.build_lanes([road("A", [[0, -100], [0, 100]], sens="Sens direct", voies=1)],
                                    extent_m=(-50, -10, 50, 10))
        self.assertEqual(len(lanes), 1)
        ys = [p[1] for p in lanes[0]["points"]]
        self.assertAlmostEqual(min(ys), -10, places=3)
        self.assertAlmostEqual(max(ys), 10, places=3)


if __name__ == "__main__":
    unittest.main()
