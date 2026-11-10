"""Voice, police radio and interface banks (make_voices_ui.py) against the C++ enums."""

import os
import re
import sys
import tempfile
import unittest
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..")))

import make_voices_ui  # noqa: E402

TYPES = os.path.join(HERE, "..", "..", "..", "GranTheftPraline", "Source", "GranTheftPraline", "Public", "GTPAudioTypes.h")


def enum_values(name):
    with open(TYPES, encoding="utf-8") as f:
        text = f.read()
    body = re.search(r"enum class " + name + r" : uint8\s*\{(.*?)\};", text, re.S).group(1)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return [v.strip() for v in body.split(",") if v.strip() and not v.strip().startswith("Count")]


class VoicesUITest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.written = make_voices_ui.build(cls.tmp.name)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_folders_match_the_enums(self):
        self.assertEqual(sorted(make_voices_ui.VOICE_COUNTS), sorted(enum_values("EGTPVoiceKind")))
        self.assertEqual(sorted(make_voices_ui.RADIO_COUNTS), sorted(enum_values("EGTPRadioCall")))
        self.assertEqual(sorted(make_voices_ui.UI_NAMES), sorted(enum_values("EGTPUISound")))
        for top, names in (("Voices", make_voices_ui.VOICE_COUNTS), ("Radio", make_voices_ui.RADIO_COUNTS), ("UI", make_voices_ui.UI_NAMES)):
            for name in names:
                self.assertTrue(os.listdir(os.path.join(self.tmp.name, top, name)), f"{top}/{name} empty")

    def test_files_are_clean(self):
        for path in self.written:
            with wave.open(path) as w:
                self.assertEqual((w.getframerate(), w.getnchannels(), w.getsampwidth()), (48000, 1, 2))
                x = np.frombuffer(w.readframes(w.getnframes()), "<i2") / 32768.0
            self.assertTrue(np.all(np.isfinite(x)))
            self.assertLess(np.max(np.abs(x)), 0.75, path)
            self.assertGreater(np.max(np.abs(x)), 0.1, path)

    def test_radio_stays_in_the_radio_band(self):
        for path in self.written:
            if os.sep + "Radio" + os.sep not in path:
                continue
            with wave.open(path) as w:
                x = np.frombuffer(w.readframes(w.getnframes()), "<i2") / 32768.0
            spec = np.abs(np.fft.rfft(x)) ** 2
            f = np.fft.rfftfreq(len(x), 1 / 48000)
            self.assertGreater(spec[(f > 300) & (f < 3500)].sum() / spec.sum(), 0.7, path)

    def test_screams_are_high(self):
        x = None
        path = [p for p in self.written if "Scream" in p][0]
        with wave.open(path) as w:
            x = np.frombuffer(w.readframes(w.getnframes()), "<i2") / 32768.0
        mid = x[len(x) // 3: len(x) // 3 + 4096]
        ac = np.correlate(mid, mid, "full")[len(mid) - 1:]
        lo, hi = 48000 // 1500, 48000 // 150
        f0 = 48000 / (lo + int(np.argmax(ac[lo:hi])))
        self.assertGreater(f0, 400)


if __name__ == "__main__":
    unittest.main()
