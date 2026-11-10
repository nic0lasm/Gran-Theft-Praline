"""Stage recorded sounds (Sonniss GDC bundle, CC0 packs) into the bank layout.

The recorded libraries stay outside Git and outside the repository (on the
Mac, next to the clone). A pick list says which file goes to which bank:

  picks.csv   source,bank[,start_s,length_s]
              /path/Footsteps_Gravel_Walk_03.wav,Footsteps/Gravel
              /path/Car_Crash_Big.wav,Impacts/Car/Heavy,0.0,1.8

Each pick is read (PCM 16, 24 or 32-bit, any rate), mixed down to mono,
optionally cut, trimmed of leading silence, faded out, normalised to the
peak level of its family and written as <bank>/SW_<Bank_With_Underscores>_NN.wav
under OUT, ready for unreal/import_sounds.py. A file of the same bank made by
make_sfx.py is replaced only if it has the same name: delete the generated
folder first to switch a whole bank to recordings. The licence of every pick
goes in docs/audio/sources.md before the sound enters a release.

Usage: python3 -I tools/audio/stage_recorded.py picks.csv OUT
"""

import csv
import os
import sys
import wave

import numpy as np

PEAK_DB = {"Footsteps": -6.0, "Land": -4.0, "Impacts": -1.5}


def read_pcm(path):
    with wave.open(path) as w:
        rate, channels, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
        raw = w.readframes(w.getnframes())
    if width == 2:
        x = np.frombuffer(raw, "<i2").astype(np.float64) / 32768.0
    elif width == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        v = np.where(v >= 1 << 23, v - (1 << 24), v)
        x = v.astype(np.float64) / float(1 << 23)
    elif width == 4:
        x = np.frombuffer(raw, "<i4").astype(np.float64) / float(1 << 31)
    else:
        raise ValueError(f"{path}: unsupported sample width {width}")
    return x.reshape(-1, channels).mean(axis=1), rate


def prepare(x, rate, peak_db, start=0.0, length=0.0, threshold_db=-45.0):
    s = int(start * rate)
    x = x[s: s + int(length * rate)] if length > 0 else x[s:]
    level = 10 ** (threshold_db / 20.0) * (np.max(np.abs(x)) + 1e-12)
    loud = np.nonzero(np.abs(x) > level)[0]
    if len(loud) == 0:
        raise ValueError("silent pick")
    lead = max(0, loud[0] - int(0.003 * rate))
    tail = min(len(x), loud[-1] + int(0.05 * rate))
    x = x[lead:tail] - np.mean(x[lead:tail])
    fade = min(len(x) // 4, int(0.02 * rate))
    if fade:
        x[-fade:] *= np.linspace(1.0, 0.0, fade)
    return x / (np.max(np.abs(x)) + 1e-12) * 10 ** (peak_db / 20.0)


def write(path, x, rate):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())


def main(picks_path, out):
    counters = {}
    done = 0
    with open(picks_path, newline="", encoding="utf-8") as f:
        for row in csv.reader(f):
            if not row or row[0].startswith("#") or row[0] == "source":
                continue
            source, bank = row[0], row[1].strip("/")
            start = float(row[2]) if len(row) > 2 and row[2] else 0.0
            length = float(row[3]) if len(row) > 3 and row[3] else 0.0
            family = bank.split("/")[0]
            if family not in PEAK_DB:
                raise SystemExit(f"unknown bank {bank}: Footsteps/..., Land/... or Impacts/...")
            x, rate = read_pcm(source)
            counters[bank] = counters.get(bank, 0) + 1
            name = "SW_" + bank.replace("/", "_") + f"_R{counters[bank]:02d}.wav"
            write(os.path.join(out, bank, name), prepare(x, rate, PEAK_DB[family], start, length), rate)
            done += 1
    print(f"{done} recorded sounds staged under {out}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
