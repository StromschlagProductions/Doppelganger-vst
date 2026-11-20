#include <cstdlib>
#include "TestSupport.h"
#include "SpeechSynth.h"
#include "../Source/LiveSpectrum.h"
#include <limits>
#include <atomic>
#include <tuple>

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
        // Angleich-Bereich: ausserhalb 0 dB, innen unveraendert, Uebergang hoechstens eine Drittel-Oktave
        {
            const auto full = dop::correctionCurve (p.curveDb, dop::kTief, 1, 80, 20, 20000);
            const auto lim = dop::correctionCurve (p.curveDb, dop::kTief, 1, 80, 200, 6000);
            for (int i = 0; i < dop::kRefGridPoints; ++i)
            {
                const double f = dop::kRefGridHz[i];
                if (f < 200 / std::pow (2.0, 1.0 / 3) || f > 6000 * std::pow (2.0, 1.0 / 3)) CHECK (std::abs (lim[(size_t) i]) < 1e-6f);
                if (f >= 200 && f <= 6000) CHECK (lim[(size_t) i] == full[(size_t) i]);
            }
            const auto def = dop::correctionCurve (p.curveDb, dop::kTief, 1, 80);
            CHECK (def == full);   // ohne Angabe keine Grenze (Abwaertskompatibilitaet der Funktion)
            const auto crossed = dop::correctionCurve (p.curveDb, dop::kTief, 1, 80, 2000, 1000);   // vertauscht: wird abgefangen
            for (float v : crossed) CHECK (std::isfinite (v));
            // Gefilterter Frequenzgang folgt: bei 15 kHz mit Grenze 10 kHz praktisch keine Korrektur
            const auto hiLim = dop::correctionCurve (p.curveDb, dop::kTief, 1, 80, 20, 10000);
            if (fs >= 44100) {
            const auto b2 = dop::fitBank (hiLim, fs);
            double resp = 0; for (const auto& c : b2.coeffs) resp += dop::magnitudeDb (c, fs, 15000.0);
            if (std::getenv ("DOP_VERBOSE")) std::printf ("Bereich: fs %d, 15 kHz %.2f dB\n", (int) fs, resp);
            CHECK (std::abs (resp) < 1.0);
            }
        }
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

// De-Esser reagiert auf Zischlaute, nicht auf helle Vokale
void deesserSelectivity()
{
    const int fs = 48000, n = fs;
    std::vector<float> vowel ((size_t) n), sib ((size_t) n);
    uint32_t r = 12345;
    auto noise = [&] { r = r * 1664525u + 1013904223u; return (double) (int32_t) r / 2147483648.0; };
    for (int i = 0; i < n; ++i)   // heller Vokal: Obertoene von 180 Hz bis 6 kHz, -6 dB/Oktave
    {
        double v = 0;
        for (int h = 1; 180 * h < 9000; ++h)   // helles A: Formanten um 800 Hz und 2,8 kHz, Glanz bis 9 kHz
        {
            const double f = 180.0 * h;
            const double g = 1.0 / h + 0.5 * std::exp (-std::pow ((f - 800) / 300, 2)) + 0.15 * std::exp (-std::pow ((f - 2800) / 500, 2)) + 0.02;
            v += g * std::sin (2 * dop::kPi * f * i / fs);
        }
        vowel[(size_t) i] = (float) (0.05 * v);
    }
    auto bp = dop::highPass (fs, 5000, 0.7071), lp = dop::lowPass (fs, 10000, 0.7071);
    dop::BiquadState s1, s2;
    for (int i = 0; i < n; ++i)   // S mit Stimmanteil darunter (wie im Wort, Uebergang zum Vokal)
        sib[(size_t) i] = (float) (0.15 * s2.process (lp, s1.process (bp, noise())) + 0.4 * vowel[(size_t) i]);
    auto reductionDb = [&] (const std::vector<float>& x, float amount)
    {
        auto run = [&] (bool on)
        {
            dop::Engine e; e.prepare (fs);
            dop::EngineParams p = bypass(); p.deessOn = on; p.deess = amount; p.deessFreqHz = 5500;
            e.setParams (p); e.setBank (dop::flatBank().coeffs); e.reset();
            auto y = x;
            for (int o = 0; o < n; o += 256) { float* q[] = { y.data() + o }; e.process (q, 1, std::min (256, n - o), nullptr); }
            auto hp = dop::highPass (fs, 5000, 0.7071); dop::BiquadState h1;
            double acc = 0; for (int i = n / 4; i < n; ++i) { const double v = h1.process (hp, y[(size_t) i]); acc += v * v; }
            return acc;
        };
        return 10.0 * std::log10 (run (true) / run (false));
    };
    const double dv = reductionDb (vowel, 0.25f), ds = reductionDb (sib, 0.25f);
    const double dvMax = reductionDb (vowel, 1.0f), dsMax = reductionDb (sib, 1.0f);
    if (std::getenv ("DOP_VERBOSE")) std::printf ("De-Esser: Standard Vokal %.2f / S %.2f dB, voll Vokal %.2f / S %.2f dB\n", dv, ds, dvMax, dsMax);
    CHECK (dv > -0.3 && dvMax > -0.5);   // heller Vokal bleibt praktisch unberuehrt, auch bei voller Staerke
    CHECK (ds < -3.2);                   // Zischlaut wird schon bei Standardstaerke deutlich abgesenkt (1.3: -2,6 dB)
    CHECK (dsMax < ds - 2.0);            // mehr Staerke = mehr Absenkung
}

