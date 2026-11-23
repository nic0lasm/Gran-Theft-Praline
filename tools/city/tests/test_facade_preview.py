import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import facade_preview  # noqa: E402


class FacadePreviewTests(unittest.TestCase):
    def test_the_shader_text_is_found_without_unreal(self):
        text = facade_preview.shader_text()
        self.assertIn("return float4(col, glass);", text)
        self.assertIn("slide", text)       # the openings are set back by parallax

    @unittest.skipUnless(shutil.which(os.environ.get("CXX", "g++")), "no C++ compiler")
    def test_the_shader_compiles_and_draws_a_street(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "facade.ppm")
            figures = facade_preview.render(out, (320, 180))
            self.assertEqual(facade_preview.problems(figures), [])
            with open(out, "rb") as f:
                self.assertEqual(f.read(2), b"P6")

    def test_a_flat_wall_is_refused(self):
        flat = {"wall_pixels": 1000.0, "glass_share": 0.0, "mean": 0.3, "deviation": 0.0}
        self.assertEqual(len(facade_preview.problems(flat)), 2)


if __name__ == "__main__":
    unittest.main()
