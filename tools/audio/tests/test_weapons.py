"""Combat banks (make_weapons.py) against the C++ enums and the spec of docs/gameplay/combat.md."""

import os
import sys
import tempfile
import unittest
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..")))
sys.path.insert(0, HERE)

import make_weapons  # noqa: E402
from test_voices_ui import enum_values  # noqa: E402


def read(path):
    with wave.open(path) as w:
        return np.frombuffer(w.readframes(w.getnframes()), "<i2") / 32768.0


class WeaponsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.written = make_weapons.build(cls.tmp.name)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def first(self, top, name):
        return read(sorted(p for p in self.written if os.sep.join(["", top, name, ""]) in p)[0])

    def test_folders_match_the_enums(self):
        self.assertEqual(sorted(make_weapons.WEAPON_COUNTS), sorted(enum_values("EGTPWeaponSound")))
        self.assertEqual(sorted(make_weapons.IMPACT_COUNTS), sorted(enum_values("EGTPBulletImpact")))
        self.assertEqual(sorted(make_weapons.MELEE_COUNTS), sorted(enum_values("EGTPMeleeSound")))

    def test_files_are_clean(self):
        for path in self.written:
            x = read(path)
            self.assertTrue(np.all(np.isfinite(x)), path)
            self.assertLess(np.max(np.abs(x)), 0.95, path)
            self.assertGreater(np.max(np.abs(x)), 0.05, path)

    def test_close_shot_is_short_far_shot_is_dull(self):
        close, far = self.first("Weapons", "ShotClose"), self.first("Weapons", "ShotFar")
        energy = np.cumsum(close ** 2)
        self.assertLess(np.searchsorted(energy, 0.9 * energy[-1]) / 48000, 0.08)  # dry and short

        def centroid(x):
            s = np.abs(np.fft.rfft(x)) ** 2
            f = np.fft.rfftfreq(len(x), 1 / 48000)
            return (s * f).sum() / s.sum()
        self.assertLess(centroid(far), centroid(close) / 2)  # far: lows and tail

    def test_whizz_falls_in_pitch(self):
        x = self.first("Weapons", "Whizz")
        half = len(x) // 2

        def centroid(y):
            s = np.abs(np.fft.rfft(y)) ** 2
            f = np.fft.rfftfreq(len(y), 1 / 48000)
            return (s * f).sum() / s.sum()
        self.assertGreater(centroid(x[:half]), centroid(x[half:]))


if __name__ == "__main__":
    unittest.main()
