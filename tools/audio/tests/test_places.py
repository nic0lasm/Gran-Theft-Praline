"""Place and mission sounds (make_places.py) against the C++ enums of GTPPlaceAudio.h."""

import os
import re
import sys
import tempfile
import unittest
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..")))

import make_places  # noqa: E402

HEADER = os.path.join(HERE, "..", "..", "..", "GranTheftPraline", "Source", "GranTheftPraline", "Public", "GTPPlaceAudio.h")
RATE = make_places.RATE


def enum_values(name):
    with open(HEADER, encoding="utf-8") as f:
        text = f.read()
    body = re.search(r"enum class " + name + r" : uint8\s*\{(.*?)\};", text, re.S).group(1)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return [v.strip() for v in body.split(",") if v.strip() and v.strip().split()[0] != "Count"]


def read(path):
    with wave.open(path, "rb") as w:
        return np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(float) / 32767.0


def centroid(x):
    spec = np.abs(np.fft.rfft(x)) ** 2
    f = np.fft.rfftfreq(len(x), 1 / RATE)
    return float((spec * f).sum() / (spec.sum() + 1e-12))


class PlacesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.written = make_places.build(cls.tmp.name)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def path(self, group, name, index=1):
        if group == "Interior":
            return os.path.join(self.tmp.name, group, name, f"SW_Interior_{name}.wav")
        return os.path.join(self.tmp.name, group, name, f"SW_{group}_{name}_{index:02d}.wav")

    def test_folders_match_the_enums(self):
        self.assertEqual(make_places.LOOPS, enum_values("EGTPRoomLoop"))
        self.assertEqual(make_places.SHOP, enum_values("EGTPShopSound"))
        self.assertEqual(make_places.MISSION, enum_values("EGTPMissionSound"))
        for group, names in (("Interior", make_places.LOOPS), ("Shop", make_places.SHOP), ("Mission", make_places.MISSION)):
            for name in names:
                self.assertTrue(os.listdir(os.path.join(self.tmp.name, group, name)), name)

    def test_loops_are_seamless(self):
        for name in make_places.LOOPS:
            x = read(self.path("Interior", name))
            self.assertAlmostEqual(len(x) / RATE, make_places.LOOP_SECONDS, places=2)
            # The jump from the last sample to the first is no bigger than a usual step.
            steps = np.abs(np.diff(x))
            self.assertLess(abs(x[-1] - x[0]), np.percentile(steps, 99.9) + 1e-3, name)

    def test_loops_sit_where_they_should(self):
        c = {name: centroid(read(self.path("Interior", name))) for name in make_places.LOOPS}
        # Hot oil and grill hiss are high; the cold cabinet and the hood fan are low.
        self.assertGreater(c["Fryer"], 3000)
        self.assertGreater(c["Grill"], 3000)
        self.assertLess(c["Cold"], 600)
        self.assertLess(c["Hood"], 1000)
        self.assertLess(c["RoomTone"], c["Fryer"])

    def test_one_shots_are_short_and_not_clipped(self):
        for path in self.written:
            if os.sep + "Interior" + os.sep in path:
                continue
            x = read(path)
            self.assertLess(len(x) / RATE, 3.5, path)
            self.assertLess(np.max(np.abs(x)), 0.99, path)
            self.assertGreater(np.max(np.abs(x)), 0.1, path)

    def test_fail_is_lower_than_success(self):
        self.assertLess(centroid(read(self.path("Mission", "Fail"))), centroid(read(self.path("Mission", "Success"))))


if __name__ == "__main__":
    unittest.main()
