"""Synthesise the daytime street ambience loop of the Presqu'ile (original, MIT).

No recording is used: the loop is built from filtered noise and simple
engine models, so it carries no third-party licence. Layers:
- distant traffic rumble, slowly breathing;
- cars and scooters passing by, with Doppler shift and stereo travel;
- a faint pedestrian murmur;
- a few sparrow chirps.
Every layer wraps around the loop length, so the file loops without a seam.

Usage: python3 tools/audio/make_street_ambience.py [out.wav] [--seconds 120] [--seed 7]
Needs numpy. Output: 48 kHz, 16-bit stereo WAV (about 23 MB for 120 s).
"""

import argparse
import wave

import numpy as np

RATE = 48000
SOUND_SPEED = 343.0


def shaped_noise(rng, n, shape):
    """White noise coloured in the frequency domain; shape(freqs) gives the amplitude."""
    spectrum = np.fft.rfft(rng.standard_normal(n))
    freqs = np.fft.rfftfreq(n, 1.0 / RATE)
    spectrum *= shape(freqs)
    out = np.fft.irfft(spectrum, n)  # circular: loops seamlessly
    return out / (np.std(out) + 1e-12)


def band(lo, hi, slope=2.0):
    def shape(f):
        f = np.maximum(f, 1.0)
        low = 1.0 / (1.0 + (lo / f) ** (2 * slope))
        high = 1.0 / (1.0 + (f / hi) ** (2 * slope))
        return np.sqrt(low * high)
    return shape


def slow_wobble(rng, n, seconds, depth, cycles=(1, 2, 3, 5)):
    """Sum of sines whose periods divide the loop length: seamless slow modulation."""
    t = np.arange(n) / RATE
    out = np.zeros(n)
    for k in cycles:
        out += rng.uniform(0.5, 1.0) * np.sin(2 * np.pi * k * t / seconds + rng.uniform(0, 2 * np.pi))
    out /= np.max(np.abs(out)) + 1e-12
    return 1.0 + depth * out


def add_wrapped(buf, start, signal):
    """Adds a mono or stereo signal at start, wrapping past the end of the loop."""
    n = buf.shape[0]
    idx = (start + np.arange(signal.shape[0])) % n
    np.add.at(buf, idx, signal)


def pass_by(rng, kind):
    """One vehicle passing in front of the listener. Returns a stereo signal."""
    if kind == "scooter":
        speed = rng.uniform(8.0, 13.0)
        distance = rng.uniform(4.0, 12.0)
        f0 = rng.uniform(85.0, 120.0)
        harmonics = [(1, 1.0), (2, 0.7), (3, 0.5), (4, 0.35), (6, 0.2)]
        tyre = 0.25
        level = rng.uniform(0.25, 0.45)
    elif kind == "bus":
        speed = rng.uniform(5.0, 8.0)
        distance = rng.uniform(6.0, 14.0)
        f0 = rng.uniform(22.0, 30.0)
        harmonics = [(1, 1.0), (2, 0.8), (3, 0.6), (4, 0.4), (5, 0.3), (8, 0.15)]
        tyre = 0.8
        level = rng.uniform(0.45, 0.7)
    else:
        speed = rng.uniform(7.0, 13.0)
        distance = rng.uniform(5.0, 18.0)
        f0 = rng.uniform(28.0, 45.0)
        harmonics = [(1, 1.0), (2, 0.6), (3, 0.35), (4, 0.2)]
        tyre = 1.0
        level = rng.uniform(0.3, 0.6)

    half = 70.0 / speed * 0.5  # seconds from far left to far right
    n = int(2 * half * RATE)
    t = np.arange(n) / RATE - half
    direction = rng.choice([-1.0, 1.0])
    x = speed * t * direction
    r = np.sqrt(x ** 2 + distance ** 2)
    approach = -x * speed * direction / r  # m/s towards the listener (> 0 before the pass)
    doppler = SOUND_SPEED / (SOUND_SPEED - approach)
    gain = level * (distance / r) ** 1.6

    # Engine: harmonic series with a little random revving, Doppler shifted.
    rev = 1.0 + 0.04 * np.sin(2 * np.pi * rng.uniform(0.1, 0.4) * t + rng.uniform(0, 6.28))
    phase = 2 * np.pi * np.cumsum(f0 * rev * doppler) / RATE
    engine = sum(a * np.sin(h * phase + rng.uniform(0, 6.28)) for h, a in harmonics)
    engine /= np.std(engine) + 1e-12

    # Tyres on asphalt: band-passed noise, brighter close up.
    rng_tyre = np.random.default_rng(rng.integers(1 << 30))
    tyres = shaped_noise(rng_tyre, n, band(250.0, 2200.0, 1.5))

    mono = gain * (0.55 * engine + tyre * 0.6 * tyres)
    # Air absorption: soften when far (simple one-pole low-pass per sample block).
    alpha = np.clip(distance / r, 0.15, 1.0)
    smoothed = np.empty_like(mono)
    acc = 0.0
    for i in range(0, n, 256):
        a = float(alpha[i])
        block = mono[i:i + 256]
        out = np.empty_like(block)
        for j, v in enumerate(block):
            acc += a * (v - acc)
            out[j] = acc
        smoothed[i:i + 256] = out
    pan = np.tanh(x / distance * 0.8)  # -1 left .. 1 right
    left = smoothed * np.sqrt(0.5 * (1 - pan))
    right = smoothed * np.sqrt(0.5 * (1 + pan))
    fade = np.minimum(1.0, np.minimum(np.arange(n), np.arange(n)[::-1]) / (0.3 * RATE))
    return np.stack([left * fade, right * fade], axis=1)


