"""The audio section of DefaultGame.ini uses only forms the cook accepts.

The 0.3.0 cook stopped on "+CategoryVolumes=(Radio, 0.8)" (a TMap written as
array lines). Lists are now arrays of structs with named fields; this test
checks each line against the C++ struct and enum names.
"""

import os
import re
import unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
INI = os.path.join(ROOT, "GranTheftPraline", "Config", "DefaultGame.ini")
PUBLIC = os.path.join(ROOT, "GranTheftPraline", "Source", "GranTheftPraline", "Public")


def audio_section():
    with open(INI, encoding="utf-8") as f:
        text = f.read()
    return re.search(r"^\[/Script/GranTheftPraline\.GTPAudioSettings\]\n(.*?)(?=^\[|\Z)", text, re.S | re.M).group(1)


def enum_values(name):
    with open(os.path.join(PUBLIC, "GTPAudioTypes.h"), encoding="utf-8") as f:
        body = re.search(r"enum class " + name + r" : uint8\s*\{(.*?)\};", f.read(), re.S).group(1)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return {v.strip() for v in body.split(",") if v.strip() and not v.strip().startswith("Count")}


def struct_fields(name):
    with open(os.path.join(PUBLIC, "GTPAudioSettings.h"), encoding="utf-8") as f:
        body = re.search(r"struct " + name + r"\s*\{(.*?)\};", f.read(), re.S).group(1)
    return set(re.findall(r"UPROPERTY\((?:[^()]|\([^()]*\))*\)\s*[\w<>:]+\s+(\w+)", body))


class AudioConfigTest(unittest.TestCase):
    def test_no_map_style_lines(self):
        for line in audio_section().splitlines():
            if line.startswith("+"):
                value = line.split("=", 1)[1]
                self.assertRegex(value, r"^\(\w+=", f"array line without named fields: {line}")

    def test_category_volumes(self):
        lines = [l for l in audio_section().splitlines() if l.startswith("+CategoryVolumes=")]
        self.assertTrue(lines)
        categories = enum_values("EGTPAudioCategory")
        self.assertEqual(struct_fields("FGTPCategoryVolume"), {"Category", "Volume"})
        for line in lines:
            m = re.fullmatch(r"\+CategoryVolumes=\(Category=(\w+),Volume=([0-9.]+)\)", line)
            self.assertIsNotNone(m, line)
            self.assertIn(m.group(1), categories)
            self.assertLessEqual(float(m.group(2)), 1.0)

    def test_engine_profiles(self):
        fields = struct_fields("FGTPEngineProfile")
        lines = [l for l in audio_section().splitlines() if l.startswith("+EngineProfiles=")]
        self.assertGreaterEqual(len(lines), 3)
        for line in lines:
            keys = set(re.findall(r"[(,](\w+)=", line.split("=", 1)[1]))
            self.assertTrue(keys <= fields, f"{keys - fields} not in FGTPEngineProfile")


if __name__ == "__main__":
    unittest.main()
