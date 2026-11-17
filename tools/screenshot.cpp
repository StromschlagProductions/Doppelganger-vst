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
    void timerCallback() override
    {
        // Feed an original synthetic voice through the real public learning API.
        // Returning between batches lets the processor's own timer drain its queue.
        if (sample < 48000 * 8)
        {
            juce::AudioBuffer<float> audio (2, 512); juce::MidiBuffer midi;
            for (int b = 0; b < 40 && sample < 48000 * 8; ++b)
            {
                for (int i = 0; i < 512; ++i, ++sample)
                {
                    const double t = sample / 48000.0;
                    const float value = std::fmod (t, 3.0) < 2.6
                        ? (float) (0.12 * (0.6 + 0.4 * std::pow (std::sin (2 * 3.14159265358979 * 2.3 * t), 2))
                            * (std::sin (2 * 3.14159265358979 * 125 * t)
                                + 0.4 * std::sin (2 * 3.14159265358979 * 250 * t)
                                + 0.15 * std::sin (2 * 3.14159265358979 * 1500 * t))) : 0.0f;
                    audio.setSample (0, i, value); audio.setSample (1, i, value);
                }
                processor.processBlock (audio, midi);
            }
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
            stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return;
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
            editor->setSize (680, 566); editor->setVisible (true);
            return; // allow attachments/timers to settle on the actual message loop
        }
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto file = output.getChildFile (index == 0 ? "oberflaeche-dunkel.png" : "oberflaeche-hell.png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
        { fail ("Could not write screenshot"); return; }
        std::cout << file.getFullPathName() << std::endl;
        images.add (file.getFileName());
        editor.reset(); ++index;
    }
    void fail (const char* reason)
    {
        std::cerr << reason << std::endl; result = 1; stopTimer();
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }
    DoppelgangerProcessor processor;
    juce::File output;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::Array<juce::var> images;
    int index = 0;
    int sample = 0; bool learned = false;
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialise;
    if (argc != 2) { std::cerr << "Usage: Screenshot <output-directory>" << std::endl; return 1; }
    Capture capture (juce::File (juce::String::fromUTF8 (argv[1])));
    juce::MessageManager::getInstance()->runDispatchLoop();
    return capture.result;
}
