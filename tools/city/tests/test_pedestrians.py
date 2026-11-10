import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import pedestrians  # noqa: E402


def road(cid, coords, vitesse=30, voies=2, largeur=6.0, nature="Route à 1 chaussée", pos="0"):
    return {"properties": {"cleabs": cid, "nature": nature, "nombre_de_voies": voies,
                           "largeur_de_chaussee": largeur, "position_par_rapport_au_sol": pos,
                           "vitesse_moyenne_vl": vitesse},
            "geometry": {"type": "MultiLineString", "coordinates": [coords]}}


def walks(edges):
    return [e for e in edges if e["kind"] in (0, 1)]


class PedestrianTests(unittest.TestCase):
    def test_car_road_has_a_walkway_each_side_clear_of_the_asphalt(self):
        edges = pedestrians.build_walkways([road("A", [[0, 0], [0, 100]])])
        w = walks(edges)
        self.assertEqual(len(w), 2)
        xs = sorted(e["points"][0][0] for e in w)
        # 2 lanes = 6.4 m carriageway (3.2 m per lane): half width + kerb + 1.1 m.
        self.assertAlmostEqual(xs[0], -4.65, places=2)
        self.assertAlmostEqual(xs[1], 4.65, places=2)
        # Same direction as the drawing (south to north).
        self.assertTrue(all(e["points"][-1][1] > e["points"][0][1] for e in w))

    def test_pedestrian_street_gets_three_lines(self):
        edges = pedestrians.build_walkways([road("P", [[0, 0], [0, 100]], vitesse=0)])
        w = walks(edges)
        self.assertEqual(len(w), 3)
        self.assertTrue(all(e["kind"] == pedestrians.KIND_PEDESTRIAN for e in w))
        self.assertAlmostEqual(sorted(e["points"][0][0] for e in w)[1], 0.0, places=3)

    def test_steps_tunnels_and_ferries_are_left_out(self):
        edges = pedestrians.build_walkways([
            road("S", [[0, 0], [0, 50]], nature="Escalier"),
            road("T", [[10, 0], [10, 50]], pos="-1"),
            road("F", [[20, 0], [20, 50]], nature="Bac ou liaison maritime"),
        ])
        self.assertEqual(edges, [])

    def test_crossroads_link_walkways_and_mark_crossings(self):
        edges = pedestrians.build_walkways([
            road("A", [[0, -100], [0, 0]]), road("B", [[0, 0], [0, 100]]),
            road("C", [[0, 0], [100, 0]]),
        ])
        crossings = [e for e in edges if e["kind"] == pedestrians.KIND_CROSSING]
        self.assertGreater(len(crossings), 3)
        # Every link joins two nodes that some walkway ends at.
        nodes = {n for e in walks(edges) for n in (e["node_a"], e["node_b"])}
        for e in crossings:
            self.assertIn(e["node_a"], nodes)
            self.assertIn(e["node_b"], nodes)

    def test_plain_join_is_not_a_crossing(self):
        edges = pedestrians.build_walkways([road("A", [[0, 0], [0, 50]]), road("B", [[0, 50], [0, 100]])])
        self.assertFalse([e for e in edges if e["kind"] == pedestrians.KIND_CROSSING])
        self.assertTrue([e for e in edges if e["kind"] == pedestrians.KIND_JOIN])

    def test_walkers_can_get_from_any_walkway_to_any_other(self):
        edges = pedestrians.build_walkways([
            road("A", [[0, -100], [0, 0]]), road("B", [[0, 0], [0, 100]]),
            road("C", [[0, 0], [100, 0]]), road("D", [[0, 0], [-100, 0]]),
        ])
        adj = {}
        for e in edges:
            adj.setdefault(e["node_a"], set()).add(e["node_b"])
            adj.setdefault(e["node_b"], set()).add(e["node_a"])
        start = edges[0]["node_a"]
        seen, todo = {start}, [start]
        while todo:
            for n in adj[todo.pop()]:
                if n not in seen:
                    seen.add(n)
                    todo.append(n)
        self.assertEqual(seen, set(adj))


if __name__ == "__main__":
    unittest.main()
