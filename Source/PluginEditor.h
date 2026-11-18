#pragma once
#include "PluginProcessor.h"

// Onboard-Hilfe: formatierter Text, scrollbar in einem Viewport (wie bei Locations, Radiophone und Dreamcatcher)
class HelpView : public juce::Component
{
public:
    void setAttributed (const juce::AttributedString& s) { text = s; relayout(); }
    void setWidth (int w) { if (w != laidOutW) { laidOutW = w; relayout(); } }
    void paint (juce::Graphics& g) override { tl.draw (g, getLocalBounds().toFloat().reduced (14.0f, 10.0f)); }
private:
    void relayout()
    {
        const int w = juce::jmax (120, laidOutW);
        tl.createLayout (text, (float) (w - 28));
        setSize (w, (int) std::ceil (tl.getHeight()) + 24);
        repaint();
    }
    juce::AttributedString text;
    juce::TextLayout tl;
    int laidOutW = 0;
};

class HelpPanel : public juce::Viewport
{
public:
    juce::Colour fill, edge;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (fill);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    }
};

// Feinabstimmung in der Anzeige: Baender ziehen (waagerecht Frequenz, senkrecht Verstaerkung, Mausrad Guete)
class FineNodes : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FineNodes (juce::AudioProcessorValueTreeState& s);
    ~FineNodes() override { endDrag(); }
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void refresh (double rate);
    juce::Colour accent, text, dim;

private:
    struct Band { juce::RangedAudioParameter* hz = nullptr; juce::RangedAudioParameter* db = nullptr; juce::RangedAudioParameter* q = nullptr; };
    std::array<Band, dop::kFineBands> bands {};
    juce::RangedAudioParameter* on = nullptr;
    juce::RangedAudioParameter* matchLo = nullptr;   // Angleich-Bereich: linker und rechter Begrenzer
    juce::RangedAudioParameter* matchHi = nullptr;
    int limiter = -1;                                // 0 = links, 1 = rechts, waehrend des Ziehens
    float limiterX = 0;
    int selected = -1;
    bool dragging = false;
    double sampleRate = 48000;
    std::array<float, 18> lastValues {};
    bool snapshotReady = false;
    juce::Point<float> dragOrigin, dragNode;
    void endDrag();

    bool active() const;
    float valueOf (juce::RangedAudioParameter*) const;
    void setValue (juce::RangedAudioParameter*, float v);
    juce::Point<float> nodeAt (int k) const;
    int nearestBand (juce::Point<float> p, float maxDist) const;
    float yOfDb (float db) const;
    float xOfHz (float hz) const;
    float hzOfX (float x) const;
    int nearestLimiter (juce::Point<float> p) const;
    bool inGrip (juce::Point<float> p) const { return p.y > (float) getHeight() - 18.0f; }   // Griffbereich unten: Begrenzer vor Knoten
    juce::RangedAudioParameter* limiterParam (int k) const { return k == 0 ? matchLo : matchHi; }
    void setLimiterHz (int k, float hz);
    float dbOfY (float y) const;
};

// Integrierte Ausgangslautheit in der Kopfzeile der Karte LAUTHEIT; Klick setzt die Messung zurueck
class IntegratedLoudness : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onReset) onReset(); }
    void set (float lufs, float seconds, float target);
    std::function<void()> onReset;
    juce::Colour accent, text, dim;
private:
    float value = -100.0f, secs = 0.0f, goal = -23.0f;
};

class DoppelgangerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DoppelgangerEditor (DoppelgangerProcessor&);
    ~DoppelgangerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    struct Theme { juce::Colour bg, panel, accent, text, dim, line, display, take, out; bool dark; };

    void timerCallback() override;
    void applyTheme();
    void updateLearnButton();
    static Theme darkTheme();
    static Theme lightTheme();

    static constexpr int kBaseW = 680, kBaseH = 566;
    static constexpr float kStartScale = 1.5f;   // beim ersten Oeffnen 150 %

    DoppelgangerProcessor& proc;
    juce::LookAndFeel_V4 look;
    juce::Component content;
    float uiScale = 1.0f;
    Theme theme;
    bool sizeReady = false;
    juce::Image logo;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& title, const juce::String& tip, double resetValue);
    Knob amountKnob, lowcutKnob, deessKnob, deessFreqKnob, levelerKnob, targetKnob;

    struct FineBand
    {
        juce::Label name;
        juce::Slider freq, gain, q;
        std::unique_ptr<APVTS::SliderAttachment> aFreq, aGain, aQ;
    };
    std::array<FineBand, (size_t) dop::kFineBands> fineBands;
    juce::TextButton fineOnButton;
    std::unique_ptr<APVTS::ButtonAttachment> fineOnAtt;
    FineNodes fineNodes { proc.apvts };
    IntegratedLoudness intLoudness;

    juce::TextButton learnButton, forgetButton, deessOnButton, deessListenButton, themeButton, helpButton;
    std::unique_ptr<APVTS::ButtonAttachment> deessOnAtt, deessListenAtt;
    juce::ComboBox registerBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> registerAtt;
    HelpPanel helpScroll;
    HelpView helpText;
    juce::TooltipWindow tooltipWindow { this, 400 };

    // Live-Spektrum (geglaettet fuer die Anzeige)
    dop::GridCurve liveOut {};
    float liveAlpha = 0.0f;
    static constexpr int kDeessHist = 90;           // ca. 3,6 s bei 25 Bildern/s
    std::array<float, (size_t) kDeessHist> deessHist {};
    int deessHistPos = 0;
    float deessCurDb = 0.0f, deessHoldDb = 0.0f;
    bool liveHasData = false;
    std::array<float, 4> shownMeters {};

    DoppelgangerProcessor::DisplayInfo shown;
    juce::String statusText, headline;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoppelgangerEditor)
};
