"""Synthesise the voice, police radio and interface banks (original, MIT).

No recording and no words: voices are formant synthesis (a glottal pulse
train through the resonances of a vowel), the police radio is unintelligible
"speech" of random vowels through a radio band with its squelch, and the
interface sounds are short tones. A working base until recorded sounds
replace them, folder by folder (docs/audio/audio-design.md).

Layout written under OUT (mirrors /Game/Sound/SFX in Unreal, read by UGTPAudioMix):
  Voices/<Scream|Hurt|Gasp>/SW_Voice_<Kind>_NN.wav
  Radio/<Chatter|Alert|Clear>/SW_Radio_<Call>_NN.wav
  UI/<Navigate|Confirm|Back|Toggle|Open|Close|StarUp|StarDown|MapZoom>/SW_UI_<Name>_NN.wav
Names are those of EGTPVoiceKind, EGTPRadioCall and EGTPUISound.

Usage: python3 -I tools/audio/make_voices_ui.py OUT [--seed 7]
Needs numpy. Output: 48 kHz, 16-bit mono WAV.
"""

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_sfx import RATE, bandpass_noise, finish, seconds, write_wav  # noqa: E402

# Formants (Hz) and bandwidths of a few vowels, adult voice.
VOWELS = {
    "a": ((800, 80), (1200, 90), (2500, 120)),
    "e": ((450, 70), (1900, 100), (2600, 120)),
    "i": ((300, 60), (2300, 100), (3000, 140)),
    "o": ((500, 70), (900, 80), (2400, 120)),
    "u": ((320, 60), (800, 80), (2300, 120)),
    "ae": ((650, 80), (1600, 100), (2600, 130)),
}
VOICE_COUNTS = {"Scream": 6, "Hurt": 6, "Gasp": 5}
RADIO_COUNTS = {"Chatter": 8, "Alert": 3, "Clear": 3}
UI_NAMES = ["Navigate", "Confirm", "Back", "Toggle", "Open", "Close", "StarUp", "StarDown", "MapZoom"]
PEAK_DB = {"voice": -3.0, "radio": -6.0, "ui": -9.0}


# --- voice -------------------------------------------------------------------

def glottal(rng, f0, rough=0.0, breath=0.05):
    """Pulse train following the f0 contour (Hz per sample), with jitter and breath noise."""
    n = len(f0)
    jitter = 1.0 + 0.012 * np.cumsum(rng.standard_normal(n)) / np.sqrt(np.arange(1, n + 1))
    phase = np.cumsum(f0 * jitter) / RATE
    frac = phase - np.floor(phase)
    # Rosenberg-like pulse: slow open, fast close; its derivative is the excitation.
    pulse = np.where(frac < 0.6, 0.5 - 0.5 * np.cos(np.pi * frac / 0.6), np.cos(np.pi * (frac - 0.6) / 0.8))
    pulse = np.clip(pulse, 0.0, None)
    src = np.diff(pulse, prepend=pulse[0]) * RATE / np.maximum(f0, 50.0)
    if rough > 0:
        # Vocal fry and strain: every other period stronger (subharmonic), amplitude noise.
        period = np.floor(phase).astype(int)
        src *= 1.0 + rough * np.where(period % 2 == 0, 0.6, -0.4) * rng.uniform(0.5, 1.0)
        src *= 1.0 + rough * 0.5 * bandpass_noise(rng, n, 20, 300)
    src /= np.std(src) + 1e-12
    return src + breath * bandpass_noise(rng, n, 500, 7000)


def formant_filter(x, vowel, shift=1.0, tilt=0.0):
    """Spectral envelope of a vowel applied by FFT (a sum of resonance peaks); tilt > 0 brightens (strained voice)."""
    n = len(x)
    spec = np.fft.rfft(x)
    f = np.fft.rfftfreq(n, 1 / RATE)
    env = np.full_like(f, 0.02)
    for k, (fc, bw) in enumerate(VOWELS[vowel]):
        fc *= shift
        env += (1.0, 0.8, 0.5)[k] / (1.0 + ((f - fc) / (bw * shift)) ** 2)
    env *= 1.0 / (1.0 + (f / 5000.0) ** 2)
    if tilt > 0:
        env *= np.clip(f / 800.0, 0.3, 6.0) ** tilt
    return np.fft.irfft(spec * env, n)


def strain(y, drive):
    """Soft saturation: the harsh edge of a shout (adds upper harmonics)."""
    y = y / (np.std(y) + 1e-12)
    return np.tanh(drive * y / 3.0)


def contour(n, points):
    """Piecewise-linear f0 contour through (time share, Hz) points."""
    t = np.linspace(0.0, 1.0, n)
    xs, ys = zip(*points)
    return np.interp(t, xs, ys)


