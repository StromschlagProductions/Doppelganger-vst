#include "PluginEditor.h"

namespace
{
const juce::Colour kBg (0xff1e2226), kPanel (0xff2f353a), kDisplay (0xff16191c), kText (0xffd5dce1),
                   kDim (0xff8a949c), kAccent (0xfff97316), kRef (0xffd5dce1), kTake (0xffa6d2f7), kEq (0xfff3d46c);
const char* kRegNames[] = { "Tief (Mann)", "Mittel (Frau)", "Hoch (Frau)" };
// Quelltext ist UTF-8: so kommen Umlaute und Sonderzeichen richtig an
juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
}

DoppelgangerEditor::DoppelgangerEditor (DoppelgangerProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    learnButton.onClick = [this]
    {
        if (proc.isLearning()) proc.finishLearning();
        else proc.startLearning();
        updateLearnButton();
    };
    forgetButton.setButtonText ("Vergessen");
    forgetButton.onClick = [this] { proc.forgetProfile(); };
    for (auto* b : { &learnButton, &forgetButton })
    {
        b->setColour (juce::TextButton::buttonColourId, kPanel);
        b->setColour (juce::TextButton::textColourOffId, kText);
        addAndMakeVisible (*b);
    }

    registerBox.addItemList (DoppelgangerProcessor::registerNames(), 1);
    registerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "register", registerBox);
    addAndMakeVisible (registerBox);
    registerLabel.setText ("Stimmlage", juce::dontSendNotification);
    registerLabel.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (registerLabel);

    const char* ids[] = { "amount", "lowcut", "deess", "leveler", "target" };
    const char* names[] = { "Angleichung", "Lowcut", "De-Esser", "Leveler", "Ziel-Lautheit" };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        k.slider.setColour (juce::Slider::textBoxTextColourId, kText);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.slider);
        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, kDim);
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
    }

    for (auto* l : { &status, &meters })
    {
        l->setColour (juce::Label::textColourId, kText);
        addAndMakeVisible (*l);
    }
    meters.setColour (juce::Label::textColourId, kDim);

    setSize (720, 470);
    updateLearnButton();
    startTimerHz (15);
}

DoppelgangerEditor::~DoppelgangerEditor()
{
    stopTimer();
}

void DoppelgangerEditor::updateLearnButton()
{
    if (proc.isLearning())
        learnButton.setButtonText ("Fertig  (" + juce::String (proc.learnedSecondsSoFar(), 1) + " s Sprache)");
    else
        learnButton.setButtonText (shown.profile.valid ? "Neu lernen" : "Lernen");
    learnButton.setColour (juce::TextButton::buttonColourId, proc.isLearning() ? kAccent : kPanel);
}

void DoppelgangerEditor::timerCallback()
{
    shown = proc.getDisplayInfo();
    updateLearnButton();

    juce::String s;
    if (proc.isLearning())
        s = u8 ("Lernt … Spur abspielen. Stoppt automatisch nach ")
            + juce::String ((int) DoppelgangerProcessor::kAutoStopSeconds) + " s Sprache.";
    else if (shown.message.isNotEmpty())
        s = shown.message;
    else if (! shown.profile.valid)
        s = u8 ("Noch nichts gelernt: „Lernen“ drücken und die Spur abspielen. Bis dahin arbeiten nur Lowcut, De-Esser, Leveler und Limiter.");
    else
        s = "Gelernt: " + juce::String (juce::roundToInt (shown.profile.speechSeconds)) + u8 (" s Sprache, Tonhöhe ")
            + (shown.profile.pitchHz > 0 ? juce::String (juce::roundToInt (shown.profile.pitchHz)) + " Hz" : juce::String ("nicht erkannt"))
            + u8 ("  →  Referenz ") + kRegNames[shown.effectiveRegister];
    status.setText (s, juce::dontSendNotification);

    auto& e = proc.getEngine();
    meters.setText ("Eingang " + juce::String (e.meterInLufs.load(), 1) + " LUFS   Ausgang " + juce::String (e.meterOutLufs.load(), 1)
                    + " LUFS   Leveler " + juce::String (e.meterLevelerDb.load(), 1) + u8 (" dB   De-Esser −")
                    + juce::String (e.meterDeessDb.load(), 1) + u8 (" dB   Limiter −") + juce::String (e.meterLimiterDb.load(), 1) + " dB",
                    juce::dontSendNotification);
    repaint();
}

void DoppelgangerEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kText);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("DOPPELGANGER", 20, 12, 400, 28, juce::Justification::left);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (u8 ("Audio Drama Voice Matcher  ·  vorläufige Testoberfläche  ·  v" PLUGIN_VERSION_TEXT), 20, 38, 500, 18, juce::Justification::left);

    // Kurvenanzeige: Referenz (hell), gelernte Stimme (blau), Korrektur (gelb, rechte Skala +-12 dB)
    auto r = juce::Rectangle<float> (20.0f, 104.0f, (float) getWidth() - 40.0f, 170.0f);
    g.setColour (kDisplay);
    g.fillRoundedRectangle (r, 6.0f);
    auto xOf = [&] (float hz) { return r.getX() + r.getWidth() * (std::log2 (hz / 40.0f) / std::log2 (20000.0f / 40.0f)); };
    g.setFont (juce::FontOptions (11.0f));
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
    {
        g.setColour (kPanel);
        g.drawVerticalLine ((int) xOf (hz), r.getY(), r.getBottom());
        g.setColour (kDim);
        g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + " kHz" : juce::String ((int) hz) + " Hz",
                    (int) xOf (hz) + 3, (int) r.getBottom() - 14, 50, 12, juce::Justification::left);
    }
    g.setColour (kPanel);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    auto drawCurve = [&] (const float* v, float centreDb, float rangeDb, juce::Colour c, float thick)
    {
        juce::Path path;
        for (int i = 0; i < dop::kRefGridPoints; ++i)
        {
            const float y = r.getCentreY() - (v[i] - centreDb) / rangeDb * (r.getHeight() * 0.5f);
            const juce::Point<float> pt (xOf (dop::kRefGridHz[i]), juce::jlimit (r.getY(), r.getBottom(), y));
            if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
        }
        g.setColour (c);
        g.strokePath (path, juce::PathStrokeType (thick));
    };
    drawCurve (dop::referenceCurve (shown.effectiveRegister), -6.0f, 34.0f, kRef.withAlpha (0.6f), 1.5f);
    if (shown.profile.valid)
    {
        drawCurve (shown.profile.curveDb.data(), -6.0f, 34.0f, kTake, 1.5f);
        drawCurve (shown.correction.data(), 0.0f, 12.0f, kEq, 2.0f);
    }
    g.setColour (kDim);
    g.drawText ("Referenz", (int) r.getX() + 8, (int) r.getY() + 6, 70, 12, juce::Justification::left);
    g.setColour (kTake);
    g.drawText ("Stimme (gelernt)", (int) r.getX() + 78, (int) r.getY() + 6, 110, 12, juce::Justification::left);
    g.setColour (kEq);
    g.drawText (u8 ("Korrektur (±12 dB)"), (int) r.getX() + 190, (int) r.getY() + 6, 140, 12, juce::Justification::left);
}

void DoppelgangerEditor::resized()
{
    learnButton.setBounds (20, 64, 220, 30);
    forgetButton.setBounds (250, 64, 100, 30);
    registerLabel.setBounds (getWidth() - 300, 64, 80, 30);
    registerBox.setBounds (getWidth() - 220, 64, 200, 30);

    const int top = 290, w = (getWidth() - 40) / (int) knobs.size();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        knobs[i].label.setBounds (20 + (int) i * w, top, w, 18);
        knobs[i].slider.setBounds (20 + (int) i * w, top + 18, w, 100);
    }
    status.setBounds (20, top + 124, getWidth() - 40, 22);
    meters.setBounds (20, top + 148, getWidth() - 40, 22);
}
