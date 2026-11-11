#!/usr/bin/env python3
"""Generate the place and mission sounds of version 0.9 (ADR-0009).

Synthesised from scratch, like make_sfx.py (no recording, no licence). Three groups,
one folder per name, imported under /Game/Sound/SFX by tools/audio/unreal/import_sounds.py:

  Interior/<Loop>/   seamless loops of an interior (room tone, fryer, grill, hood fan,
                     cold cabinet, shake machine, drink tower), played by UGTPPlaceAudio
                     around the emitters found in an interior level;
  Shop/<Sound>/      one-shots of a shop: till open and close, coins, notes, receipt
                     printer, card terminal beep, fryer timer, basket drop, order chime,
                     door chime, back-door alarm buzzer, safe door;
  Mission/<Sound>/   short 2D stings of the missions: available, start, objective,
                     checkpoint, success, fail, reward, countdown tick.

Usage: make_places.py <out folder> [--seed 17]
"""

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from make_sfx import (  # noqa: E402
    RATE,
    bandpass_noise,
    env_exp,
    finish,
    grains,
    modes,
    place,
    seconds,
    thump,
    write_wav,
)

# Names match the C++ enums EGTPRoomLoop, EGTPShopSound and EGTPMissionSound (GTPPlaceAudio.h).
LOOPS = ["RoomTone", "Fryer", "Grill", "Hood", "Cold", "Shake", "Drinks"]
SHOP = ["TillOpen", "TillClose", "Coins", "Notes", "Receipt", "CardBeep", "FryerTimer",
        "BasketDrop", "OrderChime", "DoorChime", "AlarmBuzzer", "Safe"]
MISSION = ["Available", "Start", "Objective", "Checkpoint", "Success", "Fail", "Reward", "Countdown"]

LOOP_SECONDS = 12.0
LOOP_FADE = 1.0
VARIANTS = {"Coins": 3, "Notes": 2, "CardBeep": 2, "BasketDrop": 2, "Countdown": 1}

PEAK_DB = {"loop": -14.0, "shop": -6.0, "mission": -8.0}


# --- helpers -----------------------------------------------------------------

def tone(n, freq, phase=0.0):
    t = np.arange(n) / RATE
    return np.sin(2 * np.pi * freq * t + phase)


def square(n, freq, softness=6):
    """Band-limited square wave (odd harmonics up to `softness`)."""
    out = np.zeros(n)
    for k in range(1, softness * 2, 2):
        if freq * k >= RATE / 2:
            break
        out += tone(n, freq * k) / k
    return out


def seamless(x, fade):
    """Folds the extra `fade` seconds at the end onto the start: the result loops without a click."""
    f = seconds(fade)
    body = x[:-f].copy()
    ramp = np.linspace(0.0, 1.0, f)
    body[:f] = body[:f] * ramp + x[-f:] * (1.0 - ramp)
    return body


def lowpass(x, cutoff):
    spec = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / RATE)
    return np.fft.irfft(spec / (1.0 + (f / cutoff) ** 4), len(x))


def slow_wobble(rng, n, rate, depth):
    """Smooth random amplitude movement around 1."""
    points = max(4, int(n / RATE * rate) + 2)
    knots = 1.0 + depth * rng.uniform(-1, 1, points)
    return np.interp(np.arange(n), np.linspace(0, n, points), knots)


def bell(rng, n, f0, decay=0.8):
    return modes(rng, n, [f0, f0 * 2.76, f0 * 5.4], [decay, decay * 0.5, decay * 0.25], [1.0, 0.45, 0.2], spread=0.0)


def pluck(n, freq, decay):
    t = np.arange(n) / RATE
    return (np.sin(2 * np.pi * freq * t) + 0.3 * np.sin(4 * np.pi * freq * t)) * np.exp(-t / decay)


# --- interior loops -------------------------------------------------------------