def vibrato(rng, n, rate, depth):
    t = np.arange(n) / RATE
    return 1.0 + depth * np.sin(2 * np.pi * rate * t + rng.uniform(0, 2 * np.pi))


def voice_env(n, attack, release):
    e = np.ones(n)
    a, r = seconds(attack), seconds(release)
    e[:a] = np.linspace(0.0, 1.0, a) ** 1.5
    e[-r:] *= np.linspace(1.0, 0.0, r) ** 2
    return e


def scream(rng):
    length = rng.uniform(0.75, 1.4)
    n = seconds(length)
    peak = rng.uniform(560, 900)
    f0 = contour(n, [(0, peak * 0.7), (0.15, peak), (0.7, peak * rng.uniform(0.9, 1.05)), (1, peak * 0.6)])
    f0 *= vibrato(rng, n, rng.uniform(5.5, 7.5), 0.025)
    src = glottal(rng, f0, rough=rng.uniform(0.2, 0.45), breath=0.12)
    vowel = rng.choice(["a", "ae", "e"])
    y = strain(formant_filter(src, vowel, shift=rng.uniform(1.3, 1.5), tilt=1.6), 3.0)
    return y * voice_env(n, 0.05, length * 0.35)


def hurt(rng):
    length = rng.uniform(0.3, 0.55)
    n = seconds(length)
    start = rng.uniform(260, 420)
    f0 = contour(n, [(0, start), (0.25, start * 1.12), (1, start * 0.6)])
    src = glottal(rng, f0, rough=rng.uniform(0.35, 0.6), breath=0.15)
    vowel = rng.choice(["a", "ae", "o"])
    y = strain(formant_filter(src, vowel, shift=rng.uniform(0.95, 1.1), tilt=0.6), 1.8) * 0.3
    # A short plosive "h" in front.
    y = np.concatenate([0.35 * bandpass_noise(rng, seconds(0.04), 600, 4000) * np.linspace(0, 1, seconds(0.04)), y])
    return y * voice_env(len(y), 0.01, length * 0.5)


def gasp(rng):
    length = rng.uniform(0.28, 0.45)
    n = seconds(length)
    # Inhaled: mostly breath through an open vowel, a short voiced end.
    breath = bandpass_noise(rng, n, 400, 6000)
    y = formant_filter(breath, rng.choice(["a", "ae", "o"]), shift=rng.uniform(1.0, 1.15)) * 0.8
    voiced_n = seconds(length * 0.35)
    f0 = contour(voiced_n, [(0, rng.uniform(300, 450)), (1, rng.uniform(380, 520))])
    tail = formant_filter(glottal(rng, f0, breath=0.3), "a", 1.1) * voice_env(voiced_n, 0.03, 0.08)
    y[-voiced_n:] += 0.6 * tail / (np.std(tail) + 1e-12) * np.std(y)
    return y * voice_env(n, length * 0.4, 0.06)


# --- police radio ------------------------------------------------------------

def radio_band(rng, x):
    """Handheld radio: 350-2800 Hz, a little clipping, hiss."""
    n = len(x)
    spec = np.fft.rfft(x)
    f = np.maximum(np.fft.rfftfreq(n, 1 / RATE), 1.0)
    spec *= 1.0 / (1.0 + (350.0 / f) ** 4) / (1.0 + (f / 2800.0) ** 4)
    y = np.fft.irfft(spec, n)
    y /= np.max(np.abs(y)) + 1e-12
    y = np.tanh(2.2 * y) / np.tanh(2.2)
    return y + 0.04 * bandpass_noise(rng, n, 300, 3500)


def babble(rng, length, f0_base):
    """Unintelligible speech: syllables of random vowels at 4-6 per second, falling phrase pitch."""
    n = seconds(length)
    out = np.zeros(n)
    t = 0.0
    while t < length - 0.12:
        syl = rng.uniform(0.11, 0.24)
        m = seconds(syl)
        s = seconds(t)
        if s + m > n:
            break
        phrase = 1.0 - 0.18 * (t / length)
        f0 = np.full(m, f0_base * phrase * rng.uniform(0.92, 1.12))
        src = glottal(rng, f0, rough=0.1, breath=0.08)
        y = formant_filter(src, rng.choice(list(VOWELS)), shift=rng.uniform(0.95, 1.05))
        # Consonant burst at the start of some syllables.
        if rng.random() < 0.6:
            k = seconds(0.025)
            y[:k] += 0.8 * bandpass_noise(rng, k, 2000, 6000) * np.std(y)
        y *= np.hanning(m) ** 0.6
        out[s:s + m] += y
        t += syl + (rng.uniform(0.08, 0.3) if rng.random() < 0.18 else rng.uniform(0.0, 0.03))
    return out


def kerchunk(rng):
    """Key-up of a radio: a click and a short burst of carrier noise."""
    n = seconds(0.06)
    click = np.zeros(n)
    click[: seconds(0.002)] = 1.0
    return 0.6 * click + 0.25 * bandpass_noise(rng, n, 800, 4000) * np.linspace(1, 0, n)


