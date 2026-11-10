"""Synthesise the zone, night and weather ambience loops (original, MIT).

Same method as make_street_ambience.py (filtered noise, simple engine and
water models, every layer wrapped around the loop length so it loops without
a seam), one stereo loop per layer of the ambience bed (UGTPAmbienceBed):

  SW_Amb_Pedestrian  rue de la République: crowd voices, many steps, sparrows,
                     traffic only far away
  SW_Amb_Quay        quai du Rhône: river flow and lapping, wind, busy traffic
                     on the quay, gulls
  SW_Amb_Bridge      pont Lafayette: stronger wind, cars close by with the
                     thump of the expansion joints, the river below
  SW_Amb_Night       the street at night: city hum, rare cars, a few voices
  SW_Amb_Rain        rain on the street: hiss, drops, gutters
  SW_Amb_RainCar     rain drumming on the car roof, heard from inside
  SW_Amb_Crowd       murmur of a group of people, faded in near pedestrians

Every loop is levelled to the same loudness (RMS), so the bed weights in
Config/DefaultGame.ini are the mix. The daytime street loop stays the one of
make_street_ambience.py.

Usage: python3 -I tools/audio/make_ambiences.py OUT_DIR [--seconds 60] [--seed 5] [--only NAME]
Needs numpy. Output: 48 kHz, 16-bit stereo WAV, about 11 MB per minute.
"""

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_street_ambience import (  # noqa: E402
    RATE, add_wrapped, band, chirp, pass_by, shaped_noise, slow_wobble, write_wav)

LAYERS = ["Pedestrian", "Quay", "Bridge", "Night", "Rain", "RainCar", "Crowd"]
TARGET_RMS_DB = -24.0


def stereo_noise(rng, n, shape, width=0.5):
    """Two partly decorrelated channels of the same coloured noise."""
    common = shaped_noise(rng, n, shape)
    return np.stack([(1 - width) * common + width * shaped_noise(rng, n, shape) for _ in range(2)], axis=1)


def murmur(rng, n, seconds, level, rate=(2.0, 7.0)):
    out = np.zeros((n, 2))
    for ch in range(2):
        voice = shaped_noise(rng, n, band(220.0, 2800.0, 2.0))
        syllables = shaped_noise(rng, n, band(*rate, 1.0))
        out[:, ch] = level * voice * np.clip(0.6 + 0.5 * syllables, 0.0, None) * slow_wobble(rng, n, seconds, 0.5, (2, 3, 7))
    return out


def steps(rng, n, count, level):
    """Distant footsteps of other people: short soft clicks, panned."""
    out = np.zeros((n, 2))
    for _ in range(count):
        g = int(0.03 * RATE)
        click = shaped_noise(rng, g, band(400.0, 4000.0, 1.5)) * np.exp(-np.arange(g) / (0.006 * RATE))
        pan = rng.uniform(-0.9, 0.9)
        sig = np.stack([click * np.sqrt(0.5 * (1 - pan)), click * np.sqrt(0.5 * (1 + pan))], axis=1)
        add_wrapped(out, int(rng.uniform(0, n)), level * rng.uniform(0.3, 1.0) * sig)
    return out


def traffic(rng, n, seconds, gap, kinds, level=1.0, near=False):
    out = np.zeros((n, 2))
    clock = 0.0
    while clock < seconds:
        sig = pass_by(rng, rng.choice(kinds))
        if not near:
            # Far traffic: duller and quieter.
            for ch in range(2):
                spec = np.fft.rfft(sig[:, ch])
                f = np.fft.rfftfreq(len(sig), 1 / RATE)
                sig[:, ch] = np.fft.irfft(spec / (1 + (f / 900.0) ** 2), len(sig))
            sig *= 0.35
        add_wrapped(out, int(clock * RATE), level * sig)
        clock += rng.uniform(*gap)
    return out


def water(rng, n, seconds, level):
    """River: low roar, mid-band flow, and lapping against the quay."""
    out = level * 0.6 * stereo_noise(rng, n, band(60.0, 900.0, 1.0), 0.6) * slow_wobble(rng, n, seconds, 0.2)[:, None]
    flow = stereo_noise(rng, n, band(500.0, 3500.0, 1.5), 0.7)
    lap = np.clip(shaped_noise(rng, n, band(0.3, 1.2, 1.0)), 0.0, None)
    out += level * 0.35 * flow * lap[:, None]
    return out


def wind(rng, n, seconds, level):
    gust = np.clip(0.5 + 0.5 * shaped_noise(rng, n, band(0.05, 0.4, 1.0)), 0.05, None)
    return level * stereo_noise(rng, n, band(80.0, 1200.0, 1.2), 0.8) * gust[:, None]


def gull(rng):
    """A gull call: a few falling, nasal cries."""
    out = []
    for _ in range(rng.integers(2, 5)):
        dur = rng.uniform(0.18, 0.32)
        k = np.arange(int(dur * RATE)) / RATE
        f = rng.uniform(1300, 1700) * (1 - 0.35 * k / dur)
        phase = 2 * np.pi * np.cumsum(f) / RATE
        tone = np.sin(phase) + 0.5 * np.sin(2 * phase) + 0.3 * np.sin(3 * phase)
        out.append(tone * np.sin(np.pi * k / dur) ** 0.7)
        out.append(np.zeros(int(rng.uniform(0.05, 0.15) * RATE)))
    mono = np.concatenate(out) * rng.uniform(0.03, 0.06)
    pan = rng.uniform(-0.8, 0.8)
    return np.stack([mono * np.sqrt(0.5 * (1 - pan)), mono * np.sqrt(0.5 * (1 + pan))], axis=1)