def loop(rng, name):
    n = seconds(LOOP_SECONDS + LOOP_FADE)
    if name == "RoomTone":
        # Ventilation: soft broadband air, a faint mains hum, very low far-off clatter.
        x = lowpass(bandpass_noise(rng, n, 50, 2500), 900) * slow_wobble(rng, n, 0.3, 0.1)
        x += 0.05 * (tone(n, 100) + 0.5 * tone(n, 200))
        x += 0.15 * lowpass(grains(rng, n, 40, 1500, 5000, grain_ms=(1, 4)), 2500)
    elif name == "Fryer":
        # Hot oil: a dense bed of tiny bursts, slowly breathing, and a few low bubbles.
        x = grains(rng, n, int(LOOP_SECONDS * 2600), 2500, 9000, grain_ms=(0.2, 0.9))
        x *= slow_wobble(rng, n, 1.5, 0.25)
        x += 0.3 * bandpass_noise(rng, n, 1800, 7000) * slow_wobble(rng, n, 2.0, 0.3)
        x += 0.6 * grains(rng, n, int(LOOP_SECONDS * 25), 250, 900, grain_ms=(4, 12))
    elif name == "Grill":
        # Flat-top grill: harder hiss and spitting fat.
        x = 0.7 * bandpass_noise(rng, n, 3000, 11000) * slow_wobble(rng, n, 1.0, 0.2)
        x += grains(rng, n, int(LOOP_SECONDS * 900), 2000, 10000, grain_ms=(0.3, 1.5))
        x += 0.8 * grains(rng, n, int(LOOP_SECONDS * 6), 1500, 6000, grain_ms=(6, 20))
    elif name == "Hood":
        # Extraction hood fan: air rush plus the blade tone and its harmonics.
        x = bandpass_noise(rng, n, 60, 900) * slow_wobble(rng, n, 0.5, 0.06)
        for k, a in [(1, 0.35), (2, 0.2), (3, 0.12), (6, 0.05)]:
            x += a * tone(n, 92.0 * k, rng.uniform(0, 6.28))
    elif name == "Cold":
        # Cold cabinet: compressor hum at mains harmonics, a light rattle.
        x = 0.25 * bandpass_noise(rng, n, 60, 600)
        for k, a in [(1, 0.5), (2, 0.35), (3, 0.2), (4, 0.1)]:
            x += a * tone(n, 50.0 * k, rng.uniform(0, 6.28))
        x += 0.25 * grains(rng, n, int(LOOP_SECONDS * 30), 800, 2400, grain_ms=(2, 5))
    elif name == "Shake":
        # Shake and ice-cream machine: motor whine and its gear noise, cycling slowly.
        x = 0.4 * bandpass_noise(rng, n, 200, 3000)
        x += 0.5 * square(n, 183.0, softness=4) * slow_wobble(rng, n, 0.25, 0.15)
        x += 0.15 * tone(n, 1100.0)
    elif name == "Drinks":
        # Drink tower: fizz, the odd pour and drip.
        x = 0.6 * grains(rng, n, int(LOOP_SECONDS * 500), 3000, 9000, grain_ms=(0.2, 0.6))
        for at in rng.uniform(0.5, LOOP_SECONDS - 2.5, 2):
            pour = bandpass_noise(rng, seconds(2.0), 300, 3500) * np.hanning(seconds(2.0))
            place(x, 0.8 * pour, at)
        for at in rng.uniform(0.2, LOOP_SECONDS, 6):
            place(x, 0.5 * pluck(seconds(0.08), rng.uniform(900, 1500), 0.015), at)
    else:
        raise ValueError(name)
    x = seamless(x, LOOP_FADE)
    x -= np.mean(x)
    return x / (np.max(np.abs(x)) + 1e-12) * 10 ** (PEAK_DB["loop"] / 20.0)


# --- shop one-shots ---------------------------------------------------------------

