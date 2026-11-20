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
    pMatchLo = apvts.getRawParameterValue ("matchLow");
    pMatchHi = apvts.getRawParameterValue ("matchHigh");
    pDeess = apvts.getRawParameterValue ("deess");
    pLeveler = apvts.getRawParameterValue ("leveler");
    pTarget = apvts.getRawParameterValue ("target");
    pDeessOn = apvts.getRawParameterValue ("deessOn");
    pDeessFreq = apvts.getRawParameterValue ("deessFreq");
    pDeessListen = apvts.getRawParameterValue ("deessListen");
    pFineOn = apvts.getRawParameterValue ("fineOn");
    for (int k = 0; k < dop::kFineBands; ++k)
    {
        const auto n = juce::String (k + 1);
        pFineHz[(size_t) k] = apvts.getRawParameterValue ("fineHz" + n);
        pFineDb[(size_t) k] = apvts.getRawParameterValue ("fineDb" + n);
        pFineQ[(size_t) k] = apvts.getRawParameterValue ("fineQ" + n);
    }
    pAutopilot = apvts.getRawParameterValue ("autopilot");
    learner.prepare (currentFs);
    calibWorker.startThread();
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

    // Autopilot: stellt nach dem Lernen Angleichung und Lowcut selbst ein und misst die Lautheit der ganzen Kette nach
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "autopilot", 2 }, "Autopilot", true));

    NormalisableRange<float> lcRange (20.0f, 200.0f, 1.0f);
    lcRange.setSkewForCentre (80.0f);
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 }, "Lowcut", lcRange, 80.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return v <= 20.5f ? String ("aus") : String (juce::roundToInt (v)) + " Hz"; })));

    // Angleich-Bereich: nur dazwischen wird angeglichen, ausserhalb bleibt der Take unveraendert
    auto hzText = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (juce::roundToInt (v)) + " Hz"; };
    NormalisableRange<float> mloRange (dop::kMatchLoMin, dop::kMatchLoMax, 1.0f);
    mloRange.setSkewForCentre (200.0f);
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "matchLow", 1 }, "Angleich ab", mloRange, dop::kMatchLoDefault,
        AudioParameterFloatAttributes().withStringFromValueFunction ([hzText] (float v, int n)
            { return v <= dop::kMatchLoMin + 0.5f ? String ("ganz unten") : hzText (v, n); })));
    NormalisableRange<float> mhiRange (dop::kMatchHiMin, dop::kMatchHiMax, 10.0f);
    mhiRange.setSkewForCentre (6000.0f);
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "matchHigh", 1 }, "Angleich bis", mhiRange, dop::kMatchHiDefault,
        AudioParameterFloatAttributes().withStringFromValueFunction ([hzText] (float v, int n)
            { return v >= dop::kMatchHiMax - 0.5f ? String ("ganz oben") : hzText (v, n); })));

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

    // Feinabstimmung: fuenf optionale Glockenfilter, Standard flach (0 dB)
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "fineOn", 1 }, "Feinabstimmung an", false));
    const float fineDefaults[dop::kFineBands] { 100.0f, 300.0f, 1000.0f, 3000.0f, 8000.0f };
    for (int k = 0; k < dop::kFineBands; ++k)
    {
        const auto n = String (k + 1);
        NormalisableRange<float> fRange (40.0f, 20000.0f, 1.0f);
        fRange.setSkewForCentre (1000.0f);
        params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "fineHz" + n, 1 }, "Band " + n + " Frequenz", fRange, fineDefaults[k],
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (juce::roundToInt (v)) + " Hz"; })));
        params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "fineDb" + n, 1 }, "Band " + n + " Gain",
            NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                { return String (v, 1) + " dB"; })));
        params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "fineQ" + n, 1 }, "Band " + n + String (CharPointer_UTF8 (" Güte")),
            NormalisableRange<float> (0.3f, 8.0f, 0.01f, 0.5f), 1.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                { return String (v, 2); })));
    }

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
    p.calibDb = pAutopilot->load() > 0.5f ? calibDb.load() : 0.0f;
    p.peakGuard = pAutopilot->load() > 0.5f;
    p.leveler = pLeveler->load() * 0.01f;
    p.targetLufs = pTarget->load();
    p.fineOn = pFineOn->load() > 0.5f;
    for (size_t k = 0; k < (size_t) dop::kFineBands; ++k)
    {
        p.fineHz[k] = pFineHz[k]->load();
        p.fineDb[k] = pFineDb[k]->load();
        p.fineQ[k] = pFineQ[k]->load();
    }
    engine.setParams (p);

    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            const bool playing = pos->getIsPlaying();
            if (playing && ! wasPlaying) { engine.resetIntegrated(); engine.resetLeveler(); }
            wasPlaying = playing;
        }

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
    if (learnNow && audioLearnBlock.generation != session) engine.resetLearningMeter();
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
        if (learnNow)
        {
            int clipped = 0;
            for (int c = 0; c < numCh; ++c)
                for (int i = 0; i < m; ++i) clipped += std::abs (ptrs[c][i]) >= 0.999f ? 1 : 0;
            audioLearnBlock.clipped = clipped;
            for (int i = 0; i < m; ++i)   // vor dem Lowcut, damit die Nachmessung jeden Lowcut nachrechnen kann
                audioLearnBlock.raw[(size_t) i] = numCh > 1 ? 0.5f * (ptrs[0][i] + ptrs[1][i]) : ptrs[0][i];
        }
        if (capture)
            for (int i = 0; i < m; ++i)
                audioLiveFrame.input[(size_t) (liveFill + i)] = ptrs[1] ? 0.5f * (ptrs[0][i] + ptrs[1][i]) : ptrs[0][i];
        engine.process (ptrs, numCh, m, learnNow ? audioLearnBlock.samples.data() : nullptr,
                        learnNow ? audioLearnBlock.loudnessPower.data() : nullptr);
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
            audioLearnBlock.channels = numCh;
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
            learner.push (drainBlock.samples.data(), drainBlock.count, drainBlock.loudnessPower.data(), drainBlock.channels, drainBlock.clipped,
                          drainBlock.raw.data());
    if ((session & 1u) != 0 && overflowGeneration.load() == session)
    {
        learnGeneration.fetch_add (1);
        info.message = juce::String::fromUTF8 ("Lernen unterbrochen: Analyse konnte nicht folgen. Bitte erneut lernen.");
    }
}

