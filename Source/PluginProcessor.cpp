#include "PluginProcessor.h"
#include "PluginEditor.h"

DoppelgangerProcessor::DoppelgangerProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout DoppelgangerProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "amount", 1 }, "Angleichung",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f));
    return { params.begin(), params.end() };
}

void DoppelgangerProcessor::prepareToPlay (double, int) {}

bool DoppelgangerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void DoppelgangerProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    // Gerüst: Signal bleibt unverändert
}

juce::AudioProcessorEditor* DoppelgangerProcessor::createEditor()
{
    return new DoppelgangerEditor (*this);
}

void DoppelgangerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DoppelgangerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// Pflicht für JUCE: Die Factory-Funktion erzeugt die Instanz des Plugins.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DoppelgangerProcessor();
}
