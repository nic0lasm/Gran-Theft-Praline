"""Tests of the ambience bed and recorded-sound picking tools (CI job "Audio tools tests")."""

import csv
import json
import os
import struct
import sys
import tempfile
import unittest
import wave

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import make_ambiences  # noqa: E402
import pick_sonniss  # noqa: E402
import zone_map  # noqa: E402


def write_wav(path, x, rate=48000):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())


class AmbienceTest(unittest.TestCase):
    def test_every_layer_loops_at_the_same_loudness(self):
        for name in make_ambiences.LAYERS:
            x = make_ambiences.build(name, 6.0, 1)
            self.assertEqual(x.shape, (6 * 48000, 2), name)
            self.assertTrue(np.all(np.isfinite(x)), name)
            rms_db = 20 * np.log10(np.sqrt(np.mean(x ** 2)))
            self.assertAlmostEqual(rms_db, make_ambiences.TARGET_RMS_DB, delta=1.0, msg=name)
            self.assertLess(np.max(np.abs(x)), 0.9, name)
            # Seamless loop: the wrap-around step is no larger than steps inside the loop.
            inner = np.percentile(np.abs(np.diff(x, axis=0)), 99.9)
            self.assertLess(np.max(np.abs(x[0] - x[-1])), max(inner * 2.0, 0.02), name)

    def test_layers_differ(self):
        def centroid(x):
            spec = np.abs(np.fft.rfft(x[:, 0]))
            f = np.fft.rfftfreq(len(x), 1 / 48000)
            return float((spec * f).sum() / spec.sum())
        rain = centroid(make_ambiences.build("Rain", 4.0, 2))
        roof = centroid(make_ambiences.build("RainCar", 4.0, 2))
        night = centroid(make_ambiences.build("Night", 4.0, 2))
        self.assertGreater(rain, roof)   # the roof drumming is duller than street rain
        self.assertGreater(rain, night)


