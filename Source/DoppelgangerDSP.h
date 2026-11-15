// DoppelgangerDSP.h - Klang-Engine fuer "Doppelganger" (Stromschlag Audio)
// Reines C++ ohne JUCE, damit die Engine unabhaengig vom Plugin getestet werden kann (tests/).
//
// Signalweg je Kanal:
//   Eingang -> Lowcut (24 dB/Okt.) -> Match-EQ (26 Glockenfilter, gelernt) -> De-Esser -> Leveler -> Limiter -> Ausgang
//
// Lernen (Learner): Das Plugin schickt das Signal nach dem Lowcut in einen Puffer, ein Hintergrund-Takt
// (nicht der Audio-Thread) misst daraus das Langzeit-Spektrum der Sprachanteile und die Tonhoehe.
// Aus gelerntem Spektrum und Referenzkurve entsteht die Korrekturkurve, die mit einer Bank aus
// Glockenfiltern nachgebildet wird (latenzfrei). Nur der Limiter braucht 1,5 ms Vorlauf.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstring>
#include <utility>
#include <vector>
#include "ReferenceCurves.h"

namespace dop
{
constexpr double kPi = 3.14159265358979323846;

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
    bool valid = false;
};

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
        fs = sampleRate;
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
        for (auto* s : { &s1, &s2, &s3, &s4 }) s->reset();
    }

    // Samples (mono) einspeisen
    void push (const float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const double v = x[i];
            // Spektrum
            buf[(size_t) (filled % frameLen)] = v;
            ++filled;
            if (filled >= frameLen && ++sinceHop >= hop)
            {
                sinceHop = 0;
                analyseFrame();
            }
            // Tonhoehe
            double b = s4.process (bp4, s3.process (bp3, s2.process (bp2, s1.process (bp1, v))));
            pBuf[(size_t) (pFilled % pLen)] = b;
            pRaw = v;
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
    long filled = 0, sinceHop = 0, speechFrames = 0;

    int pLen = 1920, pHop = 960, minLag = 137, maxLag = 738, pFftLen = 4096;
    FFT pFft;
    std::vector<double> pWindow, pBuf, wAcf;
    std::vector<std::complex<double>> pWork;
    Biquad bp1, bp2, bp3, bp4;
    BiquadState s1, s2, s3, s4;
    long pFilled = 0, pSinceHop = 0;
    double pRaw = 0;
    std::vector<std::pair<float, float>> pitches;   // (Hz, Pegel dB)
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
inline GridCurve correctionCurve (const GridCurve& take, int reg, float amount, float lowcutHz)
{
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
        out[(size_t) i] = (float) (d * fadeIn * fadeTop * fadeBand);
    }
    return out;
}

//==============================================================================
//  Filterbank: 26 Glockenfilter im Terzabstand (50 Hz - 16 kHz), Verstaerkungen per Ausgleichsrechnung
//==============================================================================
constexpr int kBands = 26;
constexpr double kBandQ = 2.1;

inline double bandFreq (int k) { return 50.0 * std::pow (2.0, k / 3.0); }

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
//  Engine (Audio-Thread): Lowcut, Match-EQ, De-Esser, Leveler, Limiter
//==============================================================================
struct EngineParams
{
    float lowcutHz = 80.0f;     // <= 20 = aus
    float deess = 0.25f;        // 0..1, 0 = aus
    float leveler = 0.5f;       // 0..1: wie stark kurzzeitige Lautstaerkeschwankungen ausgeglichen werden
    float targetLufs = -23.0f;  // Ziel-Lautheit
};

class Engine
{
public:
    static constexpr int kMaxCh = 2;
    static constexpr double kCeilingDb = -1.2;   // Limiter-Decke (Zwischenwerte grob mitgeschaetzt -> ca. -1 dBTP)

