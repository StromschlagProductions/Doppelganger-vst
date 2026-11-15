// Testprogramm fuer die Engine (ohne JUCE):
//   test_engine IN.f32 OUT.f32 SAMPLERATE [amount] [auto|tief|mittel|hoch] [lowcut] [deess] [leveler] [target] [deessFreq]
// Liest/schreibt rohe float32-Mono-Daten. Gibt zusaetzlich OUT.ride.txt (Leveler-Verlauf, alle 10 ms) aus.
#include "../Source/DoppelgangerDSP.h"
#include <cstdio>
#include <cstdlib>
#include <string>

int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "usage\n"); return 1; }
    const double fs = std::atof (argv[3]);
    if (! std::isfinite (fs) || fs < 8000 || fs > 768000) return 1;
    const float amount = argc > 4 ? (float) std::atof (argv[4]) : 0.6f;
    const std::string regArg = argc > 5 ? argv[5] : "auto";
    dop::EngineParams p;
    if (argc > 6) p.lowcutHz = (float) std::atof (argv[6]);
    if (argc > 7) { p.deess = (float) std::atof (argv[7]); p.deessOn = p.deess > 0.0f; }
    if (argc > 8) p.leveler = (float) std::atof (argv[8]);
    if (argc > 9) p.targetLufs = (float) std::atof (argv[9]);
    if (argc > 10) p.deessFreqHz = (float) std::atof (argv[10]);

    FILE* f = std::fopen (argv[1], "rb");
    if (f == nullptr) { std::perror ("input"); return 1; }
    if (std::fseek (f, 0, SEEK_END) != 0) { std::fclose (f); return 1; }
    const long bytes = std::ftell (f);
    if (bytes <= 0 || bytes % 4 != 0) { std::fclose (f); return 1; }
    const long n = bytes / 4;
    std::fseek (f, 0, SEEK_SET);
    std::vector<float> x ((size_t) n);
    if (std::fread (x.data(), 4, (size_t) n, f) != (size_t) n) { std::fclose (f); return 1; }
    std::fclose (f);

    dop::Engine eng;
    eng.prepare (fs);
    eng.setParams (p);

    // 1) Lernen
    dop::Learner learner;
    learner.prepare (fs);
    {
        std::vector<float> tmp (x), tap (512);
        for (long i = 0; i < n; i += 512)
        {
            const int m = (int) std::min (512L, n - i);
            float* ch[1] = { tmp.data() + i };
            eng.process (ch, 1, m, tap.data());
            learner.push (tap.data(), m);
        }
    }
    const auto prof = learner.finish();
    int reg = regArg == "tief" ? dop::kTief : regArg == "hoch" ? dop::kHoch : regArg == "mittel" ? dop::kMittel
                                                                         : dop::registerForPitch (prof.pitchHz);
    const auto corr = dop::correctionCurve (prof.curveDb, reg, amount, p.lowcutHz);
    const auto bank = dop::fitBank (corr, fs);
    const float eqDelta = dop::eqLoudnessDeltaDb (prof.curveDb, corr);
    std::printf ("gelernt: sprache=%.1fs tonhoehe=%.0fHz register=%d lautheit=%.1f LUFS, EQ aendert %+.2f dB, fit %.2f dB\n",
                 prof.speechSeconds, prof.pitchHz, reg, prof.loudnessLufs, eqDelta, bank.maxErrorDb);

    // 2) Verarbeiten mit gelernter Bank und Lautheit
    p.takeLufs = prof.loudnessLufs + eqDelta;
    p.rideOffsetDb = prof.typicalOffsetDb;
    std::printf ("typischer Sprechpegel %+.1f dB\n", prof.typicalOffsetDb);
    eng.setParams (p);
    eng.reset();
    eng.setBank (bank.coeffs);
    std::vector<float> y (x);
    FILE* rf = std::fopen ((std::string (argv[2]) + ".ride.txt").c_str(), "w");
    if (rf == nullptr) return 1;
    const int blk = (int) (fs / 100);
    for (long i = 0; i < n; i += blk)
    {
        const int m = (int) std::min ((long) blk, n - i);
        float* ch[1] = { y.data() + i };
        eng.process (ch, 1, m, nullptr);
        std::fprintf (rf, "%f %f\n", eng.meterRideDb.load(), eng.meterStaticDb.load());
    }
    std::fclose (rf);
    std::printf ("grundverstaerkung %.1f dB, latenz %d\n", eng.meterStaticDb.load(), eng.latencySamples());
    FILE* o = std::fopen (argv[2], "wb");
    if (o == nullptr) return 1;
    const bool written = std::fwrite (y.data(), 4, (size_t) n, o) == (size_t) n;
    return std::fclose (o) == 0 && written ? 0 : 1;
}
