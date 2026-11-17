#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
// Quelltext ist UTF-8: so kommen Umlaute und Sonderzeichen richtig an
juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
const char* kThemeProp = "theme";   // 0 = dunkel, 1 = hell (wird mit dem Projekt gespeichert)
const char* kUiWProp = "uiW";
const char* kUiHProp = "uiH";
const char* kRegNames[] = { "Tief (Mann)", "Mittel (Frau)", "Hoch (Frau)" };

juce::String fmtDb (float v)
{
    return (v >= 0.0f ? juce::String ("+") : u8 ("−")) + juce::String (std::abs (v), 1) + " dB";
}
}

DoppelgangerEditor::Theme DoppelgangerEditor::darkTheme()
{
    return { juce::Colour (0xff182326), juce::Colour (0xff233539), juce::Colour (0xfff5781a),
             juce::Colour (0xffe8ecec), juce::Colour (0xff8fa1a5), juce::Colour (0xff35494e),
             juce::Colour (0xff121b1d), juce::Colour (0xff8fc8f0), true };
}
DoppelgangerEditor::Theme DoppelgangerEditor::lightTheme()
{
    return { juce::Colour (0xffd5dbda), juce::Colour (0xffe8eae9), juce::Colour (0xfff5781a),
             juce::Colour (0xff233539), juce::Colour (0xff5d6e72), juce::Colour (0xffb5bfbe),
             juce::Colour (0xfff4f6f5), juce::Colour (0xff2c6e9e), false };
}

static juce::AttributedString buildHelp (juce::Colour text, juce::Colour accent, juce::Colour dim)
{
    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.setJustification (juce::Justification::topLeft);

    auto head = [&] (const char* t) { s.append (u8 (t) + "\n", juce::Font (juce::FontOptions (14.0f, juce::Font::bold)), accent); };
    auto body = [&] (const char* t) { s.append (u8 (t) + "\n\n", juce::Font (juce::FontOptions (13.0f)), text); };
    auto note = [&] (const char* t) { s.append (u8 (t) + "\n\n", juce::Font (juce::FontOptions (12.0f)), dim); };

    s.append (u8 ("DOPPELGANGER – Hilfe") + u8 (" · v") + juce::String (PLUGIN_VERSION_TEXT) + "\n\n",
              juce::Font (juce::FontOptions (17.0f, juce::Font::bold)), accent);

    body ("Doppelganger gleicht die Klangfarbe einer Sprecherspur an eine feste Referenzstimme an und bringt sie auf die "
          "Ziel-Lautheit. Mikrofon und Raum der Aufnahme werden so an den Standard des Projekts angepasst. Lowcut, Match-EQ, "
          "De-Esser und Leveler stecken in einem Plugin.");

    head ("STIMME LERNEN");
    body ("„Learn“ drücken und die Spur abspielen. Das Plugin misst dabei Klangbild, Tonhöhe und Lautheit der Stimme. Es prüft selbst, ob das Klangbild "
          "sich nicht mehr verändert, und hört dann von allein auf (frühestens nach 10 Sekunden Sprache, spätestens nach 30). Mit „Done“ geht es früher (mindestens 4 Sekunden Sprache). Pausen zählen nicht mit. "
          "Das Gelernte wird mit dem Projekt gespeichert. „Clear“ löscht es wieder.");
    body ("Am besten eine Stelle lernen, an der die Figur normal spricht, nicht nur schreit oder flüstert.");

    head ("STIMMLAGE");
    body ("Auto wählt die Referenz nach der gelernten Tonhöhe: unter 160 Hz Tief (Mann), bis 225 Hz Mittel (Frau), darüber Hoch (Frau). "
          "Liegt Auto einmal daneben, die Stimmlage einfach fest einstellen.");

    head ("ANGLEICHUNG und LOWCUT");
    body ("Angleichung: wie stark die Klangfarbe an die Referenz angeglichen wird. 100 % = ganz, 60 % (Standard) = Mikrofon und Raum werden "
          "angeglichen, die eigene Farbe der Stimme bleibt. Angeglichen wird nur die Klangform, nie der Pegel. Die Korrektur ist auf ±9 dB begrenzt. "
          "Die Referenzen enden bei 16 kHz, darüber bleibt der Take unverändert.");
    body ("Lowcut: 24 dB/Oktave, ganz links aus. Standard 80 Hz.");
    note ("Im Bild: grau die Referenz, blau die gelernte Stimme, orange die Korrektur (±12 dB über die Bildhöhe), hell mit Punkten 1 bis 5 die Feinabstimmung. Dazu läuft live das Spektrum: die helle Fläche ist der Ausgang, die dünne Linie der Eingang. Beide sind auf gleiche mittlere Höhe gebracht, man sieht also die Klangform. Je näher die Fläche an der grauen Referenz liegt, desto besser passt die Spur.");

    head ("DE-ESSER");
    body ("AN / AUS schaltet den De-Esser gezielt ab, zum Vergleich. Stärke: wie weit Zischlaute abgesenkt werden, bei 25 % etwa 3 dB, "
          "bei 100 % etwa 10 dB. Vokale bleiben unberührt. Frequenz: ab hier sucht und bearbeitet der De-Esser die S-Laute (Standard 5,5 kHz). "
          "Tiefer bei dumpfen oder männlichen Stimmen, höher bei hellen.");
    body ("Rechts daneben zeigt die Anzeige, was der De-Esser gerade tut: der Balken hängt von oben herunter, so tief wie die aktuelle Absenkung (0 bis 12 dB), "
          "der Strich markiert die letzte Spitze. Daneben läuft ein Verlauf der letzten Sekunden: jeder Ausschlag ist ein erkannter S-Laut, der gedämpft wurde. "
          "Bleibt die Anzeige leer, erkennt er keine S-Laute, dann ist die Stärke zu niedrig oder die Frequenz passt nicht.");
    body ("Listen: spielt nur den Bereich ab, auf den der De-Esser hört. So findest du schnell die richtige Frequenz. Danach wieder ausschalten.");
    note ("Klingen die S-Laute eines Takes ohnehin schwach oder lispelnd, den De-Esser ausschalten.");

    head ("FEINABSTIMMUNG");
    body ("Optional: fünf frei einstellbare Glockenfilter nach dem Angleich. Damit lässt sich die Spur direkt im Plugin nachschärfen, ohne zusätzlichen EQ in der Slotkette. "
          "Standard: alle Bänder 0 dB, Feinabstimmung aus, der Klang bleibt dann unverändert.");
    body ("In der Anzeige ziehen: waagerecht die Frequenz, senkrecht die Verstärkung (±12 dB). Das Mausrad ändert die Güte des Bands, Doppelklick setzt es auf 0 dB. "
          "Ziehen schaltet die Feinabstimmung automatisch ein. Die Regler darunter stellen dieselben Werte als Zahl ein.");
    body ("Die Grundverstärkung rechnet die Feinabstimmung mit ein: Die Spur landet weiterhin auf der Ziel-Lautheit, der EQ ändert nur die Klangfarbe.");

    head ("LAUTHEIT");
    body ("Die Grundverstärkung kommt aus der gelernten Lautheit und bringt die Spur fest auf die Ziel-Lautheit (Standard −23 LUFS). "
          "Sie steht ab dem ersten Sample, egal wo die Wiedergabe startet.");
    body ("Leveler: fährt darüber hinaus sanft wie ein Fader und gleicht leise und laute Passagen an. Der Regler bestimmt den Bereich: "
          "0 % = aus, 100 % = bis zu ±10 dB. In Pausen und bei Atmern hält er still. Die Gesamtlautheit ändert er nicht.");

    head ("Allgemein");
    body ("Doppelganger ist latenzfrei und hat bewusst keinen Limiter. Spitzen fängt der Limiter auf dem Master ab.");
    body ("Look: Dunkel / Hell. Das Fenster ist am rechten unteren Rand skalierbar (50–300 %). Beides wird mit dem Projekt gespeichert.");

    head ("Idee und Konzept");
    body ("Lars Eickstädt, Stromschlag Audio");
    note ("Gebaut mit JUCE. Lautheitsmessung nach ITU-R BS.1770 / EBU R128.");
    return s;
}