def shop(rng, name):
    if name == "TillOpen":
        n = seconds(1.2)
        x = thump(rng, n, 110, 0.05) * 0.8
        slide = bandpass_noise(rng, seconds(0.28), 400, 3500) * env_exp(seconds(0.28), 0.02, 0.12)
        place(x, 0.6 * slide, 0.02)
        place(x, 0.7 * grains(rng, seconds(0.3), 60, 3000, 9000), 0.22)
        place(x, 0.5 * bell(rng, seconds(0.9), 2100.0, 0.5), 0.05)
    elif name == "TillClose":
        n = seconds(0.6)
        x = np.zeros(n)
        slide = bandpass_noise(rng, seconds(0.18), 400, 3500) * env_exp(seconds(0.18), 0.01, 0.08)
        place(x, 0.6 * slide, 0.0)
        place(x, thump(rng, seconds(0.3), 150, 0.04), 0.16)
        place(x, 0.5 * modes(rng, seconds(0.3), [1800, 3900], [0.05, 0.03], [1, 0.5]), 0.16)
    elif name == "Coins":
        n = seconds(0.9)
        x = np.zeros(n)
        for at in np.sort(rng.uniform(0.0, 0.6, rng.integers(5, 10))):
            f0 = rng.uniform(3200, 6000)
            place(x, rng.uniform(0.4, 1.0) * modes(rng, seconds(0.25), [f0, f0 * 1.52, f0 * 2.3],
                                                    [0.08, 0.05, 0.03], [1, 0.6, 0.3]), at)
    elif name == "Notes":
        n = seconds(0.6)
        x = bandpass_noise(rng, n, 1500, 9000) * env_exp(n, 0.04, 0.2)
        x *= 0.6 + 0.4 * np.abs(np.sin(2 * np.pi * rng.uniform(14, 22) * np.arange(n) / RATE))
    elif name == "Receipt":
        n = seconds(1.6)
        x = np.zeros(n)
        body = bandpass_noise(rng, seconds(1.1), 1500, 5000)
        body *= 0.5 + 0.5 * np.sign(np.sin(2 * np.pi * 32 * np.arange(seconds(1.1)) / RATE))
        body *= np.hanning(seconds(1.1)) ** 0.3
        place(x, 0.5 * body, 0.0)
        place(x, bandpass_noise(rng, seconds(0.12), 1000, 8000) * env_exp(seconds(0.12), 0.003, 0.03), 1.3)
    elif name == "CardBeep":
        n = seconds(0.35)
        x = np.zeros(n)
        b = square(seconds(0.09), 2700.0, softness=3) * np.hanning(seconds(0.09)) ** 0.2
        place(x, b, 0.0)
        if rng.uniform() < 0.5:
            place(x, b, 0.14)
    elif name == "FryerTimer":
        n = seconds(2.4)
        x = np.zeros(n)
        b = square(seconds(0.07), 3150.0, softness=3) * np.hanning(seconds(0.07)) ** 0.2
        for group in range(3):
            for k in range(3):
                place(x, b, group * 0.8 + k * 0.12)
    elif name == "BasketDrop":
        n = seconds(1.8)
        x = 0.7 * modes(rng, n, [620, 1340, 2650, 4100], [0.12, 0.09, 0.06, 0.04], [1, 0.7, 0.5, 0.3])
        x *= env_exp(n, 0.002, 0.3)
        burst = grains(rng, seconds(1.6), 3000, 2500, 9000, grain_ms=(0.2, 0.8)) * env_exp(seconds(1.6), 0.05, 0.5)
        place(x, burst, 0.05)
    elif name == "OrderChime":
        n = seconds(1.6)
        x = np.zeros(n)
        place(x, bell(rng, seconds(1.2), 1318.5, 0.6), 0.0)
        place(x, bell(rng, seconds(1.2), 1046.5, 0.7), 0.35)
    elif name == "DoorChime":
        n = seconds(2.0)
        x = np.zeros(n)
        place(x, bell(rng, seconds(1.4), 880.0, 0.7), 0.0)
        place(x, bell(rng, seconds(1.4), 698.5, 0.8), 0.45)
    elif name == "AlarmBuzzer":
        n = seconds(3.0)
        # A harsh 880 Hz buzz, pulsed four times a second (finish() adds the fades).
        x = square(n, 880.0, softness=8) * (np.sin(2 * np.pi * 4 * np.arange(n) / RATE) > -0.2)
    elif name == "Safe":
        n = seconds(1.4)
        x = thump(rng, n, 70, 0.18)
        x += 0.5 * modes(rng, n, [240, 610, 1330], [0.25, 0.15, 0.08], [1, 0.6, 0.3]) * env_exp(n, 0.002, 0.3)
        place(x, 0.3 * grains(rng, seconds(0.2), 30, 2000, 6000), 0.6)
    else:
        raise ValueError(name)
    return finish(x, PEAK_DB["shop"])


