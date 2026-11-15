#include "PluginProcessor.h"
#include "PluginEditor.h"

DoppelgangerProcessor::DoppelgangerProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pAmount = apvts.getRawParameterValue ("amount");
    pRegister = apvts.getRawParameterValue ("register");
    pLowcut = apvts.getRawParameterValue ("lowcut");
    pDeess = apvts.getRawParameterValue ("deess");
    pLeveler = apvts.getRawParameterValue ("leveler");
    pTarget = apvts.getRawParameterValue ("target");
    pDeessOn = apvts.getRawParameterValue ("deessOn");
    pDeessFreq = apvts.getRawParameterValue ("deessFreq");
    pDeessListen = apvts.getRawParameterValue ("deessListen");
    fifoBuf.assign ((size_t) fifo.getTotalSize(), 0.0f);
    drainBuf.assign ((size_t) fifo.getTotalSize(), 0.0f);
    liveIn.assign ((size_t) kLiveRing, 0.0f);
    liveOut.assign ((size_t) kLiveRing, 0.0f);
    startTimerHz (30);
}

DoppelgangerProcessor::~DoppelgangerProcessor()
{
    stopTimer();
}

juce::StringArray DoppelgangerProcessor::registerNames()
{
    return { "Auto", "Tief (Mann)", "Mittel (Frau)", "Hoch (Frau)" };
}

juce::AudioProcessorValueTreeState::ParameterLayout DoppelgangerProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    auto pct = [] (float v, int) { return String (juce::roundToInt (v)) + " %"; };

    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Angleichung",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 60.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "register", 1 }, "Stimmlage",
        registerNames(), 0));

    NormalisableRange<float> lcRange (20.0f, 200.0f, 1.0f);
    lcRange.setSkewForCentre (80.0f);
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 }, "Lowcut", lcRange, 80.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return v <= 20.5f ? String ("aus") : String (juce::roundToInt (v)) + " Hz"; })));

    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "deessOn", 1 }, "De-Esser an", true));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "deess", 1 }, String (CharPointer_UTF8 ("De-Esser Stärke")),
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 25.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    NormalisableRange<float> dfRange (3000.0f, 10000.0f, 10.0f);
    dfRange.setSkewForCentre (5500.0f);
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "deessFreq", 1 }, "De-Esser Frequenz", dfRange, 5500.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return String (v / 1000.0f, 1) + " kHz"; })));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "deessListen", 1 }, String (CharPointer_UTF8 ("De-Esser nur S hören")), false));

    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "leveler", 1 }, "Leveler",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "target", 1 }, "Ziel-Lautheit",
        NormalisableRange<float> (-30.0f, -14.0f, 0.5f), -23.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return String (v, 1) + " LUFS"; })));

    return { params.begin(), params.end() };
}

void DoppelgangerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentFs = sampleRate;
    engine.prepare (sampleRate);
    {
        const juce::ScopedLock sl (learnerLock);
        learner.prepare (sampleRate);
    }
    learning = false;   // ein laufendes Lernen passt nach einem Neustart nicht mehr zur Abtastrate
    tapBuf.assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);
    setLatencySamples (engine.latencySamples());   // 0: latenzfrei
    designDirty = true;
    redesign();
}

