import os
import pathlib
import stat
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import check_game_log
import check_notices
import make_archives
import stamp_version

INI = """[/Script/EngineSettings.GeneralProjectSettings]\nProjectName=X\nProjectVersion=0.2.0\nCopyrightNotice=c\n\n[Other]\nProjectVersion=9\n"""


class StampVersion(unittest.TestCase):
    def test_parse(self):
        self.assertEqual(stamp_version.parse_tag("v0.9.0"), ("0.9.0", ""))
        self.assertEqual(stamp_version.parse_tag("0.9"), ("0.9.0", ""))
        self.assertEqual(stamp_version.parse_tag("v0.9.0-rc.2"), ("0.9.0", "rc2"))
        self.assertEqual(stamp_version.parse_tag("v1.0.1-rc1"), ("1.0.1", "rc1"))
        for bad in ("latest", "v1", "v0.9.0-beta", "0.9.0.1"):
            with self.assertRaises(ValueError):
                stamp_version.parse_tag(bad)

    def test_set_only_in_section(self):
        out = stamp_version.set_version(INI, "0.9.0")
        self.assertIn("ProjectVersion=0.9.0\nCopyright", out)
        self.assertIn("[Other]\nProjectVersion=9", out)
        self.assertEqual(stamp_version.read_version(out), "0.9.0")

    def test_keeps_crlf(self):
        out = stamp_version.set_version(INI.replace("\n", "\r\n"), "0.9.0")
        self.assertIn("ProjectVersion=0.9.0\r\n", out)

    def test_cli_roundtrip(self):
        with tempfile.TemporaryDirectory() as d:
            ini = pathlib.Path(d) / "g.ini"
            ini.write_text(INI)
            self.assertEqual(stamp_version.main(["v0.9.0-rc1", "--ini", str(ini), "--check"]), 1)
            self.assertEqual(stamp_version.main(["v0.9.0-rc1", "--ini", str(ini)]), 0)
            self.assertEqual(stamp_version.main(["v0.9.0-rc1", "--ini", str(ini), "--check"]), 0)
            self.assertEqual(stamp_version.main(["nonsense", "--ini", str(ini)]), 2)


