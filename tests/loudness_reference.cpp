#include "TestSupport.h"
#include "../Source/DoppelgangerDSP.h"
#include <fstream>
#include <iomanip>
#include <iostream>
int main (int argc, char** argv)
{
    CHECK (argc == 2);
    const double fs = 48000;
    auto x = voice (fs, 8);
    dop::Learner learn; learn.prepare (fs); learn.push (x.data(), (int) x.size());
    const auto profile = learn.finish(); CHECK (profile.valid);
    struct Case { const char* name; int channels; double left, right; };
    for (const auto& test : { Case { "mono", 1, 1, 0 }, { "dual", 2, 1, 1 }, { "energy", 2, std::sqrt (.5), std::sqrt (.5) },
                             { "left", 2, 1, 0 }, { "right", 2, 0, 1 }, { "antiphase", 2, 1, -1 }, { "asymmetric", 2, 1, -.5 } })
        for (bool normalise : { false, true })
        {
            dop::Engine e; e.prepare (fs);
            dop::EngineParams p; p.lowcutHz = 20; p.deessOn = false; p.leveler = 0; p.targetLufs = -23;
            if (normalise) p.takeLufs = profile.loudnessLufs + (float) (10 * std::log10 ((test.left * test.left + test.right * test.right) / test.channels));
            e.setParams (p); e.reset();
            const std::string path = (std::string (argv[1]) + "/") + test.name + (normalise ? "-normalised" : "-raw") + ".f32";
            std::ofstream output (path, std::ios::binary);
            float l[512], r[512]; float* ptr[] { l, r };
            const int end = (int) (fs * 10);
            for (int off = 0; off < end; off += 512)
            {
                const int n = std::min (512, end - off);
                for (int i = 0; i < n; ++i)
                {
                    const float v = off + i < (int) x.size() ? x[(size_t) (off + i)] : 0;
                    l[i] = (float) (test.left * v); r[i] = (float) (test.right * v);
                }
                e.process (ptr, test.channels, n, nullptr);
                for (int i = 0; i < n; ++i) for (int c = 0; c < test.channels; ++c) output.write ((const char*) &ptr[c][i], sizeof (float));
            }
            std::cout << path << '\t' << test.channels << '\t' << std::setprecision (8) << e.meterIntLufs.load() << '\n';
        }
}