bool DoppelgangerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void DoppelgangerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    dop::EngineParams p;
    p.lowcutHz = pLowcut->load();
    p.deess = pDeess->load() * 0.01f;
    p.deessOn = pDeessOn->load() > 0.5f;
    p.deessFreqHz = pDeessFreq->load();
    p.deessListen = pDeessListen->load() > 0.5f;
    p.takeLufs = takeLufs.load();
    p.rideOffsetDb = rideOffset.load();
    p.leveler = pLeveler->load() * 0.01f;
    p.targetLufs = pTarget->load();
    engine.setParams (p);

    {
        const juce::SpinLock::ScopedTryLockType tl (bankLock);
        if (tl.isLocked() && bankPending)
        {
            engine.setBank (pendingBank);
            bankPending = false;
        }
    }

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    const bool learnNow = learning.load();
    const int chunk = (int) tapBuf.size();
    const uint32_t liveBase = livePos.load (std::memory_order_relaxed);
    for (int off = 0; off < n; off += chunk)
    {
        const int m = juce::jmin (chunk, n - off);
        float* ptrs[2] = { buffer.getWritePointer (0, off), numCh > 1 ? buffer.getWritePointer (1, off) : nullptr };
        for (int i = 0; i < m; ++i)
            liveIn[(size_t) ((liveBase + (uint32_t) (off + i)) & (kLiveRing - 1))] = ptrs[1] ? 0.5f * (ptrs[0][i] + ptrs[1][i]) : ptrs[0][i];
        engine.process (ptrs, numCh, m, learnNow ? tapBuf.data() : nullptr);
        for (int i = 0; i < m; ++i)
            liveOut[(size_t) ((liveBase + (uint32_t) (off + i)) & (kLiveRing - 1))] = ptrs[1] ? 0.5f * (ptrs[0][i] + ptrs[1][i]) : ptrs[0][i];
        if (learnNow)
        {
            const auto scope = fifo.write (m);   // ist der Puffer voll, gehen Samples verloren (nur Lernen betroffen)
            if (scope.blockSize1 > 0) std::copy_n (tapBuf.data(), scope.blockSize1, fifoBuf.data() + scope.startIndex1);
            if (scope.blockSize2 > 0) std::copy_n (tapBuf.data() + scope.blockSize1, scope.blockSize2, fifoBuf.data() + scope.startIndex2);
        }
    }
    livePos.store (liveBase + (uint32_t) n, std::memory_order_release);
}

//==============================================================================
// Live-Spektrum (Message-Thread)
void DoppelgangerProcessor::analyseLive (const std::vector<float>& ring, uint32_t pos, dop::GridCurve& out)
{
    const int N = kLiveFft;
    for (int i = 0; i < N; ++i)
        liveWork[(size_t) i] = { (double) ring[(size_t) ((pos - (uint32_t) N + (uint32_t) i) & (kLiveRing - 1))] * liveWin[(size_t) i], 0.0 };
    liveFft.forward (liveWork);
    for (int k = 0; k <= N / 2; ++k) livePow[(size_t) k] = std::norm (liveWork[(size_t) k]);

    double mean = 0; int cnt = 0;
    for (int g = 0; g < dop::kRefGridPoints; ++g)
    {
        const auto& t = liveTab[(size_t) g];
        double s = 0;
        for (size_t j = 0; j < t.w.size(); ++j) s += t.w[j] * livePow[(size_t) t.k0 + j];
        const double db = 10.0 * std::log10 (s + 1e-20);
        out[(size_t) g] = (float) db;
        if (dop::kRefGridHz[g] >= 100.0f && dop::kRefGridHz[g] <= 8000.0f) { mean += db; ++cnt; }
    }
    mean /= std::max (1, cnt);
    for (auto& v : out) v = (float) std::clamp ((double) v - mean, -60.0, 40.0);
}

