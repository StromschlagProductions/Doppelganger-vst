#pragma once
#include "PluginProcessor.h"

// Vorlaeufige, schlichte Oberflaeche zum Testen der Klangfunktion.
// Das endgueltige Design im Stil von LOCATIONS / RADIOPHONE / DREAMCATCHER folgt in einem eigenen Schritt.
class DoppelgangerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DoppelgangerEditor (DoppelgangerProcessor&);
    ~DoppelgangerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateLearnButton();

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    DoppelgangerProcessor& proc;
    juce::TextButton learnButton, forgetButton;
    juce::ComboBox registerBox;
    juce::Label registerLabel, status, meters;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> registerAtt;
    std::array<Knob, 5> knobs;
    DoppelgangerProcessor::DisplayInfo shown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoppelgangerEditor)
};
