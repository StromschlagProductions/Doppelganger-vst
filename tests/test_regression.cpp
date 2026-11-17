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
void transitionTests()
{
    dop::Engine uninterrupted, repeated, retargeted;
    auto boost = dop::flatBank().coeffs, cut = boost;
    boost[8] = dop::peaking (48000, 1000, 2.1, 6);
    cut[8] = dop::peaking (48000, 1000, 2.1, -6);
    for (auto* e : { &uninterrupted, &repeated, &retargeted })
    { e->prepare (48000); e->setParams (bypass()); e->setBank (boost); }
    double inputEnergy = 0, outputEnergy = 0;
    for (int i = 0; i < 12000; ++i)
    {
        const float input = (float) (0.1 * std::sin (2 * dop::kPi * 1000 * i / 48000));
        float a = input, b = input, c = input;
        if (i == 240) { repeated.setBank (boost); retargeted.setBank (cut); }
        float* pa[] = { &a }; float* pb[] = { &b }; float* pc[] = { &c };
        uninterrupted.process (pa, 1, 1, nullptr);
        repeated.process (pb, 1, 1, nullptr);
        retargeted.process (pc, 1, 1, nullptr);
        CHECK (a == b); // repeating the same requested bank must never interrupt audio
        if (i < 1440) CHECK (a == c); // a new target cannot jump out of the ongoing 30-ms fade
        if (i >= 6000) { inputEnergy += (double) input * input; outputEnergy += (double) c * c; }
    }
    CHECK (std::abs (10 * std::log10 (outputEnergy / inputEnergy) + 6) < 0.01);
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
void fineTests()
{
    const int n = 48000;
    std::vector<float> x (n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = (float) (0.1 * std::sin (2 * dop::kPi * 1000.0 * i / 48000.0));
    auto run = [&] (const dop::EngineParams& p)
    {
        dop::Engine e;
        e.prepare (48000);
        e.setParams (p);
        e.setBank (dop::flatBank().coeffs);
        e.reset();
        auto y = x;
        float* ptr[] = { y.data() };
        e.process (ptr, 1, n, nullptr);
        return y;
    };
    auto rmsDb = [&] (const std::vector<float>& v)
    {
        double acc = 0;
        for (int i = n / 2; i < n; ++i) acc += (double) v[(size_t) i] * v[(size_t) i];
        return 10.0 * std::log10 (acc / (n / 2));
    };
    const auto base = run (bypass());

    dop::EngineParams off = bypass();                       // Feinabstimmung aus
    CHECK (run (off) == base);
    dop::EngineParams flat = bypass(); flat.fineOn = true;  // an, aber alle Baender 0 dB
    CHECK (run (flat) == base);
    dop::EngineParams boost = bypass(); boost.fineOn = true;
    boost.fineHz[2] = 1000.0f; boost.fineDb[2] = 6.0f; boost.fineQ[2] = 1.0f;
    const double gainDb = rmsDb (run (boost)) - rmsDb (base);
    CHECK (std::abs (gainDb - 6.0) < 0.3);                  // Sinus genau auf der Mittenfrequenz
    dop::EngineParams nonFinite = bypass(); nonFinite.fineOn = true;
    nonFinite.fineDb[1] = std::numeric_limits<float>::quiet_NaN();
    CHECK (run (nonFinite) == base);                        // ungueltiger Wert faellt auf 0 dB zurueck
    dop::EngineParams extreme = bypass(); extreme.fineOn = true;
    for (int k = 0; k < dop::kFineBands; ++k) { extreme.fineDb[(size_t) k] = 12.0f; extreme.fineQ[(size_t) k] = 0.3f; }
    for (float v : run (extreme)) CHECK (std::isfinite (v));
}

void fineRateAndAutomationTests()
{
    auto params = bypass(); params.fineOn = true;
    params.fineHz.fill (20000); params.fineDb.fill (12); params.fineQ.fill (0.3f);
    for (double fs : { 8000., 22050., 44100., 48000., 96000., 192000. })
    {
        const auto curve = dop::fineCurveDb (true, params.fineHz, params.fineDb, params.fineQ, fs);
        for (float v : curve) CHECK (std::isfinite (v));
        dop::Engine e; e.prepare (fs); e.setParams (params); e.reset();
        float previous = 0;
        for (int i = 0; i < (int) fs; ++i)
        {
            float sample = (float) (0.001 * std::sin (i * 0.1)); float* ptr[] = { &sample };
            e.process (ptr, 1, 1, nullptr); CHECK (std::isfinite (sample) && std::abs (sample) < 10);
            previous = sample;
        }
        (void) previous;
    }
    // Prepare at a different rate must redesign unchanged parameter values.
    dop::Engine reused, fresh; reused.prepare (48000); reused.setParams (params);
    reused.prepare (22050); reused.setParams (params); reused.reset();
    fresh.prepare (22050); fresh.setParams (params); fresh.reset();
    for (int i = 0; i < 4000; ++i)
    {
        float a = (float) (0.001 * std::sin (i * 0.3)), b = a;
        float* pa[] = { &a }; float* pb[] = { &b };
        reused.process (pa, 1, 1, nullptr); fresh.process (pb, 1, 1, nullptr); CHECK (a == b);
    }
    dop::FineEq reference, changed; reference.prepare (48000); changed.prepare (48000);
    auto hz = params.fineHz, db = params.fineDb, q = params.fineQ;
    hz.fill (1000); db.fill (0); db[0] = 12; q.fill (1);
    reference.set (true, hz, db, q); changed.set (true, hz, db, q);
    for (int i = 0; i < 1500; ++i)
    {
        if (i == 200) changed.set (false, hz, db, q);
        const double x = 0.1 * std::sin (i * 0.1);
        const double a = reference.process (0, x), b = changed.process (0, x);
        if (i < 1440) CHECK (a == b);
        reference.advance(); changed.advance();
    }
    for (int i = 0; i < 4000; ++i) { changed.process (0, 0); changed.advance(); }
    CHECK (changed.process (0, 0.123) == 0.123); // settled bypass has no stale filter tail
}

void removedListenTests()
{
    for (double fs : { 22050., 44100., 48000., 96000., 192000. })
    {
        dop::Engine normal, monitor;
        auto p = bypass(); p.deessOn = true; p.deess = 1; p.deessFreqHz = 3000;
        normal.prepare (fs); monitor.prepare (fs); normal.setParams (p);
        p.deessListen = true; monitor.setParams (p);
        double removedEnergy = 0, processedEnergy = 0;
        for (int i = 0; i < (int) fs; ++i)
        {
            float a = (float) (0.1 * std::sin (2 * dop::kPi * 6000 * i / fs)), b = a, original = a;
            float* pa[] = { &a }; float* pb[] = { &b };
            normal.process (pa, 1, 1, nullptr); monitor.process (pb, 1, 1, nullptr);
            if (i > fs * 0.2)
            {
                CHECK (std::abs (a + b - original) < 1e-6f);
                removedEnergy += (double) b * b; processedEnergy += (double) a * a;
            }
        }
        CHECK (removedEnergy > processedEnergy); // actively removes this sustained sibilant
        p.deessOn = false;
        monitor.setParams (p);
        for (int i = 0; i < (int) fs; ++i)
        {
            float x = (float) (0.1 * std::sin (i * .1)); float* ptr[] = { &x };
            monitor.process (ptr, 1, 1, nullptr);
            if (i > fs * .8) CHECK (std::abs (x) < 1e-7f);
        }
        monitor.reset(); // Listen + disabled De-Esser is silence immediately after reset.
        float x = .1f; float* ptr[] = { &x }; monitor.process (ptr, 1, 1, nullptr); CHECK (x == 0);
    }
}

int main() { spectrumTests(); engineTests(); transitionTests(); learningTests(); fineTests(); fineRateAndAutomationTests(); removedListenTests(); std::puts ("DSP, FFT and learning regressions passed"); }
