import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import minimap  # noqa: E402


def road(nature="Route à 1 chaussée", importance="3", pos="0", coords=((0, 0), (10, 0))):
    return {"properties": {"nature": nature, "importance": importance, "position_par_rapport_au_sol": pos,
                           "largeur_de_chaussee": 6, "nom_voie_ban_gauche": "Rue de la République"},
            "geometry": {"type": "LineString", "coordinates": [list(c) for c in coords]}}


class MinimapTests(unittest.TestCase):
    def test_kinds(self):
        self.assertEqual(minimap.road_kind(road()["properties"]), "road_major")
        self.assertEqual(minimap.road_kind(road(importance="5")["properties"]), "road_minor")
        self.assertEqual(minimap.road_kind(road(nature="Sentier")["properties"]), "path")
        self.assertEqual(minimap.road_kind(road(pos="-1")["properties"]), "tunnel")

    def test_unreal_coordinates(self):
        # y north in metres becomes Y south in centimetres.
        self.assertEqual(minimap.to_ue_cm([(1.0, 2.0)]), [(100, -200)])

    def test_simplify_keeps_ends_and_drops_collinear(self):
        pts = [(0, 0), (5, 0.1), (10, 0), (10, 10)]
        out = minimap.simplify(pts, 1.0)
        self.assertEqual(out, [(0, 0), (10, 0), (10, 10)])

    def test_small_buildings_are_dropped(self):
        small = {"geometry": {"type": "Polygon", "coordinates": [[[0, 0], [2, 0], [2, 2], [0, 2], [0, 0]]]}}
        big = {"geometry": {"type": "Polygon", "coordinates": [[[0, 0], [20, 0], [20, 20], [0, 20], [0, 0]]]}}
        rows = minimap.build_rows([], [], [small, big])
        self.assertEqual([r[0] for r in rows], ["building"])

    def test_csv_round_trip(self):
        rows = minimap.build_rows([road()], [], [])
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "m.csv")
            minimap.write_rows(rows, p)
            text = open(p, encoding="utf-8").read().splitlines()
        self.assertEqual(text[0], "kind,width_m,points_ue_cm,name")
        self.assertEqual(text[1], "road_major,6,0 0;1000 0,Rue de la République")


if __name__ == "__main__":
    unittest.main()
