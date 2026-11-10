"""Checks the siren and tyre squeal rendered by audio_logic_test.

- siren_modes.wav: silent at first, then the « deux tons » alternate between
  435 and 580 Hz every 0.55 s, then the yelp sweeps 650-1450 Hz, then silence;
- siren_pass.wav: pitch higher than 580 Hz while approaching, lower after;
- skid_ramp.wav: louder as the slide grows, silent when it stops;
- no NaN, no clipping, no click in any file.
Usage: python3 -I check_synths.py <folder>
"""
import sys
import wave
from pathlib import Path

import numpy as np


def read(path):
    with wave.open(str(path)) as w:
        data = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16)
        return data.astype(np.float64) / 32767.0, w.getframerate()


def pitch(x, rate, lo, hi):
    """Strongest frequency in [lo, hi] of a slice."""
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)), 1 << 16))
    freqs = np.fft.rfftfreq(1 << 16, 1 / rate)
    band = (freqs >= lo) & (freqs <= hi)
    return freqs[band][np.argmax(spec[band])]


def rms(x):
    return float(np.sqrt(np.mean(x ** 2)))


def common(name, x, failures):
    if not np.all(np.isfinite(x)):
        failures.append(f"{name}: NaN")
    if np.max(np.abs(x)) >= 0.999:
        failures.append(f"{name}: clipping")


def main(folder):
    folder = Path(folder)
    failures = []

    x, rate = read(folder / "siren_modes.wav")
    common("siren_modes", x, failures)
    if rms(x[: int(0.4 * rate)]) > 1e-3:
        failures.append("siren: not silent before it starts")
    tones = []
    for k in range(4):
        start = 0.5 + 0.55 * k + 0.1
        seg = x[int(start * rate): int((start + 0.35) * rate)]
        tones.append(pitch(seg, rate, 300, 700))
    print("two-tone pitches (Hz):", [round(t) for t in tones])
    for k, t in enumerate(tones):
        goal = 435 if k % 2 == 0 else 580
        if abs(t - goal) > 8:
            failures.append(f"siren tone {k}: {t:.0f} Hz instead of {goal}")
    yelp = x[int(3.6 * rate): int(5.4 * rate)]
    win = int(0.02 * rate)
    peaks = [pitch(yelp[i: i + win], rate, 500, 1600) for i in range(0, len(yelp) - win, win)]
    print(f"yelp range: {min(peaks):.0f}-{max(peaks):.0f} Hz")
    if min(peaks) > 760 or max(peaks) < 1300:
        failures.append("yelp does not sweep 650-1450 Hz")
    if rms(x[int(5.8 * rate):]) > 2e-3:
        failures.append("siren: not silent after Off")
    steps = np.abs(np.diff(x))
    print(f"siren max sample step {steps.max():.3f}")
    # A band-limited square edge moves fast; a click would be a full-scale jump.
    if steps.max() > 0.9:
        failures.append("siren: click")

    x, rate = read(folder / "siren_pass.wav")
    common("siren_pass", x, failures)
    before = pitch(x[int(1.0 * rate): int(1.5 * rate)], rate, 400, 700)
    after = pitch(x[int(4.5 * rate): int(5.0 * rate)], rate, 380, 700)
    print(f"Doppler: {before:.0f} Hz approaching, {after:.0f} Hz leaving")
    # 15 m/s: x1.046 approaching, x0.958 leaving, on whichever tone the slice holds.
    near = lambda f, goals: min(abs(f - g) for g in goals) < 6
    if not near(before, (435 * 1.046, 580 * 1.046)) or not near(after, (435 * 0.958, 580 * 0.958)):
        failures.append("Doppler shift missing")

    x, rate = read(folder / "skid_ramp.wav")
    common("skid_ramp", x, failures)
    levels = [rms(x[int(t * rate): int((t + 0.5) * rate)]) for t in (0.2, 1.0, 1.8, 2.4)]
    print("skid levels:", [round(v, 4) for v in levels])
    if not all(a < b for a, b in zip(levels, levels[1:])):
        failures.append("skid does not grow with the slide")
    if rms(x[int(3.6 * rate):]) > 2e-3:
        failures.append("skid: not silent after the slide")

    for f in failures:
        print("FAIL", f)
    if failures:
        sys.exit(1)
    print("synths: all checks passed")


main(sys.argv[1])
