#pragma once
// Synthetische, sprachaehnliche Testsignale fuer den automatischen De-Esser (keine Aufnahmen).
// Silben aus Vokalen (Obertoene mit Formanten) und optional Zischlauten (bandbegrenztes Rauschen) davor,
// Pausen, Atmer, Fluestern. Alle Pegel relativ zum Vokal-Effektivwert.
#include "../Source/DoppelgangerDSP.h"
#include <cstdint>
#include <vector>

struct SpeechOptions
{
    double f0 = 120.0;           // Grundton
    bool bright = false;         // natuerlich helle Stimme: kraeftige Obertoene 2,5-9 kHz
    double sibDb = -6.0;         // Zischlaut-Effektivwert relativ zum Vokal (dB)
    double sibPerSecond = 1.5;   // wie viele Zischlaute pro Sekunde
    double sibCenterHz = 6500.0; // Schwerpunkt des Zischens
    bool whisper = false;        // Vokale gefluestert (Rauschen statt Obertoene)
    bool breaths = false;        // Atmer in den Pausen
    double level = 0.1;          // Vokal-Spitzenamplitude
    uint32_t seed = 1;
};

inline std::vector<float> synthSpeech (double fs, double seconds, const SpeechOptions& o, int* sibilantCount = nullptr)
{
    int sibilants = 0;
    std::vector<float> x ((size_t) (fs * seconds), 0.0f);
    uint32_t r = o.seed * 2654435761u + 1u;
    auto noise = [&] { r = r * 1664525u + 1013904223u; return (double) (int32_t) r / 2147483648.0; };
    // Zischband: Hochpass 4. Ordnung + Tiefpass 2. Ordnung um den Schwerpunkt
    const auto h1 = dop::highPass (fs, o.sibCenterHz / 1.45, 0.5412), h2 = dop::highPass (fs, o.sibCenterHz / 1.45, 1.3066);
    const auto l1 = dop::lowPass (fs, std::min (o.sibCenterHz * 1.6, 0.45 * fs), 0.7071);
    const auto b1 = dop::highPass (fs, 300.0, 0.7071), b2 = dop::lowPass (fs, 2500.0, 0.7071);
    // Formantfilter fuer Fluestern
    const double F[3] = { 650.0, 1300.0, 2600.0 };
    dop::Biquad wf[3];
    for (int j = 0; j < 3; ++j) wf[j] = dop::peaking (fs, F[j], 3.0, 18.0);
    dop::BiquadState s1, s2, s3, bs1, bs2, ws[3], wl;
    const auto wlp = dop::lowPass (fs, 4000.0, 0.7071);
    const double syl = 0.36, vowelDur = 0.24, sibDur = 0.11;
    double sibAcc = 0.0, phase = 0.0;
    // Eine Periode des Vokals als Wellentabelle (Obertoene mit Formanten), schnell abspielbar mit leichtem Vibrato
    std::vector<double> table (4096, 0.0);
    for (int k = 1; k * o.f0 * 1.06 < std::min (10000.0, 0.45 * fs); ++k)
    {
        const double f = k * o.f0;
        double g = 0.05;
        for (int j = 0; j < 3; ++j) { const double d = std::log2 (f / F[j]) / 0.25; g += (j == 0 ? 1.0 : j == 1 ? 0.6 : 0.3) * std::exp (-0.5 * d * d); }
        if (o.bright) g += 0.35 * std::exp (-0.5 * std::pow (std::log2 (f / 4500.0) / 0.9, 2));   // Praesenz/Glanz bis 9 kHz
        for (size_t n = 0; n < table.size(); ++n)
            table[n] += 0.35 * g / std::pow ((double) k, 0.7) * std::sin (2 * dop::kPi * k * (double) n / (double) table.size());
    }
    // Effektivwert eines Vokals (mit Huellkurve) als Bezug fuer den Zischlaut-Pegel
    double tableMs = 0, envMs = 0;
    for (double v : table) tableMs += v * v;
    tableMs /= (double) table.size();
    for (int n = 0; n < 1000; ++n) envMs += std::pow (std::sin (dop::kPi * (n + 0.5) / 1000.0), 1.2);
    envMs /= 1000.0;
    const double vowelRms = o.whisper ? o.level * 0.05 : o.level * std::sqrt (tableMs * envMs);
    std::vector<double> burst;
    for (double t0 = 0.05; t0 + syl < seconds; t0 += syl)
    {
        const double cyc = std::fmod (t0, 3.0);
        if (cyc > 2.55)   // Pause, eventuell Atmer
        {
            if (o.breaths)
                for (size_t i = (size_t) (t0 * fs); i < (size_t) ((t0 + 0.3) * fs) && i < x.size(); ++i)
                {
                    const double t = (double) i / fs - t0;
                    x[i] += (float) (vowelRms * 0.035 * std::sin (dop::kPi * t / 0.3) * bs2.process (b2, bs1.process (b1, noise())) * 4.0);
                }
            continue;
        }
        sibAcc += o.sibPerSecond * syl;
        double tv = t0;
        if (sibAcc >= 1.0)   // Zischlaut vor dem Vokal
        {
            sibAcc -= 1.0;
            ++sibilants;
            // Zischlaut auf den gewuenschten Effektivwert relativ zum Vokal normiert
            const size_t i0 = (size_t) (t0 * fs), n = (size_t) (sibDur * fs);
            burst.assign (n, 0.0);
            double ms = 0;
            for (size_t i = 0; i < n; ++i)
            {
                const double env = std::pow (std::sin (dop::kPi * (double) i / (double) n), 0.5);
                burst[i] = env * s3.process (l1, s2.process (h2, s1.process (h1, noise())));
                ms += burst[i] * burst[i];
            }
            const double gain = vowelRms * std::pow (10.0, o.sibDb / 20.0) / std::sqrt (ms / (double) n + 1e-30);
            for (size_t i = 0; i < n && i0 + i < x.size(); ++i) x[i0 + i] += (float) (gain * burst[i]);
            tv = t0 + sibDur;
        }
        for (size_t i = (size_t) (tv * fs); i < (size_t) ((tv + vowelDur) * fs) && i < x.size(); ++i)
        {
            const double t = (double) i / fs - tv;
            const double env = std::pow (std::sin (dop::kPi * t / vowelDur), 0.6);
            double v = 0;
            if (o.whisper)
            {
                double n = noise();
                for (int j = 0; j < 3; ++j) n = ws[j].process (wf[j], n);
                v = 0.12 * wl.process (wlp, n);
            }
            else
            {
                const double f0 = o.f0 * (1.0 + 0.05 * std::sin (2 * dop::kPi * 1.3 * ((double) i / fs)));
                phase += f0 / fs; phase -= std::floor (phase);
                const double pos = phase * (double) table.size();
                const size_t i0 = (size_t) pos % table.size(), i1 = (i0 + 1) % table.size();
                v = table[i0] + (pos - std::floor (pos)) * (table[i1] - table[i0]);
            }
            x[i] += (float) (o.level * env * v);
        }
    }
    if (sibilantCount != nullptr) *sibilantCount = sibilants;
    return x;
}