# --- mission stings -----------------------------------------------------------------

def pad(n, freqs, attack, decay):
    t = np.arange(n) / RATE
    out = np.zeros(n)
    for f in freqs:
        for detune in (-0.004, 0.004):
            out += np.sin(2 * np.pi * f * (1 + detune) * t) + 0.3 * np.sin(4 * np.pi * f * (1 + detune) * t)
    return out * env_exp(n, attack, decay)


def mission(rng, name):
    if name == "Available":
        n = seconds(1.0)
        x = np.zeros(n)
        place(x, pluck(seconds(0.6), 659.3, 0.25), 0.0)
        place(x, pluck(seconds(0.6), 987.8, 0.3), 0.15)
    elif name == "Start":
        n = seconds(2.2)
        x = 0.4 * pad(n, [130.8, 196.0], 0.05, 0.8)
        for k, f in enumerate([523.3, 659.3, 784.0]):
            place(x, pluck(seconds(0.8), f, 0.3), 0.1 + k * 0.12)
        place(x, thump(rng, seconds(0.5), 55, 0.15), 0.0)
    elif name == "Objective":
        n = seconds(0.6)
        x = np.zeros(n)
        place(x, pluck(seconds(0.3), 1174.7, 0.08), 0.0)
        place(x, pluck(seconds(0.3), 880.0, 0.1), 0.1)
    elif name == "Checkpoint":
        n = seconds(1.2)
        x = bell(rng, n, 1568.0, 0.5) + 0.5 * bell(rng, n, 2093.0, 0.35)
    elif name == "Success":
        n = seconds(3.0)
        x = 0.5 * pad(n, [261.6, 329.6, 392.0, 523.3], 0.15, 1.6)
        for k, f in enumerate([1046.5, 1318.5, 1568.0, 2093.0]):
            place(x, 0.4 * bell(rng, seconds(1.0), f, 0.4), 0.2 + k * 0.08)
    elif name == "Fail":
        n = seconds(2.4)
        x = np.zeros(n)
        place(x, 0.8 * pad(seconds(1.2), [220.0, 261.6], 0.02, 0.5), 0.0)
        place(x, 0.8 * pad(seconds(1.8), [174.6, 207.7], 0.02, 0.8), 0.45)
        place(x, thump(rng, seconds(0.6), 45, 0.25), 0.45)
    elif name == "Reward":
        n = seconds(1.6)
        x = np.zeros(n)
        place(x, 0.7 * bell(rng, seconds(1.0), 2637.0, 0.5), 0.0)
        for at in np.sort(rng.uniform(0.15, 0.9, 14)):
            f0 = rng.uniform(3200, 6000)
            place(x, 0.5 * modes(rng, seconds(0.25), [f0, f0 * 1.52], [0.07, 0.04], [1, 0.5]), at)
    elif name == "Countdown":
        n = seconds(0.2)
        x = pluck(n, 1760.0, 0.03)
    else:
        raise ValueError(name)
    return finish(x, PEAK_DB["mission"])


# --- output --------------------------------------------------------------------

def build(out, seed=17):
    """Writes every sound; returns the list of written paths."""
    rng = np.random.default_rng(seed)
    written = []
    for name in LOOPS:
        p = os.path.join(out, "Interior", name, f"SW_Interior_{name}.wav")
        write_wav(p, loop(rng, name))
        written.append(p)
    for name in SHOP:
        for i in range(VARIANTS.get(name, 1)):
            p = os.path.join(out, "Shop", name, f"SW_Shop_{name}_{i + 1:02d}.wav")
            write_wav(p, shop(rng, name))
            written.append(p)
    for name in MISSION:
        for i in range(VARIANTS.get(name, 1)):
            p = os.path.join(out, "Mission", name, f"SW_Mission_{name}_{i + 1:02d}.wav")
            write_wav(p, mission(rng, name))
            written.append(p)
    return written


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out")
    ap.add_argument("--seed", type=int, default=17)
    args = ap.parse_args()
    written = build(args.out, args.seed)
    size = sum(os.path.getsize(p) for p in written)
    print(f"{len(written)} files, {size / 1e6:.1f} MB in {args.out}")


if __name__ == "__main__":
    main()
