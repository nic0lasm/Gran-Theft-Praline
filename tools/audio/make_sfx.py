"""Synthesise the first footstep and impact banks of the game (original, MIT).

No recording is used: every sound is built from noise bursts, grains and
damped resonant modes, so the files carry no third-party licence. They are a
working base until recorded sounds (Sonniss GDC bundle, CC0 packs) replace
them, folder by folder, with the same layout (docs/audio/audio-design.md).

Layout written under OUT (mirrors /Game/Sound/SFX in Unreal):
  Footsteps/<Surface>/SW_Step_<Surface>_NN.wav     8 per surface
  Land/<Surface>/SW_Land_<Surface>_NN.wav          3 per surface
  Impacts/<Kind>/<Tier>/SW_Impact_<Kind>_<Tier>_NN.wav   3 per tier
  Impacts/Glass/SW_Impact_Glass_NN.wav             4
Surfaces and kinds use the names of EGTPSurface and EGTPImpactKind.

Usage: python3 -I tools/audio/make_sfx.py OUT [--seed 11]
Needs numpy. Output: 48 kHz, 16-bit mono WAV, about 9 MB in all.
"""

import argparse
import os
import wave

import numpy as np

RATE = 48000

SURFACES = ["Asphalt", "Stone", "Cobble", "Gravel", "Grass", "Metal", "Wood", "Water"]
KINDS = ["Car", "Metal", "Wall", "Body", "Wood"]
TIERS = ["Light", "Medium", "Heavy"]
STEPS_PER_SURFACE = 8
LANDS_PER_SURFACE = 3
IMPACTS_PER_TIER = 3
GLASS_COUNT = 4

# Peak level of each family (dBFS): the runtime volumes then set the mix.
PEAK_DB = {"step": -6.0, "land": -4.0, "impact": -1.5, "glass": -3.0}


# --- building blocks -------------------------------------------------------

def seconds(n):
    return int(round(n * RATE))


def bandpass_noise(rng, n, lo, hi):
    """White noise limited to [lo, hi] Hz (FFT mask with soft edges)."""
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / RATE)
    f = np.maximum(f, 1.0)
    mask = 1.0 / (1.0 + (lo / f) ** 4) / (1.0 + (f / hi) ** 4)
    out = np.fft.irfft(spec * np.sqrt(mask), n)
    return out / (np.std(out) + 1e-12)


def env_exp(n, attack, decay):
    """Fast linear attack, exponential decay (seconds)."""
    t = np.arange(n) / RATE
    a = np.clip(t / max(attack, 1e-4), 0.0, 1.0)
    return a * np.exp(-np.maximum(t - attack, 0.0) / decay)


def modes(rng, n, freqs, decays, amps, spread=0.03):
    """Sum of damped sines: the ringing of a plate, a pole, a board."""
    t = np.arange(n) / RATE
    out = np.zeros(n)
    for f, d, a in zip(freqs, decays, amps):
        f = f * (1.0 + rng.uniform(-spread, spread))
        out += a * np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi)) * np.exp(-t / d)
    return out


def grains(rng, n, count, lo, hi, grain_ms=(0.4, 2.0), shape=None):
    """Many tiny clicks: gravel crunch, debris, glass tinkle."""
    out = np.zeros(n)
    if shape is None:
        shape = np.ones(n)
    p = shape / shape.sum()
    starts = rng.choice(n, size=count, p=p)
    for s in starts:
        g = seconds(rng.uniform(*grain_ms) / 1000.0)
        g = max(g, 8)
        if s + g >= n:
            continue
        burst = bandpass_noise(rng, g, lo, hi) * np.hanning(g) * rng.uniform(0.2, 1.0)
        out[s:s + g] += burst
    return out


def thump(rng, n, f0, decay, attack=0.002):
    """Low body of an impact: a pitch-dropping sine plus a little low noise."""
    t = np.arange(n) / RATE
    f = f0 * (1.0 + 0.6 * np.exp(-t / 0.01))
    phase = 2 * np.pi * np.cumsum(f) / RATE
    body = np.sin(phase) * env_exp(n, attack, decay)
    body += 0.3 * bandpass_noise(rng, n, 40, f0 * 4) * env_exp(n, attack, decay * 0.6)
    return body


