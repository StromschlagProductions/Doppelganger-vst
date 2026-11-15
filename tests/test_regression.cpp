#include "TestSupport.h"
#include "../Source/LiveSpectrum.h"
#include <limits>

void spectrumTests()
{
    dop::LiveSpectrum analyser;
    dop::LiveFrame frame;
    dop::GridCurve in {}, out {};
    CHECK (! analyser.analyse (frame, in, out));
    for (double fs : { 1000.0, 8000.0, 11025.0, 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        frame.sampleRate = fs;
        for (int i = 0; i < dop::kLiveFft; ++i)
            frame.input[(size_t) i] = frame.output[(size_t) i] = (float) (0.1 * std::sin (2 * dop::kPi * 300 * i / fs));
        CHECK (analyser.analyse (frame, in, out));
        for (size_t i = 0; i < in.size(); ++i)
        {
            CHECK (std::isfinite (in[i]) && in[i] >= -60 && in[i] <= 40);
            CHECK (in[i] == out[i]);
            if (dop::kRefGridHz[i] >= fs * 0.5) CHECK (in[i] == -60.0f);
        }
    }
    frame.sampleRate = 0; CHECK (! analyser.analyse (frame, in, out));
    frame.sampleRate = std::numeric_limits<double>::quiet_NaN(); CHECK (! analyser.analyse (frame, in, out));
    dop::FFT fft;
    bool threw = false;
    try { fft.prepare (3000); } catch (const std::invalid_argument&) { threw = true; }
    CHECK (threw);
    for (int n : { 2, 256, 4096, 16384 })
    {
        fft.prepare (n);
        std::vector<std::complex<double>> v ((size_t) n);
        v[0] = 1.0; fft.forward (v);
        for (auto z : v) CHECK (std::abs (z - 1.0) < 1e-9);
        fft.inverse (v); CHECK (std::abs (v[0] - 1.0) < 1e-9);
        for (int i = 1; i < n; ++i) CHECK (std::abs (v[(size_t) i]) < 1e-9);
    }
    std::vector<std::complex<double>> wrong (1);
    threw = false; try { fft.forward (wrong); } catch (const std::invalid_argument&) { threw = true; } CHECK (threw);
}

dop::EngineParams bypass()
{
    dop::EngineParams p; p.lowcutHz = 20; p.deessOn = false; p.leveler = 0; return p;
}
std::vector<float> render (double fs, int channels, bool variable)
{
    dop::Engine e; e.prepare (fs); e.setParams (bypass());
    auto bank = dop::flatBank().coeffs;
    bank[8] = dop::peaking (fs, 800, 2.1, 6);
    e.setBank (bank);
    auto x = voice (fs, 0.8), right = x;
    const int sizes[] = { 1, 7, 31, 127, 512, 8193 };
    for (size_t off = 0, b = 0; off < x.size(); ++b)
    {
        const int n = (int) std::min (x.size() - off, (size_t) (variable ? sizes[b % 6] : 256));
        float* ptr[] = { x.data() + off, right.data() + off };
        e.process (ptr, channels, n, nullptr); off += (size_t) n;
    }
    if (channels == 2) CHECK (x == right);
    return x;
}
void engineTests()
{
    for (double fs : { 8000., 22050., 44100., 48000., 96000., 192000. })
    {
        for (int channels : { 1, 2 }) CHECK (render (fs, channels, true) == render (fs, channels, false));
        const auto peakIdentity = dop::peaking (fs, 1000, 2.1, 0);
        const auto shelfIdentity = dop::highShelf (fs, 3000, 0);
        CHECK (peakIdentity.b0 == 1 && peakIdentity.b1 == 0 && peakIdentity.a1 == 0);
        CHECK (shelfIdentity.b0 == 1 && shelfIdentity.b1 == 0 && shelfIdentity.a1 == 0);
        dop::Engine e; e.prepare (fs); e.setParams (bypass());
        auto x = voice (fs, 0.1), expected = x;
        float* ptr[] = { x.data() }; e.process (ptr, 1, (int) x.size(), nullptr); CHECK (x == expected);
        // A fitted zero-gain bank must remain sample-exact neutral.
        e.setBank (dop::fitBank (dop::GridCurve {}, fs).coeffs);
        x = expected; e.process (ptr, 1, (int) x.size(), nullptr); CHECK (x == expected);
        auto bank = dop::flatBank().coeffs; bank[9] = dop::peaking (fs, 1000, 2.1, 6);
        e.setBank (bank);
        std::vector<float> block (97), right (97);
        double previous = 0, maxStep = 0;
        for (int b = 0; b < 600; ++b)
        {
            if (b % 5 == 0) e.setBank (b % 10 == 0 ? bank : dop::flatBank().coeffs);
            for (int i = 0; i < 97; ++i) block[(size_t) i] = right[(size_t) i] = (float) (0.1 * std::sin (2 * dop::kPi * 1000 * (b * 97 + i) / fs));
            float* pp[] = { block.data(), right.data() }; e.process (pp, 2, 97, nullptr);
            CHECK (block == right);
            for (float v : block) { CHECK (std::isfinite (v) && std::abs (v) < 0.4); maxStep = std::max (maxStep, std::abs (v - previous)); previous = v; }
        }
        CHECK (maxStep < 0.18); // bounded by the sine's natural maximum step at 8 kHz
        e.setBank (dop::flatBank().coeffs); e.reset();
        x = expected; ptr[0] = x.data(); e.process (ptr, 1, (int) x.size(), nullptr); CHECK (x == expected);
        // Extremes, silence, low levels, and hostile non-finite input must recover.
        dop::EngineParams p; p.lowcutHz = 200; p.deess = 1; p.deessFreqHz = 10000; p.leveler = 1; p.targetLufs = -14; p.takeLufs = -70;
        e.setParams (p);
        for (float value : { 0.f, 1e-30f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
        { std::fill (block.begin(), block.end(), value); float* q[] = { block.data() }; e.process (q, 1, 97, nullptr); for (float v : block) CHECK (std::isfinite (v)); }
        e.process (nullptr, 0, 0, nullptr);
    }
}
void learningTests()
{
    for (double fs : { 22050., 44100., 48000., 96000., 192000. })
    {
        dop::Learner learner; learner.prepare (fs);
        std::vector<float> silence ((size_t) fs, 0.0f); learner.push (silence.data(), (int) silence.size()); CHECK (! learner.finish().valid);
        auto x = voice (fs, 5.0); learner.reset(); learner.push (x.data(), 1); CHECK (! learner.finish().valid);
        learner.reset(); learner.push (x.data(), (int) x.size()); auto p = learner.finish();
        CHECK (p.valid && dop::isValidProfile (p)); CHECK (p.speechSeconds > 4); CHECK (std::abs (p.pitchHz - 125) < 2);
        CHECK (p.loudnessLufs > -30 && p.loudnessLufs < -20);
        learner.reset();
        for (size_t off = 0; off < x.size(); off += 137) learner.push (x.data() + off, (int) std::min ((size_t) 137, x.size() - off));
        auto repeated = learner.finish(); CHECK (p.curveDb == repeated.curveDb); CHECK (p.pitchHz == repeated.pitchHz);
        auto correction = dop::correctionCurve (p.curveDb, dop::kTief, 1, 80);
        for (float v : correction) CHECK (std::isfinite (v) && std::abs (v) <= 9.001);
        const auto bank = dop::fitBank (correction, fs); CHECK (std::isfinite (bank.maxErrorDb) && bank.maxErrorDb < 5);
        dop::ConvergenceTracker tracker;
        CHECK (! tracker.update (p)); CHECK (! tracker.update (p)); CHECK (! tracker.update (p)); CHECK (tracker.update (p));
        tracker.reset(); CHECK (! tracker.update (p));
        p.curveDb[0] = std::numeric_limits<float>::quiet_NaN(); CHECK (! dop::isValidProfile (p));
    }
}
int main() { spectrumTests(); engineTests(); learningTests(); std::puts ("DSP, FFT and learning regressions passed"); }