// Integrierte Lautheit: Eichung mit 997-Hz-Sinus und Gate (Stille zaehlt nicht)
void integratedLoudnessTests()
{
    for (double fs : { 44100., 48000., 96000. })
    {
        dop::Engine e; e.prepare (fs);
        dop::EngineParams p = bypass(); p.lowcutHz = 20; p.deessOn = false;
        e.setParams (p); e.setBank (dop::flatBank().coeffs); e.reset();
        const int n = (int) fs * 16;
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i) x[(size_t) i] = i < (int) fs * 6 ? (float) (0.1 * std::sin (2 * dop::kPi * 997.0 * i / fs)) : 0.0f;   // 6 s Ton, 10 s Stille
        for (int o = 0; o < n; o += 512) { float* q[] = { x.data() + o }; e.process (q, 1, std::min (512, n - o), nullptr); }
        const float L = e.meterIntLufs.load();
        if (std::getenv ("DOP_VERBOSE")) std::printf ("Integriert fs %.0f: %.2f LUFS nach %.1f s\n", fs, L, e.meterIntSeconds.load());
        CHECK (std::abs (L - (-23.0f)) < 0.15f);   // Sinus 0,1 Spitze = -23,0 LUFS; Stille wird vom Gate ausgeblendet
        e.resetIntegrated();
        float* q[] = { x.data() }; e.process (q, 1, 64, nullptr);
        CHECK (e.meterIntLufs.load() == -100.0f && e.meterIntSeconds.load() == 0.0f);
    }
}

void channelLoudnessTests()
{
    struct Reading { float integrated, momentary, ride; };
    constexpr double stereoDb = 3.01029995664;
    for (double fs : { 22050., 44100., 48000., 96000., 192000. })
    {
        auto measure = [fs] (int channels, double left, double right, bool normalise, bool leveler)
        {
            dop::Engine e; e.prepare (fs);
            auto p = bypass();
            p.targetLufs = -23; p.leveler = leveler ? 1.0f : 0.0f;
            const double energy = left * left + (channels == 2 ? right * right : 0);
            if (normalise) p.takeLufs = (float) (-23.0 + 10.0 * std::log10 (energy / channels));
            e.setParams (p); e.reset();
            float l[512], r[512]; float* ptr[] { l, r };
            for (int off = 0, end = (int) (fs * 8); off < end; off += 512)
            {
                const int n = std::min (512, end - off);
                for (int i = 0; i < n; ++i)
                {
                    const float x = (float) (0.1 * std::sin (2 * dop::kPi * 997 * (off + i) / fs));
                    l[i] = (float) (left * x); r[i] = (float) (right * x);
                }
                e.process (ptr, channels, n, nullptr);
            }
            return Reading { e.meterIntLufs.load(), e.meterOutLufs.load(), e.meterRideDb.load() };
        };
        const auto mono = measure (1, 1, 0, false, false);
        CHECK (std::abs (mono.integrated + 23.0f) < .15f);
        for (double phase : { -1., 1. })
        {
            const auto stereo = measure (2, 1, phase, false, false);
            if (std::getenv ("DOP_VERBOSE")) std::printf ("Channels fs %.0f: mono %.3f stereo %.3f, momentary delta %.6f\n", fs, mono.integrated, stereo.integrated, stereo.momentary - mono.momentary);
            CHECK (std::abs (stereo.integrated - mono.integrated - stereoDb) < .05); // histogram resolution 0.05 LU
            CHECK (std::abs (stereo.momentary - mono.momentary - stereoDb) < .001);
        }
        for (const auto& gains : { std::array<double, 2> { 1, 0 }, { 0, 1 }, { std::sqrt (.5), std::sqrt (.5) } })
        {
            const auto stereo = measure (2, gains[0], gains[1], false, false);
            CHECK (std::abs (stereo.integrated - mono.integrated) < .01);
        }
        for (bool leveler : { false, true })
        {
            const auto normalMono = measure (1, 1, 0, true, leveler);
            for (const auto& gains : { std::array<double, 2> { 1, 1 }, { 1, 0 }, { 0, 1 }, { .8, .3 } })
            {
                const auto stereo = measure (2, gains[0], gains[1], true, leveler);
                CHECK (std::abs (stereo.integrated - normalMono.integrated) < .05);
                CHECK (std::abs (stereo.momentary - normalMono.momentary) < .01);
                CHECK (std::abs (stereo.ride - normalMono.ride) < .01);
                if (! leveler) CHECK (std::abs (stereo.integrated + 23.0f) < .15f);
            }
        }
    }
}