//==============================================================================
//  Feinabstimmung in der Anzeige. Koordinaten wie die Anzeige: 40 Hz bis 20 kHz logarithmisch, ±12 dB.
//==============================================================================
namespace
{
constexpr float kFineMinHz = 40.0f, kFineMaxHz = 20000.0f, kFineRangeDb = 12.0f;
}

FineNodes::FineNodes (juce::AudioProcessorValueTreeState& s)
{
    for (int k = 0; k < dop::kFineBands; ++k)
    {
        const auto n = juce::String (k + 1);
        auto& b = bands[(size_t) k];
        b.hz = s.getParameter ("fineHz" + n);
        b.db = s.getParameter ("fineDb" + n);
        b.q = s.getParameter ("fineQ" + n);
    }
    on = s.getParameter ("fineOn");
}

bool FineNodes::active() const { return on != nullptr && on->getValue() > 0.5f; }

float FineNodes::valueOf (juce::RangedAudioParameter* p) const { return p->convertFrom0to1 (p->getValue()); }

void FineNodes::setValue (juce::RangedAudioParameter* p, float v)
{
    p->setValueNotifyingHost (p->convertTo0to1 (v));
}

juce::Point<float> FineNodes::nodeAt (int k) const
{
    const auto& b = bands[(size_t) k];
    const float hz = juce::jlimit (kFineMinHz, kFineMaxHz, valueOf (b.hz));
    const float x = getWidth() * std::log2 (hz / kFineMinHz) / std::log2 (kFineMaxHz / kFineMinHz);
    return { x, yOfDb (valueOf (b.db)) };
}

float FineNodes::yOfDb (float db) const
{
    const float ppd = (getHeight() * 0.5f - 6.0f) / kFineRangeDb;
    return getHeight() * 0.5f - db * ppd;
}

float FineNodes::dbOfY (float y) const
{
    const float ppd = (getHeight() * 0.5f - 6.0f) / kFineRangeDb;
    return juce::jlimit (-kFineRangeDb, kFineRangeDb, std::round ((getHeight() * 0.5f - y) / ppd * 10.0f) / 10.0f);
}

int FineNodes::nearestBand (juce::Point<float> p, float maxDist) const
{
    int best = -1;
    float bestD = maxDist;
    for (int k = 0; k < dop::kFineBands; ++k)
    {
        const float d = nodeAt (k).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = k; }
    }
    return best;
}

