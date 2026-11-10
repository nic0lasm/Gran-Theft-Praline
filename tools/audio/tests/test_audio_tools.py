"""Tests of the audio tools that run outside Unreal (CI job "Audio tools tests")."""

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

import make_sfx  # noqa: E402
import stage_recorded  # noqa: E402
import surface_map  # noqa: E402


def read(path):
    with wave.open(path) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(np.float64) / 32767.0
        return x, w.getframerate(), w.getnchannels()


def centroid(x, rate):
    spec = np.abs(np.fft.rfft(x))
    f = np.fft.rfftfreq(len(x), 1 / rate)
    return float((spec * f).sum() / (spec.sum() + 1e-12))


class MakeSfxTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.files = make_sfx.build(cls.tmp.name, seed=3)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_layout_matches_the_banks(self):
        per_surface = make_sfx.STEPS_PER_SURFACE + make_sfx.LANDS_PER_SURFACE
        expected = (len(make_sfx.SURFACES) * per_surface
                    + len(make_sfx.KINDS) * len(make_sfx.TIERS) * make_sfx.IMPACTS_PER_TIER + make_sfx.GLASS_COUNT)
        self.assertEqual(len(self.files), expected)
        for surface in make_sfx.SURFACES:
            folder = os.path.join(self.tmp.name, "Footsteps", surface)
            self.assertEqual(len(os.listdir(folder)), make_sfx.STEPS_PER_SURFACE)
        self.assertTrue(os.path.isdir(os.path.join(self.tmp.name, "Impacts", "Car", "Heavy")))

    def test_files_are_clean(self):
        for path in self.files:
            x, rate, channels = read(path)
            self.assertEqual((rate, channels), (48000, 1), path)
            self.assertTrue(np.all(np.isfinite(x)))
            self.assertLess(np.max(np.abs(x)), 0.95, path)
            self.assertGreater(np.max(np.abs(x)), 0.3, path)
            self.assertLess(abs(np.mean(x)), 0.01, path)
            self.assertLess(np.max(np.abs(x[-48:])), 0.01, path + " does not fade out")

    def test_surfaces_sound_different(self):
        def mean_centroid(surface):
            folder = os.path.join(self.tmp.name, "Footsteps", surface)
            return np.mean([centroid(*read(os.path.join(folder, n))[:2]) for n in os.listdir(folder)])

        def tail_energy(surface):
            folder = os.path.join(self.tmp.name, "Footsteps", surface)
            out = []
            for n in os.listdir(folder):
                x, rate, _ = read(os.path.join(folder, n))
                out.append(np.sum(x[int(0.2 * rate):] ** 2) / np.sum(x ** 2))
            return np.mean(out)

        self.assertGreater(mean_centroid("Gravel"), mean_centroid("Asphalt"))
        self.assertGreater(mean_centroid("Stone"), mean_centroid("Asphalt"))
        self.assertGreater(tail_energy("Metal"), tail_energy("Asphalt"))

    def test_variants_differ(self):
        folder = os.path.join(self.tmp.name, "Footsteps", "Stone")
        a, b = (read(os.path.join(folder, n))[0] for n in sorted(os.listdir(folder))[:2])
        n = min(len(a), len(b))
        self.assertLess(abs(np.corrcoef(a[:n], b[:n])[0, 1]), 0.9)


class SurfaceMapTest(unittest.TestCase):
    def test_mask_and_areas(self):
        with tempfile.TemporaryDirectory() as tmp:
            ground = os.path.join(tmp, "ground")
            os.makedirs(ground)
            # 100 x 40 m, 1 px = 50 cm; asphalt band in the middle rows.
            w, h = 200, 80
            mask = np.zeros((h, w, 3), np.uint8)
            mask[30:50, :, 0] = 255
            Image.fromarray(mask, "RGB").save(os.path.join(ground, "T_GroundMask.png"))
            with open(os.path.join(ground, "ground.json"), "w") as f:
                json.dump({"extent_ue_cm": [0.0, 0.0, 10000.0, 4000.0]}, f)
            areas = os.path.join(tmp, "areas.geojson")
            # A gravel square at x 10-20 m, north -5..-35 m (Unreal Y 500..3500 cm): crosses the road.
            ring = [[10, -5], [20, -5], [20, -35], [10, -35], [10, -5]]
            with open(areas, "w") as f:
                json.dump({"type": "FeatureCollection", "features": [
                    {"type": "Feature", "properties": {"surface": "Gravel"},
                     "geometry": {"type": "Polygon", "coordinates": [ring]}}]}, f)
            out = os.path.join(tmp, "map.gtpsurf")
            grid, counts = surface_map.build(ground, out, 50.0, areas=areas)
            with open(out, "rb") as f:
                data = f.read()
            self.assertEqual(data[:8], b"GTPSURF1")
            nx, ny, x0, y0, cell = struct.unpack("<iifff", data[8:28])
            self.assertEqual((nx, ny, x0, y0, cell), (200, 80, 0.0, 0.0, 50.0))
            self.assertEqual(len(data), 28 + nx * ny)
            ids = np.frombuffer(data[28:], np.uint8).reshape(ny, nx)
            asphalt, stone, gravel = surface_map.ID["Asphalt"], surface_map.ID["Stone"], surface_map.ID["Gravel"]
            self.assertEqual(ids[40, 5], asphalt)       # road
            self.assertEqual(ids[10, 5], stone)          # sidewalk
            self.assertEqual(ids[10, 30], gravel)        # inside the square, off the road
            self.assertEqual(ids[40, 30], asphalt)       # the road keeps its asphalt
            self.assertTrue(os.path.isfile(os.path.join(tmp, "map.png")))
            self.assertIn("Gravel", counts)


class StageRecordedTest(unittest.TestCase):
    def test_24_bit_stereo_pick(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = os.path.join(tmp, "rec.wav")
            rate = 96000
            t = np.arange(int(0.5 * rate)) / rate
            sig = np.zeros_like(t)
            sig[int(0.1 * rate):] = 0.5 * np.sin(2 * np.pi * 300 * t[int(0.1 * rate):]) * np.exp(-t[: len(t) - int(0.1 * rate)] / 0.05)
            v = (np.stack([sig, sig], 1).reshape(-1) * (1 << 23)).astype(np.int32)
            raw = bytearray()
            for s in v:
                raw += int(s & 0xFFFFFF).to_bytes(3, "little")
            with wave.open(src, "wb") as w:
                w.setnchannels(2)
                w.setsampwidth(3)
                w.setframerate(rate)
                w.writeframes(bytes(raw))
            picks = os.path.join(tmp, "picks.csv")
            with open(picks, "w") as f:
                f.write("source,bank\n" + src + ",Footsteps/Gravel\n")
            out = os.path.join(tmp, "out")
            stage_recorded.main(picks, out)
            staged = os.path.join(out, "Footsteps", "Gravel", "SW_Footsteps_Gravel_R01.wav")
            x, r, ch = read(staged)
            self.assertEqual((r, ch), (rate, 1))
            self.assertAlmostEqual(np.max(np.abs(x)), 10 ** (-6 / 20), places=2)
            # Leading silence trimmed to a few ms.
            self.assertLess(np.argmax(np.abs(x) > 0.05), int(0.01 * rate))


if __name__ == "__main__":
    unittest.main()