def place(buf, signal, at):
    s = seconds(at)
    e = min(len(buf), s + len(signal))
    if s < len(buf):
        buf[s:e] += signal[: e - s]


def finish(x, peak_db):
    """Remove DC, fade the tail out, normalise to a peak level."""
    x = x - np.mean(x)
    fade = min(len(x) // 4, seconds(0.02))
    if fade > 0:
        x[-fade:] *= np.linspace(1.0, 0.0, fade)
    x[: seconds(0.001)] *= np.linspace(0.0, 1.0, seconds(0.001))
    peak = np.max(np.abs(x)) + 1e-12
    return x / peak * 10 ** (peak_db / 20.0)


# --- footsteps ---------------------------------------------------------------

def strike(rng, surface, weight):
    """One contact (heel or toe) on a surface. weight ~ 1 for a heel, 0.5 for a toe."""
    n = seconds(0.35)
    x = np.zeros(n)
    w = weight
    if surface == "Asphalt":
        x += w * thump(rng, n, rng.uniform(95, 130), 0.018)
        x += 0.35 * w * bandpass_noise(rng, n, 500, 3500) * env_exp(n, 0.001, 0.012)
        x += 0.12 * grains(rng, n, 25, 2500, 9000, shape=env_exp(n, 0.002, 0.03))
    elif surface == "Stone":
        x += 0.7 * w * thump(rng, n, rng.uniform(140, 190), 0.012)
        x += 0.8 * w * bandpass_noise(rng, n, 900, 6500) * env_exp(n, 0.0005, 0.008)
        x += 0.25 * w * modes(rng, n, [1750, 2900, 4300], [0.02, 0.012, 0.008], [1.0, 0.6, 0.3])
    elif surface == "Cobble":
        x += 0.7 * w * thump(rng, n, rng.uniform(120, 170), 0.014)
        x += 0.7 * w * bandpass_noise(rng, n, 800, 5500) * env_exp(n, 0.0005, 0.01)
        # The sole lands on two stones a few ms apart.
        late = bandpass_noise(rng, n, 1200, 6000) * env_exp(n, 0.0005, 0.007) * 0.5 * w
        place(x, late, rng.uniform(0.006, 0.014))
        x += 0.25 * grains(rng, n, 40, 2000, 8000, shape=env_exp(n, 0.003, 0.05))
    elif surface == "Gravel":
        x += 0.35 * w * thump(rng, n, rng.uniform(90, 120), 0.02)
        shape = env_exp(n, rng.uniform(0.004, 0.01), rng.uniform(0.05, 0.08))
        x += 1.0 * w * grains(rng, n, int(260 * w) + 60, 1500, 9000, shape=shape)
    elif surface == "Grass":
        x += 0.25 * w * thump(rng, n, rng.uniform(70, 100), 0.03)
        x += 0.6 * w * bandpass_noise(rng, n, 300, 3500) * env_exp(n, 0.015, 0.06)
        x += 0.2 * grains(rng, n, 40, 2500, 7000, shape=env_exp(n, 0.01, 0.06))
    elif surface == "Metal":
        x += 0.6 * w * thump(rng, n, rng.uniform(110, 150), 0.02)
        x += 0.5 * w * bandpass_noise(rng, n, 1500, 8000) * env_exp(n, 0.0005, 0.006)
        x += 0.55 * w * modes(rng, n, [420, 1130, 2210, 3480], [0.16, 0.11, 0.07, 0.05], [1.0, 0.7, 0.45, 0.25])
    elif surface == "Wood":
        x += 0.7 * w * thump(rng, n, rng.uniform(110, 150), 0.03)
        x += 0.5 * w * modes(rng, n, [190, 420, 790, 1350], [0.06, 0.04, 0.03, 0.02], [1.0, 0.8, 0.5, 0.3], spread=0.06)
        x += 0.3 * w * bandpass_noise(rng, n, 700, 4000) * env_exp(n, 0.0007, 0.01)
    elif surface == "Water":
        x += 0.3 * w * thump(rng, n, rng.uniform(80, 110), 0.025)
        x += 0.7 * w * bandpass_noise(rng, n, 500, 5000) * env_exp(n, 0.004, 0.05)
        t = np.arange(n) / RATE
        for _ in range(rng.integers(3, 7)):
            f0 = rng.uniform(700, 1500)
            at = rng.uniform(0.005, 0.08)
            k = np.clip(t - at, 0, None)
            bubble = np.sin(2 * np.pi * (f0 * k + 900 * k * k)) * np.exp(-k / 0.012) * (t >= at)
            x += 0.25 * w * bubble
    return x


def footstep(rng, surface, landing=False):
    length = 0.6 if landing else 0.4
    n = seconds(length)
    x = np.zeros(n)
    if landing:
        # Both feet, a heavy body thud first.
        x[: seconds(0.35)] += 1.0 * thump(rng, seconds(0.35), rng.uniform(55, 75), 0.06)
        place(x, strike(rng, surface, 1.3), 0.0)
        place(x, strike(rng, surface, 1.0), rng.uniform(0.02, 0.05))
    else:
        place(x, strike(rng, surface, 1.0), 0.0)
        # Toe roll after the heel.
        place(x, strike(rng, surface, rng.uniform(0.35, 0.6)), rng.uniform(0.05, 0.09))
        # Shoe scuff, sometimes.
        if rng.random() < 0.5 and surface not in ("Grass", "Water"):
            scuff = bandpass_noise(rng, seconds(0.08), 1500, 7000) * np.hanning(seconds(0.08)) * 0.12
            place(x, scuff, rng.uniform(0.09, 0.14))
    return x


# --- impacts -----------------------------------------------------------------

def sheet_metal(rng, n, strength):
    freqs = sorted(rng.uniform(250, 3200, 9))
    decays = rng.uniform(0.05, 0.25, 9) * (0.6 + 0.6 * strength)
    amps = rng.uniform(0.2, 1.0, 9)
    return modes(rng, n, freqs, decays, amps, spread=0.0)


def crunch(rng, n, strength, length):
    shape = env_exp(n, 0.002, length)
    return grains(rng, n, int(400 * strength) + 50, 600, 7000, grain_ms=(0.5, 4.0), shape=shape)


def impact(rng, kind, tier):
    strength = {"Light": 0.35, "Medium": 0.7, "Heavy": 1.0}[tier]
    length = {"Light": 0.6, "Medium": 1.2, "Heavy": 2.0}[tier]
    n = seconds(length)
    x = np.zeros(n)
    if kind == "Car":
        x += 1.2 * thump(rng, n, rng.uniform(50, 75), 0.05 + 0.1 * strength)
        x += 0.6 * strength * sheet_metal(rng, n, strength)
        x += (0.3 + 0.9 * strength) * crunch(rng, n, strength, 0.05 + 0.25 * strength)
        if tier == "Heavy":
            # Debris falling a moment later.
            debris = grains(rng, n, 120, 1500, 9000, shape=env_exp(n, 0.2, 0.5))
            x += 0.35 * debris
    elif kind == "Metal":
        x += 0.9 * thump(rng, n, rng.uniform(60, 90), 0.06)
        ring = modes(rng, n, [rng.uniform(380, 520), rng.uniform(1000, 1300), rng.uniform(2100, 2600), rng.uniform(3400, 4200)],
                     [0.9 * strength + 0.2, 0.6 * strength + 0.15, 0.4, 0.25], [1.0, 0.7, 0.5, 0.3], spread=0.0)
        x += (0.4 + 0.6 * strength) * ring
        x += 0.4 * strength * crunch(rng, n, strength * 0.6, 0.08)
    elif kind == "Wall":
        x += 1.3 * thump(rng, n, rng.uniform(40, 60), 0.08 + 0.12 * strength)
        x += (0.4 + 0.8 * strength) * crunch(rng, n, strength, 0.06 + 0.2 * strength)
        x += 0.3 * strength * sheet_metal(rng, n, strength * 0.7)
        x += 0.3 * strength * grains(rng, n, 80, 300, 2500, grain_ms=(2, 8), shape=env_exp(n, 0.05, 0.3))
    elif kind == "Body":
        x += 1.2 * thump(rng, n, rng.uniform(70, 95), 0.05)
        x += 0.4 * bandpass_noise(rng, n, 150, 1200) * env_exp(n, 0.002, 0.04)
        if tier != "Light":
            # Rolling onto the bonnet.
            second = thump(rng, seconds(0.4), rng.uniform(80, 110), 0.05) * 0.8
            place(x, second, rng.uniform(0.18, 0.3))
            place(x, 0.3 * sheet_metal(rng, seconds(0.4), 0.4), rng.uniform(0.18, 0.3))
    elif kind == "Wood":
        x += 0.9 * thump(rng, n, rng.uniform(80, 120), 0.05)
        crack = bandpass_noise(rng, n, 1000, 8000) * env_exp(n, 0.0005, 0.01)
        x += 0.9 * crack
        x += 0.6 * modes(rng, n, [170, 380, 720, 1250], [0.08, 0.05, 0.04, 0.03], [1.0, 0.8, 0.5, 0.3], spread=0.08)
        x += 0.5 * strength * grains(rng, n, int(150 * strength), 800, 6000, grain_ms=(1, 5), shape=env_exp(n, 0.01, 0.15))
    return x


def glass(rng):
    n = seconds(1.6)
    x = bandpass_noise(rng, n, 2000, 12000) * env_exp(n, 0.001, 0.05) * 0.8
    t = np.arange(n) / RATE
    shape = env_exp(n, 0.01, 0.4)
    starts = rng.choice(n, size=90, p=shape / shape.sum())
    for s in starts:
        f = rng.uniform(2500, 9500)
        k = t[s:] - t[s]
        x[s:] += rng.uniform(0.05, 0.3) * np.sin(2 * np.pi * f * k) * np.exp(-k / rng.uniform(0.01, 0.06))
    return x


# --- output ------------------------------------------------------------------

def write_wav(path, x):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    data = (np.clip(x, -1.0, 1.0) * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())


def build(out, seed=11):
    """Writes every bank; returns the list of written paths."""
    rng = np.random.default_rng(seed)
    written = []
    for surface in SURFACES:
        for i in range(STEPS_PER_SURFACE):
            p = os.path.join(out, "Footsteps", surface, f"SW_Step_{surface}_{i + 1:02d}.wav")
            write_wav(p, finish(footstep(rng, surface), PEAK_DB["step"]))
            written.append(p)
        for i in range(LANDS_PER_SURFACE):
            p = os.path.join(out, "Land", surface, f"SW_Land_{surface}_{i + 1:02d}.wav")
            write_wav(p, finish(footstep(rng, surface, landing=True), PEAK_DB["land"]))
            written.append(p)
    for kind in KINDS:
        for tier in TIERS:
            for i in range(IMPACTS_PER_TIER):
                p = os.path.join(out, "Impacts", kind, tier, f"SW_Impact_{kind}_{tier}_{i + 1:02d}.wav")
                write_wav(p, finish(impact(rng, kind, tier), PEAK_DB["impact"]))
                written.append(p)
    for i in range(GLASS_COUNT):
        p = os.path.join(out, "Impacts", "Glass", f"SW_Impact_Glass_{i + 1:02d}.wav")
        write_wav(p, finish(glass(rng), PEAK_DB["glass"]))
        written.append(p)
    return written


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out")
    ap.add_argument("--seed", type=int, default=11)
    args = ap.parse_args()
    written = build(args.out, args.seed)
    size = sum(os.path.getsize(p) for p in written)
    print(f"{len(written)} files, {size / 1e6:.1f} MB under {args.out}")


if __name__ == "__main__":
    main()