    std::atomic<float> meterLevelerDb { 0.0f }, meterDeessDb { 0.0f }, meterLimiterDb { 0.0f },
                       meterInLufs { -100.0f }, meterOutLufs { -100.0f };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        L = std::max (8, (int) std::lround (0.0015 * fs));
        W = L + 3;
        delayLen = L + 3;
        for (auto& ch : chans) ch.delay.assign ((size_t) delayLen, 0.0);
        dqCap = W + 1;
        dqVal.assign ((size_t) dqCap, 1.0);
        dqIdx.assign ((size_t) dqCap, 0);
        envRing.assign ((size_t) L, 1.0);
        kShelf = kWeightShelf (fs);
        kHp = kWeightHighPass (fs);
        deessHp = highPass (fs, 5000.0, 0.7071);
        for (auto& b : bank[0]) b = Biquad {};
        for (auto& b : bank[1]) b = Biquad {};
        fadeLen = std::max (1, (int) std::lround (0.03 * fs));
        loudCoef = 1.0 - std::exp (-1.0 / (0.4 * fs));
        subLen = (int) std::lround (0.1 * fs);
        envAtt = 1.0 - std::exp (-1.0 / (0.0005 * fs));
        envRel = 1.0 - std::exp (-1.0 / (0.05 * fs));
        limRel = 1.0 - std::exp (-1.0 / (0.08 * fs));
        lastLowcut = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto& ch : chans)
        {
            for (auto& s : ch.lc) s.reset();
            for (auto& s : ch.eq[0]) s.reset();
            for (auto& s : ch.eq[1]) s.reset();
            ch.dh.reset(); ch.ds.reset(); ch.k1.reset(); ch.k2.reset();
            std::fill (ch.delay.begin(), ch.delay.end(), 0.0);
            ch.h1 = ch.h2 = ch.h3 = 0.0;
        }
        dqHead = 0; dqCount = 0; sampleIdx = 0;
        std::fill (envRing.begin(), envRing.end(), 1.0);
        envSum = (double) L;
        limEnv = 1.0;
        pos = 0; envPos = 0;
        fading = false; fadePos = 0;
        hiPow = fullPow = 0.0; deessGain = 1.0; shelfDb = 0.0; shelf = Biquad {};
        msK = 0.0; kpAcc = blockAcc = 0.0; blockN = 0; subPos = 0; subCount = 0; sub.fill (0.0);
        histW.fill (0.0); histP.fill (0.0); integratedLufs = -70.0; gatedBlocks = 0.0; meanDb = -70.0; openTime = 0.0; haveIntegrated = false;
        gFastDb = 0.0; gTotalDb = 0.0; levelerGain = 1.0;
        ctl = 0;
        outMs = 0.0;
    }

    int latencySamples() const { return L + 2; }

    void setParams (const EngineParams& p)
    {
        params = p;
        if (p.lowcutHz != lastLowcut)
        {
            lastLowcut = p.lowcutHz;
            lcOn = p.lowcutHz > 20.0f;
            if (lcOn)
            {
                lc[0] = highPass (fs, p.lowcutHz, 0.5412);   // Butterworth 4. Ordnung
                lc[1] = highPass (fs, p.lowcutHz, 1.3066);
            }
        }
    }

    // Neue Filterbank uebernehmen: weiche Ueberblendung ueber 30 ms
    void setBank (const std::array<Biquad, kBands>& c)
    {
        if (fading) { cur = 1 - cur; fading = false; }
        const int nx = 1 - cur;
        bank[nx] = c;
        for (auto& ch : chans) ch.eq[nx] = ch.eq[cur];   // Zustand mitnehmen, damit nichts knackt
        fading = true;
        fadePos = 0;
    }

    // tap: optional, erhaelt das Mono-Signal nach dem Lowcut (fuers Lernen)
    void process (float* const* io, int numCh, int n, float* tap)
    {
        numCh = std::min (numCh, kMaxCh);
        if (numCh <= 0) return;
        const double invCh = 1.0 / numCh;
        const double ceiling = std::pow (10.0, kCeilingDb / 20.0);
        // De-Esser: Schwelle (Anteil der Hoehen ueber 5 kHz an der Gesamtleistung) und maximale Absenkung
        const double dAmt = std::clamp ((double) params.deess, 0.0, 1.0);
        const double dThr = -6.0 - 10.0 * dAmt;
        const double dMax = 12.0 * dAmt;
        double maxLev = 0, maxDe = 0, maxLim = 0;

        for (int i = 0; i < n; ++i)
        {
            double y[kMaxCh] = {}, hi[kMaxCh] = {};
            double tapSum = 0, hiP = 0, fullP = 0;
            const double a = fading ? (double) fadePos / fadeLen : 0.0;
            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                double x = io[c][i];
                if (lcOn) x = ch.lc[1].process (lc[1], ch.lc[0].process (lc[0], x));
                tapSum += x;
                double e = x;
                for (int k = 0; k < kBands; ++k) e = ch.eq[cur][(size_t) k].process (bank[cur][(size_t) k], e);
                if (fading)
                {
                    double e2 = x;
                    for (int k = 0; k < kBands; ++k) e2 = ch.eq[1 - cur][(size_t) k].process (bank[1 - cur][(size_t) k], e2);
                    e = e + a * (e2 - e);
                }
                y[c] = e;
                hi[c] = ch.dh.process (deessHp, e);
                hiP += hi[c] * hi[c];
                fullP += e * e;
            }
            if (tap != nullptr) tap[i] = (float) (tapSum * invCh);
            if (fading && ++fadePos >= fadeLen) { cur = 1 - cur; fading = false; }

            // De-Esser (Kanaele gekoppelt)
            hiP *= invCh; fullP *= invCh;
            hiPow += (hiP > hiPow ? envAtt : envRel) * (hiP - hiPow);
            fullPow += (fullP > fullPow ? envAtt : envRel) * (fullP - fullPow);
            double targetGain = 1.0;
            if (dAmt > 0.0 && hiPow > 1e-9)   // Hoehenanteil ueber -90 dBFS
            {
                const double ratioDb = 10.0 * std::log10 (hiPow / (fullPow + 1e-20));
                if (ratioDb > dThr)
                {
                    const double red = std::min (dMax, (ratioDb - dThr) * (2.0 / 3.0));
                    targetGain = std::pow (10.0, -red / 20.0);
                }
            }
            deessGain += (targetGain < deessGain ? envAtt : envRel) * (targetGain - deessGain);
            maxDe = std::max (maxDe, -20.0 * std::log10 (deessGain));
            // Absenkung als Hoehen-Shelf ab 4,5 kHz (phasenrichtig), Koeffizienten im 16er-Takt nachfuehren
            if ((ctl & 15) == 0)
            {
                const double redDb = 20.0 * std::log10 (deessGain);
                if (std::abs (redDb - shelfDb) > 0.05) { shelfDb = redDb; shelf = highShelf (fs, 4500.0, shelfDb); }
            }

            // K-gewichtete Lautheit vor dem Leveler (Mittel der Kanaele = Monobezug wie bei einer Sprachdatei)
            double kp = 0;
            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                if (shelfDb < -0.01) y[c] = ch.ds.process (shelf, y[c]);
                else ch.ds.process (shelf, y[c]);   // Zustand mitlaufen lassen
                const double k = ch.k2.process (kHp, ch.k1.process (kShelf, y[c]));
                kp += k * k;
            }
            msK += loudCoef * (kp * invCh - msK);
            kpAcc += kp * invCh;

            if (++ctl >= 16) { ctl = 0; updateLeveler(); }

            // Limiter mit Vorlauf
            double peak = 0;
            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                const double v = y[c] * levelerGain;
                ch.delay[(size_t) pos] = v;
                // Zwischenwerte zwischen den beiden vorherigen Samples (kubisch) mitschaetzen
                const double p0 = ch.h3, p1 = ch.h2, p2 = ch.h1, p3 = v;
                double pk = std::max (std::abs (p2), std::abs (p3));
                for (double t : { 0.25, 0.5, 0.75 })
                {
                    const double m = p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0)));
                    pk = std::max (pk, std::abs (m));
                }
                ch.h3 = ch.h2; ch.h2 = ch.h1; ch.h1 = v;
                peak = std::max (peak, pk);
            }
            // gleitendes Minimum ueber W Samples (monotone Warteschlange, O(1) im Mittel)
            const double rq = peak > ceiling ? ceiling / peak : 1.0;
            ++sampleIdx;
            while (dqCount > 0 && dqVal[(size_t) ((dqHead + dqCount - 1) % dqCap)] >= rq) --dqCount;
            dqVal[(size_t) ((dqHead + dqCount) % dqCap)] = rq;
            dqIdx[(size_t) ((dqHead + dqCount) % dqCap)] = sampleIdx;
            ++dqCount;
            while (dqIdx[(size_t) dqHead] <= sampleIdx - W) { dqHead = (dqHead + 1) % dqCap; --dqCount; }
            const double req = dqVal[(size_t) dqHead];
            limEnv = req < limEnv ? req : limEnv + limRel * (req - limEnv);
            envSum += limEnv - envRing[(size_t) envPos];
            envRing[(size_t) envPos] = limEnv;
            envPos = (envPos + 1) % L;
            const double g = std::min (1.0, envSum / L);
            maxLim = std::max (maxLim, -20.0 * std::log10 (std::max (g, 1e-6)));

            const int rd = (pos + 1) % delayLen;   // aeltester Wert = L+2 Samples verzoegert
            double om = 0;
            for (int c = 0; c < numCh; ++c)
            {
                const double o = chans[(size_t) c].delay[(size_t) rd] * g;
                io[c][i] = (float) o;
                om += o * o;
            }
            outMs += loudCoef * (om * invCh - outMs);
            pos = rd;
            maxLev = std::max (maxLev, std::abs (gTotalDb));
        }
        meterLevelerDb.store ((float) gTotalDb);
        meterDeessDb.store ((float) maxDe);
        meterLimiterDb.store ((float) maxLim);
        meterInLufs.store ((float) (-0.691 + 10.0 * std::log10 (msK + 1e-20)));
        meterOutLufs.store ((float) (-0.691 + 10.0 * std::log10 (outMs + 1e-20)));
    }