void FineNodes::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const bool on_ = active();

    // Mittellinie = 0 dB
    g.setColour (dim.withAlpha (0.35f));
    g.drawHorizontalLine ((int) (h * 0.5f), 0.0f, w);

    // Frequenzgang der Feinabstimmung als Summe der Baender
    std::array<dop::Biquad, dop::kFineBands> coefs {};
    for (int k = 0; k < dop::kFineBands; ++k)
    {
        const auto& b = bands[(size_t) k];
        coefs[(size_t) k] = dop::peaking (48000.0, valueOf (b.hz), valueOf (b.q), valueOf (b.db));
    }
    juce::Path curve;
    for (int i = 0; i <= 120; ++i)
    {
        const float f = kFineMinHz * std::pow (kFineMaxHz / kFineMinHz, (float) i / 120.0f);
        double sum = 0.0;
        if (on_)
            for (const auto& c : coefs) sum += dop::magnitudeDb (c, 48000.0, f);
        const float x = w * (float) i / 120.0f;
        const float y = yOfDb ((float) sum);
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    g.setColour (on_ ? text.withAlpha (0.9f) : dim.withAlpha (0.4f));   // Textfarbe: klar getrennt von der orangen Korrektur
    g.strokePath (curve, juce::PathStrokeType (1.6f));

    // Knoten, die man ziehen kann
    for (int k = 0; k < dop::kFineBands; ++k)
    {
        const auto p = nodeAt (k);
        const bool sel = k == selected;
        const float r = sel ? 7.0f : 5.5f;
        g.setColour (on_ ? text : dim);
        g.fillEllipse (p.x - r, p.y - r, 2 * r, 2 * r);
        if (on_) { g.setColour (accent); g.drawEllipse (p.x - r, p.y - r, 2 * r, 2 * r, 1.6f); }
        g.setColour (text.withAlpha (on_ ? 0.95f : 0.5f));
        g.drawFittedText (juce::String (k + 1), (int) p.x - 8, (int) p.y - 22, 16, 12, juce::Justification::centred, 1);
    }
}

void FineNodes::mouseDown (const juce::MouseEvent& e)
{
    selected = nearestBand (e.position, 18.0f);
    if (selected < 0) return;
    const auto& b = bands[(size_t) selected];
    b.hz->beginChangeGesture();
    b.db->beginChangeGesture();
    b.q->beginChangeGesture();
    on->beginChangeGesture();
    dragging = true;
}

void FineNodes::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || selected < 0) return;
    const auto& b = bands[(size_t) selected];
    const float ratio = juce::jlimit (0.0f, 1.0f, e.position.x / (float) getWidth());
    const float hz = kFineMinHz * std::pow (kFineMaxHz / kFineMinHz, ratio);
    setValue (b.hz, juce::jlimit (kFineMinHz, kFineMaxHz, hz));
    setValue (b.db, dbOfY (e.position.y));
    if (! active()) setValue (on, 1.0f);   // Ziehen schaltet die Feinabstimmung ein
    repaint();
}

void FineNodes::mouseUp (const juce::MouseEvent&)
{
    if (dragging && selected >= 0)
    {
        const auto& b = bands[(size_t) selected];
        b.hz->endChangeGesture();
        b.db->endChangeGesture();
        b.q->endChangeGesture();
        on->endChangeGesture();
    }
    dragging = false;
    selected = -1;
    repaint();
}

void FineNodes::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int k = nearestBand (e.position, 18.0f);
    if (k < 0) return;
    const auto& b = bands[(size_t) k];
    b.db->beginChangeGesture();
    setValue (b.db, 0.0f);
    b.db->endChangeGesture();
    repaint();
}

void FineNodes::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int k = nearestBand (e.position, 18.0f);
    if (k < 0 || wheel.deltaY == 0.0f) return;
    const auto& b = bands[(size_t) k];
    const float q = juce::jlimit (0.3f, 8.0f, valueOf (b.q) * std::pow (2.0f, wheel.deltaY * 2.0f));
    b.q->beginChangeGesture();
    setValue (b.q, std::round (q * 100.0f) / 100.0f);
    b.q->endChangeGesture();
    if (! active()) { on->beginChangeGesture(); setValue (on, 1.0f); on->endChangeGesture(); }
    repaint();
}

