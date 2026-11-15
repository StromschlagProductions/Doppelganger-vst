#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DoppelgangerDSP.h"

// DOPPELGANGER – Audio Drama Voice Matcher (Stromschlag Audio)
//
// Ablauf: "Lernen" druecken und die Spur abspielen. Das Plugin misst Klangbild und Tonhoehe der Stimme,
// waehlt (bei "Auto") die passende Referenz und gleicht die Klangfarbe mit einer Filterbank an.
// Das Gelernte wird mit dem Projekt gespeichert.
class DoppelgangerProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    DoppelgangerProcessor();
    ~DoppelgangerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override { engine.reset(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Doppelganger"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::StringArray registerNames();
    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // Lernen (nur vom Message-Thread aufrufen)
    void startLearning();
    void finishLearning();
    void cancelLearning();
    void forgetProfile();
    bool isLearning() const { return learning.load(); }
    float learnedSecondsSoFar() const;

    static constexpr float kMinLearnSeconds = 4.0f;    // weniger Sprache reicht nicht fuer ein verlaessliches Klangbild
    static constexpr float kAutoStopSeconds = 30.0f;   // danach beendet das Plugin das Lernen spaetestens selbst
    static constexpr float kConvergeCheckFromSeconds = 10.0f;   // ab hier prueft es jede Sekunde, ob es genug gelernt hat
    bool wasStoppedAutomatically() const { return stoppedAuto.load(); }

    // Fuer die Anzeige (Message-Thread)
    struct DisplayInfo
    {
        dop::LearnedProfile profile;
        dop::GridCurve correction {};
        int effectiveRegister = dop::kMittel;
        float fitErrorDb = 0.0f;
        juce::String message;          // letzte Meldung (z. B. "zu wenig Sprache")
        bool needsRelearnForLoudness = false;   // Profil aus v0.2 ohne Lautheitsmessung
    };
    DisplayInfo getDisplayInfo() const;

    dop::Engine& getEngine() { return engine; }

    // Live-Spektrum von Eingang und Ausgang (nur Message-Thread). Liefert true, wenn Signal anliegt;
    // die Kurven sind in dB (Mittel von 100-8000 Hz = 0) auf dem Referenzgitter.
    bool getLiveSpectrum (dop::GridCurve& inDb, dop::GridCurve& outDb);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void drainFifo();
    void redesign();
    int effectiveRegister() const;

    dop::Engine engine;
    dop::Learner learner;
    juce::CriticalSection learnerLock;          // Learner nur ausserhalb des Audio-Threads

    // Audio-Thread -> Lernen
    juce::AbstractFifo fifo { 1 << 17 };
    std::vector<float> fifoBuf, tapBuf, drainBuf;
    std::atomic<bool> learning { false };

    // Message-Thread -> Audio-Thread: neue Filterbank
    juce::SpinLock bankLock;
    std::array<dop::Biquad, dop::kBands> pendingBank {};
    bool bankPending = false;

    // Gelerntes und Anzeige
    mutable juce::CriticalSection infoLock;
    DisplayInfo info;
    std::atomic<bool> designDirty { true };
    float lastAmount = -1.0f, lastLowcut = -1.0f;
    int lastRegister = -1;
    double currentFs = 48000.0;

    // Live-Anzeige: Audio-Thread schreibt Mono-Ringpuffer, der Editor analysiert sie
    static constexpr int kLiveRing = 16384, kLiveFft = 4096;
    std::vector<float> liveIn, liveOut;
    std::atomic<uint32_t> livePos { 0 };
    dop::FFT liveFft;
    std::vector<std::complex<double>> liveWork;
    std::vector<double> liveWin, livePow;
    struct LiveTap { int k0 = 0; std::vector<float> w; };
    std::array<LiveTap, (size_t) dop::kRefGridPoints> liveTab;
    double liveTabFs = 0.0;
    void analyseLive (const std::vector<float>& ring, uint32_t pos, dop::GridCurve& out);

    dop::ConvergenceTracker convergence;
    float nextConvergeCheck = 0.0f;
    std::atomic<bool> stoppedAuto { false };

    std::atomic<float>* pAmount = nullptr;
    std::atomic<float>* pRegister = nullptr;
    std::atomic<float>* pLowcut = nullptr;
    std::atomic<float>* pDeess = nullptr;
    std::atomic<float>* pLeveler = nullptr;
    std::atomic<float>* pDeessOn = nullptr;
    std::atomic<float>* pDeessFreq = nullptr;
    std::atomic<float>* pDeessListen = nullptr;
    std::atomic<float> takeLufs { -100.0f }, rideOffset { -2.5f };
    std::atomic<float>* pTarget = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoppelgangerProcessor)
};