private:
    // Leveler in zwei Stufen:
    //  1) langsamer Normalisierer (ca. 8 s): bringt die mittlere Lautheit immer aufs Ziel (wie das Pre-Flight-Tool)
    //  2) schnellerer Ausgleich (ca. 0,5-1 s), Staerke einstellbar: gleicht Schwankungen innerhalb der Saetze an
    // In Pausen (unter Ziel - 25 LU) wird nichts nachgeregelt, damit Rauschen nicht hochgezogen wird.
    // Integrierte Lautheit nach BS.1770, laufend: 400-ms-Bloecke im 100-ms-Raster, absolutes Tor -70 LUFS,
    // relatives Tor -10 LU. Aeltere Bloecke werden langsam vergessen (ca. 20 s), damit der Pegel auf langen
    // Spuren mitwandert. Ohne Vergessen waere es exakt die integrierte Lautheit einer Datei.
    static constexpr int kHistBins = 300;          // 0,25-LU-Schritte von -70 bis +5 LUFS
    void addLoudnessBlock (double blockPow)
    {
        const double lb = -0.691 + 10.0 * std::log10 (blockPow + 1e-20);
        const double decay = std::exp (-0.1 / 20.0);
        for (int b = 0; b < kHistBins; ++b) { histW[(size_t) b] *= decay; histP[(size_t) b] *= decay; }
        if (lb <= -70.0) return;
        const int bin = std::clamp ((int) ((lb + 70.0) * 4.0), 0, kHistBins - 1);
        histW[(size_t) bin] += 1.0;
        histP[(size_t) bin] += blockPow;
        double w = 0, pw = 0;
        for (int b = 0; b < kHistBins; ++b) { w += histW[(size_t) b]; pw += histP[(size_t) b]; }
        if (w < 1e-9) return;
        const double relThr = -0.691 + 10.0 * std::log10 (pw / w) - 10.0;
        const int from = std::clamp ((int) std::ceil ((relThr + 70.0) * 4.0), 0, kHistBins);
        w = 0; pw = 0;
        for (int b = from; b < kHistBins; ++b) { w += histW[(size_t) b]; pw += histP[(size_t) b]; }
        if (w < 1e-9) return;
        integratedLufs = -0.691 + 10.0 * std::log10 (pw / w);
        gatedBlocks += 1.0;
        haveIntegrated = gatedBlocks >= 3.0;   // nach 300 ms Sprache
    }

    // Leveler in zwei Stufen:
    //  1) Normalisierer: bringt die integrierte Lautheit (s. o.) aufs Ziel, wie das Pre-Flight-Tool
    //  2) Ausgleich innerhalb der Saetze (ca. 0,5-1 s), Staerke einstellbar
    // In Pausen (10 LU unter dem Mittel) wird nichts nachgeregelt, damit Rauschen nicht hochgezogen wird.
    void updateLeveler()
    {
        const double dt = 16.0 / fs;
        blockAcc += kpAcc; kpAcc = 0.0; blockN += 16;
        if (blockN >= subLen)
        {
            sub[(size_t) subPos] = blockAcc / blockN;
            subPos = (subPos + 1) % 4;
            blockAcc = 0.0; blockN = 0;
            if (++subCount >= 4) addLoudnessBlock ((sub[0] + sub[1] + sub[2] + sub[3]) * 0.25);
        }

        const double lufs = -0.691 + 10.0 * std::log10 (msK + 1e-20);
        if (haveIntegrated && lufs > std::max (integratedLufs - 10.0, -60.0))
        {
            // Bezug ist der Mittelwert in dB (nicht in Leistung): so hebt der Ausgleich die Gesamtlautheit nicht an
            openTime += dt;
            meanDb += (dt / std::min (openTime, 20.0)) * (lufs - meanDb);
            const double desiredFast = std::clamp ((meanDb - lufs) * (double) params.leveler, -9.0, 9.0);
            const double tau = desiredFast > gFastDb ? 1.0 : 0.5;
            gFastDb += (1.0 - std::exp (-dt / tau)) * (desiredFast - gFastDb);
        }
        const double slowGain = haveIntegrated ? std::clamp ((double) params.targetLufs - integratedLufs, -18.0, 18.0) : 0.0;
        const double desired = std::clamp (slowGain + gFastDb, -18.0, 18.0);
        gTotalDb += (1.0 - std::exp (-dt / 0.2)) * (desired - gTotalDb);   // letzte Glaettung gegen Spruenge
        levelerGain = std::pow (10.0, gTotalDb / 20.0);
    }

    struct Chan
    {
        std::array<BiquadState, 2> lc;
        std::array<std::array<BiquadState, kBands>, 2> eq;
        BiquadState dh, ds, k1, k2;
        std::vector<double> delay;
        double h1 = 0, h2 = 0, h3 = 0;
    };

    double fs = 48000.0;
    EngineParams params;
    std::array<Chan, kMaxCh> chans;
    Biquad lc[2], kShelf, kHp, deessHp;
    bool lcOn = true;
    float lastLowcut = -1.0f;
    std::array<std::array<Biquad, kBands>, 2> bank {};
    int cur = 0;
    bool fading = false;
    int fadePos = 0, fadeLen = 1440;

    double envAtt = 0, envRel = 0, hiPow = 0, fullPow = 0, deessGain = 1, shelfDb = 0;
    Biquad shelf;
    double loudCoef = 0, msK = 0, outMs = 0, gFastDb = 0, gTotalDb = 0, levelerGain = 1;
    double kpAcc = 0, blockAcc = 0, integratedLufs = -70, gatedBlocks = 0, meanDb = -70, openTime = 0;
    int blockN = 0, subLen = 4800, subPos = 0, subCount = 0;
    bool haveIntegrated = false;
    std::array<double, 4> sub {};
    std::array<double, kHistBins> histW {}, histP {};
    int ctl = 0;

    int L = 72, W = 75, delayLen = 75, pos = 0, envPos = 0;
    int dqCap = 76, dqHead = 0, dqCount = 0;
    long long sampleIdx = 0;
    std::vector<double> dqVal, envRing;
    std::vector<long long> dqIdx;
    double envSum = 72, limEnv = 1, limRel = 0;
};

} // namespace dop