def joint_thumps(rng, n, count, level):
    """Ka-thunk of a car crossing the bridge's expansion joint (two axles)."""
    out = np.zeros((n, 2))
    for _ in range(count):
        g = int(0.25 * RATE)
        k = np.arange(g) / RATE
        thump = np.sin(2 * np.pi * rng.uniform(45, 70) * k) * np.exp(-k / 0.04)
        gap = int(rng.uniform(0.09, 0.16) * RATE)
        sig = np.zeros(g + gap)
        sig[:g] += thump
        sig[gap:gap + g] += 0.8 * thump
        pan = rng.uniform(-0.6, 0.6)
        add_wrapped(out, int(rng.uniform(0, n)),
                    level * np.stack([sig * np.sqrt(0.5 * (1 - pan)), sig * np.sqrt(0.5 * (1 + pan))], axis=1))
    return out


def rain(rng, n, seconds, level, roof=False):
    if roof:
        # Drumming on sheet metal heard inside: dense low thuds, little hiss.
        out = level * 0.5 * stereo_noise(rng, n, band(150.0, 1800.0, 1.5), 0.3)
        drops = np.zeros((n, 2))
        for _ in range(int(seconds * 140)):
            g = int(0.02 * RATE)
            k = np.arange(g) / RATE
            d = np.sin(2 * np.pi * rng.uniform(300, 900) * k) * np.exp(-k / 0.004)
            pan = rng.uniform(-0.7, 0.7)
            add_wrapped(drops, int(rng.uniform(0, n)),
                        rng.uniform(0.1, 0.4) * np.stack([d * np.sqrt(0.5 * (1 - pan)), d * np.sqrt(0.5 * (1 + pan))], axis=1))
        return out + level * drops
    out = level * stereo_noise(rng, n, band(800.0, 9000.0, 1.2), 0.9) * slow_wobble(rng, n, seconds, 0.15)[:, None]
    # Big drops on things nearby, and a gutter.
    for _ in range(int(seconds * 25)):
        g = int(0.015 * RATE)
        d = shaped_noise(rng, g, band(1500.0, 7000.0, 2.0)) * np.exp(-np.arange(g) / (0.003 * RATE))
        pan = rng.uniform(-1, 1)
        add_wrapped(out, int(rng.uniform(0, n)),
                    level * rng.uniform(0.1, 0.5) * np.stack([d * np.sqrt(0.5 * (1 - pan)), d * np.sqrt(0.5 * (1 + pan))], axis=1))
    gutter = shaped_noise(rng, n, band(300.0, 2500.0, 2.0)) * np.clip(shaped_noise(rng, n, band(3.0, 12.0, 1.0)), 0, None)
    out[:, 0] += level * 0.25 * gutter
    return out


def build(name, seconds, seed):
    rng = np.random.default_rng(seed + LAYERS.index(name) * 101)
    n = int(seconds * RATE)
    if name == "Pedestrian":
        mix = murmur(rng, n, seconds, 0.12) + steps(rng, n, int(seconds * 6), 0.08)
        mix += traffic(rng, n, seconds, (4.0, 10.0), ["car", "bus", "scooter"])
        mix += 0.02 * stereo_noise(rng, n, band(400.0, 6000.0, 1.0))
        for _ in range(int(seconds / 6)):
            add_wrapped(mix, int(rng.uniform(0, n)), chirp(rng))
    elif name == "Quay":
        mix = water(rng, n, seconds, 0.25) + wind(rng, n, seconds, 0.12)
        mix += traffic(rng, n, seconds, (0.8, 3.5), ["car", "car", "scooter", "bus"], 0.9, near=True)
        for _ in range(int(seconds / 15) + 1):
            add_wrapped(mix, int(rng.uniform(0, n)), gull(rng))
    elif name == "Bridge":
        mix = wind(rng, n, seconds, 0.25) + water(rng, n, seconds, 0.12)
        mix += traffic(rng, n, seconds, (1.0, 4.0), ["car", "car", "scooter", "bus"], 1.0, near=True)
        mix += joint_thumps(rng, n, int(seconds / 3), 0.5)
        for _ in range(int(seconds / 20) + 1):
            add_wrapped(mix, int(rng.uniform(0, n)), gull(rng))
    elif name == "Night":
        hum = stereo_noise(rng, n, lambda f: band(30.0, 400.0, 1.2)(f) / np.sqrt(np.maximum(f, 20.0) / 20.0), 0.5)
        mix = 0.3 * hum * slow_wobble(rng, n, seconds, 0.3)[:, None]
        mix += traffic(rng, n, seconds, (6.0, 18.0), ["car", "scooter"], 0.8)
        mix += murmur(rng, n, seconds, 0.03, (1.0, 4.0))
        mix += steps(rng, n, int(seconds * 0.8), 0.06)
    elif name == "Rain":
        mix = rain(rng, n, seconds, 0.2)
    elif name == "RainCar":
        mix = rain(rng, n, seconds, 0.3, roof=True)
    elif name == "Crowd":
        mix = murmur(rng, n, seconds, 0.2, (2.5, 8.0)) + steps(rng, n, int(seconds * 4), 0.06)
    else:
        raise ValueError(name)
    rms = np.sqrt(np.mean(mix ** 2)) + 1e-12
    mix *= 10 ** (TARGET_RMS_DB / 20.0) / rms
    peak = np.max(np.abs(mix))
    if peak > 0.89:  # keep 1 dB of headroom: soften the loudest transients
        mix = np.tanh(mix / 0.89) * 0.89
    return mix


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out")
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--seed", type=int, default=5)
    ap.add_argument("--only", choices=LAYERS)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    for name in ([args.only] if args.only else LAYERS):
        path = os.path.join(args.out, f"SW_Amb_{name}.wav")
        write_wav(path, build(name, args.seconds, args.seed))
        print(path)


if __name__ == "__main__":
    main()
