// Reproducible Engine-only benchmark. Build with -O3 -DNDEBUG for both revisions.
#include "../Source/DoppelgangerDSP.h"
#include <chrono>
#include <cstdio>
#include <vector>

int main()
{
    constexpr int blockSize = 256, warmupBlocks = 1000, measuredBlocks = 10000;
    for (bool active : { false, true })
    {
        dop::Engine engine;
        engine.prepare (48000);
        engine.setParams (dop::EngineParams {});
        if (active)
        {
            dop::GridCurve correction {};
            for (size_t i = 0; i < correction.size(); ++i)
                correction[i] = (float) (4 * std::sin (i * 0.1));
            engine.setBank (dop::fitBank (correction, 48000).coeffs);
        }
        std::vector<double> times;
        times.reserve (measuredBlocks);
        double checksum = 0;
        for (int b = 0; b < warmupBlocks + measuredBlocks; ++b)
        {
            float left[blockSize], right[blockSize];
            for (int i = 0; i < blockSize; ++i)
                left[i] = right[i] = (float) (0.1 * std::sin ((b * blockSize + i) * 0.031));
            float* channels[] = { left, right };
            const auto start = std::chrono::steady_clock::now();
            engine.process (channels, 2, blockSize, nullptr);
            const auto end = std::chrono::steady_clock::now();
            if (b >= warmupBlocks)
                times.push_back (std::chrono::duration<double, std::micro> (end - start).count());
            checksum += left[0];
        }
        double sum = 0;
        for (double t : times) sum += t;
        std::sort (times.begin(), times.end());
        std::printf ("%s mean_us=%.3f p99_us=%.3f max_us=%.3f checksum=%.9f\n",
                     active ? "active" : "neutral", sum / times.size(), times[times.size() * 99 / 100], times.back(), checksum);
    }
}
