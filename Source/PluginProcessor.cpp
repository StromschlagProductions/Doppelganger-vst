#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <charconv>

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
    learner.prepare (currentFs);
    startTimer (200); // no profile/learning: only lightweight parameter polling

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
    const juce::ScopedLock stateGuard (stateLock);
    currentFs = audioFs = dop::validSampleRate (sampleRate);
    sampleRate = currentFs;
    engine.prepare (sampleRate);
    {
        const juce::ScopedLock sl (stateLock);
        learner.prepare (sampleRate);
    }
    juce::ignoreUnused (samplesPerBlock);
    cancelLearning(); // invalidate queued data from the old sample rate
    liveFill = 0;
    livePrimed = false;
    liveGeneration.fetch_add (2); // preserve enabled state, discard old frames
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
    if (numCh == 0 || n == 0) return;
    const auto session = learnGeneration.load (std::memory_order_acquire);
    const bool learnNow = (session & 1u) != 0;
    const auto liveSession = liveGeneration.load (std::memory_order_acquire);
    const bool capture = (liveSession & 1u) != 0;
    if (audioLiveGeneration != liveSession)
    {
        audioLiveGeneration = liveSession;
        liveFill = 0;
        livePrimed = false;
    }
    for (int off = 0; off < n;)
    {
        const int m = std::min ({ 512, n - off, capture ? 1024 - liveFill % 1024 : 512 });
        float* ptrs[2] = { buffer.getWritePointer (0, off), numCh > 1 ? buffer.getWritePointer (1, off) : nullptr };
        if (capture)
            for (int i = 0; i < m; ++i)
                audioLiveFrame.input[(size_t) (liveFill + i)] = ptrs[1] ? 0.5f * (ptrs[0][i] + ptrs[1][i]) : ptrs[0][i];
        engine.process (ptrs, numCh, m, learnNow ? audioLearnBlock.samples.data() : nullptr);
        if (capture)
        {
            for (int i = 0; i < m; ++i)
                audioLiveFrame.output[(size_t) (liveFill + i)] = ptrs[1] ? 0.5f * (ptrs[0][i] + ptrs[1][i]) : ptrs[0][i];
            liveFill += m;
            if (liveFill == dop::kLiveFft) { liveFill = 0; livePrimed = true; }
            if (livePrimed && liveFill % 1024 == 0)
            {
                audioLiveFrame.end = liveFill;
                audioLiveFrame.generation = liveSession;
                audioLiveFrame.sampleRate = audioFs;
                liveQueue.push (audioLiveFrame); // full: skip frame, never wait
            }
        }
        if (learnNow)
        {
            audioLearnBlock.count = m;
            audioLearnBlock.generation = session;
            if (! learnQueue.push (audioLearnBlock))
                overflowGeneration.store (session, std::memory_order_release);
        }
        off += m;
    }
}

// Message thread only; opening/closing never resets shared queue indices.
void DoppelgangerProcessor::setLiveSpectrumEnabled (bool enabled)
{
    const auto generation = liveGeneration.load();
    if (((generation & 1u) != 0) != enabled) liveGeneration.fetch_add (1);
}

bool DoppelgangerProcessor::getLiveSpectrum (dop::GridCurve& inDb, dop::GridCurve& outDb)
{
    const auto generation = liveGeneration.load();
    bool fresh = false;
    // Bound consumer work even when offline audio is producing continuously.
    for (int i = 0; i < 3 && liveQueue.pop (readFrame); ++i)
        if ((generation & 1u) != 0 && readFrame.generation == generation)
        { displayFrame = readFrame; fresh = true; }
    return fresh && liveSpectrum.analyse (displayFrame, inDb, outDb);
}

//==============================================================================
void DoppelgangerProcessor::drainFifo()
{
    const auto session = learnGeneration.load();
    for (int i = 0; i < 256 && learnQueue.pop (drainBlock); ++i)
        if ((session & 1u) != 0 && drainBlock.generation == session)
            learner.push (drainBlock.samples.data(), drainBlock.count);
    if ((session & 1u) != 0 && overflowGeneration.load() == session)
    {
        learnGeneration.fetch_add (1);
        info.message = juce::String::fromUTF8 ("Lernen unterbrochen: Analyse konnte nicht folgen. Bitte erneut lernen.");
    }
}

void DoppelgangerProcessor::timerCallback()
{
    const juce::ScopedLock stateGuard (stateLock);
    if (isLearning()) drainFifo();
    if (isLearning())
    {
        const float sec = learnedSecondsSoFar();
        bool enough = sec >= kAutoStopSeconds;
        if (! enough && sec >= kConvergeCheckFromSeconds && sec >= nextConvergeCheck)
        {
            nextConvergeCheck = sec + 1.0f;
            dop::LearnedProfile snap;
            {
                const juce::ScopedLock sl (stateLock);
                snap = learner.finish();
            }
            enough = convergence.update (snap);
        }
        if (enough)
        {
            finishLearning();
            stoppedAuto = info.message.isEmpty();
        }
    }

    const float amount = pAmount->load(), lowcut = pLowcut->load();
    const int reg = (int) dop::finiteClamp (pRegister->load(), 0, 3, 0);
    if (amount != lastAmount || lowcut != lastLowcut || reg != lastRegister)
        designDirty = true;
    if (designDirty.load())
        redesign();
    const int interval = isLearning() || info.profile.valid ? 33 : 200;
    if (getTimerInterval() != interval) startTimer (interval);
}