def chirp(rng):
    """A sparrow-like double chirp."""
    out = []
    for _ in range(rng.integers(2, 5)):
        dur = rng.uniform(0.04, 0.09)
        n = int(dur * RATE)
        t = np.arange(n) / RATE
        f = rng.uniform(3500, 5200) + rng.uniform(-1800, 900) * t / dur
        tone = np.sin(2 * np.pi * np.cumsum(f) / RATE) * np.sin(np.pi * t / dur) ** 2
        out.append(tone)
        out.append(np.zeros(int(rng.uniform(0.03, 0.08) * RATE)))
    mono = np.concatenate(out) * rng.uniform(0.04, 0.08)
    pan = rng.uniform(-0.8, 0.8)
    return np.stack([mono * np.sqrt(0.5 * (1 - pan)), mono * np.sqrt(0.5 * (1 + pan))], axis=1)


def build(seconds, seed):
    rng = np.random.default_rng(seed)
    n = int(seconds * RATE)
    mix = np.zeros((n, 2))

    # Distant traffic: deep, partly decorrelated between the ears.
    rumble_shape = lambda f: band(25.0, 700.0, 1.2)(f) / np.sqrt(np.maximum(f, 20.0) / 20.0)
    common = shaped_noise(rng, n, rumble_shape)
    for ch in range(2):
        own = shaped_noise(rng, n, rumble_shape)
        mix[:, ch] += 0.22 * (0.75 * common + 0.45 * own) * slow_wobble(rng, n, seconds, 0.35)

    # City air: very faint broadband hiss.
    for ch in range(2):
        mix[:, ch] += 0.02 * shaped_noise(rng, n, band(400.0, 6000.0, 1.0))

    # Pedestrian murmur: band-passed noise with syllable-rate modulation.
    t = np.arange(n) / RATE
    for ch in range(2):
        voice = shaped_noise(rng, n, band(250.0, 2500.0, 2.0))
        syllables = shaped_noise(rng, n, band(2.0, 7.0, 1.0))
        envelope = np.clip(0.6 + 0.5 * syllables, 0.0, None)
        mix[:, ch] += 0.035 * voice * envelope * slow_wobble(rng, n, seconds, 0.5, (2, 3, 7))

    # Pass-bys, spread over the loop with some bunching (traffic lights).
    clock = 0.0
    while clock < seconds:
        kind = rng.choice(["car", "car", "car", "scooter", "bus"], p=[0.3, 0.25, 0.2, 0.18, 0.07])
        add_wrapped(mix, int(clock * RATE), pass_by(rng, kind))
        clock += rng.choice([rng.uniform(1.0, 2.5), rng.uniform(3.0, 9.0)], p=[0.35, 0.65])

    # Sparrows.
    for _ in range(int(seconds / 12)):
        add_wrapped(mix, int(rng.uniform(0, seconds) * RATE), chirp(rng))

    peak = np.max(np.abs(mix))
    return mix * (0.5 / peak)  # about -6 dBFS peak; level is set in game


def write_wav(path, data):
    pcm = np.clip(data * 32767.0, -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(pcm.tobytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("out", nargs="?", default="SW_Amb_Street_Day.wav")
    parser.add_argument("--seconds", type=float, default=120.0)
    parser.add_argument("--seed", type=int, default=7)
    args = parser.parse_args()
    write_wav(args.out, build(args.seconds, args.seed))
    print(args.out)


if __name__ == "__main__":
    main()