bool DoppelgangerProcessor::getLiveSpectrum (dop::GridCurve& inDb, dop::GridCurve& outDb)
{
    const uint32_t pos = livePos.load (std::memory_order_acquire);
    if (pos < (uint32_t) kLiveFft) return false;

    if (liveTabFs != currentFs || liveWork.empty())
    {
        liveTabFs = currentFs;
        liveFft.prepare (kLiveFft);
        liveWork.assign ((size_t) kLiveFft, {});
        livePow.assign ((size_t) kLiveFft / 2 + 1, 0.0);
        liveWin.resize ((size_t) kLiveFft);
        for (int i = 0; i < kLiveFft; ++i) liveWin[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * dop::kPi * i / kLiveFft);
        const double binHz = currentFs / kLiveFft, sigma = 1.0 / 4.0;   // 1/4 Oktave: ruhiger als die gelernte Kurve
        for (int g = 0; g < dop::kRefGridPoints; ++g)
        {
            auto& t = liveTab[(size_t) g];
            const double f = (double) dop::kRefGridHz[g];
            const int k0 = std::max (1, (int) std::floor (f * std::pow (2.0, -3.0 * sigma) / binHz));
            const int k1 = std::min (kLiveFft / 2, (int) std::ceil (f * std::pow (2.0, 3.0 * sigma) / binHz));
            t.k0 = k0;
            t.w.assign ((size_t) std::max (1, k1 - k0 + 1), 0.0f);
            double sw = 0;
            for (int k = k0; k <= k1; ++k)
            {
                const double d = std::log2 (k * binHz / f) / sigma;
                const double w = std::exp (-0.5 * d * d);
                t.w[(size_t) (k - k0)] = (float) w;
                sw += w;
            }
            if (sw <= 0) { t.w.assign (1, 1.0f); continue; }
            for (auto& w : t.w) w = (float) (w / sw);
        }
    }

    // Stille (Pausen, Wiedergabe gestoppt): nichts Neues anzeigen
    double e = 0;
    for (int i = 0; i < kLiveFft; ++i)
    {
        const float v = liveIn[(size_t) ((pos - (uint32_t) kLiveFft + (uint32_t) i) & (kLiveRing - 1))];
        e += (double) v * v;
    }
    if (std::sqrt (e / kLiveFft) < 3e-4) return false;   // unter etwa -70 dBFS

    analyseLive (liveIn, pos, inDb);
    analyseLive (liveOut, pos, outDb);
    return true;
}

//==============================================================================
void DoppelgangerProcessor::drainFifo()
{
    const int ready = fifo.getNumReady();
    if (ready <= 0) return;
    const auto scope = fifo.read (ready);
    if (scope.blockSize1 > 0) std::copy_n (fifoBuf.data() + scope.startIndex1, scope.blockSize1, drainBuf.data());
    if (scope.blockSize2 > 0) std::copy_n (fifoBuf.data() + scope.startIndex2, scope.blockSize2, drainBuf.data() + scope.blockSize1);
    if (learning.load())
    {
        const juce::ScopedLock sl (learnerLock);
        learner.push (drainBuf.data(), scope.blockSize1 + scope.blockSize2);
    }
}

void DoppelgangerProcessor::timerCallback()
{
    drainFifo();
    if (learning.load() && learnedSecondsSoFar() >= kAutoStopSeconds)
        finishLearning();

    const float amount = pAmount->load(), lowcut = pLowcut->load();
    const int reg = (int) pRegister->load();
    if (amount != lastAmount || lowcut != lastLowcut || reg != lastRegister)
        designDirty = true;
    if (designDirty.load())
        redesign();
}

int DoppelgangerProcessor::effectiveRegister() const
{
    const int choice = (int) pRegister->load();
    if (choice > 0) return choice - 1;
    return dop::registerForPitch (info.profile.pitchHz);
}

void DoppelgangerProcessor::redesign()
{
    designDirty = false;
    lastAmount = pAmount->load();
    lastLowcut = pLowcut->load();
    lastRegister = (int) pRegister->load();

    dop::BankDesign d = dop::flatBank();
    {
        const juce::ScopedLock sl (infoLock);
        info.effectiveRegister = effectiveRegister();
        info.correction.fill (0.0f);
        info.fitErrorDb = 0.0f;
        if (info.profile.valid && lastAmount > 0.0f)
        {
            info.correction = dop::correctionCurve (info.profile.curveDb, info.effectiveRegister, lastAmount * 0.01f, lastLowcut);
            d = dop::fitBank (info.correction, currentFs);
            info.fitErrorDb = d.maxErrorDb;
        }
        // Lautheit der Spur nach dem EQ: daraus ergibt sich die feste Grundverstaerkung
        if (info.profile.valid && info.profile.loudnessLufs > -99.0f)
            takeLufs = info.profile.loudnessLufs + dop::eqLoudnessDeltaDb (info.profile.curveDb, info.correction);
        else
            takeLufs = -100.0f;
        rideOffset = info.profile.valid ? info.profile.typicalOffsetDb : -2.5f;
        info.needsRelearnForLoudness = info.profile.valid && info.profile.loudnessLufs <= -99.0f;
    }
    const juce::SpinLock::ScopedLockType sl (bankLock);
    pendingBank = d.coeffs;
    bankPending = true;
}