def squelch(rng):
    n = seconds(rng.uniform(0.12, 0.2))
    return 0.5 * bandpass_noise(rng, n, 500, 6000) * np.linspace(1, 0.2, n)


def tone(freq, length, level=1.0):
    n = seconds(length)
    t = np.arange(n) / RATE
    e = np.minimum(1.0, np.minimum(t / 0.004, (length - t) / 0.006))
    return level * np.sin(2 * np.pi * freq * t) * np.clip(e, 0, 1)


def radio_call(rng, kind):
    f0 = rng.choice([105.0, 120.0, 135.0, 175.0, 195.0])
    parts = []
    if kind == "Alert":
        for _ in range(3):
            parts += [tone(1050, 0.09), np.zeros(seconds(0.04)), tone(1400, 0.09), np.zeros(seconds(0.05))]
        parts += [np.zeros(seconds(0.25)), kerchunk(rng), babble(rng, rng.uniform(1.6, 2.4), f0)]
    elif kind == "Clear":
        parts += [kerchunk(rng), babble(rng, rng.uniform(1.0, 1.6), f0), np.zeros(seconds(0.1)),
                  tone(880, 0.12), np.zeros(seconds(0.05)), tone(660, 0.16)]
    else:
        parts += [kerchunk(rng), babble(rng, rng.uniform(1.4, 3.6), f0)]
    parts.append(squelch(rng))
    parts = [p / (np.max(np.abs(p)) + 1e-12) for p in parts]
    return radio_band(rng, np.concatenate(parts))


# --- interface ---------------------------------------------------------------

def blip(freqs, length, decay):
    n = seconds(length)
    t = np.arange(n) / RATE
    out = np.zeros(n)
    for k, f in enumerate(freqs):
        start = k * length / len(freqs) * 0.6
        tk = np.clip(t - start, 0, None)
        out += (t >= start) * np.sin(2 * np.pi * f * tk) * np.exp(-tk / decay) * np.minimum(1, tk / 0.002)
        out += (t >= start) * 0.25 * np.sin(4 * np.pi * f * tk) * np.exp(-tk / (decay * 0.5))
    return out


def sweep(rng, f_from, f_to, length):
    n = seconds(length)
    f = np.geomspace(f_from, f_to, n)
    body = np.sin(2 * np.pi * np.cumsum(f) / RATE) * np.hanning(n)
    air = 0.08 * bandpass_noise(rng, n, 1500, 5000) * np.hanning(n)
    return body + air


def ui_sound(rng, name):
    if name == "Navigate":
        return blip([2100], 0.05, 0.012)
    if name == "Confirm":
        return blip([880, 1320], 0.16, 0.05)
    if name == "Back":
        return blip([990, 660], 0.15, 0.045)
    if name == "Toggle":
        return blip([1500], 0.04, 0.008) + 0.5 * np.pad(blip([1100], 0.03, 0.006), (seconds(0.012), 0))[: seconds(0.04)]
    if name == "Open":
        return sweep(rng, 420, 1250, 0.2)
    if name == "Close":
        return sweep(rng, 1250, 420, 0.18)
    if name == "StarUp":
        return blip([1320, 1760], 0.5, 0.18)
    if name == "StarDown":
        return blip([1046, 784, 523], 0.6, 0.15)
    if name == "MapZoom":
        return blip([1250], 0.04, 0.01)
    raise ValueError(name)


# --- output ------------------------------------------------------------------

def build(out, seed=7):
    rng = np.random.default_rng(seed)
    written = []
    makers = {"Scream": scream, "Hurt": hurt, "Gasp": gasp}
    for kind, count in VOICE_COUNTS.items():
        for i in range(count):
            p = os.path.join(out, "Voices", kind, f"SW_Voice_{kind}_{i + 1:02d}.wav")
            write_wav(p, finish(makers[kind](rng), PEAK_DB["voice"]))
            written.append(p)
    for call, count in RADIO_COUNTS.items():
        for i in range(count):
            p = os.path.join(out, "Radio", call, f"SW_Radio_{call}_{i + 1:02d}.wav")
            write_wav(p, finish(radio_call(rng, call), PEAK_DB["radio"]))
            written.append(p)
    for name in UI_NAMES:
        p = os.path.join(out, "UI", name, f"SW_UI_{name}_01.wav")
        write_wav(p, finish(ui_sound(rng, name), PEAK_DB["ui"]))
        written.append(p)
    return written


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out")
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()
    written = build(args.out, args.seed)
    size = sum(os.path.getsize(p) for p in written)
    print(f"{len(written)} files, {size / 1e6:.1f} MB under {args.out}")


if __name__ == "__main__":
    main()
