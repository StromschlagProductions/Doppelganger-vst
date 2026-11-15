#!/usr/bin/env python3
"""
Erzeugt Source/ReferenceCurves.h aus den Referenz-WAVs.

Aufruf:
    python3 tools/make_reference_curves.py tief=PFAD_MANN.wav mittel=PFAD_FRAU1.wav hoch=PFAD_FRAU2.wav

Die Zielkurve jeder Referenz ist die mittlere Spektralform der Sprachanteile (Langzeit-Spektrum),
auf einem festen Log-Frequenz-Gitter gespeichert und so normiert, dass ihr Mittelwert im Bereich
100 bis 8000 Hz 0 dB ist. Der Pegel wird also nicht übernommen, nur die Klangform.
Außerdem wird die Bandbreitengrenze der Referenz erkannt (Codec-Grenze, z. B. 16 kHz).
"""
import sys
import os
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly, get_window

SR = 48000
NFFT = 4096
HOP = 1024
GRID_HZ = np.geomspace(40.0, 20000.0, 128)   # Log-Gitter, auf dem die Kurven gespeichert werden
SPEECH_WINDOW_DB = 40.0                       # Frames, die weniger als 40 dB unter dem lautesten liegen, zählen nicht


def load_mono_48k(path):
    x, sr = sf.read(path, always_2d=True)
    x = x.mean(axis=1)
    if sr != SR:
        g = np.gcd(sr, SR)
        x = resample_poly(x, SR // g, sr // g)
    return x.astype(np.float64)


def ltas_db(x):
    win = get_window("hann", NFFT)
    frames = []
    for start in range(0, len(x) - NFFT, HOP):
        seg = x[start:start + NFFT] * win
        frames.append(seg)
    frames = np.array(frames)
    rms_db = 20 * np.log10(np.sqrt(np.mean(frames ** 2, axis=1)) + 1e-12)
    keep = rms_db > (rms_db.max() - SPEECH_WINDOW_DB)   # nur Sprachanteile, Pausen weg
    spec = np.fft.rfft(frames[keep], axis=1)
    power = np.mean(np.abs(spec) ** 2, axis=0)
    freqs = np.fft.rfftfreq(NFFT, 1.0 / SR)
    return freqs, 10 * np.log10(power + 1e-20)


def smooth_log(freqs, db, sigma_oct=1/6):
    # Glättung über Oktaven, damit die Kurve das Klangbild zeigt und nicht einzelne Peaks
    logf = np.log2(np.maximum(freqs, 1.0))
    out = np.empty_like(db)
    for i, f in enumerate(logf):
        w = np.exp(-0.5 * ((logf - f) / sigma_oct) ** 2)
        out[i] = np.sum(w * db) / np.sum(w)
    return out


def bandwidth_hz(freqs, db):
    # Höchste Frequenz, bei der die Referenz noch mindestens 50 dB unter ihrem Mittelpegel (1–8 kHz) liegt
    ref = np.mean(db[(freqs >= 1000) & (freqs <= 8000)])
    above = np.where(db > ref - 50)[0]
    return float(freqs[above[-1]]) if len(above) else 0.0


def curve_on_grid(path):
    x = load_mono_48k(path)
    freqs, db = ltas_db(x)
    db = smooth_log(freqs, db)
    bw = bandwidth_hz(freqs, db)
    grid_db = np.interp(np.log2(GRID_HZ), np.log2(np.maximum(freqs, 1.0)), db)
    band = (GRID_HZ >= 100) & (GRID_HZ <= 8000)
    grid_db = grid_db - np.mean(grid_db[band])
    # Extrembereiche (sehr tief, über der Codec-Grenze) sind nur Rauschboden: begrenzen
    grid_db = np.clip(grid_db, -40.0, 20.0)
    return grid_db, bw


def fmt(name, arr):
    vals = ",\n    ".join(", ".join(f"{v:.3f}f" for v in arr[i:i + 8]) for i in range(0, len(arr), 8))
    return f"inline const float {name}[{len(arr)}] = {{\n    {vals}\n}};\n"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    pairs = [a.split("=", 1) for a in sys.argv[1:]]
    names = {"tief": "kRefTiefDb", "mittel": "kRefMittelDb", "hoch": "kRefHochDb"}
    out = [
        "// AUTOMATISCH ERZEUGT durch tools/make_reference_curves.py – nicht von Hand ändern.",
        "// Zielkurven der Referenzstimmen (dB, relativ, Mittelwert 100–8000 Hz = 0 dB).",
        "#pragma once",
        "",
        "namespace dop {",
        "",
        f"inline constexpr int kRefGridPoints = {len(GRID_HZ)};",
        "inline constexpr float kRefGridHz[kRefGridPoints] = {",
        "    " + ", ".join(f"{f:.3f}f" for f in GRID_HZ),
        "};",
        "",
    ]
    cutoffs = {}
    for reg, path in pairs:
        if reg not in names:
            sys.exit(f"Unbekannte Stimmlage: {reg}")
        curve, bw = curve_on_grid(path)
        cutoffs[reg] = bw
        out.append(f"// Quelle: {os.path.basename(path)}, Bandbreitengrenze ca. {bw:.0f} Hz")
        out.append(fmt(names[reg], curve))
    out.append("} // namespace dop")
    dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Source", "ReferenceCurves.h")
    with open(dst, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    for reg, bw in cutoffs.items():
        print(f"{reg}: Bandbreitengrenze ca. {bw:.0f} Hz")
    print("geschrieben:", os.path.normpath(dst))


if __name__ == "__main__":
    main()