//==============================================================================
void DoppelgangerProcessor::startLearning()
{
    learning = false;
    drainFifo();   // Reste verwerfen
    {
        const juce::ScopedLock sl (learnerLock);
        learner.reset();
    }
    {
        const juce::ScopedLock sl (infoLock);
        info.message = {};
    }
    learning = true;
}

void DoppelgangerProcessor::finishLearning()
{
    if (! learning.load()) return;
    drainFifo();
    learning = false;
    dop::LearnedProfile p;
    {
        const juce::ScopedLock sl (learnerLock);
        p = learner.finish();
    }
    const juce::ScopedLock sl (infoLock);
    if (p.valid && p.speechSeconds >= kMinLearnSeconds)
    {
        info.profile = p;
        info.message = {};
        designDirty = true;
    }
    else
        info.message = "Zu wenig Sprache erkannt (" + juce::String (p.speechSeconds, 1)
                       + " s). Bitte mindestens " + juce::String ((int) kMinLearnSeconds) + " s Sprache abspielen.";
}

void DoppelgangerProcessor::cancelLearning()
{
    learning = false;
    drainFifo();
}

void DoppelgangerProcessor::forgetProfile()
{
    cancelLearning();
    const juce::ScopedLock sl (infoLock);
    info.profile = {};
    info.message = {};
    designDirty = true;
}

float DoppelgangerProcessor::learnedSecondsSoFar() const
{
    const juce::ScopedLock sl (learnerLock);
    return learner.speechSeconds();
}

DoppelgangerProcessor::DisplayInfo DoppelgangerProcessor::getDisplayInfo() const
{
    const juce::ScopedLock sl (infoLock);
    return info;
}

//==============================================================================
juce::AudioProcessorEditor* DoppelgangerProcessor::createEditor()
{
    return new DoppelgangerEditor (*this);
}

void DoppelgangerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("PROFILE"), nullptr);
    {
        const juce::ScopedLock sl (infoLock);
        if (info.profile.valid)
        {
            juce::ValueTree pt ("PROFILE");
            juce::StringArray vals;
            for (float v : info.profile.curveDb) vals.add (juce::String (v, 3));
            pt.setProperty ("curve", vals.joinIntoString (" "), nullptr);
            pt.setProperty ("pitch", info.profile.pitchHz, nullptr);
            pt.setProperty ("seconds", info.profile.speechSeconds, nullptr);
            pt.setProperty ("lufs", info.profile.loudnessLufs, nullptr);
            pt.setProperty ("typical", info.profile.typicalOffsetDb, nullptr);
            state.appendChild (pt, nullptr);
        }
    }
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DoppelgangerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);

    dop::LearnedProfile p;
    const auto pt = state.getChildWithName ("PROFILE");
    if (pt.isValid())
    {
        juce::StringArray vals;
        vals.addTokens (pt.getProperty ("curve").toString(), " ", "");
        vals.removeEmptyStrings();
        if (vals.size() == dop::kRefGridPoints)
        {
            for (int i = 0; i < dop::kRefGridPoints; ++i) p.curveDb[(size_t) i] = vals[i].getFloatValue();
            p.pitchHz = (float) pt.getProperty ("pitch", 0.0f);
            p.speechSeconds = (float) pt.getProperty ("seconds", 0.0f);
            p.loudnessLufs = (float) pt.getProperty ("lufs", -100.0f);
            p.typicalOffsetDb = (float) pt.getProperty ("typical", -2.5f);
            p.valid = true;
        }
        state.removeChild (pt, nullptr);
    }
    apvts.replaceState (state);
    {
        const juce::ScopedLock sl (infoLock);
        info.profile = p;
        info.message = {};
    }
    designDirty = true;
}

// Pflicht fuer JUCE: Die Factory-Funktion erzeugt die Instanz des Plugins.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DoppelgangerProcessor();
}
