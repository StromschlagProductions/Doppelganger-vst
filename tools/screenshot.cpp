// Render real JUCE editor components; no image retouching. Args: output directory.
#include "PluginProcessor.h"
#include <iostream>

class Capture final : private juce::Timer
{
public:
    explicit Capture (juce::File destination) : output (std::move (destination))
    {
        output.createDirectory();
        processor.prepareToPlay (48000.0, 512);
        parameter ("fineOn", 1); parameter ("fineDb2", -2.5f); parameter ("fineDb4", 2);
        processor.startLearning();
        startTimer (100);
    }
    int result = 0;
private:
    void parameter (const char* id, float value)
    {
        auto* p = processor.apvts.getParameter (id);
        if (p != nullptr) p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
    // One block of an original synthetic, speech-like signal: a gliding harmonic voice
    // (about 120 Hz) shaped by three vowel formants, a little breath noise and syllables.
    // Nothing is taken from a recording.
    float voiceSample (double t)
    {
        const double twoPi = 2.0 * juce::MathConstants<double>::pi;
        const double f0 = 120.0 * (1.0 + 0.10 * std::sin (twoPi * 0.45 * t) + 0.04 * std::sin (twoPi * 2.7 * t));
        phase += f0 / 48000.0;
        if (phase > 1000.0) phase -= 1000.0;
        // vowel formants move slowly between "a", "e" and "o"
        const double m = 0.5 + 0.5 * std::sin (twoPi * 0.31 * t);
        const double F[3] = { 550.0 + 150.0 * m, 1200.0 + 700.0 * (1.0 - m), 2600.0 + 200.0 * m };
        const double gains[3] = { 1.0, 0.55, 0.25 };
        double v = 0.0;
        for (int k = 1; k <= 80; ++k)
        {
            const double f = k * f0;
            if (f > 20000.0) break;
            double env = 0.06;
            for (int j = 0; j < 3; ++j)
            {
                const double d = std::log2 (f / F[j]) / 0.22;
                env += gains[j] * std::exp (-0.5 * d * d);
            }
            v += env / std::pow ((double) k, 0.8) * std::sin (twoPi * k * phase);
        }
        const double noise = random.nextFloat() * 2.0 - 1.0;
        const double hiss = noise - lastNoise; lastNoise = noise;   // breath, tilted to the top
        const double cycle = std::fmod (t, 3.0);
        const double syllable = cycle < 2.6 ? 0.55 + 0.45 * std::pow (std::sin (twoPi * 2.3 * t), 2) : 0.0;
        return (float) (syllable * (0.20 * v + 0.035 * hiss));
    }
    void feedBlock()
    {
        juce::AudioBuffer<float> audio (2, 512); juce::MidiBuffer midi;
        for (int i = 0; i < 512; ++i, ++sample)
        {
            const float value = voiceSample (sample / 48000.0);
            audio.setSample (0, i, value); audio.setSample (1, i, value);
        }
        processor.processBlock (audio, midi);
    }
    void timerCallback() override
    {
        // Feed the original synthetic voice through the real public learning API.
        // Returning between batches lets the processor's own timer drain its queue.
        if (sample < 48000 * 8)
        {
            for (int b = 0; b < 40 && sample < 48000 * 8; ++b)
                feedBlock();
            return;
        }
        if (! learned)
        {
            processor.finishLearning(); learned = true;
            if (! processor.getDisplayInfo().profile.valid) { fail ("Learning fixture failed"); return; }
        }
        if (index >= 2)
        {
            auto* manifest = new juce::DynamicObject();
            manifest->setProperty ("version", PLUGIN_VERSION_TEXT);
            manifest->setProperty ("images", images);
            const juce::var data (manifest);
            if (! output.getChildFile ("screenshots.json").replaceWithText (juce::JSON::toString (data)))
            { fail ("Could not write screenshot manifest"); return; }
            stopTimer(); juce::JUCEApplication::quit(); return;
        }
        if (! editor)
        {

            juce::MemoryBlock state;
            processor.getStateInformation (state);
            auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
            if (! xml) { fail ("Invalid processor state"); return; }
            xml->setAttribute ("theme", index);
            xml->setAttribute ("uiW", 680); xml->setAttribute ("uiH", 566);
            juce::AudioProcessor::copyXmlToBinary (*xml, state);
            processor.setStateInformation (state.getData(), (int) state.getSize());
            editor.reset (processor.createEditor());
            editor->setSize (680, 566);
            // The editor only refreshes its display while it is actually showing on screen
            editor->addToDesktop (0); editor->setVisible (true);
            settle = 0;
            return; // allow attachments/timers to settle on the actual message loop
        }
        // Let the editor's own timer pick up button texts, learned curves and the live
        // output spectrum while the synthetic voice keeps playing (about 1.5 s).
        if (settle < 15)
        {
            for (int b = 0; b < 10; ++b) feedBlock();
            ++settle;
            return;
        }
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto file = output.getChildFile (index == 0 ? "oberflaeche-dunkel.png" : "oberflaeche-hell.png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
        { fail ("Could not write screenshot"); return; }
        std::cout << file.getFullPathName() << std::endl;
        images.add (file.getFileName());
        editor->removeFromDesktop(); editor.reset(); ++index;
    }
    void fail (const char* reason)
    {
        std::cerr << reason << std::endl; result = 1; stopTimer();
        juce::JUCEApplication::quit();
    }
    DoppelgangerProcessor processor;
    juce::File output;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::Array<juce::var> images;
    int index = 0;
    int sample = 0; bool learned = false; int settle = 0;
    double phase = 0.0, lastNoise = 0.0;
    juce::Random random { 1983 };
};

// Runs as a regular JUCE application so the editor can be shown on a real (or Xvfb)
// desktop: the editor only refreshes its curves, button texts and live display while showing.
class ScreenshotApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Doppelganger screenshot"; }
    const juce::String getApplicationVersion() override { return PLUGIN_VERSION_TEXT; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise (const juce::String&) override
    {
        const auto args = getCommandLineParameterArray();
        if (args.size() != 1) { std::cerr << "Usage: Screenshot <output-directory>" << std::endl; setApplicationReturnValue (1); quit(); return; }
        capture = std::make_unique<Capture> (juce::File (args[0]));
    }
    void shutdown() override { if (capture) setApplicationReturnValue (capture->result); capture.reset(); }
private:
    std::unique_ptr<Capture> capture;
};

START_JUCE_APPLICATION (ScreenshotApp)
