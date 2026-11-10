"""Checks the procedural engine sound rendered by engine_synth_check.

For each steady point: the strongest low peak must be the firing frequency
(rpm / 60 * cylinders / 2) or one of its first harmonics, the level must
rise with load, and no sample may clip or be NaN. The drive demo must have
no click (sample-to-sample jump) larger than the steady sounds have.
Usage: python3 check_engine_synth.py <dir with the rendered files>
"""
import csv
import sys
import wave
from pathlib import Path

import numpy as np

CYLINDERS = 3


def read(path):
    with wave.open(str(path)) as w:
        data = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16)
        return data.astype(np.float64) / 32767.0, w.getframerate()


def main(folder):
    folder = Path(folder)
    failures = []
    rms_by_point = {}
    max_step = 0.0
    with open(folder / "steady.csv") as f:
        for row in csv.DictReader(f):
            rpm, load = float(row["rpm"]), float(row["load"])
            x, rate = read(folder / row["file"])
            x = x[rate // 2:]  # skip the settling time
            firing = rpm / 60 * CYLINDERS / 2
            spec = np.abs(np.fft.rfft(x * np.hanning(len(x))))
            freqs = np.fft.rfftfreq(len(x), 1 / rate)
            band = (freqs > firing * 0.5) & (freqs < firing * 4.5)
            peak = freqs[band][np.argmax(spec[band])]
            ratio = peak / firing
            harmonic = round(ratio)
            rms = float(np.sqrt(np.mean(x ** 2)))
            peak_abs = float(np.max(np.abs(x)))
            max_step = max(max_step, float(np.max(np.abs(np.diff(x)))))
            rms_by_point[(rpm, load)] = rms
            ok = abs(ratio - harmonic) < 0.03 and 1 <= harmonic <= 4
            print(f"{rpm:5.0f} rpm load {load:.1f}: firing {firing:6.1f} Hz, peak {peak:6.1f} Hz (x{ratio:.2f}), rms {rms:.3f}, peak {peak_abs:.3f}")
            if not ok:
                failures.append(f"{rpm} rpm: spectral peak {peak:.1f} Hz is not a firing harmonic")
            if not np.all(np.isfinite(x)) or peak_abs >= 0.999:
                failures.append(f"{rpm} rpm: clipped or invalid samples")
            if rms < 0.02:
                failures.append(f"{rpm} rpm: too quiet ({rms:.3f})")
    if rms_by_point[(3000, 1)] <= rms_by_point[(3000, 0)] * 1.3:
        failures.append("load does not make the engine louder at 3000 rpm")
    x, rate = read(folder / "drive_demo.wav")
    drive_step = float(np.max(np.abs(np.diff(x))))
    print(f"drive demo: max step {drive_step:.3f} (steady max {max_step:.3f}), peak {np.max(np.abs(x)):.3f}")
    if drive_step > max_step * 1.5 + 0.02:
        failures.append("click in the drive demo (larger jump than any steady sound)")
    if failures:
        print("FAIL")
        for line in failures:
            print(" -", line)
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
