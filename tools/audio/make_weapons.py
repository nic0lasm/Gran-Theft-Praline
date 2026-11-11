"""Synthesise the weapon, bullet impact and melee banks of the combat (original, MIT).

Spec: docs/gameplay/combat.md, "Sounds". No recording: transients, noise
bands and damped modes, like make_sfx.py. A working base until recorded
sounds replace them, folder by folder (docs/audio/audio-design.md).

Layout written under OUT (mirrors /Game/Sound/SFX in Unreal, read by UGTPAudioMix):
  Weapons/<ShotClose|ShotFar|Tail|DryFire|MagOut|MagIn|Slide|Whizz>/SW_Weapon_<Name>_NN.wav
  BulletImpacts/<Dust|Sparks|Car|Glass|Wood|Dirt|Water|Flesh>/SW_Bullet_<Kind>_NN.wav
  Melee/<Light|Heavy|Block|Swish>/SW_Melee_<Name>_NN.wav
Names are those of EGTPWeaponSound, EGTPBulletImpact and EGTPMeleeSound.

Usage: python3 -I tools/audio/make_weapons.py OUT [--seed 13]
Needs numpy. Output: 48 kHz, 16-bit mono WAV.
"""

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_sfx import (RATE, bandpass_noise, env_exp, finish, grains, modes, place,  # noqa: E402
                      seconds, sheet_metal, thump, write_wav)

WEAPON_COUNTS = {"ShotClose": 4, "ShotFar": 4, "Tail": 3, "DryFire": 2, "MagOut": 2, "MagIn": 2, "Slide": 2, "Whizz": 3}
IMPACT_COUNTS = {k: 3 for k in ["Dust", "Sparks", "Car", "Glass", "Wood", "Dirt", "Water", "Flesh"]}
MELEE_COUNTS = {"Light": 3, "Heavy": 3, "Block": 3, "Swish": 3}
PEAK_DB = {"shot": -1.0, "tail": -6.0, "mech": -9.0, "impact": -4.0, "whizz": -6.0, "melee": -4.0}


def lowpass(x, cut):
    spec = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / RATE)
    return np.fft.irfft(spec / (1.0 + (f / cut) ** 4), len(x))


def click(rng, n, lo=2500, hi=9000, decay=0.003, ring=(3200, 5100, 7300)):
    """A small metal click: a noise tick plus a few short high modes."""
    x = bandpass_noise(rng, n, lo, hi) * env_exp(n, 0.0002, decay)
    x += 0.5 * modes(rng, n, list(ring), [0.012, 0.008, 0.005], [1.0, 0.6, 0.3], spread=0.05)
    return x


# --- weapons -----------------------------------------------------------------

def shot_close(rng):
    """Pistol heard close: a dry blast (very short), chest thump and a mechanical slide tick."""
    n = seconds(0.45)
    x = np.zeros(n)
    x += 1.4 * bandpass_noise(rng, n, 200, 14000) * env_exp(n, 0.0001, 0.004)
    x += 0.9 * bandpass_noise(rng, n, 900, 4500) * env_exp(n, 0.0002, 0.018)
    x += 1.1 * thump(rng, n, rng.uniform(95, 130), 0.035, attack=0.0005)
    x += 0.35 * lowpass(bandpass_noise(rng, n, 80, 2000), 1200) * env_exp(n, 0.002, 0.09)
    place(x, 0.25 * click(rng, seconds(0.05)), rng.uniform(0.025, 0.04))
    return x


def shot_far(rng):
    """Pistol heard from afar: the supersonic crack, then a dull boom and a rolling low tail."""
    n = seconds(1.6)
    x = np.zeros(n)
    x += 0.9 * bandpass_noise(rng, n, 2000, 8000) * env_exp(n, 0.0002, 0.003)
    boom = lowpass(bandpass_noise(rng, n, 40, 900), 600) * env_exp(n, 0.004, 0.12)
    place(x, 1.0 * boom, rng.uniform(0.01, 0.03))
    roll = lowpass(bandpass_noise(rng, n, 40, 700), 400) * env_exp(n, 0.06, 0.5)
    place(x, 0.5 * roll, 0.05)
    return x


def tail(rng):
    """Street reflections: early echoes from the facades, then a diffuse decay."""
    n = seconds(1.8)
    x = np.zeros(n)
    t = 0.0
    for k in range(rng.integers(5, 9)):
        t += rng.uniform(0.02, 0.09)
        echo = bandpass_noise(rng, seconds(0.12), 150, 3000) * env_exp(seconds(0.12), 0.002, 0.03)
        place(x, echo * (0.8 ** k), t)
    x += 0.35 * lowpass(bandpass_noise(rng, n, 60, 2500), 1500) * env_exp(n, 0.03, 0.45)
    return x