DoppelgangerEditor::DoppelgangerEditor (DoppelgangerProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (kBaseW / 2, kBaseH / 2, kBaseW * 3, kBaseH * 3);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
    theme = (int) proc.getUiProperty (kThemeProp, 0) == 1 ? lightTheme() : darkTheme();
    setLookAndFeel (&look);

    learnButton.onClick = [this]
    {
        if (proc.isLearning()) proc.finishLearning();
        else proc.startLearning();
        timerCallback();
    };
    learnButton.setTooltip (u8 ("Learn drücken, dann die Spur abspielen. Stoppt von selbst, sobald es genug gelernt hat, oder mit „Done“."));
    forgetButton.setButtonText ("CLEAR");
    forgetButton.setTooltip (u8 ("Gelerntes löschen. Danach arbeiten nur Lowcut, De-Esser und Leveler."));
    forgetButton.onClick = [this] { proc.forgetProfile(); timerCallback(); };
    content.addAndMakeVisible (learnButton);
    content.addAndMakeVisible (forgetButton);

    registerBox.addItemList (DoppelgangerProcessor::registerNames(), 1);
    registerBox.setTooltip (u8 ("Welche Referenzstimme? Auto wählt nach der gelernten Tonhöhe."));
    registerAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "register", registerBox);
    content.addAndMakeVisible (registerBox);

    setupKnob (amountKnob, "amount", u8 ("ANGLEICHUNG"),
               u8 ("Wie stark die Klangfarbe an die Referenz angeglichen wird. 60 % = Mikrofon und Raum, die Stimmfarbe bleibt. Doppelklick: 60 %"), 60.0);
    setupKnob (lowcutKnob, "lowcut", u8 ("LOWCUT"), u8 ("Hochpass 24 dB/Oktave, ganz links aus. Doppelklick: 80 Hz"), 80.0);
    setupKnob (deessKnob, "deess", u8 ("STÄRKE"), u8 ("Wie weit Zischlaute abgesenkt werden: 25 % ≈ 3 dB, 100 % ≈ 10 dB. Doppelklick: 25 %"), 25.0);
    setupKnob (deessFreqKnob, "deessFreq", u8 ("FREQUENZ"),
               u8 ("Ab hier sucht und bearbeitet der De-Esser die S-Laute. Mit „Listen“ abhören. Doppelklick: 5,5 kHz"), 5500.0);
    setupKnob (levelerKnob, "leveler", u8 ("LEVELER"),
               u8 ("Sanfter Ausgleich leiser und lauter Passagen: 0 % = aus, 100 % = bis ±10 dB. Doppelklick: 50 %"), 50.0);
    setupKnob (targetKnob, "target", u8 ("ZIEL"), u8 ("Ziel-Lautheit der Spur. Doppelklick: −23 LUFS"), -23.0);

    deessOnButton.setClickingTogglesState (true);
    deessOnButton.setTooltip (u8 ("De-Esser an- und ausschalten, zum Vergleich"));
    deessOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "deessOn", deessOnButton);
    deessListenButton.setButtonText ("LISTEN");
    deessListenButton.setClickingTogglesState (true);
    deessListenButton.setTooltip (u8 ("Nur den Bereich hören, auf den der De-Esser reagiert. Zum Einstellen der Frequenz, danach wieder aus."));
    deessListenAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "deessListen", deessListenButton);
    content.addAndMakeVisible (deessOnButton);
    content.addAndMakeVisible (deessListenButton);

    fineOnButton.setClickingTogglesState (true);
    fineOnButton.setTooltip (u8 ("Feinabstimmung an- und ausschalten. Aus: der Klang bleibt wie nach dem Angleich."));
    fineOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "fineOn", fineOnButton);
    content.addAndMakeVisible (fineOnButton);
    content.addAndMakeVisible (fineNodes);
    auto fine = [this] (juce::Slider& s, std::unique_ptr<APVTS::SliderAttachment>& att, const juce::String& id,
                        const juce::String& tip, double reset)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 18);   // Text kommt vom Parameter (Hz/kHz, dB)
        s.setTooltip (tip);
        s.setDoubleClickReturnValue (true, reset);
        content.addAndMakeVisible (s);
        att = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, s);
    };
    const double fineDefaultHz[dop::kFineBands] { 100.0, 300.0, 1000.0, 3000.0, 8000.0 };
    for (int k = 0; k < dop::kFineBands; ++k)
    {
        auto& b = fineBands[(size_t) k];
        const auto n = juce::String (k + 1);
        b.name.setText ("BAND " + n, juce::dontSendNotification);
        b.name.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        content.addAndMakeVisible (b.name);
        fine (b.freq, b.aFreq, "fineHz" + n, u8 ("Mittenfrequenz des Bands, 40 Hz bis 20 kHz. Doppelklick setzt zurück."), fineDefaultHz[k]);
        fine (b.gain, b.aGain, "fineDb" + n, u8 ("Anhebung oder Absenkung des Bands, ±12 dB. Doppelklick: 0 dB"), 0.0);
        fine (b.q, b.aQ, "fineQ" + n, u8 ("Güte: klein = breit, groß = schmal. Doppelklick: 1,00"), 1.0);
    }

    themeButton.onClick = [this]
    {
        theme = theme.dark ? lightTheme() : darkTheme();
        proc.setUiProperty (kThemeProp, theme.dark ? 0 : 1);
        applyTheme();
    };
    content.addAndMakeVisible (themeButton);

    helpButton.setButtonText ("?");
    helpButton.setTooltip (u8 ("Hilfe: was machen die Regler?"));
    helpButton.onClick = [this] { helpScroll.setVisible (! helpScroll.isVisible()); };
    content.addAndMakeVisible (helpButton);

    helpScroll.setViewedComponent (&helpText, false);
    helpScroll.setScrollBarsShown (true, false);
    helpScroll.setVisible (false);
    content.addChildComponent (helpScroll);

    applyTheme();

    const int startW = juce::roundToInt (kBaseW * kStartScale), startH = juce::roundToInt (kBaseH * kStartScale);
    // Hoehe immer aus der Breite: Projekte vor 1.2 haben die Groesse im alten Seitenverhaeltnis (680 x 450) gespeichert
    juce::ignoreUnused (startH);
    const int savedW = juce::jlimit (kBaseW / 2, kBaseW * 3, (int) proc.getUiProperty (kUiWProp, startW));
    setSize (savedW, juce::roundToInt (savedW * (double) kBaseH / (double) kBaseW));
    sizeReady = true;
    timerCallback();
    startTimerHz (25);
}

DoppelgangerEditor::~DoppelgangerEditor()
{
    stopTimer();
    proc.setLiveSpectrumEnabled (false);
    setLookAndFeel (nullptr);
}

void DoppelgangerEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& title, const juce::String& tip, double resetValue)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
    k.slider.setTooltip (tip);
    content.addAndMakeVisible (k.slider);
    k.label.setText (title, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    content.addAndMakeVisible (k.label);
    k.attachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramId, k.slider);
    k.slider.setDoubleClickReturnValue (true, resetValue);
}

void DoppelgangerEditor::updateLearnButton()
{
    const bool learning = proc.isLearning();
    learnButton.setButtonText (learning ? u8 ("DONE  ·  ") + juce::String (proc.learnedSecondsSoFar(), 1) + " s"
                                        : u8 ("LEARN"));
    learnButton.setToggleState (learning, juce::dontSendNotification);
    forgetButton.setEnabled (shown.profile.valid || learning);
    deessOnButton.setButtonText (deessOnButton.getToggleState() ? "AN" : "AUS");
    fineOnButton.setButtonText (fineOnButton.getToggleState() ? "AN" : "AUS");
    fineNodes.repaint();
}

void DoppelgangerEditor::timerCallback()
{
    const bool visible = isShowing();
    proc.setLiveSpectrumEnabled (visible);
    const int interval = visible ? 40 : 200;
    if (getTimerInterval() != interval) startTimer (interval);
    if (! visible) return;
    const auto oldHeadline = headline, oldStatus = statusText;
    const auto oldCorrection = shown.correction;
    const auto oldCurve = shown.profile.curveDb;
    const bool oldValid = shown.profile.valid;
    const int oldRegister = shown.effectiveRegister;
    const auto oldHist = deessHist;
    const float oldHold = deessHoldDb, oldAlpha = liveAlpha;

    shown = proc.getDisplayInfo();
    updateLearnButton();

    const bool manual = proc.apvts.getRawParameterValue ("register")->load() > 0.5f;
    headline = {};
    if (proc.isLearning())
    {
        headline = u8 ("Lernt …");
        statusText = u8 ("Jetzt die Spur abspielen. Es hört von selbst auf, sobald es genug gelernt hat.");
    }
    else if (shown.message.isNotEmpty())
        statusText = shown.message;
    else if (! shown.profile.valid)
        statusText = u8 ("Noch nichts gelernt. Bis dahin arbeiten nur Lowcut, De-Esser und Leveler.");
    else if (shown.needsRelearnForLoudness)
        statusText = u8 ("Bitte einmal neu mit Learn lernen: Dieses Profil stammt aus v0.2 und kennt die Lautheit der Spur noch nicht.");
    else
    {
        headline = (manual ? u8 ("REFERENZ: ") : u8 ("ERKANNT: ")) + juce::String (kRegNames[shown.effectiveRegister]);
        statusText = (manual ? u8 ("fest gewählt  ·  ") : juce::String())
                     + (shown.profile.pitchHz > 0 ? u8 ("Tonhöhe ") + juce::String (juce::roundToInt (shown.profile.pitchHz)) + " Hz"
                                                  : u8 ("Tonhöhe nicht erkannt, Referenz Mittel"))
                     + u8 ("  ·  ") + juce::String (juce::roundToInt (shown.profile.speechSeconds)) + u8 (" s gelernt")
                     + (proc.wasStoppedAutomatically() ? u8 ("  ·  automatisch beendet") : juce::String());
    }

    // De-Esser-Anzeige: Spitze seit dem letzten Bild, Haltemarke faellt langsam
    {
        const float pk = proc.getEngine().meterDeessPeakDb.exchange (0.0f);
        deessCurDb = pk;
        deessHoldDb = pk >= deessHoldDb ? pk : std::max (0.0f, deessHoldDb - 0.25f);
        deessHist[(size_t) deessHistPos] = pk;
        deessHistPos = (deessHistPos + 1) % kDeessHist;
    }

    // Live-Spektrum: schnell hoch, etwas traeger runter; in Pausen langsam ausblenden
    dop::GridCurve li {}, lo {};
    const bool fresh = proc.getLiveSpectrum (li, lo);
    if (fresh)
    {
        const float a = liveHasData ? 0.35f : 1.0f;
        for (size_t i = 0; i < li.size(); ++i)
        {
            liveIn[i] += a * (li[i] - liveIn[i]);
            liveOut[i] += a * (lo[i] - liveOut[i]);
        }
        liveHasData = true;
        liveAlpha = std::min (1.0f, liveAlpha + 0.25f);
    }
    else
        liveAlpha = std::max (0.0f, liveAlpha - 0.02f);
    auto& engine = proc.getEngine();
    const std::array<float, 4> meters { engine.meterOutLufs.load(), engine.meterStaticDb.load(),
                                       engine.meterRideDb.load(), engine.meterDeessDb.load() };
    bool metersChanged = false;
    for (size_t i = 0; i < meters.size(); ++i)
        metersChanged = metersChanged || std::round (meters[i] * 10) != std::round (shownMeters[i] * 10);
    shownMeters = meters;
    if (fresh || oldAlpha != liveAlpha || oldHold != deessHoldDb || oldHist != deessHist
        || oldHeadline != headline || oldStatus != statusText || oldCorrection != shown.correction
        || oldCurve != shown.profile.curveDb || oldValid != shown.profile.valid
        || oldRegister != shown.effectiveRegister || metersChanged)
        repaint();
}

