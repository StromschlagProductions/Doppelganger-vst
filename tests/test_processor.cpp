#include "PluginProcessor.h"
#include "TestSupport.h"
#include <thread>
#include <memory>
#include <limits>
#include <new>

// Count C++ allocations only inside the current thread's audio callback.
thread_local bool audioAllocationGuard = false;
thread_local size_t audioAllocations = 0;
void* operator new (size_t size) { if (audioAllocationGuard) ++audioAllocations; if (auto* p = std::malloc (size ? size : 1)) return p; throw std::bad_alloc(); }
void* operator new[] (size_t size) { return ::operator new (size); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, size_t) noexcept { std::free (p); }
void operator delete[] (void* p, size_t) noexcept { std::free (p); }

struct DoppelgangerProcessorTestAccess
{
    static void tick (DoppelgangerProcessor& p) { p.timerCallback(); }
    static int interval (DoppelgangerProcessor& p) { return p.getTimerInterval(); }
};
void process (DoppelgangerProcessor& p, juce::AudioBuffer<float>& b)
{
    juce::MidiBuffer midi;
    audioAllocationGuard = true;
    p.processBlock (b, midi);
    audioAllocationGuard = false;
    CHECK (audioAllocations == 0);
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) CHECK (std::isfinite (b.getSample (c, i)));
}
void feed (DoppelgangerProcessor& p, double fs, double seconds, bool tick = true)
{
    auto x = voice (fs, seconds);
    juce::AudioBuffer<float> b (2, 512);
    for (size_t off = 0; off < x.size(); off += 512)
    {
        const int n = (int) std::min ((size_t) 512, x.size() - off);
        b.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c) std::copy_n (x.data() + off, n, b.getWritePointer (c));
        process (p, b);
        if (tick) DoppelgangerProcessorTestAccess::tick (p);
    }
}
void stateAndLearning()
{
    auto p = std::make_unique<DoppelgangerProcessor>(); p->prepareToPlay (48000, 128);
    p->startLearning(); feed (*p, 48000, 5); p->finishLearning();
    auto learned = p->getDisplayInfo(); CHECK (learned.profile.valid && learned.profile.speechSeconds >= 4);
    juce::MemoryBlock state; p->getStateInformation (state);
    p->startLearning(); feed (*p, 48000, 0.05); p->finishLearning(); CHECK (p->getDisplayInfo().profile.curveDb == learned.profile.curveDb);
    p->forgetProfile(); CHECK (! p->getDisplayInfo().profile.valid);
    p->setStateInformation (state.getData(), (int) state.getSize());
    CHECK (p->getDisplayInfo().profile.valid);
    CHECK (dop::curveDistanceDb (p->getDisplayInfo().profile.curveDb, learned.profile.curveDb) < 0.001);
    p->startLearning(); feed (*p, 48000, 16); CHECK (! p->isLearning()); CHECK (p->wasStoppedAutomatically());
    CHECK (p->getDisplayInfo().profile.valid);
    // Overflow must be reported instead of silently saving a biased profile.
    p->startLearning(); feed (*p, 48000, 4, false); DoppelgangerProcessorTestAccess::tick (*p);
    CHECK (! p->isLearning()); CHECK (p->getDisplayInfo().message.isNotEmpty()); CHECK (p->getDisplayInfo().profile.valid);
    // Queued old-generation samples cannot count towards a new take.
    p->startLearning(); feed (*p, 48000, 0.2, false); p->cancelLearning(); p->startLearning();
    DoppelgangerProcessorTestAccess::tick (*p); CHECK (p->learnedSecondsSoFar() == 0); p->cancelLearning();
    p->startLearning(); p->setStateInformation (state.getData(), (int) state.getSize()); CHECK (! p->isLearning());
    // Legacy loudness fields remain optional; malformed curves must not reach the EQ.
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()); CHECK (xml != nullptr);
    auto tree = juce::ValueTree::fromXml (*xml); auto profile = tree.getChildWithName ("PROFILE");
    profile.removeProperty ("lufs", nullptr); profile.removeProperty ("typical", nullptr);
    auto writeTree = [&] { auto x = tree.createXml(); juce::AudioProcessor::copyXmlToBinary (*x, state); p->setStateInformation (state.getData(), (int) state.getSize()); };
    writeTree(); CHECK (p->getDisplayInfo().needsRelearnForLoudness);
    juce::StringArray values; for (int i = 0; i < dop::kRefGridPoints; ++i) values.add (i == 7 ? "nan" : "0");
    profile.setProperty ("curve", values.joinIntoString (" "), nullptr); writeTree(); CHECK (! p->getDisplayInfo().profile.valid);
    p->setStateInformation ("bad", 3); CHECK (! p->getDisplayInfo().profile.valid);
    p->forgetProfile(); DoppelgangerProcessorTestAccess::tick (*p); CHECK (DoppelgangerProcessorTestAccess::interval (*p) == 200);
    p->startLearning(); CHECK (DoppelgangerProcessorTestAccess::interval (*p) <= 34); p->cancelLearning();
}
void rateAndDisplay()
{
    auto p = std::make_unique<DoppelgangerProcessor>();
    dop::GridCurve in {}, out {};
    for (double fs : { 8000., 22050., 44100., 48000., 96000., 192000. })
    {
        p->prepareToPlay (fs, 31); p->setLiveSpectrumEnabled (true); CHECK (! p->getLiveSpectrum (in, out));
        for (int n : { 0, 1, 7, 31, 512, 8193, 32769 })
        {
            juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) b.setSample (c, i, (float) (0.1 * std::sin (i * 0.1)));
            process (*p, b);
        }
        CHECK (p->getLiveSpectrum (in, out)); CHECK (! p->getLiveSpectrum (in, out));
        p->setLiveSpectrumEnabled (false); feed (*p, fs, 0.2); CHECK (! p->getLiveSpectrum (in, out));
        p->setLiveSpectrumEnabled (true); CHECK (! p->getLiveSpectrum (in, out));
        feed (*p, fs, 0.6); CHECK (p->getLiveSpectrum (in, out));
        for (int j = 0; j < 3; ++j)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditor());
            editor->setVisible (true); editor->setVisible (false);
        }
        p->setLiveSpectrumEnabled (false);
    }
    auto layout = p->getBusesLayout(); layout.inputBuses.set (0, juce::AudioChannelSet::mono()); layout.outputBuses.set (0, juce::AudioChannelSet::mono());
    CHECK (p->setBusesLayout (layout)); p->prepareToPlay (48000, 512);
    juce::AudioBuffer<float> mono (1, 8193); mono.clear(); process (*p, mono);
}
void concurrentHostAndAudio()
{
    auto p = std::make_unique<DoppelgangerProcessor>(); auto other = std::make_unique<DoppelgangerProcessor>();
    p->prepareToPlay (48000, 512); other->prepareToPlay (22050, 31);
    juce::MemoryBlock state; p->getStateInformation (state);
    std::atomic<bool> stop { false };
    std::thread audio ([&] { juce::AudioBuffer<float> b (2, 512); while (! stop.load()) { for (int c=0;c<2;++c) for (int i=0;i<512;++i) b.setSample(c,i,(float)(0.1*std::sin(i*.1))); process (*p, b); } });
    std::thread secondAudio ([&]
    {
        juce::AudioBuffer<float> b (2, 31);
        while (! stop.load()) { b.clear(); process (*other, b); }
    });
    std::thread host ([&] { for (int i = 0; i < 80; ++i) { juce::MemoryBlock copy; p->getStateInformation(copy); p->setStateInformation(state.getData(), (int)state.getSize()); } });
    dop::GridCurve in {}, out {};
    for (int i = 0; i < 100; ++i)
    {
        p->setLiveSpectrumEnabled (i % 3 != 0); p->getLiveSpectrum (in, out);
        p->startLearning(); DoppelgangerProcessorTestAccess::tick (*p); p->cancelLearning();
        p->setUiProperty ("theme", i % 2);
        CHECK (! other->isLearning() && ! other->getDisplayInfo().profile.valid);
    }
    host.join(); stop = true; audio.join(); secondAudio.join();
}
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    stateAndLearning(); rateAndDisplay(); concurrentHostAndAudio();
    std::puts ("Processor state, learning, display, concurrency and allocation checks passed");
}
