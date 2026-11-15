#pragma once
#include "../Source/DoppelgangerDSP.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)
inline std::vector<float> voice (double fs, double seconds)
{
    std::vector<float> x ((size_t) (fs * seconds));
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = i / fs;
        x[i] = std::fmod (t, 3.0) < 2.6 ? (float) (0.12 * (0.6 + 0.4 * std::pow (std::sin (2 * dop::kPi * 2.3 * t), 2))
            * (std::sin (2 * dop::kPi * 125 * t) + 0.4 * std::sin (2 * dop::kPi * 250 * t)
               + 0.15 * std::sin (2 * dop::kPi * 1500 * t))) : 0.0f;
    }
    return x;
}