void DoppelgangerProcessor::timerCallback()
{
    if (autoApplyPending) applyAutopilotSettings();   // Parameter nie unter stateLock setzen
    if (deessApplyPending) applyDeesserChoice();
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
    const float mlo = pMatchLo->load(), mhi = pMatchHi->load();
    if (amount != lastAmount || lowcut != lastLowcut || reg != lastRegister || mlo != lastMatchLo || mhi != lastMatchHi)
        designDirty = true;
    if (designDirty.load())
        redesign();

    // Feinabstimmung aendert nur die Grundverstaerkung, kein Neudesign des Angleichs
    {
        std::array<float, 3 * dop::kFineBands + 1> now {};
        now[0] = pFineOn->load() > 0.5f ? 1.0f : 0.0f;
        for (size_t k = 0; k < (size_t) dop::kFineBands; ++k)
        {
            now[1 + 3 * k] = pFineHz[k]->load();
            now[2 + 3 * k] = pFineDb[k]->load();
            now[3 + 3 * k] = pFineQ[k]->load();
        }
        if (now != lastFineSnap)
            updateTakeLoudness();
    }
    updateCalibration();
    const int interval = isLearning() || info.profile.valid ? 33 : 200;
    if (getTimerInterval() != interval) startTimer (interval);
}

int DoppelgangerProcessor::effectiveRegister() const
{
    const int choice = (int) dop::finiteClamp (pRegister->load(), 0, 3, 0);
    if (choice > 0) return choice - 1;
    return dop::registerForPitch (info.profile.pitchHz);
}