// Autopilot: Einstellungen aus dem Gelernten, Pruefung der Aufnahme und Nachmessung der Lautheit
void autopilotTests()
{
    // Angleichung: "so wenig wie moeglich, so viel wie noetig", immer im erlaubten Bereich, in 5-%-Schritten
    for (int reg : { dop::kTief, dop::kMittel, dop::kHoch })
    {
        dop::GridCurve neutral {};
        for (int i = 0; i < dop::kRefGridPoints; ++i) neutral[(size_t) i] = dop::referenceCurve (reg)[i];
        CHECK (dop::autopilot::colourationDb (neutral, reg, 80, 20, 12000) < 0.01f);
        CHECK (dop::autopilot::suggestAmount (neutral, reg, 80, 20, 12000) == dop::autopilot::kMinAmount);
        float previous = 0;
        for (float tilt : { 1.f, 2.f, 4.f, 8.f, 16.f })   // Mikrofon immer dumpfer
        {
            auto take = neutral;
            for (int i = 0; i < dop::kRefGridPoints; ++i) take[(size_t) i] -= tilt * (float) std::log2 (dop::kRefGridHz[i] / 1000.0f) / 4.0f;
            const float a = dop::autopilot::suggestAmount (take, reg, 80, 20, 12000);
            CHECK (a >= dop::autopilot::kMinAmount && a <= dop::autopilot::kMaxAmount);
            CHECK (std::abs (a * 20 - std::round (a * 20)) < 1e-4f);
            CHECK (a >= previous);   // staerkere Faerbung -> nie weniger Angleichung
            previous = a;
            // Was nach dem Angleich bleibt, ist hoechstens die Toleranz (ausser die Obergrenze greift)
            const auto full = dop::correctionCurve (take, reg, 1, 80, 20, 12000);
            const auto used = dop::correctionCurve (take, reg, a, 80, 20, 12000);
            dop::GridCurve rest {}; for (size_t i = 0; i < rest.size(); ++i) rest[i] = full[i] - used[i];
            if (a < dop::autopilot::kMaxAmount) CHECK (dop::autopilot::meanAbsDb (rest) <= dop::autopilot::kToleranceDb + 0.06f);
        }
        CHECK (previous == dop::autopilot::kMaxAmount);
    }
    // Lowcut nach Tonhoehe, nie unter dem Lowcut, mit dem gelernt wurde
    CHECK (dop::autopilot::suggestLowcut (0, 80) == 80);
    CHECK (dop::autopilot::suggestLowcut (110, 80) == 80);
    CHECK (dop::autopilot::suggestLowcut (180, 80) == 100);
    CHECK (dop::autopilot::suggestLowcut (260, 80) == 110);
    CHECK (dop::autopilot::suggestLowcut (110, 120) == 120);
    CHECK (dop::autopilot::suggestLowcut (std::numeric_limits<float>::quiet_NaN(), 80) == 80);

    // Rauschabstand und Uebersteuerung beim Lernen
    {
        const double fs = 48000;
        auto x = voice (fs, 12.0);
        dop::Learner clean; clean.prepare (fs); clean.push (x.data(), (int) x.size());
        CHECK (clean.finish().noiseSnrDb == -1.0f);   // nur digitale Stille in den Pausen: nichts zu beurteilen
        uint32_t r = 99;
        for (auto& v : x) { r = r * 1664525u + 1013904223u; v += (float) (0.001 * std::sqrt (3.0) * ((double) (int32_t) r / 2147483648.0)); }
        dop::Learner noisy; noisy.prepare (fs); noisy.push (x.data(), (int) x.size(), nullptr, 1, 17);
        const auto p = noisy.finish();
        CHECK (p.valid && dop::isValidProfile (p));
        CHECK (p.noiseSnrDb > 25 && p.noiseSnrDb < dop::autopilot::kNoiseWarnSnrDb);   // Rauschen bei -60 dBFS
        CHECK (p.clippedSamples == 17);
        auto bad = p; bad.noiseSnrDb = std::numeric_limits<float>::quiet_NaN(); CHECK (! dop::isValidProfile (bad));
        bad = p; bad.clippedSamples = -1; CHECK (! dop::isValidProfile (bad));
        // Mitschnitt: begrenzt, wird beim Abholen freigegeben
        dop::Learner cap; cap.prepare (fs); cap.beginCapture (2.0); cap.push (x.data(), (int) x.size());
        auto audio = cap.takeCapturedAudio();
        CHECK (audio.size() == (size_t) (2 * fs) && std::equal (audio.begin(), audio.end(), x.begin()));
        CHECK (cap.takeCapturedAudio().empty());
        cap.reset(); cap.push (x.data(), 100); CHECK (cap.takeCapturedAudio().empty());   // ohne beginCapture nichts
    }

    // calibDb verschiebt die Grundverstaerkung exakt
    {
        auto run = [] (float calib)
        {
            dop::Engine e; e.prepare (48000); auto p = bypass(); p.takeLufs = -30; p.calibDb = calib;
            e.setParams (p); e.reset();
            auto x = voice (48000, 1.0); float* q[] = { x.data() }; e.process (q, 1, (int) x.size(), nullptr);
            return x;
        };
        const auto a = run (0), b = run (2.5f), c = run (100.0f), d = run (-100.0f);
        for (size_t i = 0; i < a.size(); i += 97) if (std::abs (a[i]) > 1e-4f) CHECK (std::abs (b[i] / a[i] - std::pow (10.0f, 2.5f / 20)) < 1e-3f);
        for (size_t i = 0; i < a.size(); i += 97) if (std::abs (a[i]) > 1e-4f)
        {
            CHECK (std::abs (c[i] / a[i] - std::pow (10.0f, dop::Engine::kMaxCalibDb / 20)) < 1e-3f);   // begrenzt
            CHECK (std::abs (d[i] / a[i] - std::pow (10.0f, -dop::Engine::kMaxCalibDb / 20)) < 1e-3f);
        }
    }

    // Leise, saubere Aufnahmen: gleiches Ergebnis wie laut, nur der Pegel unterscheidet sich
    for (double fs : { 44100., 48000., 96000. })
    {
        const auto loud = voice (fs, 12.0);
        dop::Learner a; a.prepare (fs); a.push (loud.data(), (int) loud.size());
        const auto ref = a.finish();
        for (double gainDb : { -20.0, -40.0 })
        {
            auto x = loud; for (auto& v : x) v *= (float) std::pow (10.0, gainDb / 20.0);
            dop::Learner b; b.prepare (fs); b.push (x.data(), (int) x.size());
            const auto q = b.finish();
            CHECK (q.valid && dop::isValidProfile (q));
            CHECK (std::abs (q.speechSeconds - ref.speechSeconds) < 0.5f);   // Lernen wird genauso fertig
            CHECK (std::abs (q.pitchHz - ref.pitchHz) < 1.0f);
            // bei -64 LUFS greift normgerecht das absolute -70-LUFS-Gate fuer einige leise Silbenenden
            CHECK (std::abs (q.loudnessLufs - (ref.loudnessLufs + (float) gainDb)) < (gainDb < -30 ? 0.2f : 0.05f));
            CHECK (dop::curveDistanceDb (q.curveDb, ref.curveDb) < 0.1);
        }
    }

    // Spitzenschutz: haelt -1 dBFS sample-genau, latenzfrei, ohne ihn bleibt alles bitgleich wie bisher
    for (double fs : { 22050., 48000., 192000. })
    {
        auto x = voice (fs, 3.0);
        for (size_t i = 0; i < x.size(); i += (size_t) (fs * 0.25)) x[i] = 0.9f;   // einzelne harte Spitzen (Plosive, Klicks)
        auto run = [&] (bool guard, int channels)
        {
            dop::Engine e; e.prepare (fs); auto p = bypass(); p.takeLufs = -40; p.targetLufs = -14; p.peakGuard = guard;
            e.setParams (p); e.reset();
            auto l = x, r = x; for (auto& v : r) v *= 0.5f;
            float maxGuard = 0;
            for (size_t o = 0; o < l.size(); o += 256)
            {
                float* q[] = { l.data() + o, r.data() + o };
                e.process (q, channels, (int) std::min<size_t> (256, l.size() - o), nullptr);
                maxGuard = std::max (maxGuard, e.meterGuardDb.load());
            }
            return std::make_tuple (l, r, maxGuard);
        };
        const auto [plain, plainR, g0] = run (false, 2);
        const auto [held, heldR, g1] = run (true, 2);
        const float ceiling = (float) std::pow (10.0, dop::Engine::kPeakCeilingDb / 20.0);
        float plainPeak = 0;
        for (float v : plain) plainPeak = std::max (plainPeak, std::abs (v));
        CHECK (plainPeak > 2.0f && g0 == 0.0f);        // ohne Schutz: weit ueber 0 dBFS (Anhebung um 26 dB)
        for (size_t i = 0; i < held.size(); ++i)
        {
            CHECK (std::abs (held[i]) <= ceiling * 1.0001f && std::abs (heldR[i]) <= ceiling * 1.0001f);
            if (std::abs (plain[i]) > 1e-6f && std::abs (held[i]) > 1e-6f)
                CHECK (std::abs (held[i] / plain[i] - heldR[i] / plainR[i]) < 1e-4f);   // Kanaele gekoppelt
        }
        CHECK (g1 > 6.0f);
        // leise Stellen bleiben unberuehrt: ohne Ueberschreitung exakt gleich
        const auto [quiet, quietR, g2] = [&] { auto p0 = x; for (auto& v : p0) v *= 0.01f; x.swap (p0); auto r = run (true, 1); x.swap (p0); return r; }();
        const auto [quietPlain, qR, g3] = [&] { auto p0 = x; for (auto& v : p0) v *= 0.01f; x.swap (p0); auto r = run (false, 1); x.swap (p0); return r; }();
        CHECK (quiet == quietPlain && g2 == 0.0f && g3 == 0.0f);
    }

    // Nachmessung: Die ganze Kette landet auf dem Ziel, unabhaengig von Leveler, De-Esser, Ziel und Abtastrate
    for (double fs : { 44100., 48000., 96000. })
    {
        auto x = voice (fs, 12.0);
        // etwas Zischen dazu, damit der De-Esser wirklich arbeitet
        auto hp = dop::highPass (fs, 5000, 0.7071); dop::BiquadState hs; uint32_t r = 7;
        for (size_t i = 0; i < x.size(); ++i)
            if (std::fmod (i / fs, 1.0) < 0.12) { r = r * 1664525u + 1013904223u; x[i] += (float) (0.2 * hs.process (hp, (double) (int32_t) r / 2147483648.0)); }
        dop::Learner L; L.prepare (fs); L.beginCapture (40); L.push (x.data(), (int) x.size());
        const auto prof = L.finish(); const auto audio = L.takeCapturedAudio();
        CHECK (prof.valid && audio.size() == x.size());
        const int reg = dop::registerForPitch (prof.pitchHz);
        const auto corr = dop::correctionCurve (prof.curveDb, reg, dop::autopilot::suggestAmount (prof.curveDb, reg, 80, 20, 12000), 80, 20, 12000);
        const auto bank = dop::fitBank (corr, fs).coeffs;
        const float eqd = dop::eqLoudnessDeltaDb (prof.curveDb, corr);
        const bool full = fs == 48000.;   // volle Matrix bei 48 kHz, sonst die Standardeinstellung und Extremwerte
        for (float leveler : { 0.f, .5f, 1.f })
            for (float deess : { 0.f, .25f, 1.f })
                for (float target : { -23.f, -16.f })
                {
                    if (! full && ! ((leveler == .5f && deess == .25f) || (leveler == 1.f && deess == 1.f && target == -16.f))) continue;
                    dop::EngineParams p; p.leveler = leveler; p.deess = deess; p.deessOn = deess > 0; p.targetLufs = target;
                    p.rideOffsetDb = prof.typicalOffsetDb;
                    const auto c = dop::autopilot::calibrate (audio, fs, p, bank, eqd, 80.0f);
                    if (std::getenv ("DOP_VERBOSE")) std::printf ("Nachmessung fs %.0f lev %.2f de %.2f Ziel %.0f: %+.2f dB -> %.2f LUFS, Spitze %.1f\n", fs, leveler, deess, target, c.calibDb, c.outLufs, c.peakDb);
                    CHECK (c.ok && std::abs (c.outLufs - target) <= 0.05f);
                    CHECK (std::abs (c.calibDb) <= dop::Engine::kMaxCalibDb && std::isfinite (c.peakDb) && c.peakDb > -60);
                    CHECK (c.peakDb <= dop::Engine::kPeakCeilingDb + 0.01f && c.guardDb >= 0);   // Spitzenschutz haelt die Grenze
                    CHECK (c.truePeakDb >= c.peakDb - 0.01f && c.truePeakDb < c.peakDb + 3.0f);   // True Peak gemessen
                    // unabhaengige Gegenprobe: dieselbe Kette mit calibDb, frisch gemessen
                    dop::Engine e; e.prepare (fs); auto q = p; q.takeLufs = dop::autopilot::integratedLoudness (audio, fs, 80.0f) + eqd; q.calibDb = c.calibDb; q.peakGuard = true;
                    e.setParams (q); e.setBank (bank); e.reset();
                    auto y = audio; for (size_t o = 0; o < y.size(); o += 300) { float* pp[] = { y.data() + o }; e.process (pp, 1, (int) std::min<size_t> (300, y.size() - o), nullptr); }
                    CHECK (std::abs (dop::autopilot::integratedLoudness (y, fs) - target) <= 0.12f);
                }
        // Lowcut nach dem Lernen geaendert: Der ungefilterte Mitschnitt wird mit dem neuen Lowcut gerechnet, die
        // Vorausberechnung bezieht sich weiter auf den Lowcut beim Lernen (wie im Prozessor). Ausgang trifft das Ziel.
        for (float newLowcut : { 20.0f, 60.0f, 150.0f, 200.0f })
        {
            dop::EngineParams p; p.lowcutHz = newLowcut; p.rideOffsetDb = prof.typicalOffsetDb;
            const auto c = dop::autopilot::calibrate (audio, fs, p, bank, eqd, 80.0f);
            dop::Engine e; e.prepare (fs); auto q = p; q.takeLufs = dop::autopilot::integratedLoudness (audio, fs, 80.0f) + eqd;
            q.calibDb = c.calibDb; q.peakGuard = true;
            e.setParams (q); e.setBank (bank); e.reset();
            auto y = audio; for (size_t o = 0; o < y.size(); o += 300) { float* pp[] = { y.data() + o }; e.process (pp, 1, (int) std::min<size_t> (300, y.size() - o), nullptr); }
            const float measured = dop::autopilot::integratedLoudness (y, fs);
            if (std::getenv ("DOP_VERBOSE")) std::printf ("Lowcut %.0f Hz: Nachmessung %.2f, tatsaechlich %.2f LUFS (%+.2f dB)\n", newLowcut, c.outLufs, measured, c.calibDb);
            CHECK (c.ok && std::abs (c.outLufs + 23.0f) <= 0.05f && std::abs (measured + 23.0f) <= 0.12f);
        }
        // Stereo: Die Nachmessung rechnet beide Kanaele wie der Live-Pfad (Kanalenergien addiert, Spitzenschutz
        // gekoppelt). Frueher wurde 0,5*(L+R) einkanalig gerechnet; bei L=R und hohem Ziel lag der Ausgang dann
        // fast 1 LU ueber der Nachmessung. Mitschnitt ueber den Learner wie im Prozessor.
        // Kurze, starke Plosiv-Spitzen, damit der Spitzenschutz bei -16 LUFS deutlich (mehrere dB) eingreift.
        if (fs == 48000.)
            for (float rightGain : { 1.0f, -0.5f, 0.0f })
            {
                std::vector<float> xs = x;
                for (size_t i = 0; i < xs.size(); ++i)
                {
                    const double t = std::fmod (i / fs, 0.35);
                    if (t < 0.004) xs[i] += (float) (0.9 * std::exp (-t / 0.001) * std::sin (2.0 * dop::kPi * 150.0 * t));
                }
                std::vector<float> xr (x.size());
                for (size_t i = 0; i < x.size(); ++i) xr[i] = rightGain * xs[i];
                dop::Learner S; S.prepare (fs); S.beginCapture (40);
                std::vector<double> pw (x.size());
                {
                    const auto ks = dop::kWeightShelf (fs), kh = dop::kWeightHighPass (fs);
                    dop::BiquadState a1, a2, b1, b2;
                    for (size_t i = 0; i < x.size(); ++i)
                    {
                        const double kl = a2.process (kh, a1.process (ks, (double) xs[i])), kr = b2.process (kh, b1.process (ks, (double) xr[i]));
                        pw[i] = kl * kl + kr * kr;
                    }
                }
                std::vector<float> tap (x.size());
                for (size_t i = 0; i < x.size(); ++i) tap[i] = 0.5f * (xs[i] + xr[i]);
                for (size_t o = 0; o < x.size(); o += 512)
                {
                    const int n = (int) std::min<size_t> (512, x.size() - o);
                    S.push (tap.data() + o, n, pw.data() + o, 2, 0, xs.data() + o, xr.data() + o);
                }
                const auto cap = S.takeCapturedChannels();
                CHECK (cap.stereo() && cap.size() == x.size() && cap.right.size() == x.size());
                CHECK (std::equal (cap.left.begin(), cap.left.end(), xs.begin()) && std::equal (cap.right.begin(), cap.right.end(), xr.begin()));
                for (float target : { -23.f, -16.f })
                {
                    dop::EngineParams p; p.leveler = 0.5f; p.deess = 0.25f; p.deessOn = true; p.targetLufs = target;
                    p.rideOffsetDb = prof.typicalOffsetDb;
                    const auto c = dop::autopilot::calibrate (cap, fs, p, bank, eqd, 80.0f);
                    const auto old = dop::autopilot::calibrate (cap.mono(), fs, p, bank, eqd, 80.0f);   // frueherer Weg
                    CHECK (c.ok && std::abs (c.outLufs - target) <= 0.05f && c.peakDb <= dop::Engine::kPeakCeilingDb + 0.01f);
                    // Gegenprobe wie im Prozessor: gelernte Lautheit je Kanal, Zweikanal-Engine, Ausgang stereo gemessen
                    const float learned = dop::autopilot::integratedLoudness (cap, fs, 80.0f);
                    dop::Engine e; e.prepare (fs); auto q = p; q.takeLufs = learned - 10.0f * std::log10 (2.0f) + eqd;
                    q.calibDb = c.calibDb; q.peakGuard = true;
                    e.setParams (q); e.setBank (bank); e.reset();
                    auto yl = cap.left, yr = cap.right;
                    for (size_t o = 0; o < yl.size(); o += 300)
                    {
                        float* pp[] = { yl.data() + o, yr.data() + o };
                        e.process (pp, 2, (int) std::min<size_t> (300, yl.size() - o), nullptr);
                    }
                    dop::CapturedAudio y; y.left = yl; y.right = yr;
                    const float measured = dop::autopilot::integratedLoudness (y, fs);
                    if (std::getenv ("DOP_VERBOSE")) std::printf ("Stereo R=%.1f*L Ziel %.0f: Nachmessung %.2f, tatsaechlich %.2f LUFS, Schutz %.1f dB (einkanalig: %+.2f statt %+.2f dB)\n", rightGain, target, c.outLufs, measured, c.guardDb, old.calibDb, c.calibDb);
                    CHECK (std::abs (measured - target) <= 0.12f && std::abs (measured - c.outLufs) <= 0.12f);
                }
            }
        // Abbruch und zu wenig Material
        std::atomic<bool> stop { true };
        CHECK (! dop::autopilot::calibrate (audio, fs, dop::EngineParams {}, bank, eqd, 80.0f, &stop).ok);
        CHECK (! dop::autopilot::calibrate (std::vector<float> ((size_t) fs, 0.1f), fs, dop::EngineParams {}, bank, eqd, 80.0f).ok);
        CHECK (! dop::autopilot::calibrate (std::vector<float> ((size_t) (5 * fs), 0.0f), fs, dop::EngineParams {}, bank, eqd, 80.0f).ok);
    }

    // True Peak: Sinus bei fs/4 mit 45 Grad Phase hat Sample-Spitzen bei 0,707, die echte Spitze liegt bei 1,0
    {
        std::vector<float> x (48000);
        for (size_t i = 0; i < x.size(); ++i) x[i] = (float) std::sin (dop::kPi * 0.5 * (double) i + dop::kPi * 0.25);
        float sp = 0; for (float v : x) sp = std::max (sp, std::abs (v));
        CHECK (std::abs (20 * std::log10 (sp) + 3.01f) < 0.05f);
        CHECK (std::abs (dop::autopilot::truePeakDb (x)) < 0.3f);
        CHECK (dop::autopilot::truePeakDb (std::vector<float> (100, 0.0f)) < -200.0f);
    }
}

