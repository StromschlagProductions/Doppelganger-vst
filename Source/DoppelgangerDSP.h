// DoppelgangerDSP.h - Klang-Engine fuer "Doppelganger" (Stromschlag Audio)
// Reines C++ ohne JUCE, damit die Engine unabhaengig vom Plugin getestet werden kann (tests/).
//
// Signalweg je Kanal:
//   Eingang -> Lowcut (24 dB/Okt.) -> Match-EQ (26 Glockenfilter, gelernt) -> De-Esser -> Grundverstaerkung -> Leveler -> Ausgang
//
// Lernen (Learner): Das Plugin schickt das Signal nach dem Lowcut in einen Puffer, ein Hintergrund-Takt
// (nicht der Audio-Thread) misst daraus das Langzeit-Spektrum der Sprachanteile und die Tonhoehe.
// Aus gelerntem Spektrum und Referenzkurve entsteht die Korrekturkurve, die mit einer Bank aus
// Glockenfiltern nachgebildet wird. Alles ist latenzfrei.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>
#include "ReferenceCurves.h"

namespace dop
{
constexpr double kPi = 3.14159265358979323846;

// Hosts normally supply 8-192 kHz; reject invalid/unbounded rates before sizing buffers.
inline double validSampleRate (double fs)
{
    return std::isfinite (fs) && fs >= 8000.0 && fs <= 768000.0 ? fs : 48000.0;
}
inline float finiteClamp (float value, float lo, float hi, float fallback)
{
    return std::isfinite (value) ? std::clamp (value, lo, hi) : fallback;
}


//==============================================================================
//  Biquad (Transposed Direct Form II, double)
//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

struct BiquadState
{
    double z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (const Biquad& c, double x)
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

inline Biquad normalise (double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

// Glockenfilter nach RBJ Audio-EQ-Cookbook
inline Biquad peaking (double fs, double f, double q, double gainDb)
{
    if (gainDb == 0.0) return {}; // exact identity, also with fused floating-point operations
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * f / fs;
    const double alpha = std::sin (w) / (2.0 * q);
    const double c = std::cos (w);
    return normalise (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
}

inline Biquad highPass (double fs, double f, double q)
{
    const double w = 2.0 * kPi * f / fs;
    const double alpha = std::sin (w) / (2.0 * q);
    const double c = std::cos (w);
    return normalise ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
}

inline Biquad lowPass (double fs, double f, double q)
{
    const double w = 2.0 * kPi * f / fs;
    const double alpha = std::sin (w) / (2.0 * q);
    const double c = std::cos (w);
    return normalise ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
}

// Hoehen-Shelf nach RBJ (Flankensteilheit S = 1)
inline Biquad highShelf (double fs, double f, double gainDb)
{
    if (gainDb == 0.0) return {};
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * f / fs;
    const double c = std::cos (w), alpha = std::sin (w) / 2.0 * std::sqrt (2.0);
    const double sq = 2.0 * std::sqrt (A) * alpha;
    return normalise (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                      (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
}

// Betragsgang in dB bei Frequenz f
inline double magnitudeDb (const Biquad& c, double fs, double f)
{
    const double w = 2.0 * kPi * f / fs;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    const auto h = (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
    return 20.0 * std::log10 (std::max (std::abs (h), 1e-12));
}

// K-Gewichtung nach ITU-R BS.1770 fuer beliebige Abtastraten (Formeln wie in libebur128)
inline Biquad kWeightShelf (double fs)
{
    const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
    const double K = std::tan (kPi * f0 / fs);
    const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
    const double a0 = 1.0 + K / Q + K * K;
    return { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
             2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
}

inline Biquad kWeightHighPass (double fs)
{
    const double f0 = 38.13547087602444, Q = 0.5003270373238773;
    const double K = std::tan (kPi * f0 / fs);
    const double a0 = 1.0 + K / Q + K * K;
    return { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
}

//==============================================================================
//  Einfache Radix-2-FFT (komplex), nur fuer das Lernen ausserhalb des Audio-Threads
//==============================================================================
class FFT
{
public:
    void prepare (int size)
    {
        if (size < 2 || size > (1 << 20) || (size & (size - 1)) != 0)
            throw std::invalid_argument ("FFT size must be a power of two (2..1048576)");
        n = size;
        bits = 0;
        while ((1 << bits) < n) ++bits;
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) r |= ((i >> b) & 1) << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i) tw[(size_t) i] = std::polar (1.0, -2.0 * kPi * i / n);
    }

    int size() const { return n; }

    void forward (std::vector<std::complex<double>>& a) const { run (a, false); }
    void inverse (std::vector<std::complex<double>>& a) const
    {
        run (a, true);
        for (auto& v : a) v /= (double) n;
    }

private:
    void run (std::vector<std::complex<double>>& a, bool inv) const
    {
        if (n == 0 || a.size() != (size_t) n)
            throw std::invalid_argument ("FFT buffer size mismatch");
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (a[(size_t) i], a[(size_t) rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < len / 2; ++j)
                {
                    auto w = tw[(size_t) (j * step)];
                    if (inv) w = std::conj (w);
                    const auto u = a[(size_t) (i + j)];
                    const auto v = a[(size_t) (i + j + len / 2)] * w;
                    a[(size_t) (i + j)] = u + v;
                    a[(size_t) (i + j + len / 2)] = u - v;
                }
        }
    }

    int n = 0, bits = 0;
    std::vector<int> rev;
    std::vector<std::complex<double>> tw;
};

//==============================================================================
//  Referenzen
//==============================================================================
enum Register { kTief = 0, kMittel = 1, kHoch = 2 };

inline const float* referenceCurve (int reg)
{
    return reg == kTief ? kRefTiefDb : (reg == kHoch ? kRefHochDb : kRefMittelDb);
}

inline float referenceCutoffHz (int reg)
{
    return reg == kTief ? kRefTiefCutoffHz : (reg == kHoch ? kRefHochCutoffHz : kRefMittelCutoffHz);
}

// Stimmlage aus der Tonhoehe (wie im Pre-Flight-Tool): Maenner meist 85-155 Hz
inline int registerForPitch (float pitchHz)
{
    if (pitchHz <= 0.0f) return kMittel;
    if (pitchHz < 160.0f) return kTief;
    if (pitchHz <= 225.0f) return kMittel;
    return kHoch;
}

using GridCurve = std::array<float, (size_t) kRefGridPoints>;

struct LearnedProfile
{
    GridCurve curveDb {};
    float pitchHz = 0.0f;       // 0 = keine Tonhoehe erkannt
    float speechSeconds = 0.0f;
    float loudnessLufs = -100.0f;   // integrierte Lautheit der Spur (nach Lowcut), -100 = unbekannt
    float typicalOffsetDb = -2.5f;  // typischer Sprechpegel (Mittel der 400-ms-Bloecke in dB) minus integrierte Lautheit
    bool valid = false;
};

// Accept old profiles without loudness fields, but never feed malformed data to DSP.
inline bool isValidProfile (const LearnedProfile& p)
{
    for (float v : p.curveDb)
        if (! std::isfinite (v) || v < -60.01f || v > 40.01f) return false;
    return std::isfinite (p.pitchHz) && p.pitchHz >= 0 && p.pitchHz <= 2000
        && std::isfinite (p.speechSeconds) && p.speechSeconds >= 0
        && std::isfinite (p.loudnessLufs) && p.loudnessLufs >= -100 && p.loudnessLufs <= 24
        && std::isfinite (p.typicalOffsetDb) && p.typicalOffsetDb >= -8 && p.typicalOffsetDb <= 0;
}

//==============================================================================
//  Learner: misst Langzeit-Spektrum (wie tools/make_reference_curves.py) und Tonhoehe
//  Laeuft NICHT im Audio-Thread.
//==============================================================================
class Learner
{
public:
    static constexpr int kBuckets = 100;            // 1-dB-Stufen von -100 bis 0 dBFS
    static constexpr float kFloorDb = -70.0f;       // leiser = Stille, wird ignoriert
    static constexpr float kSpeechDb = -55.0f;      // ab hier zaehlt ein Frame fuer die Fortschrittsanzeige

    void prepare (double sampleRate)
    {
        fs = validSampleRate (sampleRate);
        // Framelaenge wie im Generator (4096 bei 48 kHz), bei hoeheren Raten passend groesser
        frameLen = 1;
        while (frameLen < (int) std::lround (4096.0 * fs / 48000.0)) frameLen <<= 1;
        hop = frameLen / 4;
        fft.prepare (frameLen);
        nBins = frameLen / 2 + 1;
        window.resize ((size_t) frameLen);
        for (int i = 0; i < frameLen; ++i)
            window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * kPi * i / frameLen);   // periodisches Hann wie scipy
        buf.assign ((size_t) frameLen, 0.0);
        work.resize ((size_t) frameLen);
        bucketPower.assign ((size_t) kBuckets * (size_t) nBins, 0.0);
        bucketCount.assign ((size_t) kBuckets, 0);

        // Tonhoehe: 40-ms-Frames, 20 ms Abstand, Bandpass 65-400 Hz (wie im Pre-Flight-Tool)
        pLen = (int) std::lround (0.04 * fs);
        pHop = (int) std::lround (0.02 * fs);
        minLag = (int) (fs / 350.0);
        maxLag = (int) (fs / 65.0);
        pFftLen = 1;
        while (pFftLen < 2 * pLen) pFftLen <<= 1;
        pFft.prepare (pFftLen);
        pWindow.resize ((size_t) pLen);
        for (int i = 0; i < pLen; ++i)
            pWindow[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * kPi * i / (pLen - 1));
        pWork.resize ((size_t) pFftLen);
        // Autokorrelation des Fensters zur Korrektur (Boersma)
        std::fill (pWork.begin(), pWork.end(), std::complex<double> (0.0));
        for (int i = 0; i < pLen; ++i) pWork[(size_t) i] = pWindow[(size_t) i];
        pFft.forward (pWork);
        for (auto& v : pWork) v = std::norm (v);
        pFft.inverse (pWork);
        wAcf.resize ((size_t) maxLag + 1);
        for (int l = 0; l <= maxLag; ++l)
            wAcf[(size_t) l] = std::max (pWork[(size_t) l].real() / pWork[0].real(), 1e-3);
        bp1 = highPass (fs, 65.0, 0.5412);
        bp2 = highPass (fs, 65.0, 1.3066);
        bp3 = lowPass (fs, 400.0, 0.5412);
        bp4 = lowPass (fs, 400.0, 1.3066);
        pBuf.assign ((size_t) pLen, 0.0);
        pitches.clear();
        pitches.reserve (20000);
        kShelf = kWeightShelf (fs);
        kHp = kWeightHighPass (fs);
        subLen = (int) std::lround (0.1 * fs);
        blocks.clear();
        blocks.reserve (40000);
        reset();
    }

    void reset()
    {
        std::fill (bucketPower.begin(), bucketPower.end(), 0.0);
        std::fill (bucketCount.begin(), bucketCount.end(), 0);
        std::fill (buf.begin(), buf.end(), 0.0);
        std::fill (pBuf.begin(), pBuf.end(), 0.0);
        filled = 0; sinceHop = 0; pFilled = 0; pSinceHop = 0;
        speechFrames = 0;
        pitches.clear();
        for (auto* s : { &s1, &s2, &s3, &s4, &k1, &k2 }) s->reset();
        subAcc = 0.0; subN = 0; subCount = 0; sub.fill (0.0);
        blocks.clear();
    }

    // Samples (mono) einspeisen
    void push (const float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const double v = std::isfinite (x[i]) ? x[i] : 0.0;
            // Spektrum
            buf[(size_t) (filled % frameLen)] = v;
            ++filled;
            if (filled >= frameLen && ++sinceHop >= hop)
            {
                sinceHop = 0;
                analyseFrame();
            }
            // Lautheit (BS.1770): 400-ms-Bloecke im 100-ms-Raster
            const double kw = k2.process (kHp, k1.process (kShelf, v));
            subAcc += kw * kw;
            if (++subN >= subLen)
            {
                sub[(size_t) (subCount % 4)] = subAcc / subN;
                subAcc = 0.0; subN = 0;
                if (++subCount >= 4 && blocks.size() < blocks.capacity())
                    blocks.push_back ((sub[0] + sub[1] + sub[2] + sub[3]) * 0.25);
            }
            // Tonhoehe
            double b = s4.process (bp4, s3.process (bp3, s2.process (bp2, s1.process (bp1, v))));
            pBuf[(size_t) (pFilled % pLen)] = b;
            ++pFilled;
            if (pFilled >= pLen && ++pSinceHop >= pHop)
            {
                pSinceHop = 0;
                analysePitchFrame();
            }
        }
    }

    float speechSeconds() const { return (float) (speechFrames * hop / fs); }

    LearnedProfile finish() const
    {
        LearnedProfile p;
        int top = -1;
        for (int b = kBuckets - 1; b >= 0; --b)
            if (bucketCount[(size_t) b] > 0) { top = b; break; }
        if (top < 0) return p;

        // wie im Generator: nur Frames bis 40 dB unter dem lautesten
        std::vector<double> power ((size_t) nBins, 0.0);
        long count = 0;
        for (int b = std::max (0, top - 40); b <= top; ++b)
        {
            count += bucketCount[(size_t) b];
            const double* src = &bucketPower[(size_t) b * (size_t) nBins];
            for (int k = 0; k < nBins; ++k) power[(size_t) k] += src[k];
        }
        if (count < 8) return p;

        std::vector<double> logf ((size_t) nBins), db ((size_t) nBins);
        for (int k = 1; k < nBins; ++k)
        {
            logf[(size_t) k] = std::log2 (k * fs / frameLen);
            db[(size_t) k] = 10.0 * std::log10 (power[(size_t) k] / (double) count + 1e-20);
        }
        p.curveDb = smoothToGrid (logf, db, nBins);
        p.speechSeconds = speechSeconds();

        // Tonhoehe: Median der stimmhaften Frames bis 34 dB unter dem lautesten
        float maxDb = -200.0f;
        for (auto& pv : pitches) maxDb = std::max (maxDb, pv.second);
        std::vector<float> v;
        for (auto& pv : pitches)
            if (pv.second > maxDb - 34.0f) v.push_back (pv.first);
        if (v.size() >= 3)
        {
            std::nth_element (v.begin(), v.begin() + (long) (v.size() / 2), v.end());
            p.pitchHz = v[v.size() / 2];
        }
        p.loudnessLufs = integratedLoudness (blocks);
        if (p.loudnessLufs > -99.0f)
        {
            double sdb = 0; long nb = 0;
            for (double b : blocks)
            {
                const double l = -0.691 + 10.0 * std::log10 (b + 1e-20);
                if (l > p.loudnessLufs - 10.0) { sdb += l; ++nb; }
            }
            if (nb > 0) p.typicalOffsetDb = (float) std::clamp (sdb / nb - p.loudnessLufs, -8.0, 0.0);
        }
        p.valid = true;
        return p;
    }

    // Glaettung ueber 1/6 Oktave, an den Gitterpunkten ausgewertet, normiert und begrenzt (identisch zum Generator)
    static GridCurve smoothToGrid (const std::vector<double>& logf, const std::vector<double>& db, int nBins)
    {
        GridCurve out {};
        const double sigma = 1.0 / 6.0;
        for (int g = 0; g < kRefGridPoints; ++g)
        {
            const double lg = std::log2 ((double) kRefGridHz[g]);
            double sw = 0, s = 0;
            for (int k = 1; k < nBins; ++k)
            {
                const double d = (logf[(size_t) k] - lg) / sigma;
                if (std::abs (d) > 6.0) continue;
                const double w = std::exp (-0.5 * d * d);
                sw += w;
                s += w * db[(size_t) k];
            }
            out[(size_t) g] = (float) (sw > 0 ? s / sw : -100.0);
        }
        double mean = 0; int m = 0;
        for (int g = 0; g < kRefGridPoints; ++g)
            if (kRefGridHz[g] >= 100.0f && kRefGridHz[g] <= 8000.0f) { mean += out[(size_t) g]; ++m; }
        mean /= std::max (1, m);
        for (auto& v : out) v = (float) std::clamp ((double) v - mean, -60.0, 40.0);
        return out;
    }

    // Integrierte Lautheit nach BS.1770 aus Blockleistungen (absolutes Tor -70, relatives Tor -10 LU)
    static float integratedLoudness (const std::vector<double>& blockPow)
    {
        auto lufs = [] (double pw) { return -0.691 + 10.0 * std::log10 (pw + 1e-20); };
        double s = 0; long n = 0;
        for (double b : blockPow) if (lufs (b) > -70.0) { s += b; ++n; }
        if (n == 0) return -100.0f;
        const double rel = lufs (s / n) - 10.0;
        s = 0; n = 0;
        for (double b : blockPow) if (lufs (b) > -70.0 && lufs (b) > rel) { s += b; ++n; }
        return n > 0 ? (float) lufs (s / n) : -100.0f;
    }

private:
    void analyseFrame()
    {
        double ms = 0;
        for (int i = 0; i < frameLen; ++i)
        {
            const double v = buf[(size_t) ((filled + i) % frameLen)];   // aelteste Probe zuerst
            ms += v * v;
            work[(size_t) i] = v * window[(size_t) i];
        }
        const double frameDb = 10.0 * std::log10 (ms / frameLen + 1e-20);
        if (frameDb < kFloorDb) return;
        if (frameDb > kSpeechDb) ++speechFrames;
        const int b = std::clamp ((int) std::floor (frameDb + 100.0), 0, kBuckets - 1);
        fft.forward (work);
        double* dst = &bucketPower[(size_t) b * (size_t) nBins];
        for (int k = 0; k < nBins; ++k) dst[k] += std::norm (work[(size_t) k]);
        ++bucketCount[(size_t) b];
    }

    void analysePitchFrame()
    {
        double energy = 0;
        std::fill (pWork.begin(), pWork.end(), std::complex<double> (0.0));
        for (int i = 0; i < pLen; ++i)
        {
            const double v = pBuf[(size_t) ((pFilled + i) % pLen)] * pWindow[(size_t) i];
            pWork[(size_t) i] = v;
            energy += v * v;
        }
        if (energy < 1e-7) return;
        pFft.forward (pWork);
        for (auto& v : pWork) v = std::norm (v);
        pFft.inverse (pWork);

        int best = -1;
        double bestScore = -1e9, bestVal = 0;
        for (int l = minLag; l < maxLag; ++l)
        {
            const double r = pWork[(size_t) l].real() / (energy + 1e-12) / wAcf[(size_t) l];
            const double score = r - 0.02 * std::log2 (l * 65.0 / fs);   // leichter Bonus fuer kuerzere Lags
            if (score > bestScore) { bestScore = score; best = l; bestVal = r; }
        }
        if (best < 0 || bestVal <= 0.45) return;
        const float levelDb = (float) (10.0 * std::log10 (energy / pLen + 1e-20));
        if (pitches.size() < pitches.capacity())
            pitches.emplace_back ((float) (fs / best), levelDb);
    }

    double fs = 48000.0;
    int frameLen = 4096, hop = 1024, nBins = 2049;
    FFT fft;
    std::vector<double> window, buf;
    std::vector<std::complex<double>> work;
    std::vector<double> bucketPower;
    std::vector<long> bucketCount;
    int64_t filled = 0, sinceHop = 0, speechFrames = 0;

    int pLen = 1920, pHop = 960, minLag = 137, maxLag = 738, pFftLen = 4096;
    FFT pFft;
    std::vector<double> pWindow, pBuf, wAcf;
    std::vector<std::complex<double>> pWork;
    Biquad bp1, bp2, bp3, bp4;
    BiquadState s1, s2, s3, s4;
    int64_t pFilled = 0, pSinceHop = 0;
    std::vector<std::pair<float, float>> pitches;   // (Hz, Pegel dB)

    Biquad kShelf, kHp;
    BiquadState k1, k2;
    double subAcc = 0.0;
    int subLen = 4800, subN = 0;
    int64_t subCount = 0;
    std::array<double, 4> sub {};
    std::vector<double> blocks;
};

//==============================================================================
//  Korrekturkurve (Portierung von eq_correction_curve_db aus dem Pre-Flight-Tool)
//==============================================================================
inline double gridInterp (const GridCurve& c, double hz)
{
    const double lf = std::log2 (std::max (hz, 1.0));
    if (hz <= kRefGridHz[0]) return c[0];
    if (hz >= kRefGridHz[kRefGridPoints - 1]) return c[(size_t) kRefGridPoints - 1];
    for (int i = 1; i < kRefGridPoints; ++i)
        if (kRefGridHz[i] >= hz)
        {
            const double l0 = std::log2 ((double) kRefGridHz[i - 1]), l1 = std::log2 ((double) kRefGridHz[i]);
            const double t = (lf - l0) / (l1 - l0);
            return c[(size_t) i - 1] + t * (c[(size_t) i] - c[(size_t) i - 1]);
        }
    return c[(size_t) kRefGridPoints - 1];
}

// amount: 0..1, lowcutHz <= 20 = Lowcut aus
// Angleich-Bereich: unterhalb von matchLoHz und oberhalb von matchHiHz bleibt der Take unveraendert.
// Die Korrektur blendet jenseits der Grenze ueber eine Drittel-Oktave weich auf 0 dB aus.
constexpr float kMatchLoMin = 20.0f, kMatchLoMax = 2000.0f, kMatchHiMin = 1000.0f, kMatchHiMax = 20000.0f;
constexpr float kMatchLoDefault = 20.0f, kMatchHiDefault = 12000.0f;

constexpr double kMatchFadeOct = 1.0 / 3.0;

inline double matchRangeWeight (double f, double loHz, double hiHz)
{
    auto edge = [] (double octOutside) { return octOutside <= 0.0 ? 1.0 : (octOutside >= kMatchFadeOct ? 0.0 : 0.5 + 0.5 * std::cos (kPi * octOutside / kMatchFadeOct)); };
    double w = 1.0;
    if (loHz > kMatchLoMin + 0.5) w *= edge (std::log2 (loHz / f));
    if (hiHz < kMatchHiMax - 0.5) w *= edge (std::log2 (f / hiHz));
    return w;
}

inline GridCurve correctionCurve (const GridCurve& take, int reg, float amount, float lowcutHz,
                                  float matchLoHz = kMatchLoMin, float matchHiHz = kMatchHiMax)
{
    matchLoHz = finiteClamp (matchLoHz, kMatchLoMin, kMatchLoMax, kMatchLoMin);
    matchHiHz = finiteClamp (matchHiHz, kMatchHiMin, kMatchHiMax, kMatchHiMax);
    matchHiHz = std::max (matchHiHz, matchLoHz * 1.5f);   // mindestens gut eine halbe Oktave Bereich
    const float* ref = referenceCurve (reg);
    const double cutoff = referenceCutoffHz (reg);
    GridCurve delta {};
    for (int i = 0; i < kRefGridPoints; ++i) delta[(size_t) i] = ref[i] - take[(size_t) i];

    // Oberhalb der Referenz-Bandbreite den letzten gueltigen Wert halten. Die Kurven sind schon ueber
    // 1/6 Oktave geglaettet, die Codec-Kante wirkt dadurch bis ca. 0,4 Oktaven darunter -> dort schon halten.
    const double holdFrom = cutoff * 0.75;
    const double holdVal = gridInterp (delta, holdFrom);
    for (int i = 0; i < kRefGridPoints; ++i)
        if (kRefGridHz[i] > holdFrom) delta[(size_t) i] = (float) holdVal;

    // Glaettung ueber ca. 0,24 Oktaven (entspricht sigma=12 im Pre-Flight-Tool)
    const double step = std::log2 ((double) kRefGridHz[1] / kRefGridHz[0]);
    const double sigma = 0.24 / step;
    GridCurve sm {};
    for (int i = 0; i < kRefGridPoints; ++i)
    {
        double sw = 0, s = 0;
        for (int j = 0; j < kRefGridPoints; ++j)
        {
            const double d = (j - i) / sigma;
            const double w = std::exp (-0.5 * d * d);
            sw += w;
            s += w * delta[(size_t) j];
        }
        sm[(size_t) i] = (float) (s / sw);
    }

    // Reinen Pegelunterschied entfernen: der EQ gleicht nur die Klangfarbe an
    const double lo = std::max ((double) lowcutHz, 100.0);
    double mean = 0; int m = 0;
    for (int i = 0; i < kRefGridPoints; ++i)
        if (kRefGridHz[i] >= lo && kRefGridHz[i] <= 8000.0f) { mean += sm[(size_t) i]; ++m; }
    mean /= std::max (1, m);

    GridCurve out {};
    for (int i = 0; i < kRefGridPoints; ++i)
    {
        const double f = kRefGridHz[i];
        double d = std::clamp ((sm[(size_t) i] - mean) * amount, -9.0, 9.0);
        const double fadeIn = lowcutHz > 20.0f ? std::clamp (std::log2 (f / lowcutHz) / 0.5, 0.0, 1.0) : 1.0;
        const double fadeTop = f <= 16000.0 ? 1.0 : (f >= 20000.0 ? 0.5 : 1.0 - 0.5 * (f - 16000.0) / 4000.0);
        const double fadeBand = f <= cutoff * 0.85 ? 1.0 : (f >= cutoff ? 0.0 : (cutoff - f) / (cutoff * 0.15));
        out[(size_t) i] = (float) (d * fadeIn * fadeTop * fadeBand * matchRangeWeight (f, matchLoHz, matchHiHz));
    }
    return out;
}

//==============================================================================
//  Filterbank: 26 Glockenfilter im Terzabstand (50 Hz - 16 kHz), Verstaerkungen per Ausgleichsrechnung
//==============================================================================
constexpr int kBands = 26;
// De-Esser-Abstimmung (an echten Sprachaufnahmen gemessen, siehe docs/DEESSER_1.4.md)
constexpr double kDeessThr0 = 0.0, kDeessThrPerAmt = 6.0;   // Schwelle Zischband gegen Stimmbereich: 0 dB bis -6 dB
constexpr double kDeessMax0 = 3.0, kDeessMaxPerAmt = 11.0;  // maximale Absenkung: 3 dB bis 14 dB
constexpr double kDeessSlope = 1.0;                          // je dB ueber der Schwelle 1 dB Absenkung
constexpr double kDeessRelease = 0.015, kDeessDetRelease = 0.004;   // Sekunden
constexpr int kFineBands = 5;   // Feinabstimmung: Anzahl Glockenfilter (Standard alle 0 dB)
constexpr double kBandQ = 2.1;

inline double bandFreq (int k) { return 50.0 * std::pow (2.0, k / 3.0); }

//==============================================================================
// Erkennt, wann das Lernen "genug" hat: Klangbild, Tonhoehe und Lautheit aendern sich kaum noch.
inline double curveDistanceDb (const GridCurve& a, const GridCurve& b)
{
    double sum = 0; int n = 0;
    for (int i = 0; i < kRefGridPoints; ++i)
        if (kRefGridHz[i] >= 100.0f && kRefGridHz[i] <= 8000.0f) { sum += std::abs (a[(size_t) i] - b[(size_t) i]); ++n; }
    return n > 0 ? sum / n : 0.0;
}

struct ConvergenceTracker
{
    static constexpr double kCurveTolDb = 0.35;   // mittlere Abweichung zum Stand von vor etwa 1 s
    static constexpr double kPitchTol = 0.06;     // 6 %
    static constexpr double kLoudTolLu = 0.5;
    static constexpr int kNeeded = 3;             // so oft hintereinander stabil

    LearnedProfile prev;
    int stable = 0;
    double lastCurveDist = 99.0;

    void reset() { prev = LearnedProfile(); stable = 0; lastCurveDist = 99.0; }

    // true, wenn das Profil seit mehreren Pruefungen stabil ist
    bool update (const LearnedProfile& p)
    {
        if (! p.valid) return false;
        if (prev.valid)
        {
            lastCurveDist = curveDistanceDb (prev.curveDb, p.curveDb);
            const bool pitchOk = (prev.pitchHz <= 0 || p.pitchHz <= 0) ? true
                                 : std::abs (p.pitchHz - prev.pitchHz) / prev.pitchHz < kPitchTol;
            const bool loudOk = std::abs (p.loudnessLufs - prev.loudnessLufs) < kLoudTolLu;
            stable = (lastCurveDist < kCurveTolDb && pitchOk && loudOk) ? stable + 1 : 0;
        }
        prev = p;
        return stable >= kNeeded;
    }
};

struct BankDesign
{
    std::array<Biquad, kBands> coeffs {};
    std::array<float, kBands> gainsDb {};
    float maxErrorDb = 0.0f;   // groesste Abweichung zur Zielkurve (Kontrolle)
};

inline BankDesign flatBank()
{
    BankDesign d;
    for (auto& c : d.coeffs) c = Biquad {};
    return d;
}

// Loest (A^T A + lambda I) x = A^T b per Gauss-Elimination
inline std::array<double, kBands> solveNormal (const std::vector<std::array<double, kBands>>& A,
                                               const std::vector<double>& b, double lambda)
{
    double M[kBands][kBands + 1] = {};
    for (size_t i = 0; i < A.size(); ++i)
        for (int r = 0; r < kBands; ++r)
        {
            for (int c = 0; c < kBands; ++c) M[r][c] += A[i][(size_t) r] * A[i][(size_t) c];
            M[r][kBands] += A[i][(size_t) r] * b[i];
        }
    for (int r = 0; r < kBands; ++r) M[r][r] += lambda;
    for (int c = 0; c < kBands; ++c)
    {
        int piv = c;
        for (int r = c + 1; r < kBands; ++r)
            if (std::abs (M[r][c]) > std::abs (M[piv][c])) piv = r;
        for (int k = 0; k <= kBands; ++k) std::swap (M[c][k], M[piv][k]);
        for (int r = 0; r < kBands; ++r)
        {
            if (r == c || M[c][c] == 0.0) continue;
            const double f = M[r][c] / M[c][c];
            for (int k = c; k <= kBands; ++k) M[r][k] -= f * M[c][k];
        }
    }
    std::array<double, kBands> x {};
    for (int r = 0; r < kBands; ++r) x[(size_t) r] = M[r][r] != 0.0 ? M[r][kBands] / M[r][r] : 0.0;
    return x;
}

inline BankDesign fitBank (const GridCurve& target, double fs)
{
    // Nur Gitterpunkte unterhalb von 0,45*fs verwenden
    std::vector<int> pts;
    for (int i = 0; i < kRefGridPoints; ++i)
        if (kRefGridHz[i] < 0.45 * fs) pts.push_back (i);

    // Einflussmatrix: Antwort jedes Bandes bei +1 dB
    std::vector<std::array<double, kBands>> A (pts.size());
    for (int k = 0; k < kBands; ++k)
    {
        const double f = std::min (bandFreq (k), 0.45 * fs);
        const Biquad c = peaking (fs, f, kBandQ, 1.0);
        for (size_t i = 0; i < pts.size(); ++i) A[i][(size_t) k] = magnitudeDb (c, fs, kRefGridHz[pts[i]]);
    }

    std::array<double, kBands> g {};
    BankDesign d;
    std::vector<double> resid (pts.size());
    for (int iter = 0; iter < 5; ++iter)
    {
        for (int k = 0; k < kBands; ++k)
            d.coeffs[(size_t) k] = peaking (fs, std::min (bandFreq (k), 0.45 * fs), kBandQ, g[(size_t) k]);
        double maxErr = 0;
        for (size_t i = 0; i < pts.size(); ++i)
        {
            double r = 0;
            for (int k = 0; k < kBands; ++k) r += magnitudeDb (d.coeffs[(size_t) k], fs, kRefGridHz[pts[i]]);
            resid[i] = target[(size_t) pts[i]] - r;
            maxErr = std::max (maxErr, std::abs (resid[i]));
        }
        d.maxErrorDb = (float) maxErr;
        if (iter == 4 || maxErr < 0.05) break;
        const auto dx = solveNormal (A, resid, 0.02);
        for (int k = 0; k < kBands; ++k) g[(size_t) k] = std::clamp (g[(size_t) k] + dx[(size_t) k], -12.0, 12.0);
    }
    for (int k = 0; k < kBands; ++k) d.gainsDb[(size_t) k] = (float) g[(size_t) k];
    return d;
}

//==============================================================================
//  Lautheitsaenderung durch den EQ abschaetzen (damit die Grundverstaerkung von Anfang an stimmt)
//  Spektrum der gelernten Stimme (Leistungsdichte auf dem Log-Gitter) mit K-Gewichtung, mit und ohne Korrektur.
//==============================================================================
// Shared by audio, display and loudness estimate: never design a bell past Nyquist.
inline Biquad fineCoefficient (double fs, float hz, float db, float q)
{
    fs = validSampleRate (fs);
    return peaking (fs, std::min ((double) finiteClamp (hz, 40, 20000, 1000), fs * 0.45),
                    finiteClamp (q, 0.3f, 8, 1), finiteClamp (db, -12, 12, 0));
}

// Audio-owned two-bank EQ. A new request never interrupts an ongoing fade.
class FineEq
{
public:
    using Bank = std::array<Biquad, kFineBands>;
    void prepare (double sampleRate)
    {
        fs = validSampleRate (sampleRate);
        fadeLength = std::max (1, (int) std::lround (0.03 * fs));
        current = 0; fading = queued = false;
        banks = {}; neutral[0] = neutral[1] = true;
        reset();
    }
    void set (bool on, const std::array<float, kFineBands>& hz,
              const std::array<float, kFineBands>& db, const std::array<float, kFineBands>& q)
    {
        Bank next {};
        if (on) for (size_t k = 0; k < next.size(); ++k) next[k] = fineCoefficient (fs, hz[k], db[k], q[k]);
        if (fading) { pending = next; queued = ! equal (next, banks[1 - current]); }
        else if (! equal (next, banks[current])) begin (next);
    }
    void reset()
    {
        if (queued) banks[current] = pending;
        else if (fading) current = 1 - current;
        neutral[current] = isNeutral (banks[current]);
        states = {}; fading = queued = false; position = 0;
    }
    double process (int channel, double x)
    {
        auto run = [&] (int bank)
        {
            double y = x;
            if (! neutral[bank]) for (size_t k = 0; k < banks[bank].size(); ++k)
                y = states[(size_t) channel][(size_t) bank][k].process (banks[bank][k], y);
            return y;
        };
        const double a = run (current);
        return fading ? a + (double) position / fadeLength * (run (1 - current) - a) : a;
    }
    void advance()
    {
        if (fading && ++position >= fadeLength) { current = 1 - current; fading = false; }
        if (! fading && queued) { queued = false; if (! equal (pending, banks[current])) begin (pending); }
    }
private:
    static bool equal (const Bank& a, const Bank& b)
    {
        for (size_t k = 0; k < a.size(); ++k)
            if (a[k].b0 != b[k].b0 || a[k].b1 != b[k].b1 || a[k].b2 != b[k].b2
                || a[k].a1 != b[k].a1 || a[k].a2 != b[k].a2) return false;
        return true;
    }
    static bool isNeutral (const Bank& b) { return equal (b, Bank {}); }
    void begin (const Bank& b)
    {
        const int next = 1 - current;
        banks[next] = b; neutral[next] = isNeutral (b);
        for (auto& ch : states) ch[(size_t) next] = {};
        position = 0; fading = true;
    }
    double fs = 48000;
    std::array<Bank, 2> banks {};
    std::array<std::array<std::array<BiquadState, kFineBands>, 2>, 2> states {};
    Bank pending {};
    bool neutral[2] { true, true }, fading = false, queued = false;
    int current = 0, position = 0, fadeLength = 1440;
};

// Frequenzgang der Feinabstimmung auf dem Referenzgitter (dB), fuer die Lautheitsrechnung
inline GridCurve fineCurveDb (bool on, const std::array<float, kFineBands>& hz, const std::array<float, kFineBands>& db,
                              const std::array<float, kFineBands>& q, double fs)
{
    GridCurve out {};
    if (! on) return out;
    for (int k = 0; k < kFineBands; ++k)
    {
        const auto kk = (size_t) k;
        if (db[kk] == 0.0f) continue;
        const Biquad c = fineCoefficient (fs, hz[kk], db[kk], q[kk]);
        for (int i = 0; i < kRefGridPoints; ++i) if (kRefGridHz[i] < fs * 0.5) out[(size_t) i] += (float) magnitudeDb (c, fs, kRefGridHz[i]);
    }
    return out;
}

inline float eqLoudnessDeltaDb (const GridCurve& takeDb, const GridCurve& corrDb)
{
    const Biquad ks = kWeightShelf (48000.0), kh = kWeightHighPass (48000.0);
    double a = 0, b = 0;
    for (int i = 0; i < kRefGridPoints; ++i)
    {
        const double f = kRefGridHz[i];
        const double k = magnitudeDb (ks, 48000.0, f) + magnitudeDb (kh, 48000.0, f);
        const double w = std::pow (10.0, (takeDb[(size_t) i] + k) / 10.0) * f;   // f: Gitter ist logarithmisch
        a += w * std::pow (10.0, corrDb[(size_t) i] / 10.0);
        b += w;
    }
    return b > 0 ? (float) (10.0 * std::log10 (a / b)) : 0.0f;
}

//==============================================================================
//  Engine (Audio-Thread): Lowcut, Match-EQ, De-Esser, Grundverstaerkung, Leveler. Latenzfrei.
//==============================================================================
struct EngineParams
{
    float lowcutHz = 80.0f;      // <= 20 = aus
    bool fineOn = false;                              // Feinabstimmung an
    std::array<float, kFineBands> fineHz { 100.0f, 300.0f, 1000.0f, 3000.0f, 8000.0f };
    std::array<float, kFineBands> fineDb {};          // -12 .. +12 dB
    std::array<float, kFineBands> fineQ { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    bool deessOn = true;
    float deess = 0.25f;         // Staerke 0..1
    float deessFreqHz = 5500.0f; // ab hier wirkt der De-Esser
    bool deessListen = false;    // Differenz vor/nach De-Esser: tatsaechlich entfernte Anteile
    float leveler = 0.5f;        // 0..1 -> Regelbereich 0..+-10 dB
    float targetLufs = -23.0f;
    float takeLufs = -100.0f;    // gelernte Lautheit nach dem EQ, -100 = unbekannt (dann keine Grundverstaerkung)
    float rideOffsetDb = -2.5f;  // typischer Sprechpegel relativ zur integrierten Lautheit (gelernt)
};

class Engine
{
public:
    static constexpr int kMaxCh = 2;
    static constexpr double kMaxRangeDb = 10.0;   // Leveler-Regelbereich bei 100 %

    std::atomic<float> meterRideDb { 0.0f }, meterStaticDb { 0.0f }, meterDeessDb { 0.0f }, meterDeessPeakDb { 0.0f },
                       meterInLufs { -100.0f }, meterOutLufs { -100.0f }, meterOutPeakDb { -100.0f };
    // Integrierte Ausgangslautheit nach ITU-R BS.1770 / EBU R128 (400-ms-Bloecke, 75 % Ueberlappung,
    // absolutes Gate -70 LUFS, relatives Gate -10 LU). -100 = noch keine Messung. Gleiche Kanalbehandlung wie das Ziel.
    std::atomic<float> meterIntLufs { -100.0f }, meterIntSeconds { 0.0f };
    std::atomic<bool> intResetRequest { false };
    void resetIntegrated() noexcept { intResetRequest.store (true); }

    void prepare (double sampleRate)
    {
        fs = validSampleRate (sampleRate);
        fineEq.prepare (fs);
        fineDirty = true;
        kShelf = kWeightShelf (fs);
        kHp = kWeightHighPass (fs);
        for (auto& b : bank[0]) b = Biquad {};
        for (auto& b : bank[1]) b = Biquad {};
        listenStep = 1.0 / std::max (1.0, std::round (0.01 * fs));
        fadeLen = std::max (1, (int) std::lround (0.03 * fs));
        hopLen = std::max (1, (int) std::lround (0.1 * fs));
        clearIntegrated();
        loudCoef = 1.0 - std::exp (-1.0 / (0.4 * fs));
        envAtt = 1.0 - std::exp (-1.0 / (0.0005 * fs));
        envRel = 1.0 - std::exp (-1.0 / (kDeessRelease * fs));
        detAtt = 1.0 - std::exp (-1.0 / (0.001 * fs));
        detRel = 1.0 - std::exp (-1.0 / (kDeessDetRelease * fs));
        const double dt = 16.0 / fs;
        staticCoef = 1.0 - std::exp (-dt / 0.05);
        rideOffCoef = 1.0 - std::exp (-dt / 0.5);
        rideUpCoef = 1.0 - std::exp (-dt / 1.5);
        rideDownCoef = 1.0 - std::exp (-dt / 0.8);
        rideSmoothCoef = 1.0 - std::exp (-dt / 0.3);
        neutral[0] = neutral[1] = true;
        cur = 0; fading = false; bankQueued = false;
        lastLowcut = -1.0f;
        lastDeessFreq = -1.0f;
        reset();
    }

    void reset()
    {
        // A transport reset keeps the latest requested correction, including an unfinished fade.
        if (bankQueued) { bank[cur] = queuedBank; neutral[cur] = isNeutral (queuedBank); }
        else if (fading) cur = 1 - cur;
        bankQueued = false;
        fineEq.reset();
        for (auto& ch : chans)
        {

            for (auto& s : ch.lc) s.reset();
            for (auto& s : ch.eq[0]) s.reset();
            for (auto& s : ch.eq[1]) s.reset();
            for (auto* s : { &ch.dh, &ch.dh2, &ch.dt, &ch.dl, &ch.dl2, &ch.ds, &ch.k1, &ch.k2, &ch.o1, &ch.o2 }) s->reset();
        }
        fading = false; fadePos = 0;
        hiPow = fullPow = 0.0; deessGain = 1.0; shelfDb = 0.0; shelf = Biquad {};
        msK = outMs = 0.0;
        ride1 = ride2 = 0.0;
        staticDb = std::isfinite (desiredStaticDb()) ? desiredStaticDb() : 0.0;   // sofort richtig, kein Einschwingen
        gainStatic = std::pow (10.0, staticDb / 20.0);
        gainRide = 1.0;
        listenMix = params.deessListen ? 1.0 : 0.0;
        ctl = 0;
    }

    int latencySamples() const { return 0; }

    void setParams (EngineParams p)
    {
        p.lowcutHz = finiteClamp (p.lowcutHz, 20, 200, 80);
        for (int k = 0; k < kFineBands; ++k)
        {
            p.fineHz[(size_t) k] = finiteClamp (p.fineHz[(size_t) k], 20, 20000, 1000);
            p.fineDb[(size_t) k] = finiteClamp (p.fineDb[(size_t) k], -12, 12, 0);
            p.fineQ[(size_t) k] = finiteClamp (p.fineQ[(size_t) k], 0.3f, 8, 1);
        }
        p.deess = finiteClamp (p.deess, 0, 1, 0.25f);
        p.deessFreqHz = finiteClamp (p.deessFreqHz, 3000, 10000, 5500);
        p.leveler = finiteClamp (p.leveler, 0, 1, 0.5f);
        p.targetLufs = finiteClamp (p.targetLufs, -30, -14, -23);
        p.takeLufs = finiteClamp (p.takeLufs, -100, 24, -100);
        p.rideOffsetDb = finiteClamp (p.rideOffsetDb, -8, 0, -2.5f);
        // Feinabstimmung: Koeffizienten nur bei Aenderung neu rechnen
        const bool fineChanged = fineDirty || p.fineOn != params.fineOn || p.fineHz != params.fineHz
                                 || p.fineDb != params.fineDb || p.fineQ != params.fineQ;
        params = p;
        if (fineChanged)
        {
            fineDirty = false;
            fineEq.set (p.fineOn, p.fineHz, p.fineDb, p.fineQ);
        }
        if (p.lowcutHz != lastLowcut)
        {
            lastLowcut = p.lowcutHz;
            const bool wasOn = lcOn;
            lcOn = p.lowcutHz > 20.0f;
            if (lcOn != wasOn) for (auto& ch : chans) for (auto& state : ch.lc) state.reset();
            if (lcOn)
            {
                lc[0] = highPass (fs, p.lowcutHz, 0.5412);   // Butterworth 4. Ordnung
                lc[1] = highPass (fs, p.lowcutHz, 1.3066);
            }
        }
        if (p.deessFreqHz != lastDeessFreq)
        {
            lastDeessFreq = p.deessFreqHz;
            // Erkennung: Zischband (Butterworth-Hochpass 4. Ordnung ab der Frequenz, oben bei 12 kHz begrenzt)
            // gegen den tiefen Stimmbereich (Tiefpass 4. Ordnung). Vokale haben unten viel mehr Energie als oben,
            // Zischlaute umgekehrt. So reagiert der De-Esser auf S, Z, Sch und scharfe T, nicht auf helle Vokale.
            const double fHi = std::min ((double) p.deessFreqHz, 0.40 * fs);
            const double fLo = std::min (3000.0, 0.55 * fHi);
            deessHp = highPass (fs, fHi, 0.5412);
            deessHp2 = highPass (fs, fHi, 1.3066);
            deessTop = lowPass (fs, std::min (12000.0, 0.45 * fs), 0.7071);
            deessLow = lowPass (fs, fLo, 0.5412);
            deessLow2 = lowPass (fs, fLo, 1.3066);
            shelfDb = 1.0;   // Shelf beim naechsten Takt neu berechnen
        }
    }

    // Neue Filterbank uebernehmen: weiche Ueberblendung ueber 30 ms
    void setBank (const std::array<Biquad, kBands>& c)
    {
        if (fading)
        {
            // Keep the current fade continuous. Coalesce automation to the newest bank.
            queuedBank = c;
            bankQueued = ! sameBank (c, bank[1 - cur]);
            return;
        }
        if (! sameBank (c, bank[cur])) beginFade (c);
    }

    // tap: optional, erhaelt das Mono-Signal nach dem Lowcut (fuers Lernen)
    void process (float* const* io, int numCh, int n, float* tap)
    {
        numCh = std::min (numCh, kMaxCh);
        if (numCh <= 0) return;
        if (intResetRequest.exchange (false)) clearIntegrated();
        const double invCh = 1.0 / numCh;
        // De-Esser: Schwelle (Anteil der Hoehen ueber der Frequenz an der Gesamtleistung) und maximale Absenkung
        const double dAmt = params.deessOn ? std::clamp ((double) params.deess, 0.0, 1.0) : 0.0;
        // Schwelle: Zischband gegen tiefen Stimmbereich. Vokale liegen bei etwa -20 dB und tiefer, Zischlaute darueber.
        const double dThr = kDeessThr0 - kDeessThrPerAmt * dAmt;
        const double dMax = kDeessMax0 + kDeessMaxPerAmt * dAmt;
        const double dSlope = kDeessSlope;
        double minDeessGain = 1, peak = 0;

        for (int i = 0; i < n; ++i)
        {
            if (! fading && bankQueued) { bankQueued = false; beginFade (queuedBank); }
            if (ctl == 0) controlTick();
            if (++ctl >= 16) ctl = 0;

            double y[kMaxCh] = {}, hi[kMaxCh] = {};
            double tapSum = 0, hiP = 0, fullP = 0;
            const double a = fading ? (double) fadePos / fadeLen : 0.0;
            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                double x = std::isfinite (io[c][i]) ? io[c][i] : 0.0;
                if (lcOn) x = ch.lc[1].process (lc[1], ch.lc[0].process (lc[0], x));
                tapSum += x;
                double e = x;
                if (! neutral[cur]) for (int k = 0; k < kBands; ++k) e = ch.eq[cur][(size_t) k].process (bank[cur][(size_t) k], e);
                if (fading)
                {
                    double e2 = x;
                    if (! neutral[1 - cur]) for (int k = 0; k < kBands; ++k) e2 = ch.eq[1 - cur][(size_t) k].process (bank[1 - cur][(size_t) k], e2);
                    e = e + a * (e2 - e);
                }
                e = fineEq.process (c, e);
                y[c] = e;
                hi[c] = ch.dt.process (deessTop, ch.dh2.process (deessHp2, ch.dh.process (deessHp, e)));
                const double lo = ch.dl2.process (deessLow2, ch.dl.process (deessLow, e));
                hiP += hi[c] * hi[c];
                fullP += lo * lo;   // tiefer Stimmbereich als Vergleich
            }
            fineEq.advance();
            if (tap != nullptr) tap[i] = (float) (tapSum * invCh);
            if (fading && ++fadePos >= fadeLen) { cur = 1 - cur; fading = false; }

            // De-Esser (Kanaele gekoppelt): erkennt Zischlaute am Hoehenanteil, senkt per Hoehen-Shelf ab
            hiP *= invCh; fullP *= invCh;
            hiPow += (hiP > hiPow ? detAtt : detRel) * (hiP - hiPow);
            fullPow += (fullP > fullPow ? detAtt : detRel) * (fullP - fullPow);
            double targetGain = 1.0;
            if (dAmt > 0.0 && hiPow > 1e-8)   // unter etwa -80 dBFS im Zischband: Stille, nichts tun
            {
                const double ratioDb = 10.0 * std::log10 (hiPow / (fullPow + 1e-20));
                if (ratioDb > dThr)
                    targetGain = std::pow (10.0, -std::min (dMax, (ratioDb - dThr) * dSlope) / 20.0);
            }
            deessGain += (targetGain < deessGain ? envAtt : envRel) * (targetGain - deessGain);
            minDeessGain = std::min (minDeessGain, deessGain);
            if (ctl == 0)
            {
                double redDb = 20.0 * std::log10 (deessGain);
                if (redDb > -0.025) redDb = 0.0; // finish the release at exact unity
                if (std::abs (redDb - shelfDb) > 0.05 || (redDb == 0.0 && shelfDb != 0.0))
                {
                    shelfDb = redDb;
                    shelf = highShelf (fs, std::min ((double) params.deessFreqHz * 0.85, 0.42 * fs), shelfDb);
                }
            }

            listenMix += std::clamp ((params.deessListen ? 1.0 : 0.0) - listenMix, -listenStep, listenStep);
            const double gStatic = gainStatic;
            double kpIn = 0, kpOut = 0;
            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                const double d = ch.ds.process (shelf, y[c]);
                // Listen monitors the actual sample difference, not the detector high-pass.
                // Both paths use the same gain/leveler, and metering follows the heard output.
                const double removed = y[c] - d;
                const double v = (d + listenMix * (removed - d)) * gStatic;
                // Messung fuer den Leveler: nach der Grundverstaerkung, vor dem Leveler
                const double k = ch.k2.process (kHp, ch.k1.process (kShelf, d * gStatic));
                kpIn += k * k;
                const double o = v * gainRide;
                const double ko = ch.o2.process (kHp, ch.o1.process (kShelf, o));
                kpOut += ko * ko;
                io[c][i] = (float) o;
                peak = std::max (peak, std::abs (o));
            }
            msK += loudCoef * (kpIn * invCh - msK);
            outMs += loudCoef * (kpOut * invCh - outMs);
            hopAcc += kpOut * invCh;
            if (++hopPos >= hopLen) { integrateHop (hopAcc / hopLen); hopAcc = 0.0; hopPos = 0; }
        }
        const double maxDe = -20.0 * std::log10 (minDeessGain);
        meterRideDb.store ((float) ride2);
        meterStaticDb.store ((float) staticDb);
        meterDeessDb.store ((float) maxDe);
        if ((float) maxDe > meterDeessPeakDb.load (std::memory_order_relaxed))
            meterDeessPeakDb.store ((float) maxDe, std::memory_order_relaxed);   // Editor holt und setzt zurueck
        meterInLufs.store ((float) (-0.691 + 10.0 * std::log10 (msK + 1e-20)));
        meterOutLufs.store ((float) (-0.691 + 10.0 * std::log10 (outMs + 1e-20)));
        meterOutPeakDb.store ((float) (20.0 * std::log10 (peak + 1e-12)));
    }

    // Histogramm-Klassen: 0,05 LU breit, -70 bis +10 LUFS -> integrierter Wert auf etwa 0,03 LU genau, ohne Speicheranforderung
    static constexpr int kIntBins = 1600;
    static constexpr double kIntBinLu = 0.05, kIntMinLufs = -70.0;

private:
    void integrateHop (double meanSquare)
    {
        hops[(size_t) hopIdx] = meanSquare;
        hopIdx = (hopIdx + 1) % 4;
        if (hopsFilled < 4) { ++hopsFilled; if (hopsFilled < 4) return; }
        const double block = 0.25 * (hops[0] + hops[1] + hops[2] + hops[3]);
        const double lufs = -0.691 + 10.0 * std::log10 (block + 1e-20);
        intSeconds += 0.1;
        if (lufs > kIntMinLufs)
        {
            const int bin = std::clamp ((int) ((lufs - kIntMinLufs) / kIntBinLu), 0, kIntBins - 1);
            ++intHist[(size_t) bin];
            ++intCount;
        }
        meterIntSeconds.store ((float) intSeconds);
        if (intCount == 0) { meterIntLufs.store (-100.0f); return; }
        auto energyOf = [] (int b) { return std::pow (10.0, (kIntMinLufs + (b + 0.5) * kIntBinLu + 0.691) / 10.0); };
        double sum = 0;
        for (int b = 0; b < kIntBins; ++b) if (intHist[(size_t) b] != 0) sum += intHist[(size_t) b] * energyOf (b);
        const double gate = -0.691 + 10.0 * std::log10 (sum / intCount) - 10.0;
        const int gateBin = std::clamp ((int) std::ceil ((gate - kIntMinLufs) / kIntBinLu - 0.5), 0, kIntBins);
        double gs = 0; uint64_t gn = 0;
        for (int b = gateBin; b < kIntBins; ++b) if (intHist[(size_t) b] != 0) { gs += intHist[(size_t) b] * energyOf (b); gn += intHist[(size_t) b]; }
        meterIntLufs.store (gn > 0 ? (float) (-0.691 + 10.0 * std::log10 (gs / (double) gn)) : -100.0f);
    }
    void clearIntegrated()
    {
        intHist.fill (0); intCount = 0; hops.fill (0.0); hopIdx = 0; hopsFilled = 0; hopAcc = 0.0; hopPos = 0; intSeconds = 0.0;
        meterIntLufs.store (-100.0f); meterIntSeconds.store (0.0f);
    }
    std::array<uint32_t, kIntBins> intHist {};
    uint64_t intCount = 0;
    std::array<double, 4> hops {};
    int hopIdx = 0, hopsFilled = 0, hopLen = 4800, hopPos = 0;
    double hopAcc = 0.0, intSeconds = 0.0;

public:

private:
    static bool isNeutral (const std::array<Biquad, kBands>& c)
    {
        return std::all_of (c.begin(), c.end(), [] (const Biquad& b)
            { return b.b0 == 1.0 && b.b1 == b.a1 && b.b2 == b.a2; });
    }
    static bool sameBank (const std::array<Biquad, kBands>& a, const std::array<Biquad, kBands>& b)
    {
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].b0 != b[i].b0 || a[i].b1 != b[i].b1 || a[i].b2 != b[i].b2
                || a[i].a1 != b[i].a1 || a[i].a2 != b[i].a2) return false;
        return true;
    }
    void beginFade (const std::array<Biquad, kBands>& c)
    {
        const int next = 1 - cur;
        bank[next] = c;
        neutral[next] = isNeutral (c);
        // TDF-II delays depend on coefficients. Copying unrelated delays can inject
        // a transient, even into an identity bank. Fade in a clean target instead.
        for (auto& ch : chans) for (auto& state : ch.eq[next]) state.reset();
        fading = true;
        fadePos = 0;
    }
    double desiredStaticDb() const
    {
        if (params.takeLufs <= -99.0f) return 0.0;
        return std::clamp ((double) params.targetLufs - params.takeLufs, -30.0, 30.0);
    }

    // Steuertakt alle 16 Samples: Grundverstaerkung und Leveler
    //  - Grundverstaerkung: aus der gelernten Lautheit, steht ab dem ersten Sample (keine Spruenge beim Starten)
    //  - Leveler: faehrt sanft innerhalb eines begrenzten Bereichs (wie ein Fader), in Pausen haelt er still
    void controlTick()
    {
        staticDb += staticCoef * (desiredStaticDb() - staticDb);   // nur bei Reglerbewegungen

        const double range = kMaxRangeDb * std::clamp ((double) params.leveler, 0.0, 1.0);
        const double lufs = -0.691 + 10.0 * std::log10 (msK + 1e-20);
        // Bezug: der typische Sprechpegel bei Ziel-Lautheit. So hebt der Leveler leise Stellen an und senkt laute ab,
        // ohne die Gesamtlautheit zu veraendern.
        const double target = params.targetLufs + params.rideOffsetDb;
        if (range <= 0.01)
            ride1 += rideOffCoef * (0.0 - ride1);
        else if (lufs > target - 16.0)   // Sprache (Pausen und Atmer liegen darunter)
        {
            const double desired = std::clamp (0.7 * (target - lufs), -range, range);
            const double coef = desired > ride1 ? rideUpCoef : rideDownCoef;   // anheben langsam, absenken etwas schneller
            ride1 += coef * (desired - ride1);
        }
        ride1 = std::clamp (ride1, -range, range);
        ride2 += rideSmoothCoef * (ride1 - ride2);   // zweite Glaettung: weiche Uebergaenge ohne Knick
        gainRide = std::pow (10.0, ride2 / 20.0);
        gainStatic = std::pow (10.0, staticDb / 20.0);
    }

    struct Chan
    {
        std::array<BiquadState, 2> lc;
        std::array<std::array<BiquadState, kBands>, 2> eq;
        BiquadState dh, dh2, dt, dl, dl2, ds, k1, k2, o1, o2;

    };

    double fs = 48000.0;
    EngineParams params;
    FineEq fineEq;
    bool fineDirty = true;
    std::array<Chan, kMaxCh> chans;
    Biquad lc[2], kShelf, kHp, deessHp, deessHp2, deessTop, deessLow, deessLow2, shelf;
    bool lcOn = true;
    float lastLowcut = -1.0f, lastDeessFreq = -1.0f;
    std::array<std::array<Biquad, kBands>, 2> bank {};
    int cur = 0;
    bool fading = false, bankQueued = false;
    bool neutral[2] { true, true };
    std::array<Biquad, kBands> queuedBank {};
    int fadePos = 0, fadeLen = 1440;

    double detAtt = 0, detRel = 0;
    double envAtt = 0, envRel = 0, hiPow = 0, fullPow = 0, deessGain = 1, shelfDb = 0;
    double loudCoef = 0, msK = 0, outMs = 0;
    double staticDb = 0, ride1 = 0, ride2 = 0, gainRide = 1;
    double listenMix = 0, listenStep = 1;
    double gainStatic = 1, staticCoef = 0, rideOffCoef = 0, rideUpCoef = 0, rideDownCoef = 0, rideSmoothCoef = 0;
    int ctl = 0;
};

} // namespace dop