void DoppelgangerProcessor::updateTakeLoudness()
{
    // Lautheit der Spur nach Angleich und Feinabstimmung: daraus ergibt sich die feste Grundverstaerkung
    dop::GridCurve total = info.correction;
    dop::GridCurve fine {};
    std::array<float, dop::kFineBands> hz {}, db {}, q {};
    for (size_t k = 0; k < (size_t) dop::kFineBands; ++k)
    {
        hz[k] = pFineHz[k]->load();
        db[k] = pFineDb[k]->load();
        q[k] = pFineQ[k]->load();
    }
    const bool fineOn = pFineOn->load() > 0.5f;
    fine = dop::fineCurveDb (fineOn, hz, db, q, currentFs);
    for (size_t i = 0; i < total.size(); ++i) total[i] += fine[i];

    eqDeltaDb = info.profile.valid ? dop::eqLoudnessDeltaDb (info.profile.curveDb, total) : 0.0f;
    if (info.profile.valid && info.profile.loudnessLufs > -99.0f)
        takeLufs = info.profile.loudnessLufs - (float) (10.0 * std::log10 ((double) info.profile.loudnessChannels))
                   + eqDeltaDb;
    else
        takeLufs = -100.0f;

    lastFineSnap[0] = fineOn ? 1.0f : 0.0f;
    for (size_t k = 0; k < (size_t) dop::kFineBands; ++k)
    {
        lastFineSnap[1 + 3 * k] = hz[k];
        lastFineSnap[2 + 3 * k] = db[k];
        lastFineSnap[3 + 3 * k] = q[k];
    }
}

void DoppelgangerProcessor::redesign()
{
    const juce::ScopedLock stateGuard (stateLock);
    designDirty = false;
    lastAmount = dop::finiteClamp (pAmount->load(), 0, 100, 60);
    lastLowcut = dop::finiteClamp (pLowcut->load(), 20, 200, 80);
    lastRegister = (int) dop::finiteClamp (pRegister->load(), 0, 3, 0);
    lastMatchLo = pMatchLo->load();
    lastMatchHi = pMatchHi->load();

    dop::BankDesign d = dop::flatBank();
    {
        const juce::ScopedLock sl (stateLock);
        info.effectiveRegister = effectiveRegister();
        info.correction.fill (0.0f);
        info.fitErrorDb = 0.0f;
        if (info.profile.valid && lastAmount > 0.0f)
        {
            info.correction = dop::correctionCurve (info.profile.curveDb, info.effectiveRegister, lastAmount * 0.01f, lastLowcut, lastMatchLo, lastMatchHi);
            d = dop::fitBank (info.correction, currentFs);
            info.fitErrorDb = d.maxErrorDb;
        }
        info.colourBeforeDb = info.colourAfterDb = 0.0f;
        if (info.profile.valid)
        {
            const auto full = dop::correctionCurve (info.profile.curveDb, info.effectiveRegister, 1.0f, lastLowcut, lastMatchLo, lastMatchHi);
            dop::GridCurve rest {};
            for (size_t i = 0; i < rest.size(); ++i) rest[i] = full[i] - info.correction[i];
            info.colourBeforeDb = dop::autopilot::meanAbsDb (full);
            info.colourAfterDb = dop::autopilot::meanAbsDb (rest);
        }
        designedBank = d.coeffs;
        updateTakeLoudness();
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
        learner.beginCapture (kCaptureSeconds);
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
    auto captured = learner.takeCapturedAudio();
    if (p.valid && p.speechSeconds >= kMinLearnSeconds)
    {
        info.profile = p;
        info.message = {};
        calibAudio = std::make_shared<const std::vector<float>> (std::move (captured));
        storedCalibration = {};
        info.calibration = {};
        calibDb = 0.0f;
        lastCalibKey.clear();
        submittedCalibKey.clear();
        calibratedKey.clear();
        learnedWithLowcut = dop::finiteClamp (pLowcut->load(), 20, 200, 80);
        autoApplyPending = pAutopilot->load() > 0.5f;
        deessAnalysisWanted = autoApplyPending;   // danach den De-Esser aus den Zischlauten waehlen
        deessAnalysisRunning = deessApplyPending = false;
        info.deesser = {};
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
    calibAudio.reset();
    storedCalibration = {};
    info.calibration = {};
    calibratedKey.clear();
    calibDb = 0.0f;
    autoApplyPending = false;
    deessAnalysisWanted = deessAnalysisRunning = deessApplyPending = false;
    info.deesser = {};
    stoppedAuto = false;
    designDirty = true;
    redesign();
}

//==============================================================================
void DoppelgangerProcessor::applyAutopilotSettings()
{
    autoApplyPending = false;
    dop::GridCurve curve {};
    float pitch = 0.0f, mlo = 20.0f, mhi = 12000.0f;
    int reg = dop::kMittel;
    {
        const juce::ScopedLock sl (stateLock);
        if (! info.profile.valid || pAutopilot->load() <= 0.5f) return;
        curve = info.profile.curveDb;
        pitch = info.profile.pitchHz;
        reg = effectiveRegister();
        mlo = pMatchLo->load();
        mhi = pMatchHi->load();
    }
    const float lowcut = dop::autopilot::suggestLowcut (pitch, learnedWithLowcut);
    const float amount = dop::autopilot::suggestAmount (curve, reg, lowcut, mlo, mhi);
    auto set = [this] (const char* id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
            prm->endChangeGesture();
        }
    };
    set ("lowcut", lowcut);
    set ("amount", std::round (amount * 100.0f));
}

void DoppelgangerProcessor::applyDeesserChoice()
{
    deessApplyPending = false;
    dop::autopilot::DeesserChoice c;
    {
        const juce::ScopedLock sl (stateLock);
        if (! info.profile.valid || pAutopilot->load() <= 0.5f) return;
        c = info.deesser;
    }
    if (c.decision != dop::autopilot::DeesserChoice::kApplied && c.decision != dop::autopilot::DeesserChoice::kNotNeeded) return;
    auto set = [this] (const char* id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
            prm->endChangeGesture();
        }
    };
    set ("deessOn", 1.0f);
    set ("deess", std::round (c.amount * 100.0f));   // nicht noetig: 0 % = Stimme unbearbeitet
    if (c.decision == dop::autopilot::DeesserChoice::kApplied) set ("deessFreq", c.freqHz);
}