class ZoneMapTest(unittest.TestCase):
    def test_zones(self):
        with tempfile.TemporaryDirectory() as tmp:
            ground = os.path.join(tmp, "ground")
            os.makedirs(ground)
            # Mask 100 x 100 m; a pedestrian street (G) in rows 40-50 (Unreal Y 4000-5000 cm).
            mask = np.zeros((200, 200, 3), np.uint8)
            mask[80:100, :, 1] = 255
            Image.fromarray(mask, "RGB").save(os.path.join(ground, "T_GroundMask.png"))
            with open(os.path.join(ground, "ground.json"), "w") as f:
                json.dump({"extent_ue_cm": [0.0, 0.0, 10000.0, 10000.0]}, f)
            # A river from x 150 to 200 m, north 0..-100 m.
            river = [[150, 0], [200, 0], [200, -100], [150, -100], [150, 0]]
            water = os.path.join(tmp, "water.geojson")
            with open(water, "w") as f:
                json.dump({"type": "FeatureCollection", "features": [
                    {"type": "Feature", "properties": {"nature": "Ecoulement naturel"},
                     "geometry": {"type": "Polygon", "coordinates": [river]}}]}, f)
            out = os.path.join(tmp, "zones.gtpsurf")
            zone_map.build(ground, water, out, 200.0)
            data = open(out, "rb").read()
            nx, ny, x0, y0, cell = struct.unpack("<iifff", data[8:28])
            ids = np.frombuffer(data[28:], np.uint8).reshape(ny, nx)

            def at(x_cm, y_cm):
                return ids[int((y_cm - y0) // cell), int((x_cm - x0) // cell)]

            self.assertGreaterEqual(x0 + nx * cell, 20000.0)  # reaches the far bank
            self.assertEqual(at(500, 4500), 1)     # pedestrian street
            self.assertEqual(at(500, 1000), 0)     # ordinary street
            self.assertEqual(at(13000, 5000), 2)   # quay, 20 m from the water
            self.assertEqual(at(17000, 5000), 3)   # above the water: bridge


    def test_squares_from_buildings(self):
        with tempfile.TemporaryDirectory() as tmp:
            gen = os.path.join(tmp, "gen")
            ground = os.path.join(gen, "ground")
            os.makedirs(ground)
            # Mask 200 x 200 m, a carriageway (R) along the south edge (Unreal Y 18000-20000 cm).
            mask = np.zeros((400, 400, 3), np.uint8)
            mask[360:400, :, 0] = 255
            Image.fromarray(mask, "RGB").save(os.path.join(ground, "T_GroundMask.png"))
            with open(os.path.join(ground, "ground.json"), "w") as f:
                json.dump({"extent_ue_cm": [0.0, 0.0, 20000.0, 20000.0]}, f)
            # Buildings along the north and west edges, 20 m deep; the rest is open.
            with open(os.path.join(gen, "buildings_gen.csv"), "w", newline="") as f:
                w = csv.DictWriter(f, ["ue_x", "ue_y", "bounds_ue_cm"])
                w.writeheader()
                w.writerow({"ue_x": 10000, "ue_y": 1000, "bounds_ue_cm": "-10000 -1000 0 10000 1000 1500"})
                w.writerow({"ue_x": 1000, "ue_y": 10000, "bounds_ue_cm": "-1000 -10000 0 1000 10000 1500"})
            water = os.path.join(tmp, "water.geojson")
            with open(water, "w") as f:
                json.dump({"type": "FeatureCollection", "features": []}, f)
            out = os.path.join(tmp, "zones.gtpsurf")
            zone_map.build(ground, water, out, 200.0, east_cm=20000.0)
            data = open(out, "rb").read()
            nx, ny, x0, y0, cell = struct.unpack("<iifff", data[8:28])
            ids = np.frombuffer(data[28:], np.uint8).reshape(ny, nx)

            def at(x_cm, y_cm):
                return ids[int((y_cm - y0) // cell), int((x_cm - x0) // cell)]

            self.assertEqual(at(12000, 10000), 4)  # 80 m from the buildings: square
            self.assertEqual(at(4000, 10000), 0)   # 20 m from the west row: street
            self.assertEqual(at(12000, 19000), 0)  # on the carriageway: street
            self.assertEqual(zone_map.ZONES[4], "Square")


class PickTest(unittest.TestCase):
    def test_banks_and_step_slicing(self):
        with tempfile.TemporaryDirectory() as tmp:
            lib = os.path.join(tmp, "lib")
            rate = 48000
            walk = np.zeros(int(3.0 * rate))
            for k in range(5):  # five steps, 0.5 s apart
                s = int((0.2 + 0.5 * k) * rate)
                walk[s:s + 2000] = 0.6 * np.exp(-np.arange(2000) / 300.0)
            write_wav(os.path.join(lib, "Foley", "Footsteps_Gravel_Walk_01.wav"), walk)
            write_wav(os.path.join(lib, "Vehicles", "Car_Crash_Big_03.wav"), walk[:rate])
            write_wav(os.path.join(lib, "Vehicles", "Car_Engine_Idle_Loop.wav"), walk[:rate])
            write_wav(os.path.join(lib, "Glass", "Window_Glass_Break_02.wav"), walk[:rate])
            write_wav(os.path.join(lib, "City", "Street_Ambience_Loop.wav"), walk[:rate])
            self.assertEqual(pick_sonniss.bank_of("Footsteps_Gravel_Walk_01.wav"), "Footsteps/Gravel")
            self.assertEqual(pick_sonniss.bank_of("Car_Crash_Big_03.wav"), "Impacts/Car/Heavy")
            self.assertIsNone(pick_sonniss.bank_of("Car_Engine_Idle_Loop.wav"))
            self.assertEqual(pick_sonniss.bank_of("Window_Glass_Break_02.wav"), "Impacts/Glass")
            picks = os.path.join(tmp, "picks.csv")
            sys.argv = ["pick_sonniss.py", lib, picks]
            pick_sonniss.main()
            with open(picks, newline="") as f:
                rows = list(csv.DictReader(f))
            steps = [r for r in rows if r["bank"] == "Footsteps/Gravel"]
            self.assertEqual(len(steps), 5)
            self.assertAlmostEqual(float(steps[1]["start_s"]), 0.695, delta=0.02)
            banks = {r["bank"] for r in rows}
            self.assertEqual(banks, {"Footsteps/Gravel", "Impacts/Car/Heavy", "Impacts/Glass"})


if __name__ == "__main__":
    unittest.main()