void DoppelgangerEditor::applyTheme()
{
    look.setColour (juce::Slider::thumbColourId, theme.accent);
    look.setColour (juce::Slider::trackColourId, theme.accent);
    look.setColour (juce::Slider::rotarySliderFillColourId, theme.accent);
    look.setColour (juce::Slider::rotarySliderOutlineColourId, theme.line);
    look.setColour (juce::Slider::backgroundColourId, theme.line);
    look.setColour (juce::Slider::textBoxTextColourId, theme.text);
    look.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    look.setColour (juce::PopupMenu::backgroundColourId, theme.panel);
    look.setColour (juce::PopupMenu::textColourId, theme.text);
    look.setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.accent);
    look.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    look.setColour (juce::ComboBox::backgroundColourId, theme.dark ? theme.bg : theme.display);
    look.setColour (juce::ComboBox::textColourId, theme.text);
    look.setColour (juce::ComboBox::outlineColourId, theme.line);
    look.setColour (juce::ComboBox::arrowColourId, theme.dim);
    look.setColour (juce::Label::textColourId, theme.text);
    for (auto* k : { &amountKnob, &lowcutKnob, &deessKnob, &deessFreqKnob, &levelerKnob, &targetKnob })
        k->label.setColour (juce::Label::textColourId, theme.dim);
    for (auto& b : fineBands)
        b.name.setColour (juce::Label::textColourId, theme.dim);
    fineNodes.accent = theme.accent;
    fineNodes.text = theme.text;
    fineNodes.dim = theme.dim;
    fineNodes.repaint();
    look.setColour (juce::TextButton::buttonColourId, theme.dark ? theme.bg : theme.display);
    look.setColour (juce::TextButton::buttonOnColourId, theme.accent);
    look.setColour (juce::TextButton::textColourOffId, theme.text);
    look.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    look.setColour (juce::TooltipWindow::backgroundColourId, theme.panel);
    look.setColour (juce::TooltipWindow::textColourId, theme.text);
    look.setColour (juce::TooltipWindow::outlineColourId, theme.line);

    themeButton.setButtonText (theme.dark ? "Look: Dunkel" : "Look: Hell");
    helpText.setAttributed (buildHelp (theme.text, theme.accent, theme.dim));
    helpScroll.fill = theme.panel;
    helpScroll.edge = theme.line;
    sendLookAndFeelChange();
    repaint();
}

void DoppelgangerEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme.bg);
    g.addTransform (juce::AffineTransform::scale (uiScale));
    const int W = kBaseW;

    // Kopfleiste wie bei LOCATIONS, RADIOPHONE und DREAMCATCHER
    g.setColour (theme.panel);
    g.fillRect (0, 0, W, 64);
    g.setColour (theme.line);
    g.fillRect (0, 63, W, 1);
    g.setColour (theme.accent);
    g.fillRect (0, 0, 5, 64);
    if (logo.isValid())
    {
        g.setColour (theme.text);
        g.drawImage (logo, juce::Rectangle<float> (14.0f, 4.0f, 56.0f, 56.0f), juce::RectanglePlacement::centred, true);
    }
    const juce::Font titleFont (juce::FontOptions (22.0f, juce::Font::bold));
    const juce::Font subFont (juce::FontOptions (11.5f, juce::Font::bold));
    auto textWidth = [] (const juce::Font& f, const juce::String& t)
    {
        juce::GlyphArrangement ga; ga.addLineOfText (f, t, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    };
    const float subX = 80.0f + textWidth (titleFont, "DOPPELGANGER") + 16.0f;
    g.setColour (theme.text);
    g.setFont (titleFont);
    g.drawText ("DOPPELGANGER", 80, 9, 240, 28, juce::Justification::centredLeft);
    g.setColour (theme.accent);
    g.setFont (subFont);
    g.drawText ("AUDIO DRAMA VOICE MATCHER", (int) subX, 14, 260, 22, juce::Justification::centredLeft);
    g.setColour (theme.dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (u8 ("STROMSCHLAG AUDIO  ·  v" PLUGIN_VERSION_TEXT), 80, 36, 300, 16, juce::Justification::centredLeft);

    // Kleines Symbol: zwei sich deckende Koepfe (Doppelgaenger), einfarbig, folgt dem Look
    {
        auto head = [] (float x)
        {
            juce::Path p;
            p.addEllipse (x + 5.0f, 0.0f, 12.0f, 13.0f);
            p.startNewSubPath (x, 30.0f);
            p.quadraticTo (x + 1.0f, 15.0f, x + 11.0f, 15.0f);
            p.quadraticTo (x + 21.0f, 15.0f, x + 22.0f, 30.0f);
            return p;
        };
        const float subEnd = subX + textWidth (subFont, "AUDIO DRAMA VOICE MATCHER");
        const float mid = 0.5f * (subEnd + (float) (W - 160));
        auto back = head (0.0f), front = head (10.0f);
        const auto place = juce::AffineTransform::translation (mid - 16.0f, 17.0f);
        back.applyTransform (place);
        front.applyTransform (place);
        g.setColour (theme.panel.interpolatedWith (theme.text, 0.10f));
        g.fillPath (back);
        g.setColour (theme.panel.interpolatedWith (theme.text, 0.30f));
        g.strokePath (back, juce::PathStrokeType (1.2f));
        g.setColour (theme.panel);
        g.fillPath (front);
        g.setColour (theme.panel.interpolatedWith (theme.text, 0.45f));
        g.strokePath (front, juce::PathStrokeType (1.6f));
    }

    auto card = [&] (float x, float y, float w, float h, const juce::String& title)
    {
        g.setColour (theme.panel);
        g.fillRoundedRectangle (x, y, w, h, 8.0f);
        g.setColour (theme.line);
        g.drawRoundedRectangle (x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 8.0f, 1.0f);
        g.setColour (theme.accent);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, (int) x + 12, (int) y + 4, (int) w - 24, 18, juce::Justification::centredLeft);
    };

    // Karte: Stimme lernen
    card (12.0f, 76.0f, (float) W - 24.0f, 72.0f, "STIMME");
    g.setColour (theme.dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("STIMMLAGE", W - 166, 80, 140, 16, juce::Justification::centredLeft);
    if (headline.isNotEmpty())
    {
        g.setColour (theme.accent);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (headline, 296, 94, W - 296 - 176, 22, juce::Justification::centredLeft);
    }
    g.setColour (shown.message.isNotEmpty() || shown.needsRelearnForLoudness ? theme.accent : theme.text);
    g.setFont (juce::FontOptions (12.0f));
    if (headline.isNotEmpty())
        g.drawFittedText (statusText, 296, 118, W - 296 - 176, 26, juce::Justification::topLeft, 2, 1.0f);
    else
        g.drawFittedText (statusText, 296, 98, W - 296 - 176, 44, juce::Justification::centredLeft, 3, 1.0f);

    // Karte: Klangbild
    card (12.0f, 158.0f, (float) W - 24.0f, 142.0f, "KLANGBILD");
    {
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        const int lx = W - 330;
        g.setColour (theme.dim);
        g.drawText ("REFERENZ", lx, 162, 70, 16, juce::Justification::centredLeft);
        g.setColour (theme.take);
        g.drawText ("STIMME", lx + 76, 162, 60, 16, juce::Justification::centredLeft);
        g.setColour (theme.accent);
        g.drawText (u8 ("KORREKTUR ±12 dB"), lx + 140, 162, 160, 16, juce::Justification::centredLeft);
        g.setColour (theme.text.withAlpha (0.55f));
        g.drawText ("LIVE:  EINGANG", 112, 162, 100, 16, juce::Justification::centredLeft);
        g.setColour (theme.text);
        g.drawText ("AUSGANG", 214, 162, 70, 16, juce::Justification::centredLeft);
        if (fineOnButton.getToggleState())
            g.drawText ("FEIN", lx - 46, 162, 40, 16, juce::Justification::centredLeft);
    }
    const auto r = juce::Rectangle<float> (22.0f, 182.0f, (float) W - 44.0f, 108.0f);
    g.setColour (theme.display);
    g.fillRoundedRectangle (r, 5.0f);
    auto xOf = [&] (float hz) { return r.getX() + r.getWidth() * (std::log2 (hz / 40.0f) / std::log2 (20000.0f / 40.0f)); };
    g.setFont (juce::FontOptions (10.0f));
    for (float hz : { 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        g.setColour (theme.line.withAlpha (0.7f));
        g.drawVerticalLine ((int) xOf (hz), r.getY(), r.getBottom());
        g.setColour (theme.dim);
        g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz),
                    (int) xOf (hz) + 3, (int) r.getBottom() - 13, 30, 12, juce::Justification::left);
    }
    g.setColour (theme.line);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    auto drawCurve = [&] (const float* v, float centreDb, float rangeDb, juce::Colour c, float thick)
    {
        juce::Path path;
        for (int i = 0; i < dop::kRefGridPoints; ++i)
        {
            const float y = r.getCentreY() - (v[i] - centreDb) / rangeDb * (r.getHeight() * 0.5f - 4.0f);
            const juce::Point<float> pt (xOf (dop::kRefGridHz[i]), juce::jlimit (r.getY() + 2.0f, r.getBottom() - 2.0f, y));
            if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
        }
        g.setColour (c);
        g.strokePath (path, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.toNearestInt());
        if (liveHasData && liveAlpha > 0.01f)
        {
            // Ausgang als Flaeche, Eingang als duenne Linie (Klangform, mittlere Hoehe gleich)
            juce::Path area, line;
            auto yv = [&] (float v) { return juce::jlimit (r.getY() + 2.0f, r.getBottom() - 2.0f, r.getCentreY() - (v - -6.0f) / 34.0f * (r.getHeight() * 0.5f - 4.0f)); };
            area.startNewSubPath (xOf (dop::kRefGridHz[0]), r.getBottom());
            for (int i = 0; i < dop::kRefGridPoints; ++i)
            {
                area.lineTo (xOf (dop::kRefGridHz[i]), yv (liveOut[(size_t) i]));
                const juce::Point<float> pt (xOf (dop::kRefGridHz[i]), yv (liveIn[(size_t) i]));
                if (i == 0) line.startNewSubPath (pt); else line.lineTo (pt);
            }
            area.lineTo (xOf (dop::kRefGridHz[dop::kRefGridPoints - 1]), r.getBottom());
            area.closeSubPath();
            g.setColour (theme.text.withAlpha (0.16f * liveAlpha));
            g.fillPath (area);
            g.setColour (theme.text.withAlpha (0.55f * liveAlpha));
            g.strokePath (line, juce::PathStrokeType (1.0f));
        }
        drawCurve (dop::referenceCurve (shown.effectiveRegister), -6.0f, 34.0f, theme.dim, 1.4f);
        if (shown.profile.valid)
        {
            drawCurve (shown.profile.curveDb.data(), -6.0f, 34.0f, theme.take, 1.6f);
            drawCurve (shown.correction.data(), 0.0f, 12.0f, theme.accent, 2.2f);
        }
    }
    // Messwerte
    {
        auto& e = proc.getEngine();
        const juce::String m = u8 ("AUSGANG ") + juce::String (e.meterOutLufs.load(), 1) + u8 (" LUFS   ·   GRUND ")
                               + fmtDb (e.meterStaticDb.load()) + u8 ("   ·   LEVELER ") + fmtDb (e.meterRideDb.load())
                               + u8 ("   ·   DE-ESSER ") + fmtDb (-e.meterDeessDb.load());
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (m, (int) r.getX() + 8, (int) r.getY() + 4, (int) r.getWidth() - 16, 14, juce::Justification::centredRight);
    }

    // Untere Reihe
    card (12.0f, 310.0f, 172.0f, 128.0f, "KLANG");
    card (194.0f, 310.0f, 292.0f, 128.0f, "DE-ESSER");
    card (496.0f, 310.0f, 172.0f, 128.0f, "LAUTHEIT");
    card (12.0f, 450.0f, (float) W - 24.0f, 108.0f, "FEINABSTIMMUNG");

    // De-Esser-Anzeige: Balken (aktuelle Absenkung, haengt von oben) + Verlauf der letzten Sekunden
    {
        const float top = 336.0f, bot = 416.0f, h = bot - top;
        const float maxDb = 12.0f;
        auto depth = [&] (float db) { return juce::jlimit (0.0f, 1.0f, db / maxDb) * h; };
        const juce::Rectangle<float> bar (424.0f, top, 12.0f, h), hist (442.0f, top, 40.0f, h);
        g.setColour (theme.dim.withAlpha (0.18f));
        g.fillRoundedRectangle (bar, 3.0f);
        g.fillRoundedRectangle (hist, 3.0f);
        if (deessCurDb > 0.05f)
        {
            g.setColour (theme.accent);
            g.fillRoundedRectangle (bar.withHeight (std::max (2.0f, depth (deessCurDb))), 3.0f);
        }
        if (deessHoldDb > 0.05f)
        {
            g.setColour (theme.text);
            g.fillRect (bar.getX() - 2.0f, top + depth (deessHoldDb) - 1.0f, bar.getWidth() + 4.0f, 2.0f);
        }
        juce::Path hp;
        hp.startNewSubPath (hist.getX(), top);
        for (int i = 0; i < kDeessHist; ++i)
        {
            const float v = deessHist[(size_t) ((deessHistPos + i) % kDeessHist)];
            hp.lineTo (hist.getX() + hist.getWidth() * (float) i / (kDeessHist - 1), top + depth (v));
        }
        hp.lineTo (hist.getRight(), top);
        hp.closeSubPath();
        g.setColour (theme.accent.withAlpha (0.55f));
        g.fillPath (hp);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (9.5f));
        g.drawText ("0", 424, (int) top - 11, 20, 10, juce::Justification::centredLeft);
        g.drawText (u8 ("−12 dB"), 424, (int) bot + 1, 58, 11, juce::Justification::centredLeft);
        g.setColour (deessHoldDb > 0.05f ? theme.text : theme.dim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (deessHoldDb > 0.05f ? u8 ("−") + juce::String (deessHoldDb, 1) + " dB" : u8 ("S: –"),
                    442, (int) top - 12, 40, 12, juce::Justification::centredRight);
    }
}