class Archives(unittest.TestCase):
    def build_tree(self, root):
        app = root / "App.app"
        (app / "bin").mkdir(parents=True)
        (app / "bin/run").write_bytes(b"x" * 100)
        os.chmod(app / "bin/run", 0o755)
        (app / "data1").write_bytes(b"a" * 600)
        (app / "data2").write_bytes(b"b" * 600)
        os.symlink("bin/run", app / "link")
        return app

    def test_split_symlink_and_sums(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            app = self.build_tree(root)
            out = root / "out"
            self.assertEqual(make_archives.main([str(app), str(out), "--name", "G-1-mac", "--limit-bytes", "700"]), 0)
            parts = sorted(p.name for p in out.glob("*.zip"))
            self.assertEqual(len(parts), 3)
            self.assertTrue(all("of3" in p for p in parts))
            sums = (out / "SHA256SUMS").read_text().splitlines()
            self.assertEqual(len(sums), 3)
            names = {}
            for p in out.glob("*.zip"):
                with zipfile.ZipFile(p) as zf:
                    for info in zf.infolist():
                        names[info.filename] = info
            self.assertEqual(set(names), {"App.app/bin/run", "App.app/data1", "App.app/data2", "App.app/link"})
            link = names["App.app/link"]
            self.assertTrue(stat.S_ISLNK(link.external_attr >> 16))
            self.assertTrue(names["App.app/bin/run"].external_attr >> 16 & 0o100)

    def test_single_archive_and_oversize_file(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            app = self.build_tree(root)
            self.assertEqual(make_archives.main([str(app), str(root / "o"), "--name", "G"]), 0)
            self.assertTrue((root / "o/G.zip").exists())
            self.assertEqual(make_archives.main([str(app), str(root / "o2"), "--name", "G", "--limit-bytes", "50"]), 2)


class GameLog(unittest.TestCase):
    def lines(self, *extra):
        return ["[2026.11.11-10.00.00:000][  0]LogInit: Display: ok\n"] * 60 + list(extra)

    def test_clean(self):
        self.assertEqual(check_game_log.scan(self.lines(), [])[:2], ([], []))

    def test_error_and_fatal(self):
        fatals, errors, warns = check_game_log.scan(self.lines(
            "[2026.11.11-10.00.01:000][  1]LogTemp: Error: boom\n",
            "[2026.11.11-10.00.02:000][  2]LogCore: Warning: meh\n",
            "Fatal error: access violation\n"), [])
        self.assertEqual((len(fatals), len(errors), len(warns)), (1, 1, 1))

    def test_allow_list(self):
        import re
        allow = [re.compile("LogTemp: Error: boom")]
        fatals, errors, _ = check_game_log.scan(self.lines("[a][1]LogTemp: Error: boom\n"), allow)
        self.assertEqual((fatals, errors), ([], []))

    def test_short_log_fails(self):
        with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as f:
            f.write("only one line\n")
        try:
            self.assertEqual(check_game_log.main([f.name]), 1)
        finally:
            os.unlink(f.name)


class Notices(unittest.TestCase):
    def test_detects_missing(self):
        sources = "Licence Ouverte Etalab 2.0 (IGN) ; ODbL OpenStreetMap ; Poly Haven CC0"
        ok = "IGN Etalab Métropole de Lyon OpenStreetMap ODbL CC0 Poly Haven Unreal Engine Epic Games"
        self.assertEqual(check_notices.check(sources, {"a": ok}), [])
        bad = ok.replace("Poly Haven", "")
        self.assertTrue(check_notices.check(sources, {"a": bad}))


if __name__ == "__main__":
    unittest.main()


class Bundle(unittest.TestCase):
    def test_violations(self):
        import check_release_bundle as b
        self.assertEqual(b.violations(["App/Content/T_Sign_1.uasset"]), [])
        self.assertEqual(len(b.violations(["App/Plugins/CityBrands/x.uasset", "a/T_SignLogo_9.png"])), 3 - 1)
        self.assertEqual(b.violations(["a/T_SignLogo_9.png"], allow_logos=True), [])
        self.assertTrue(b.violations(["a/shell.glb", "x/.scratch/tripo.env"]))

    def test_folder_with_archives(self):
        import check_release_bundle as b
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            with zipfile.ZipFile(root / "G.zip", "w") as zf:
                zf.writestr("App/ok.bin", "x")
            self.assertEqual(b.main([str(root)]), 1)  # no SHA256SUMS
            (root / "SHA256SUMS").write_text("abc  G.zip\n")
            self.assertEqual(b.main([str(root)]), 0)
            with zipfile.ZipFile(root / "G.zip", "a") as zf:
                zf.writestr("App/T_SignLogo_1.png", "x")
            self.assertEqual(b.main([str(root)]), 1)


class DayReport(unittest.TestCase):
    def test_parse_proof(self):
        import day_report as d
        ok = d.parse_proof("compile-x", "VERDICT: OK  feat/x @abc1234  (464s)")
        self.assertEqual((ok["kind"], ok["status"]), ("compile", "OK"))
        bad = d.parse_proof("package-c9", "Package failed; see log")
        self.assertEqual((bad["kind"], bad["status"]), ("package", "FAIL"))
        self.assertEqual(d.parse_proof("lyra", "# Lyra\nx")["kind"], "other")

    def test_percentiles(self):
        import day_report as d
        self.assertEqual(d.parse_percentiles("frame p50 25,00 ms | p95 25.02 ms"), (25.0, 25.02))
        self.assertIsNone(d.parse_percentiles("nothing"))


class CcBy(unittest.TestCase):
    def test_cc_by_required_only_when_used(self):
        base = "IGN Etalab Métropole de Lyon OpenStreetMap ODbL Unreal Engine Epic Games"
        self.assertEqual(check_notices.check("Etalab", {"a": base}), [])
        problems = check_notices.check("asset under CC BY 4.0", {"a": base})
        self.assertTrue(any("CC BY 4.0" in p for p in problems))
        ok = base + " CC BY 4.0 https://creativecommons.org/licenses/by/4.0/"
        self.assertEqual(check_notices.check("asset under CC BY 4.0", {"a": ok}), [])
