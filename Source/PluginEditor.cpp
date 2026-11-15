#include "PluginEditor.h"

DoppelgangerEditor::DoppelgangerEditor (DoppelgangerProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    title.setText ("DOPPELGANGER", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    addAndMakeVisible (title);

    setSize (480, 240);
    setResizable (false, false);
}

void DoppelgangerEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2226));
}

void DoppelgangerEditor::resized()
{
    title.setBounds (getLocalBounds().reduced (20).removeFromTop (60));
}