void DoppelgangerEditor::resized()
{
    if (sizeReady)
    {
        proc.setUiProperty (kUiWProp, getWidth());
        proc.setUiProperty (kUiHProp, getHeight());
    }
    uiScale = std::min ((float) getWidth() / kBaseW, (float) getHeight() / kBaseH);
    content.setTransform (juce::AffineTransform::scale (uiScale));
    content.setBounds (0, 0, kBaseW, kBaseH);
    repaint();

    const int w = kBaseW;
    themeButton.setBounds (w - 160, 18, 104, 28);
    helpButton.setBounds (w - 48, 18, 32, 28);
    helpScroll.setBounds (12, 76, w - 24, kBaseH - 88);
    helpText.setWidth (helpScroll.getMaximumVisibleWidth());

    learnButton.setBounds (24, 102, 160, 34);
    forgetButton.setBounds (192, 102, 92, 34);
    registerBox.setBounds (w - 166, 102, 142, 30);

    auto place = [] (Knob& k, int x, int y)
    {
        k.label.setBounds (x, y, 80, 16);
        k.slider.setBounds (x, y + 16, 80, 88);
    };
    place (amountKnob, 14, 330);
    place (lowcutKnob, 94, 330);
    place (deessKnob, 198, 330);
    place (deessFreqKnob, 278, 330);
    deessOnButton.setBounds (358, 340, 60, 28);
    deessListenButton.setBounds (358, 378, 60, 28);
    place (levelerKnob, 500, 330);

    // Feinabstimmung unten: Schalter links, fuenf Baender mit Frequenz, Verstaerkung und Guete
    fineOnButton.setBounds (24, 478, 70, 28);
    fineNodes.setBounds (22, 182, w - 44, 108);
    for (int k = 0; k < dop::kFineBands; ++k)
    {
        auto& b = fineBands[(size_t) k];
        const int x = 104 + k * 113;
        b.name.setBounds (x, 476, 109, 14);
        b.freq.setBounds (x, 492, 109, 18);
        b.gain.setBounds (x, 512, 109, 18);
        b.q.setBounds (x, 532, 109, 18);
    }
    place (targetKnob, 580, 330);

    helpScroll.toFront (false);
}
