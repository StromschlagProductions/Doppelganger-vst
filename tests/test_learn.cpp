// Testet das automatische Lernende: gibt alle ~1 s Sprache den Abstand zum vorigen Stand aus.
//   test_learn IN.f32 SAMPLERATE
#include "../Source/DoppelgangerDSP.h"
#include <cstdio>
#include <cstdlib>

int main (int argc, char** argv)
{
    if (argc < 3) { std::fprintf (stderr, "usage: test_learn IN.f32 SAMPLERATE\n"); return 1; }
    const double fs = std::atof (argv[2]);
    if (! std::isfinite (fs) || fs < 8000 || fs > 768000) return 1;
    FILE* f = std::fopen (argv[1], "rb");
    if (f == nullptr) { std::perror ("input"); return 1; }
    if (std::fseek (f, 0, SEEK_END) != 0) { std::fclose (f); return 1; } const long bytes = std::ftell (f);
    if (bytes <= 0 || bytes % 4 != 0) { std::fclose (f); return 1; }
    const long n = bytes / 4; std::fseek (f, 0, SEEK_SET);
    std::vector<float> x ((size_t) n);
    if (std::fread (x.data(), 4, (size_t) n, f) != (size_t) n) { std::fclose (f); return 1; }
    std::fclose (f);
    dop::Learner L; L.prepare (fs);
    dop::ConvergenceTracker t;
    float next = 10.0f; bool done = false;
    const int blk = (int) (fs * 0.02);
    for (long i = 0; i + blk <= n; i += blk)
    {
        L.push (x.data() + i, blk);
        const float sec = L.speechSeconds();
        if (sec >= next)
        {
            next = sec + 1.0f;
            const auto p = L.finish();
            const bool c = t.update (p);
            std::printf ("t=%5.1f s speech  dist=%.2f dB  pitch=%.1f  lufs=%.2f stable=%d%s\n", sec, t.lastCurveDist, p.pitchHz, p.loudnessLufs, t.stable, c && ! done ? "  <-- STOP" : "");
            if (c) done = true;
        }
    }
    std::printf ("total speech %.1f s\n", L.speechSeconds());
}
