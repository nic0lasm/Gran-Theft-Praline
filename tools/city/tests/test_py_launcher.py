import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "GranTheftPraline", "Content", "Python"))
import gtp_run  # noqa: E402


class LauncherTests(unittest.TestCase):
    def test_runs_a_script_under_a_path_with_spaces(self):
        with tempfile.TemporaryDirectory(prefix="work folder ") as d:
            with open(os.path.join(d, "helper_mod.py"), "w") as f:
                f.write("X = 42\n")
            with open(os.path.join(d, "job.py"), "w") as f:
                f.write("import sys, helper_mod\nRESULT = (__name__, sys.argv[1:], helper_mod.X)\n")
            os.environ["GTP_PY_ARGS"] = "--a 'b c'"
            try:
                ns = gtp_run.main(os.path.join(d, "job.py"))
            finally:
                del os.environ["GTP_PY_ARGS"]
            self.assertEqual(ns["RESULT"], ("__main__", ["--a", "b c"], 42))

    def test_missing_script_is_an_error(self):
        with self.assertRaises(FileNotFoundError):
            gtp_run.main("/nowhere/job.py")


if __name__ == "__main__":
    unittest.main()
