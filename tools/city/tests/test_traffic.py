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

    def test_bridges_kept_tunnels_dropped(self):
        bridge = road("B", [[0, 0], [0, 100]], sens="Sens direct", voies=1)
        bridge["properties"]["position_par_rapport_au_sol"] = "1"
        tunnel = road("T", [[10, 0], [10, 100]], sens="Sens direct", voies=1)
        tunnel["properties"]["position_par_rapport_au_sol"] = "-1"
        lanes = traffic.build_lanes([bridge, tunnel])
        self.assertEqual([lane["road"] for lane in lanes], ["B"])

    def test_dead_end_has_no_u_turn(self):
        lanes = traffic.build_lanes([road("A", [[0, 0], [0, 100]])])
        for lane in lanes:
            self.assertEqual(lane["next"], [])

    def test_measured_carriageway_moves_lanes_and_keeps_off_the_kerb(self):
        # BD TOPO axis along y = 0, real carriageway centred 1.5 m north, 3 m half width.
        f = road("R1", [(0, 0), (60, 0)])
        centre = [(x, 1.5) for x in range(0, 61, 2)]
        cw = {"R1": (centre, [3.0] * len(centre))}
        lanes = traffic.build_lanes([f], None, cw)
        east = next(l for l in lanes if l["points"][-1][0] > l["points"][0][0])
        west = next(l for l in lanes if l["points"][-1][0] < l["points"][0][0])
        mid_e = east["points"][len(east["points"]) // 2][1]
        mid_w = west["points"][len(west["points"]) // 2][1]
        # Right-hand traffic around the measured centre, half a 3.2 m lane out (kerbs at y -1.5 and 4.5).
        self.assertAlmostEqual(mid_e, 1.5 - 1.6, delta=0.05)
        self.assertAlmostEqual(mid_w, 1.5 + 1.6, delta=0.05)
        # Junctions still come from the BD TOPO ends.
        self.assertEqual(east["from"], traffic.node_key((0, 0)))

    def test_one_way_measured_lanes_centred_and_clamped(self):
        f = road("R2", [(0, 0), (40, 0)], sens="Sens direct", voies=3)
        centre = [(x, 0.0) for x in range(0, 41, 2)]
        cw = {"R2": (centre, [4.0] * len(centre))}
        lanes = traffic.build_lanes([f], None, cw)
        ys = sorted(l["points"][len(l["points"]) // 2][1] for l in lanes)
        self.assertEqual(len(ys), 3)
        # 3.2 m lanes centred, the outer ones clamped to 4.0 - 1.2 = 2.8 m.
        self.assertAlmostEqual(ys[0], -2.8, delta=0.05)
        self.assertAlmostEqual(ys[1], 0.0, delta=0.05)
        self.assertAlmostEqual(ys[2], 2.8, delta=0.05)

    def test_clip_to_extent(self):
        lanes = traffic.build_lanes([road("A", [[0, -100], [0, 100]], sens="Sens direct", voies=1)],
                                    extent_m=(-50, -10, 50, 10))
        self.assertEqual(len(lanes), 1)
        ys = [p[1] for p in lanes[0]["points"]]
        self.assertAlmostEqual(min(ys), -10, places=3)
        self.assertAlmostEqual(max(ys), 10, places=3)

    def test_short_whole_segment_keeps_the_street_going(self):
        # BD TOPO draws some junctions with a segment of 2 to 3 m (rue du Président Édouard Herriot at the place des
        # Jacobins): dropped, the street stopped there, with no way on and no way into what follows.
        lanes = traffic.build_lanes([
            road("A", [[0, 0], [0, 50]], sens="Sens direct", voies=1, largeur=4.0),
            road("B", [[0, 50], [0, 52.7]], sens="Sens direct", voies=1, largeur=4.0),
            road("C", [[0, 52.7], [0, 100]], sens="Sens direct", voies=1, largeur=4.0),
            road("D", [[0, 52.7], [60, 53]], sens="Sens direct", voies=1, largeur=4.0),
        ])
        by_road = {lane["road"]: lane for lane in lanes}
        self.assertEqual(set(by_road), {"A", "B", "C", "D"})
        self.assertGreaterEqual(len(by_road["B"]["points"]), 2)
        self.assertEqual(by_road["A"]["next"], [by_road["B"]["id"]])
        self.assertEqual(sorted(by_road["B"]["next"]), sorted([by_road["C"]["id"], by_road["D"]["id"]]))

    def test_sliver_left_by_the_clip_is_still_dropped(self):
        lanes = traffic.build_lanes([road("A", [[0, 0], [0, 100]], sens="Sens direct", voies=1, largeur=4.0)],
                                    (-10.0, 98.0, 10.0, 200.0))
        self.assertEqual(lanes, [])

    def test_through_road_is_kept_whole_within_the_map(self):
        features = [road("A", [[0, 0], [0, 100]], sens="Sens direct", voies=1, largeur=4.0),
                    road("B", [[0, 100], [0, 300]], sens="Sens direct", voies=1, largeur=4.0),
                    road("C", [[0, 300], [0, 400]], sens="Sens direct", voies=1, largeur=4.0)]
        area = (-10.0, -10.0, 10.0, 150.0)
        cut = {lane["road"]: lane for lane in traffic.build_lanes(features, area)}
        self.assertEqual(set(cut), {"A", "B"})
        self.assertAlmostEqual(cut["B"]["points"][-1][1], 150.0, places=3)
        self.assertEqual(cut["B"]["next"], [])
        # B is of the flight circuit: whole within the map (to y = 350), and it leads to C, cut by the map.
        whole = {lane["road"]: lane for lane in traffic.build_lanes(features, area, None, ["B", "C"], (-10.0, -10.0, 10.0, 350.0))}
        self.assertEqual(set(whole), {"A", "B", "C"})
        self.assertAlmostEqual(whole["B"]["points"][-1][1], 300.0, places=3)
        self.assertEqual(whole["B"]["next"], [whole["C"]["id"]])
        self.assertAlmostEqual(whole["C"]["points"][-1][1], 350.0, places=3)
        self.assertTrue(whole["B"]["through"] and not whole["A"]["through"])

    def test_through_roads_file(self):
        import tempfile
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False, encoding="utf-8") as f:
            f.write("# roads of the circuit\nTRONROUT1  # quai\n\nTRONROUT2\n")
        try:
            self.assertEqual(traffic.read_through_roads(f.name), ["TRONROUT1", "TRONROUT2"])
        finally:
            os.remove(f.name)
        self.assertEqual(traffic.read_through_roads(f.name), [])


if __name__ == "__main__":
    unittest.main()
