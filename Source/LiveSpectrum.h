#pragma once
#include "AudioTransport.h"
#include "DoppelgangerDSP.h"

namespace dop
{
// Consumer-owned FFT storage; never accessed by the audio producer.
class LiveSpectrum
{
public:
    bool analyse (const LiveFrame& frame, GridCurve& inDb, GridCurve& outDb)
    {
        if (! std::isfinite (frame.sampleRate) || frame.sampleRate <= 0.0) return false;
        double energy = 0;
        for (float x : frame.input) energy += (double) x * x;
        if (! std::isfinite (energy) || energy / kLiveFft < 9e-8) return false;
        if (fs != frame.sampleRate) prepare (frame.sampleRate);
        transform (frame.input, frame.end, inDb);
        transform (frame.output, frame.end, outDb);
        return true;
    }
private:
    struct Tap { int first = 0; std::vector<float> weights; };
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        fft.prepare (kLiveFft);
        work.resize (kLiveFft);
        power.resize (kLiveFft / 2 + 1);
        for (int i = 0; i < kLiveFft; ++i)
            window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * kPi * i / kLiveFft);
        const double binHz = fs / kLiveFft, sigma = 0.25;
        for (int g = 0; g < kRefGridPoints; ++g)
        {
            auto& t = taps[(size_t) g];
            t.weights.clear();
            const double f = kRefGridHz[g];
            // There is no measured spectrum above Nyquist: leave the tap empty.
            if (f >= fs * 0.5) continue;
            const int first = (int) std::clamp (std::floor (f * std::exp2 (-3 * sigma) / binHz), 1.0, (double) kLiveFft / 2);
            const int last = (int) std::clamp (std::ceil (f * std::exp2 (3 * sigma) / binHz), (double) first, (double) kLiveFft / 2);
            t.first = first;
            t.weights.resize ((size_t) (last - first + 1));
            double sum = 0;
            for (int k = first; k <= last; ++k)
            {
                const double d = std::log2 (k * binHz / f) / sigma;
                const float w = (float) std::exp (-0.5 * d * d);
                t.weights[(size_t) (k - first)] = w;
                sum += w;
            }
            if (sum > 0) for (auto& w : t.weights) w = (float) (w / sum);
        }
    }
    void transform (const std::array<float, kLiveFft>& samples, int end, GridCurve& out)
    {
        for (int i = 0; i < kLiveFft; ++i)
            work[(size_t) i] = (double) samples[(size_t) ((end + i) & (kLiveFft - 1))] * window[(size_t) i];
        fft.forward (work);
        for (size_t k = 0; k < power.size(); ++k) power[k] = std::norm (work[k]);
        double mean = 0;
        int count = 0;
        for (int g = 0; g < kRefGridPoints; ++g)
        {
            const auto& t = taps[(size_t) g];
            double sum = 0;
            for (size_t j = 0; j < t.weights.size(); ++j) sum += t.weights[j] * power[(size_t) t.first + j];
            out[(size_t) g] = (float) (10.0 * std::log10 (sum + 1e-20));
            if (! t.weights.empty() && kRefGridHz[g] >= 100 && kRefGridHz[g] <= 8000)
            { mean += out[(size_t) g]; ++count; }
        }
        mean /= std::max (1, count);
        for (size_t g = 0; g < out.size(); ++g)
            out[g] = taps[g].weights.empty() ? -60.0f : (float) std::clamp ((double) out[g] - mean, -60.0, 40.0);
    }
    double fs = 0;
    FFT fft;
    std::vector<std::complex<double>> work;
    std::vector<double> power;
    std::array<double, kLiveFft> window {};
    std::array<Tap, (size_t) kRefGridPoints> taps;
};
}