std::vector<float> DoppelgangerProcessor::calibrationKey() const
{
    // alles, was die Lautheit nach der Kette veraendert
    std::vector<float> k { lastAmount, lastLowcut, (float) lastRegister, lastMatchLo, lastMatchHi,
                           pDeess->load(), pDeessOn->load(), pDeessFreq->load(), pLeveler->load(), pTarget->load(),
                           rideOffset.load(), eqDeltaDb };
    k.insert (k.end(), lastFineSnap.begin(), lastFineSnap.end());
    return k;
}

void DoppelgangerProcessor::updateCalibration()
{
    // Ergebnis abholen
    CalibrationWorker::Result fetched;
    if (calibWorker.fetch (fetched) && fetched.id == calibRequestId)
    {
        if (fetched.isDeesser)
        {
            info.deesser = fetched.deesser;
            deessAnalysisRunning = false;
            deessApplyPending = true;   // im naechsten Takt ausserhalb der Sperre setzen, danach misst die Lautheit neu
        }
        else
        {
            const auto& res = fetched.calibration;
            info.calibration = res;
            calibratedKey = res.ok ? submittedCalibKey : std::vector<float> {};   // die Messung gilt genau fuer diese Einstellungen
            if (res.ok) calibDb = res.calibDb;
            info.calibrationPending = false;
        }
    }

    info.autopilot = pAutopilot->load() > 0.5f;
    info.canRecalibrate = calibAudio != nullptr;
    if (! info.profile.valid || ! info.autopilot)
    {
        info.calibrationCurrent = false;
        info.calibrationPending = false;
        deessAnalysisWanted = deessAnalysisRunning = deessApplyPending = false;
        lastCalibKey.clear();
        submittedCalibKey.clear();
        return;
    }
    const auto key = calibrationKey();
    if (key != lastCalibKey)
    {
        lastCalibKey = key;
        calibKeyChangedAt = juce::Time::getMillisecondCounter();
    }
    if (calibAudio == nullptr)
    {
        // Nach dem Laden: gespeicherte Nachmessung gilt nur fuer die Einstellungen, fuer die sie gemessen wurde
        info.calibration = storedCalibration;
        info.calibrationCurrent = storedCalibration.ok && calibrationKeysMatch (key, calibratedKey);
        info.calibrationPending = false;
        return;
    }
    // Nach dem Lernen zuerst den De-Esser waehlen (mit Angleichung und Lowcut des Autopiloten), dann nachmessen
    if (deessAnalysisWanted && ! autoApplyPending)
    {
        deessAnalysisWanted = false;
        deessAnalysisRunning = true;
        CalibrationWorker::Request r;
        r.deesser = true;
        r.audio = calibAudio;
        r.fs = currentFs;
        r.params.lowcutHz = dop::finiteClamp (pLowcut->load(), 20, 200, 80);
        r.params.fineOn = pFineOn->load() > 0.5f;
        for (size_t k = 0; k < (size_t) dop::kFineBands; ++k)
        {
            r.params.fineHz[k] = pFineHz[k]->load();
            r.params.fineDb[k] = pFineDb[k]->load();
            r.params.fineQ[k] = pFineQ[k]->load();
        }
        r.bank = designedBank;
        r.id = deessRequestId = ++calibRequestId;
        calibWorker.submit (std::move (r));
        submittedCalibKey.clear();
    }
    if (deessAnalysisRunning || deessApplyPending || autoApplyPending)
    {
        info.calibrationPending = true;
        info.calibrationCurrent = false;
        return;
    }
    if (key != submittedCalibKey && juce::Time::getMillisecondCounter() - calibKeyChangedAt >= 250)
    {
        submittedCalibKey = key;
        CalibrationWorker::Request r;
        r.audio = calibAudio;
        r.fs = currentFs;
        r.params.lowcutHz = dop::finiteClamp (pLowcut->load(), 20, 200, 80);
        r.learnedLowcutHz = learnedWithLowcut;
        r.params.deessOn = pDeessOn->load() > 0.5f;
        r.params.deess = pDeess->load() * 0.01f;
        r.params.deessFreqHz = pDeessFreq->load();
        r.params.leveler = pLeveler->load() * 0.01f;
        r.params.targetLufs = pTarget->load();
        r.params.rideOffsetDb = rideOffset.load();
        r.params.fineOn = pFineOn->load() > 0.5f;
        for (size_t k = 0; k < (size_t) dop::kFineBands; ++k)
        {
            r.params.fineHz[k] = pFineHz[k]->load();
            r.params.fineDb[k] = pFineDb[k]->load();
            r.params.fineQ[k] = pFineQ[k]->load();
        }
        r.bank = designedBank;
        r.eqDeltaDb = eqDeltaDb;
        r.id = ++calibRequestId;
        info.calibrationPending = true;
        calibWorker.submit (std::move (r));
    }
    info.calibrationCurrent = ! info.calibrationPending && info.calibration.ok && calibrationKeysMatch (key, calibratedKey);
}

