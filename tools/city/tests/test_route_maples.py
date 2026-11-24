import os
import sys
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import route_trees  # noqa: E402


class RouteMapleTests(unittest.TestCase):
    def test_the_alders_of_the_route_have_a_maple_and_nothing_else_does(self):
        self.assertEqual(route_trees.kind_of("SM_BlackAlder_Field_05"), "mature")
        self.assertEqual(route_trees.kind_of("SM_BlackAlder_Sapling_06_M1"), "young")
        self.assertIsNone(route_trees.kind_of("SM_TreeGrate_Square_A"))
        self.assertIsNone(route_trees.kind_of("SM_NorwayMaple_Forest_04"))       # swapped once, left alone after
        for kind in set(route_trees.REPLACED.values()):
            self.assertIn(kind, route_trees.MAPLES)
            self.assertIn(kind, route_trees.HEIGHTS_CM)
        for path in route_trees.MAPLES.values():
            self.assertTrue(path.startswith("/Game/NorwayMaple/Geometry/SimpleWind/SM_NorwayMaple_"))

    def test_a_maple_is_a_tree_for_the_lanes_step_and_a_grate_is_not(self):
        for path in list(route_trees.MAPLES.values()) + route_trees.ROUTE_TREES["mature"].split("|"):
            self.assertTrue(route_trees.is_tree(path), path)
        self.assertFalse(route_trees.is_tree("/Game/Environment/Props/Street/TreeGrate_Square/SM_TreeGrate_Square_A"))
        self.assertFalse(route_trees.is_tree("/Game/Environment/Props/Street/Bench/SM_Bench_WoodCastIron_A"))

    def test_the_mean_height_is_kept_inside_the_range_of_the_kind(self):
        # alders 14 m at scale 1.1 on average, replaced by a 15.9 m maple: still 15.4 m
        k = route_trees.rescale("mature", 1400.0, 1594.0, 1.1)
        self.assertAlmostEqual(1594.0 * 1.1 * k, 1540.0, delta=0.5)
        # a 9 m alder: the maples are brought up to 13 m, a 25 m one down to 17 m
        self.assertAlmostEqual(1594.0 * route_trees.rescale("mature", 900.0, 1594.0, 1.0), 1300.0, delta=0.5)
        self.assertAlmostEqual(1594.0 * route_trees.rescale("mature", 2500.0, 1594.0, 1.0), 1700.0, delta=0.5)
        # saplings scaled up 3 times to stand for young trees: 6 m maples at that scale would be 18 m
        k = route_trees.rescale("young", 250.0, 600.0, 3.0)
        self.assertAlmostEqual(600.0 * 3.0 * k, 700.0, delta=0.5)
        self.assertEqual(route_trees.rescale("young", 250.0, 0.0, 1.0), 1.0)      # a mesh with no height: untouched


if __name__ == "__main__":
    unittest.main()
