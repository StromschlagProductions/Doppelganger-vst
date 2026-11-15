// Testprogramm fuer die Engine (ohne JUCE):
//   test_engine IN.f32 OUT.f32 SAMPLERATE [amount] [register: auto|tief|mittel|hoch] [lowcut] [deess] [leveler] [target]
// Liest/schreibt rohe float32-Mono-Daten.
#include "../Source/DoppelgangerDSP.h"
#include <cstdio>
#include <cstdlib>
#include <string>

int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "usage\n"); return 1; }
    const double fs = std::atof (argv[3]);
    const float amount = argc > 4 ? (float) std::atof (argv[4]) : 0.6f;
    const std::string regArg = argc > 5 ? argv[5] : "auto";
    dop::EngineParams p;
    if (argc > 6) p.lowcutHz = (float) std::atof (argv[6]);
    if (argc > 7) p.deess = (float) std::atof (argv[7]);
    if (argc > 8) p.leveler = (float) std::atof (argv[8]);
    if (argc > 9) p.targetLufs = (float) std::atof (argv[9]);

    FILE* f = std::fopen (argv[1], "rb");
    std::fseek (f, 0, SEEK_END);
    const long n = std::ftell (f) / 4;
    std::fseek (f, 0, SEEK_SET);
    std::vector<float> x ((size_t) n);
    std::fread (x.data(), 4, (size_t) n, f);
    std::fclose (f);

    dop::Engine eng;
    eng.prepare (fs);
    eng.setParams (p);

    // 1) Lernen: Engine ohne EQ laufen lassen, Tap (nach Lowcut) in den Learner
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
    std::printf ("gelernt: valid=%d sprache=%.1fs tonhoehe=%.0fHz register=%d\n", prof.valid, prof.speechSeconds, prof.pitchHz, reg);

    const auto corr = dop::correctionCurve (prof.curveDb, reg, amount, p.lowcutHz);
    const auto bank = dop::fitBank (corr, fs);
    std::printf ("filterbank: max. Abweichung zur Zielkurve %.2f dB\n", bank.maxErrorDb);
    std::printf ("korrektur dB @ 100/250/1k/3k/6k/10k/15k/18k: ");
    for (double hz : { 100.0, 250.0, 1000.0, 3000.0, 6000.0, 10000.0, 15000.0, 18000.0 })
        std::printf ("%.1f ", dop::gridInterp (corr, hz));
    std::printf ("\n");
    FILE* cf = std::fopen ((std::string (argv[2]) + ".curve.txt").c_str(), "w");
    for (int i = 0; i < dop::kRefGridPoints; ++i) std::fprintf (cf, "%f %f %f\n", dop::kRefGridHz[i], prof.curveDb[(size_t) i], corr[(size_t) i]);
    std::fclose (cf);

    // 2) Verarbeiten mit gelernter Bank
    eng.reset();
    eng.setBank (bank.coeffs);
    std::vector<float> y (x);
    float maxLim = 0, maxDe = 0;
    for (long i = 0; i < n; i += 480)
    {
        const int m = (int) std::min (480L, n - i);
        float* ch[1] = { y.data() + i };
        eng.process (ch, 1, m, nullptr);
        maxLim = std::max (maxLim, eng.meterLimiterDb.load());
        maxDe = std::max (maxDe, eng.meterDeessDb.load());
    }
    std::printf ("latenz %d samples, leveler zuletzt %.1f dB, max limiter %.1f dB, max de-esser %.1f dB\n",
                 eng.latencySamples(), eng.meterLevelerDb.load(), maxLim, maxDe);
    FILE* o = std::fopen (argv[2], "wb");
    std::fwrite (y.data() + eng.latencySamples(), 4, (size_t) (n - eng.latencySamples()), o);
    std::fclose (o);
    return 0;
}
