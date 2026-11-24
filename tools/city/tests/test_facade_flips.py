import os
import sys
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import facade_flips as ff  # noqa: E402


class FacadeFlipTests(unittest.TestCase):
    def test_gate_75_block(self):
        # the probe's modules of rue de la Bourse: ShopClosed_B turned to -90 among modules at 90
        inst = [("entrance", 26250, -14938, 3, 90.0), ("shop_b", 26250, -14825, 3, -90.0),
                ("groundwin", 26250, -14712, 3, 90.0), ("groundwin2", 26221, -14467, 3, 91.8),
                ("win1", 26250, -14712, 383, 90.0), ("win2", 26250, -14938, 383, 90.0)]
        self.assertEqual([k for k, _ in ff.flipped(inst)], ["shop_b"])

    def test_corner_is_left(self):
        inst = [("a", 0, 0, 0, 90.0), ("b", 0, 300, 0, 90.0), ("corner", 150, 450, 0, 0.0),
                ("c", 450, 450, 0, 0.0)]
        self.assertEqual(ff.flipped(inst), [])

    def test_thin_building_both_faces_are_left(self):
        # two faces of a 1 m thick building, back to back, each with its own neighbours
        inst = [("w1", 0, 0, 0, 90.0), ("w2", 0, 300, 0, 90.0), ("e1", 100, 0, 0, -90.0), ("e2", 100, 300, 0, -90.0)]
        self.assertEqual(ff.flipped(inst), [])

    def test_notch_face_on_two_votes_is_left(self):
        # prep 78: a short Blank face with 2 opposite neighbours only
        inst = [("a", 0, 0, 0, 90.0), ("notch", 0, 200, 0, -90.0), ("b", 0, 400, 0, 90.0)]
        self.assertEqual(ff.flipped(inst), [])
        self.assertEqual(ff.votes(inst)[1], (0, 2))

    def test_lone_pair_is_not_enough(self):
        self.assertEqual(ff.flipped([("a", 0, 0, 0, 90.0), ("b", 0, 300, 0, -90.0)]), [])


if __name__ == "__main__":
    unittest.main()