bool DoppelgangerProcessor::calibrationKeysMatch (const std::vector<float>& a, const std::vector<float>& b)
{
    // Gespeicherte Werte (Profilkurve mit drei Nachkommastellen) koennen minimal abweichen; 0,01 ist bei allen
    // Eintraegen (Prozent, Hz, LUFS, dB) weit unter jeder hoer- oder messbaren Aenderung.
    if (a.empty() || a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (! (std::abs (a[i] - b[i]) <= 0.01f)) return false;
    return true;
}

void DoppelgangerProcessor::waitForCalibration (int timeoutMs)
{
    const auto start = juce::Time::getMillisecondCounter();
    while (juce::Time::getMillisecondCounter() - start < (juce::uint32) timeoutMs)
    {
        timerCallback();   // wie der Prozessor-Takt: setzt auch ausstehende Autopilot-Werte
        {
            const juce::ScopedLock sl (stateLock);
            if (! info.calibrationPending && ! deessAnalysisRunning && ! deessApplyPending && ! autoApplyPending
                && (info.calibrationCurrent || calibAudio == nullptr || ! info.autopilot)) return;
        }
        juce::Thread::sleep (5);
    }
}

void DoppelgangerProcessor::CalibrationWorker::submit (Request r)
{
    const juce::ScopedLock sl (lock);
    request = std::move (r);
    hasRequest = true;
    cancel = true;   // laufende Rechnung fuer alte Einstellungen abbrechen
    notify();
}

bool DoppelgangerProcessor::CalibrationWorker::fetch (Result& out)
{
    const juce::ScopedLock sl (lock);
    if (! hasResult) return false;
    hasResult = false;
    out = result;
    return true;
}

void DoppelgangerProcessor::CalibrationWorker::run()
{
    while (! threadShouldExit())
    {
        Request r;
        {
            const juce::ScopedLock sl (lock);
            if (hasRequest) { r = std::move (request); request = {}; hasRequest = false; cancel = false; }
        }
        if (r.audio == nullptr) { wait (-1); continue; }
        Result res;
        res.id = r.id;
        res.isDeesser = r.deesser;
        if (r.deesser) res.deesser = dop::autopilot::chooseDeesser (*r.audio, r.fs, r.params, r.bank, &cancel);
        else res.calibration = dop::autopilot::calibrate (*r.audio, r.fs, r.params, r.bank, r.eqDeltaDb, r.learnedLowcutHz, &cancel);
        const juce::ScopedLock sl (lock);
        if (! hasRequest)   // sonst ist das Ergebnis schon veraltet
        {
            result = res;
            hasResult = true;
        }
    }
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
            pt.setProperty ("loudnessChannels", info.profile.loudnessChannels, nullptr);
            pt.setProperty ("typical", info.profile.typicalOffsetDb, nullptr);
            pt.setProperty ("snr", info.profile.noiseSnrDb, nullptr);
            pt.setProperty ("clipped", info.profile.clippedSamples, nullptr);
            if (info.deesser.decision != dop::autopilot::DeesserChoice::kNone)   // nur fuer den Pruefbericht
            {
                const auto& d = info.deesser;
                pt.setProperty ("deessDecision", d.decision, nullptr);
                pt.setProperty ("deessReason", d.reason, nullptr);
                pt.setProperty ("deessEvents", d.events, nullptr);
                pt.setProperty ("deessSeconds", d.sibilantSeconds, nullptr);
                pt.setProperty ("deessFreqChosen", d.freqHz, nullptr);
                pt.setProperty ("deessAmount", d.amount, nullptr);
                pt.setProperty ("deessBefore", d.sharpnessBeforeDb, nullptr);
                pt.setProperty ("deessAfter", d.sharpnessAfterDb, nullptr);
                pt.setProperty ("deessReduction", d.sibilantReductionDb, nullptr);
                pt.setProperty ("deessVowel", d.vowelCostDb, nullptr);
            }
            if (info.calibration.ok)
            {
                pt.setProperty ("calib", info.calibration.calibDb, nullptr);
                pt.setProperty ("calibLufs", info.calibration.outLufs, nullptr);
                pt.setProperty ("calibPeak", info.calibration.peakDb, nullptr);
                pt.setProperty ("calibGuard", info.calibration.guardDb, nullptr);
                pt.setProperty ("calibTruePeak", info.calibration.truePeakDb, nullptr);
                juce::StringArray keyText;   // fuer welche Einstellungen gemessen wurde
                for (float v : calibratedKey) keyText.add (juce::String (v, 6));
                pt.setProperty ("calibKey", keyText.joinIntoString (" "), nullptr);
            }
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
    dop::autopilot::Calibration loadedCalib;
    dop::autopilot::DeesserChoice loadedDeess;
    std::vector<float> loadedKey;
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
            p.loudnessChannels = (int) pt.getProperty ("loudnessChannels", 1);
            p.typicalOffsetDb = (float) pt.getProperty ("typical", -2.5f);
            p.noiseSnrDb = (float) pt.getProperty ("snr", -1.0f);
            p.clippedSamples = (int) pt.getProperty ("clipped", 0);
            p.valid = parsed && dop::isValidProfile (p);
            if (p.valid && pt.hasProperty ("deessDecision"))
            {
                auto num = [&pt] (const char* id) { const float v = (float) pt.getProperty (id, 0.0f); return std::isfinite (v) ? v : 0.0f; };
                loadedDeess.decision = juce::jlimit (0, 3, (int) pt.getProperty ("deessDecision", 0));
                loadedDeess.reason = juce::jlimit (0, 4, (int) pt.getProperty ("deessReason", 0));
                loadedDeess.events = juce::jmax (0, (int) pt.getProperty ("deessEvents", 0));
                loadedDeess.sibilantSeconds = num ("deessSeconds");
                loadedDeess.freqHz = num ("deessFreqChosen");
                loadedDeess.amount = juce::jlimit (0.0f, 1.0f, num ("deessAmount"));
                loadedDeess.sharpnessBeforeDb = num ("deessBefore");
                loadedDeess.sharpnessAfterDb = num ("deessAfter");
                loadedDeess.sibilantReductionDb = num ("deessReduction");
                loadedDeess.vowelCostDb = num ("deessVowel");
            }
            if (p.valid && pt.hasProperty ("calib"))
            {
                loadedCalib.calibDb = (float) pt.getProperty ("calib", 0.0f);
                loadedCalib.outLufs = (float) pt.getProperty ("calibLufs", -100.0f);
                loadedCalib.peakDb = (float) pt.getProperty ("calibPeak", -100.0f);
                loadedCalib.guardDb = juce::jlimit (0.0f, 60.0f, (float) pt.getProperty ("calibGuard", 0.0f));
                loadedCalib.truePeakDb = (float) pt.getProperty ("calibTruePeak", -100.0f);
                if (! std::isfinite (loadedCalib.truePeakDb)) loadedCalib.truePeakDb = -100.0f;
                juce::StringArray keyText;   // fehlt in Projekten aus 1.8/1.9: dann gilt die Messung nie als geprueft
                keyText.addTokens (pt.getProperty ("calibKey").toString(), " ", "");
                keyText.removeEmptyStrings();
                for (auto& t : keyText) loadedKey.push_back (t.getFloatValue());
                loadedCalib.ok = std::isfinite (loadedCalib.calibDb) && std::abs (loadedCalib.calibDb) <= dop::Engine::kMaxCalibDb
                                 && std::isfinite (loadedCalib.outLufs) && std::isfinite (loadedCalib.peakDb);
                if (! loadedCalib.ok) loadedCalib = {};
            }
        }
        state.removeChild (pt, nullptr);
    }
    cancelLearning();
    stoppedAuto = false;
    apvts.replaceState (state);
    {
        const juce::ScopedLock sl (stateLock);
        // Derselbe Zustand noch einmal geladen (Undo, Preset-Vergleich, Host-Rundreise): gelerntes Audio und
        // ausstehende Autopilot-Schritte behalten, sonst gilt das Profil als aus einem Projekt geladen.
        bool sameProfile = p.valid && info.profile.valid && std::abs (p.loudnessLufs - info.profile.loudnessLufs) < 1e-3f
                           && std::abs (p.pitchHz - info.profile.pitchHz) < 1e-3f;
        for (size_t k = 0; sameProfile && k < p.curveDb.size(); ++k)   // gespeichert mit drei Nachkommastellen
            sameProfile = std::abs (p.curveDb[k] - info.profile.curveDb[k]) < 1e-3f;
        info.profile = p;
        info.message = {};
        if (! sameProfile)
        {
            calibAudio.reset();
            autoApplyPending = false;
            deessAnalysisWanted = deessAnalysisRunning = deessApplyPending = false;
            info.deesser = loadedDeess;
        }
        storedCalibration = loadedCalib;
        if (! sameProfile || ! info.calibration.ok)
        {
            info.calibration = loadedCalib;
            calibratedKey = loadedCalib.ok ? loadedKey : std::vector<float> {};
            calibDb = loadedCalib.ok ? loadedCalib.calibDb : 0.0f;
        }
        lastCalibKey.clear();
        submittedCalibKey.clear();
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
