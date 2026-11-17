#include "PluginProcessor.h"
#include "PluginEditor.h"
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
    static float takeLufs (DoppelgangerProcessor& p) { return p.takeLufs.load(); }
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
void fineTuning()
{
    auto learn = []
    {
        auto p = std::make_unique<DoppelgangerProcessor>(); p->prepareToPlay (48000, 128);
        p->startLearning(); feed (*p, 48000, 5); p->finishLearning();
        DoppelgangerProcessorTestAccess::tick (*p);
        return p;
    };
    auto set = [] (DoppelgangerProcessor& p, const char* id, float v)
    { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); DoppelgangerProcessorTestAccess::tick (p); };

    auto p = learn();
    const float base = DoppelgangerProcessorTestAccess::takeLufs (*p);
    CHECK (base > -99.0f);
    set (*p, "fineDb1", 6.0f);                                               // Band 1 (100 Hz): Testsignal hat dort die meiste Energie
    CHECK (DoppelgangerProcessorTestAccess::takeLufs (*p) == base);          // aus: keine Wirkung
    set (*p, "fineOn", 1.0f);
    const float boosted = DoppelgangerProcessorTestAccess::takeLufs (*p);
    CHECK (boosted > base + 3.0f && boosted < base + 6.5f);
    juce::MemoryBlock state; p->getStateInformation (state);
    set (*p, "fineOn", 0.0f);
    CHECK (std::abs (DoppelgangerProcessorTestAccess::takeLufs (*p) - base) < 1e-4f);
    p->setStateInformation (state.getData(), (int) state.getSize()); DoppelgangerProcessorTestAccess::tick (*p);
    CHECK (p->apvts.getRawParameterValue ("fineOn")->load() > 0.5f);
    CHECK (std::abs (p->apvts.getRawParameterValue ("fineDb1")->load() - 6.0f) < 1e-4f);
    CHECK (std::abs (DoppelgangerProcessorTestAccess::takeLufs (*p) - boosted) < 1e-3f);

    // Projekt aus 1.1.1 (ohne Feinabstimmungs-Parameter) in neuer Instanz: Feinabstimmung aus und flach, gleiche Lautheit
    {
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()); CHECK (xml != nullptr);
        auto tree = juce::ValueTree::fromXml (*xml);
        for (int i = tree.getNumChildren(); --i >= 0;)
            if (tree.getChild (i).getProperty ("id").toString().startsWith ("fine")) tree.removeChild (i, nullptr);
        juce::MemoryBlock old; auto oldXml = tree.createXml(); juce::AudioProcessor::copyXmlToBinary (*oldXml, old);
        auto fresh = std::make_unique<DoppelgangerProcessor>(); fresh->prepareToPlay (48000, 128);
        fresh->setStateInformation (old.getData(), (int) old.getSize()); DoppelgangerProcessorTestAccess::tick (*fresh);
        CHECK (fresh->apvts.getRawParameterValue ("fineOn")->load() < 0.5f);
        for (int k = 1; k <= dop::kFineBands; ++k) CHECK (fresh->apvts.getRawParameterValue ("fineDb" + juce::String (k))->load() == 0.0f);
        CHECK (fresh->getDisplayInfo().profile.valid);
        CHECK (std::abs (DoppelgangerProcessorTestAccess::takeLufs (*fresh) - base) < 1e-3f);
        // Also load the old project into the already-used, boosted instance.
        p->setStateInformation (old.getData(), (int) old.getSize());
        CHECK (p->apvts.getRawParameterValue ("fineOn")->load() < 0.5f);
        CHECK (p->apvts.getRawParameterValue ("fineDb1")->load() == 0.0f);
    }

    // Ausgang: mit und ohne Feinabstimmung gleich laut, weil die Grundverstaerkung ausgleicht
    auto plain = learn(), tuned = learn();
    for (auto* q : { plain.get(), tuned.get() }) set (*q, "leveler", 0.0f);
    set (*tuned, "fineDb1", 6.0f); set (*tuned, "fineOn", 1.0f);
    feed (*plain, 48000, 2.0); feed (*tuned, 48000, 2.0);
    const float a = plain->getEngine().meterOutLufs.load(), b = tuned->getEngine().meterOutLufs.load();
    // Ohne Ausgleich laege der Ausgang gut 4 dB hoeher. Das Testsignal besteht aus drei Sinustoenen, der unguenstigste Fall
    // fuer die Schaetzung auf dem Spektralgitter (hier 0,6 dB Rest); auf echter Sprache blieb der Rest bei hoechstens 0,5 dB.
    CHECK (a > -60.0f && std::abs (a - b) < 1.0f);
}
void fineMouseTests()
{
    auto processor = std::make_unique<DoppelgangerProcessor>();
    FineNodes nodes (processor->apvts); nodes.setBounds (0, 0, 636, 108);
    const float x = 636 * std::log2 (1000.0f / 40) / std::log2 (20000.0f / 40);
    auto event = [&] (juce::Point<float> p, juce::ModifierKeys mods)
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p, mods, 1.0f,
            0, 0, 0, 0, &nodes, &nodes, juce::Time::getCurrentTime(), p,
            juce::Time::getCurrentTime(), 1, false);
    };
    const auto point = juce::Point<float> (x, 54);
    juce::MouseWheelDetails wheel {}; wheel.deltaY = 0.2f;
    auto* q = processor->apvts.getRawParameterValue ("fineQ3");
    nodes.mouseWheelMove (event (point, {}), wheel); CHECK (q->load() > 1);
    const float increased = q->load();
    wheel.isReversed = true;
    nodes.mouseWheelMove (event (point, {}), wheel); CHECK (q->load() < increased);
    nodes.mouseWheelMove (event ({ 0, 0 }, {}), wheel); CHECK (q->load() < increased);
    CHECK (processor->apvts.getRawParameterValue ("fineOn")->load() > 0.5f);
    // Clicking off-centre must not jump the node to the cursor.
    const auto press = event (point + juce::Point<float> (5, 4), juce::ModifierKeys::leftButtonModifier);
    nodes.mouseDown (press); nodes.mouseDrag (press); nodes.mouseUp (press);
    CHECK (std::abs (processor->apvts.getRawParameterValue ("fineHz3")->load() - 1000) < 2);
    CHECK (processor->apvts.getRawParameterValue ("fineDb3")->load() == 0);
    auto* db = processor->apvts.getParameter ("fineDb3");
    db->setValueNotifyingHost (db->convertTo0to1 (6));
    nodes.mouseDoubleClick (event ({ x, 30 }, {}));
    CHECK (processor->apvts.getRawParameterValue ("fineDb3")->load() == 0);
}