def dry_fire(rng):
    n = seconds(0.12)
    x = click(rng, n, decay=0.002)
    place(x, 0.4 * click(rng, seconds(0.04), decay=0.0015), 0.012)
    return x


def mag_out(rng):
    n = seconds(0.35)
    x = np.zeros(n)
    place(x, 0.8 * click(rng, seconds(0.05), 1500, 7000, ring=(1800, 3300, 5200)), 0.0)
    slide = bandpass_noise(rng, seconds(0.14), 1200, 6000) * np.hanning(seconds(0.14)) * 0.35
    place(x, slide, 0.03)
    return x


def mag_in(rng):
    n = seconds(0.3)
    x = np.zeros(n)
    slide = bandpass_noise(rng, seconds(0.1), 1200, 6000) * np.hanning(seconds(0.1)) * 0.3
    place(x, slide, 0.0)
    place(x, 1.0 * click(rng, seconds(0.06), 1500, 8000, ring=(1500, 2900, 4600)), 0.09)
    place(x, 0.4 * thump(rng, seconds(0.06), 180, 0.01), 0.09)
    return x


def slide_rack(rng):
    n = seconds(0.4)
    x = np.zeros(n)
    for at, gain in ((0.0, 0.8), (rng.uniform(0.14, 0.2), 1.0)):
        stroke = bandpass_noise(rng, seconds(0.06), 1500, 7000) * np.hanning(seconds(0.06)) * 0.4
        place(x, stroke, at)
        place(x, gain * click(rng, seconds(0.05), 2000, 9000, ring=(2400, 4100, 6100)), at + 0.05)
    return x