// Automatischer De-Esser: nur aus sicher erkannten Zischlauten, unauffaellige Stimmen bleiben unbearbeitet,
// bei unzureichendem Material wird nichts veraendert.
void deesserAutopilotTests()
{
    using DC = dop::autopilot::DeesserChoice;
    const auto flat = dop::flatBank().coeffs;
    auto choose = [&] (const SpeechOptions& o, double fs, int* truth = nullptr, double seconds = 20.0)
    {
        const auto x = synthSpeech (fs, seconds, o, truth);
        const auto c = dop::autopilot::chooseDeesser (x, fs, dop::EngineParams {}, flat);
        if (std::getenv ("DOP_VERBOSE"))
            std::fprintf (stderr, "De-Esser-Autopilot fs %.0f f0 %.0f hell %d S %+.0f dB %.2f/s %.0f Hz fluester %d atem %d: Entscheidung %d (%d), %d S, %.0f Hz, %.0f %%, %+.1f -> %+.1f dB, Vokale %.2f dB\n",
                         fs, o.f0, (int) o.bright, o.sibDb, o.sibPerSecond, o.sibCenterHz, (int) o.whisper, (int) o.breaths,
                         c.decision, c.reason, c.events, c.freqHz, c.amount * 100, c.sharpnessBeforeDb, c.sharpnessAfterDb, c.vowelCostDb);
        CHECK (c.amount >= 0.0f && c.amount <= dop::autopilot::kDeessMaxAmount + 1e-6f);
        CHECK (c.vowelCostDb <= dop::autopilot::kDeessMaxVowelCostDb + 1e-6f);
        if (c.decision == DC::kApplied) CHECK (c.freqHz >= dop::autopilot::kDeessMinFreq && c.freqHz <= dop::autopilot::kDeessMaxFreq);
        return c;
    };
    for (double fs : { 44100., 48000., 96000. })
    {
        const bool full = fs == 48000.;
        // Unauffaellige S-Laute (natuerlich: 13-16 dB unter den Vokalen) -> Staerke 0, Stimme unbearbeitet
        for (double f0 : { 120.0, 210.0 })
            for (bool bright : { false, true })
                for (double sib : { -18.0, -14.0 })
                {
                    if (! full && (bright || sib != -18.0)) continue;
                    SpeechOptions o; o.f0 = f0; o.bright = bright; o.sibDb = sib;
                    int truth = 0;
                    const auto c = choose (o, fs, &truth);
                    CHECK (c.decision == DC::kNotNeeded && c.amount == 0.0f);
                    CHECK (std::abs (c.events - truth) <= 2);   // jeder Zischlaut einzeln erkannt, Vokale nicht mitgezaehlt
                }
        // Scharfe S-Laute -> angewandt, so wenig wie noetig, Vokale unberuehrt; helle Stimme nicht anders als dunkle
        float amountDark = 0, amountBright = 0;
        for (bool bright : { false, true })
        {
            SpeechOptions o; o.f0 = bright ? 210.0 : 120.0; o.bright = bright; o.sibDb = -6.0;
            const auto c = choose (o, fs);
            CHECK (c.decision == DC::kApplied && c.amount > 0.0f && c.amount < dop::autopilot::kDeessMaxAmount);
            CHECK (c.sharpnessAfterDb <= dop::autopilot::kDeessTargetDb + 0.5f);   // Ziel erreicht
            CHECK (c.sibilantReductionDb > 2.0f && c.vowelCostDb < 0.1f);
            (bright ? amountBright : amountDark) = c.amount;
        }
        CHECK (std::abs (amountBright - amountDark) <= 0.1f);
        // Sehr scharfe S-Laute -> begrenzt auf die Hoechststaerke, deutlich gemildert
        {
            SpeechOptions o; o.sibDb = 6.0;
            const auto c = choose (o, fs);
            CHECK (c.decision == DC::kApplied && c.amount == dop::autopilot::kDeessMaxAmount);
            CHECK (c.sharpnessAfterDb < c.sharpnessBeforeDb - 6.0f);
        }
        if (! full) continue;
        // Mehr Schaerfe -> nie weniger Staerke
        float previous = -1;
        for (double sib : { -18.0, -10.0, -8.0, -6.0, -3.0, 0.0 })
        {
            SpeechOptions o; o.sibDb = sib;
            const auto c = choose (o, fs);
            CHECK (c.amount >= previous);
            previous = c.amount;
        }
        // Frequenz folgt der Lage der Zischenergie
        float lastFreq = 0;
        for (double center : { 4500.0, 6500.0, 8500.0 })
        {
            SpeechOptions o; o.sibDb = 0.0; o.sibCenterHz = center;
            const auto c = choose (o, fs);
            CHECK (c.decision == DC::kApplied && c.freqHz >= lastFreq);
            lastFreq = c.freqHz;
        }
        // Natuerlich helle Stimme ohne S-Laute: nichts erkannt, nichts veraendert
        { SpeechOptions o; o.f0 = 220; o.bright = true; o.sibPerSecond = 0; const auto c = choose (o, fs);
          CHECK (c.decision == DC::kInsufficient && c.reason == DC::kTooFewSibilants && c.events == 0); }
        // Wenige (auch scharfe) S-Laute: keine vermeintlich praezise Einstellung
        { SpeechOptions o; o.sibPerSecond = 0.15; o.sibDb = 6.0; const auto c = choose (o, fs);
          CHECK (c.decision == DC::kInsufficient && c.reason == DC::kTooFewSibilants && c.amount == 0.0f); }
        // Kurzes Lernmaterial (4 s)
        { SpeechOptions o; o.sibDb = 0.0; const auto c = choose (o, fs, nullptr, 4.0);
          CHECK (c.decision == DC::kInsufficient); }
        // Fluestern: kein Stimmklang -> unveraendert
        for (double sib : { -12.0, 0.0 })
        { SpeechOptions o; o.whisper = true; o.sibDb = sib; const auto c = choose (o, fs);
          CHECK (c.decision == DC::kInsufficient && c.reason == DC::kNoVoicedSpeech); }
        // Atmer zaehlen nicht als Zischlaute und aendern die Entscheidung nicht
        {
            SpeechOptions o; o.sibDb = -6.0; o.breaths = true;
            int truth = 0;
            const auto c = choose (o, fs, &truth);
            SpeechOptions plain = o; plain.breaths = false;
            const auto d = choose (plain, fs);
            CHECK (std::abs (c.events - truth) <= 2 && c.decision == d.decision && std::abs (c.amount - d.amount) <= 0.05f);
        }
        // Abbruch liefert keine Entscheidung
        {
            std::atomic<bool> stop { true };
            SpeechOptions o; const auto x = synthSpeech (fs, 10.0, o);
            CHECK (dop::autopilot::chooseDeesser (x, fs, dop::EngineParams {}, flat, &stop).decision == DC::kNone);
            CHECK (dop::autopilot::chooseDeesser (std::vector<float> ((size_t) fs, 0.0f), fs, dop::EngineParams {}, flat).decision == DC::kInsufficient);
        }
    }
    // Staerke 0 ist bitgenau neutral: "nicht noetig" laesst die Stimme wirklich unbearbeitet
    {
        SpeechOptions o; o.sibDb = 0.0; const auto x = synthSpeech (48000, 3.0, o);
        auto run = [&] (bool on)
        {
            dop::Engine e; e.prepare (48000); auto p = bypass(); p.deessOn = on; p.deess = 0.0f; e.setParams (p); e.reset();
            auto y = x; for (size_t off = 0; off < y.size(); off += 256) { float* q[] = { y.data() + off }; e.process (q, 1, (int) std::min<size_t> (256, y.size() - off), nullptr); }
            return y;
        };
        CHECK (run (true) == run (false));
    }
}

int main() { spectrumTests(); engineTests(); transitionTests(); learningTests(); fineTests(); deesserSelectivity(); integratedLoudnessTests(); channelLoudnessTests(); fineRateAndAutomationTests(); removedListenTests(); autopilotTests(); deesserAutopilotTests(); std::puts ("DSP, FFT and learning regressions passed"); }