void matchRange()
{
    auto p = std::make_unique<DoppelgangerProcessor>(); p->prepareToPlay (48000, 128);
    p->startLearning(); feed (*p, 48000, 5); p->finishLearning();
    DoppelgangerProcessorTestAccess::tick (*p);
    auto set = [&] (const char* id, float v) { auto* q = p->apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); DoppelgangerProcessorTestAccess::tick (*p); };
    CHECK (std::abs (p->apvts.getRawParameterValue ("matchHigh")->load() - 12000.0f) < 1.0f);   // Standard: bis 12 kHz
    set ("matchLow", 300.0f); set ("matchHigh", 4000.0f);
    const auto c = p->getDisplayInfo().correction;
    for (int i = 0; i < dop::kRefGridPoints; ++i)
        if (dop::kRefGridHz[i] < 230.0f || dop::kRefGridHz[i] > 5100.0f) CHECK (c[(size_t) i] == 0.0f);
    juce::MemoryBlock state; p->getStateInformation (state);
    auto q = std::make_unique<DoppelgangerProcessor>(); q->prepareToPlay (48000, 128);
    q->setStateInformation (state.getData(), (int) state.getSize()); DoppelgangerProcessorTestAccess::tick (*q);
    CHECK (std::abs (q->apvts.getRawParameterValue ("matchLow")->load() - 300.0f) < 1.0f);
    { float md = 0; const auto c2 = q->getDisplayInfo().correction; for (size_t i = 0; i < c.size(); ++i) md = std::max (md, std::abs (c2[i] - c[i])); CHECK (md < 1e-3f); }
}
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    stateAndLearning(); fineTuning(); matchRange(); fineMouseTests(); rateAndDisplay(); concurrentHostAndAudio();
    std::puts ("Processor state, learning, display, concurrency and allocation checks passed");
}