def whizz(rng):
    """Near miss: a short hiss that falls in pitch as the bullet passes (Doppler)."""
    length = rng.uniform(0.18, 0.28)
    n = seconds(length)
    t = np.arange(n) / RATE
    centre = 4500 - 3000 * (t / length)
    src = bandpass_noise(rng, n, 1500, 9000)
    # A moving resonance: a sum of short windows filtered around the falling centre.
    out = np.zeros(n)
    win = seconds(0.02)
    for s in range(0, n - win, win // 2):
        c = centre[s + win // 2]
        seg = src[s:s + win]
        spec = np.fft.rfft(seg)
        f = np.fft.rfftfreq(win, 1 / RATE)
        spec /= 1.0 + ((f - c) / (0.25 * c)) ** 2
        out[s:s + win] += np.fft.irfft(spec, win) * np.hanning(win)
    env = np.exp(-((t - length * 0.45) / (length * 0.22)) ** 2)
    return out * env


# --- bullet impacts ------------------------------------------------------------

def bullet_impact(rng, kind):
    n = seconds(0.6)
    x = np.zeros(n)
    snap = bandpass_noise(rng, n, 1500, 10000) * env_exp(n, 0.0002, 0.004)
    if kind == "Dust":  # concrete, stone, plaster
        x += 1.0 * snap
        x += 0.6 * thump(rng, n, rng.uniform(140, 200), 0.015)
        x += 0.8 * grains(rng, n, 160, 1200, 8000, shape=env_exp(n, 0.003, 0.08))
    elif kind == "Sparks":  # steel: a ping, sometimes a ricochet
        x += 0.8 * snap
        x += 0.9 * modes(rng, n, list(rng.uniform([1800, 3200, 5100], [2400, 4200, 6600])), [0.12, 0.08, 0.05], [1.0, 0.7, 0.4], spread=0.0)
        if rng.random() < 0.6:
            rico = whizz(rng)
            place(x, 0.5 * rico, rng.uniform(0.02, 0.05))
    elif kind == "Car":  # body panel
        x += 0.8 * snap
        x += 0.9 * thump(rng, n, rng.uniform(90, 130), 0.03)
        x += 0.6 * sheet_metal(rng, n, 0.5)
    elif kind == "Glass":
        x += 1.0 * bandpass_noise(rng, n, 2500, 12000) * env_exp(n, 0.0005, 0.02)
        t = np.arange(n) / RATE
        for s in rng.choice(n // 2, size=40):
            k = t[s:] - t[s]
            x[s:] += rng.uniform(0.05, 0.25) * np.sin(2 * np.pi * rng.uniform(3000, 9000) * k) * np.exp(-k / rng.uniform(0.008, 0.04))
    elif kind == "Wood":
        x += 0.9 * snap
        x += 0.8 * modes(rng, n, [220, 510, 930], [0.05, 0.03, 0.02], [1.0, 0.7, 0.4], spread=0.08)
        x += 0.5 * grains(rng, n, 70, 800, 6000, grain_ms=(1, 4), shape=env_exp(n, 0.005, 0.06))
    elif kind == "Dirt":  # earth, grass, sand
        x += 0.3 * snap
        x += 0.9 * thump(rng, n, rng.uniform(70, 100), 0.03)
        x += 0.6 * grains(rng, n, 120, 400, 4000, grain_ms=(1, 5), shape=env_exp(n, 0.005, 0.12))
    elif kind == "Water":
        x += 0.5 * bandpass_noise(rng, n, 400, 6000) * env_exp(n, 0.002, 0.06)
        t = np.arange(n) / RATE
        for _ in range(rng.integers(4, 8)):
            f0, at = rng.uniform(600, 1600), rng.uniform(0.01, 0.12)
            k = np.clip(t - at, 0, None)
            x += 0.3 * np.sin(2 * np.pi * (f0 * k + 1200 * k * k)) * np.exp(-k / 0.015) * (t >= at)
    elif kind == "Flesh":
        x += 1.0 * thump(rng, n, rng.uniform(60, 85), 0.04)
        x += 0.6 * lowpass(bandpass_noise(rng, n, 100, 2500), 1200) * env_exp(n, 0.001, 0.03)
        x += 0.15 * snap
    return x


# --- melee -------------------------------------------------------------------

def swish(rng, length=None):
    length = length or rng.uniform(0.2, 0.3)
    n = seconds(length)
    t = np.arange(n) / RATE
    x = bandpass_noise(rng, n, 300, 3500) * np.sin(np.pi * t / length) ** 2
    return lowpass(x, 2500)


def melee(rng, kind):
    n = seconds(0.5)
    x = np.zeros(n)
    if kind == "Swish":
        return swish(rng)
    pre = swish(rng, 0.12) * 0.35
    place(x, pre, 0.0)
    at = 0.1
    if kind == "Light":
        hit = 1.0 * thump(rng, seconds(0.3), rng.uniform(110, 150), 0.025) + 0.7 * bandpass_noise(rng, seconds(0.3), 800, 5000) * env_exp(seconds(0.3), 0.0003, 0.008)
    elif kind == "Heavy":
        hit = 1.3 * thump(rng, seconds(0.4), rng.uniform(70, 95), 0.05) + 0.8 * bandpass_noise(rng, seconds(0.4), 500, 4000) * env_exp(seconds(0.4), 0.0003, 0.015)
    else:  # Block: forearms, muffled, cloth
        hit = 0.8 * thump(rng, seconds(0.3), rng.uniform(140, 190), 0.02) + 0.4 * bandpass_noise(rng, seconds(0.3), 400, 2500) * env_exp(seconds(0.3), 0.002, 0.04)
    place(x, hit, at)
    return x


# --- output ------------------------------------------------------------------

def build(out, seed=13):
    rng = np.random.default_rng(seed)
    written = []
    weapon = {"ShotClose": (shot_close, "shot"), "ShotFar": (shot_far, "shot"), "Tail": (tail, "tail"),
              "DryFire": (dry_fire, "mech"), "MagOut": (mag_out, "mech"), "MagIn": (mag_in, "mech"),
              "Slide": (slide_rack, "mech"), "Whizz": (whizz, "whizz")}
    for name, count in WEAPON_COUNTS.items():
        make, level = weapon[name]
        for i in range(count):
            p = os.path.join(out, "Weapons", name, f"SW_Weapon_{name}_{i + 1:02d}.wav")
            write_wav(p, finish(make(rng), PEAK_DB[level]))
            written.append(p)
    for kind, count in IMPACT_COUNTS.items():
        for i in range(count):
            p = os.path.join(out, "BulletImpacts", kind, f"SW_Bullet_{kind}_{i + 1:02d}.wav")
            write_wav(p, finish(bullet_impact(rng, kind), PEAK_DB["impact"]))
            written.append(p)
    for kind, count in MELEE_COUNTS.items():
        for i in range(count):
            p = os.path.join(out, "Melee", kind, f"SW_Melee_{kind}_{i + 1:02d}.wav")
            write_wav(p, finish(melee(rng, kind), PEAK_DB["melee"]))
            written.append(p)
    return written


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out")
    ap.add_argument("--seed", type=int, default=13)
    args = ap.parse_args()
    written = build(args.out, args.seed)
    size = sum(os.path.getsize(p) for p in written)
    print(f"{len(written)} files, {size / 1e6:.1f} MB under {args.out}")


if __name__ == "__main__":
    main()
