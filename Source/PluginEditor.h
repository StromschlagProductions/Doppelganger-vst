#pragma once
#include "PluginProcessor.h"

class DoppelgangerEditor : public juce::AudioProcessorEditor
{
public:
    explicit DoppelgangerEditor (DoppelgangerProcessor&);
    ~DoppelgangerEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DoppelgangerProcessor& processor;
    juce::Label title;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoppelgangerEditor)
};