int DoppelgangerProcessor::effectiveRegister() const
{
    const int choice = (int) dop::finiteClamp (pRegister->load(), 0, 3, 0);
    if (choice > 0) return choice - 1;
    return dop::registerForPitch (info.profile.pitchHz);
}

void DoppelgangerProcessor::redesign()
{
    const juce::ScopedLock stateGuard (stateLock);
    designDirty = false;
    lastAmount = dop::finiteClamp (pAmount->load(), 0, 100, 60);
    lastLowcut = dop::finiteClamp (pLowcut->load(), 20, 200, 80);
    lastRegister = (int) dop::finiteClamp (pRegister->load(), 0, 3, 0);

    dop::BankDesign d = dop::flatBank();
    {
        const juce::ScopedLock sl (stateLock);
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
    {
        const juce::SpinLock::ScopedLockType sl (bankLock);
        pendingBank = d.coeffs;
        bankPending = true;
    }
    const int interval = isLearning() || info.profile.valid ? 33 : 200;
    if (getTimerInterval() != interval) startTimer (interval);
}

//==============================================================================
void DoppelgangerProcessor::startLearning()
{
    const juce::ScopedLock stateGuard (stateLock);
    cancelLearning();
    drainFifo(); // discard old generations
    {
        const juce::ScopedLock sl (stateLock);
        learner.reset();
    }
    {
        const juce::ScopedLock sl (stateLock);
        info.message = {};
    }
    convergence.reset();
    nextConvergeCheck = kConvergeCheckFromSeconds;
    stoppedAuto = false;
    learnGeneration.fetch_add (1);
    startTimerHz (30);
}

void DoppelgangerProcessor::finishLearning()
{
    const juce::ScopedLock stateGuard (stateLock);
    if (! isLearning()) return;
    drainFifo();
    if (! isLearning()) return; // overflow: keep the existing profile
    learnGeneration.fetch_add (1);
    dop::LearnedProfile p;
    {
        const juce::ScopedLock sl (stateLock);
        p = learner.finish();
    }
    const juce::ScopedLock sl (stateLock);
    if (p.valid && p.speechSeconds >= kMinLearnSeconds)
    {
        info.profile = p;
        info.message = {};
        designDirty = true;
        redesign();
    }
    else
        info.message = "Zu wenig Sprache erkannt (" + juce::String (p.speechSeconds, 1)
                       + " s). Bitte mindestens " + juce::String ((int) kMinLearnSeconds) + " s Sprache abspielen.";
}

void DoppelgangerProcessor::cancelLearning()
{
    const juce::ScopedLock stateGuard (stateLock);
    if (isLearning()) learnGeneration.fetch_add (1);
    drainFifo();
}

void DoppelgangerProcessor::forgetProfile()
{
    const juce::ScopedLock stateGuard (stateLock);
    cancelLearning();
    const juce::ScopedLock sl (stateLock);
    info.profile = {};
    info.message = {};
    stoppedAuto = false;
    designDirty = true;
    redesign();
}

float DoppelgangerProcessor::learnedSecondsSoFar() const
{
    const juce::ScopedLock sl (stateLock);
    return learner.speechSeconds();
}

DoppelgangerProcessor::DisplayInfo DoppelgangerProcessor::getDisplayInfo() const
{
    const juce::ScopedLock sl (stateLock);
    return info;
}

//==============================================================================
juce::AudioProcessorEditor* DoppelgangerProcessor::createEditor()
{
    return new DoppelgangerEditor (*this);
}

void DoppelgangerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock stateGuard (stateLock);
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("PROFILE"), nullptr);
    {
        const juce::ScopedLock sl (stateLock);
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
    const juce::ScopedLock stateGuard (stateLock);
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
            bool parsed = true;
            for (int i = 0; i < dop::kRefGridPoints; ++i)
            {
                const auto* text = vals[i].toRawUTF8();
                const auto* end = text + vals[i].getNumBytesAsUTF8();
                const auto result = std::from_chars (text, end, p.curveDb[(size_t) i]);
                parsed = parsed && result.ec == std::errc {} && result.ptr == end;
            }
            p.pitchHz = (float) pt.getProperty ("pitch", 0.0f);
            p.speechSeconds = (float) pt.getProperty ("seconds", 0.0f);
            p.loudnessLufs = (float) pt.getProperty ("lufs", -100.0f);
            p.typicalOffsetDb = (float) pt.getProperty ("typical", -2.5f);
            p.valid = parsed && dop::isValidProfile (p);
        }
        state.removeChild (pt, nullptr);
    }
    cancelLearning();
    stoppedAuto = false;
    apvts.replaceState (state);
    {
        const juce::ScopedLock sl (stateLock);
        info.profile = p;
        info.message = {};
    }
    designDirty = true;
    redesign(); // state restore must not depend on an editor or message-loop tick
}

juce::var DoppelgangerProcessor::getUiProperty (const juce::Identifier& key, const juce::var& fallback) const
{
    const juce::ScopedLock guard (stateLock);
    return apvts.state.getProperty (key, fallback);
}

void DoppelgangerProcessor::setUiProperty (const juce::Identifier& key, const juce::var& value)
{
    const juce::ScopedLock guard (stateLock);
    apvts.state.setProperty (key, value, nullptr);
}

// Pflicht fuer JUCE: Die Factory-Funktion erzeugt die Instanz des Plugins.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DoppelgangerProcessor();
}
