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
             juce::Colour (0xff121b1d), juce::Colour (0xff8fc8f0), juce::Colour (0xff7fd49a), true };
}
DoppelgangerEditor::Theme DoppelgangerEditor::lightTheme()
{
    return { juce::Colour (0xffd5dbda), juce::Colour (0xffe8eae9), juce::Colour (0xfff5781a),
             juce::Colour (0xff233539), juce::Colour (0xff5d6e72), juce::Colour (0xffb5bfbe),
             juce::Colour (0xfff4f6f5), juce::Colour (0xff2c6e9e), juce::Colour (0xff2f8a4c), false };
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

    body ("Doppelganger gleicht Sprecherspuren an neutrale Sprecherprofile aus einem Berliner Profi-Studio an und bringt sie auf die "
          "Ziel-Lautheit anhand der gelernten Passage. Takes aus verschiedenen Heimstudios, mit verschiedenen Mikrofonen und Räumen, klingen danach, als wären sie "
          "in derselben Kabine aufgenommen worden. Lowcut, Angleich-EQ, Feinabstimmung, De-Esser und Leveler stecken in einem Plugin.");

    head ("SO ARBEITET ES");
    body ("Im Plugin stecken drei neutrale Vorlagen: eine tiefe Männerstimme, eine mittlere und eine hohe Frauenstimme. "
          "Beim Lernen misst Doppelganger das Klangbild, die Tonhöhe und die Lautheit der Spur. Aus dem Unterschied zur Vorlage "
          "entsteht eine geglättete Korrekturkurve. Angeglichen wird die Färbung von Mikrofon und Raum, nicht die Stimme selbst: "
          "ein Bass bleibt ein Bass, eine helle Stimme bleibt hell.");
    body ("Weg durchs Plugin: Lowcut, Angleich-EQ, Feinabstimmung, De-Esser, dann Grundverstärkung und Leveler, mit AUTO zum Schluss der Spitzenschutz. Latenzfrei.");
    note ("Gedacht für Dialog in Hörspielen, Hörbüchern und Podcasts. Nicht für Gesang, Effektstimmen oder als Rauschunterdrückung: "
          "Die Aufnahmen sollten sauber und trocken sein, Dereverb und Denoise gehören vor das Plugin.");

    head ("EINGANG LERNEN");
    body ("„Learn“ drücken und die Spur abspielen. Das Plugin misst dabei Klangbild, Tonhöhe und Lautheit der Stimme, auch bei sehr leisen, sauberen Aufnahmen. Es prüft selbst, ob sich Klangbild "
          "und Lautstärke nicht mehr verändern, und hört dann von allein auf (frühestens nach 20 Sekunden Sprache, spätestens nach 30). Mit „Done“ geht es früher (mindestens 4 Sekunden Sprache). Ausreichend leise Abschnitte zählen nicht zur erkannten Sprachdauer. Bei kurzen Takes bis zum Ende abspielen und „Done“ drücken. "
          "Das Gelernte wird mit dem Projekt gespeichert. „Clear“ löscht es wieder.");
    body ("Am besten eine Stelle lernen, an der die Figur normal spricht, nicht nur schreit oder flüstert.");

    head ("AUTOPILOT (AUTO)");
    body ("Mit AUTO (Standard, oben rechts in der Karte KLANG) übernimmt das Plugin alles, was sich messen lässt. Du lernst nur; nachstellen kannst du jederzeit.");
    body ("Nach dem Lernen stellt es die Angleichung so ein, dass im Mittel höchstens 1 dB Färbung von Mikrofon und Raum übrig bleibt: kaum gefärbte Takes werden nur leicht "
          "angefasst (45 %), stark gefärbte deutlicher, aber nie ganz auf die Referenz gezogen (höchstens 85 %). Den Lowcut setzt es nach der Tonhöhe (80 bis 110 Hz).");
    body ("Den De-Esser stellt es aus sicher erkannten Zischlauten ein: Gezählt werden nur kurze, rauschartige Abschnitte nahe am Sprechpegel, in denen die Höhen überwiegen; "
          "helle Vokale, Atmer und Flüstern zählen nicht. Liegen die lauten S-Laute höchstens 12 dB unter den Vokalen (natürlich sind etwa 13 bis 16 dB), bleibt die Stärke bei 0 % "
          "und die Stimme unbearbeitet. Sonst wählt es die Frequenz nach der S-Energie dieser Stimme (4 bis 8 kHz) und die kleinste Stärke, die die S-Laute auf dieses Maß bringt, "
          "höchstens 60 % und nur, solange Vokale praktisch unberührt bleiben. Bei zu wenig S-Lauten, Flüstern oder viel Rauschen bleibt die Einstellung, wie sie ist.");
    body ("Danach rechnet es den gespeicherten Lernmitschnitt im Hintergrund durch die ganze Kette und misst dessen Lautheit am Ausgang nach. Was De-Esser, Leveler und EQ "
          "an der Lautheit verschieben, gleicht es aus (bis ±6 dB). Mit vorhandenem Lernmitschnitt misst es nach Regleränderungen, auch am Lowcut, neu. Ein latenzfreier Spitzenschutz hält die Sample-Spitzen "
          "unter −1 dBFS und greift nur ein, wenn eine Spitze die Grenze wirklich überschreitet. Zwischen den Samples (True Peak) kann es knapp darüber liegen; "
          "der gemessene True Peak steht im Tooltip von AUTO.");
    body ("Die Zeile unter der Tonhöhe zeigt das Ergebnis: grün „geprüft“ mit der nachgemessenen Lautheit, orange ein Hinweis, wenn die Aufnahme etwas hat, "
          "das kein Regler reparieren kann: übersteuert (verzerrt), hörbares Rauschen in den Pausen, sehr laute Spitzen oder ein unerreichbares Ziel. "
          "Mit der Maus über AUTO stehen alle Messwerte.");
    note ("Regler bleiben jederzeit bedienbar, der Autopilot setzt sie nur einmal nach dem Lernen. AUTO aus: alles wie bis Version 1.7. "
          "Nach dem Öffnen eines Projekts gilt die gespeicherte Nachmessung, aber nur für genau die Einstellungen, mit denen sie gemessen wurde. "
          "Wurde danach etwas verändert, steht dort „Lautheit geschätzt“; für eine neue Prüfung einmal neu lernen.");

    head ("STIMMLAGE");
    body ("Auto wählt die Referenz nach der gelernten Tonhöhe: unter 160 Hz Tief (Mann), bis 225 Hz Mittel (Frau), darüber Hoch (Frau). "
          "Liegt Auto einmal daneben, die Stimmlage einfach fest einstellen.");

    head ("ANGLEICHUNG und LOWCUT");
    body ("Angleichung: wie stark die Klangfarbe an die Referenz angeglichen wird. 100 % = ganz, 60 % = Mikrofon und Raum werden "
          "angeglichen, die eigene Farbe der Stimme bleibt. Mit AUTO stellt das Plugin den Wert nach dem Lernen selbst ein (45 bis 85 %), ohne AUTO gilt 60 %. "
          "Angeglichen wird nur die Klangform, nie der Pegel. Die Korrektur ist auf ±9 dB begrenzt.");
    body ("Angleich-Bereich: Im Klangbild stehen links und rechts zwei orange Begrenzer. Angeglichen wird nur dazwischen, außerhalb bleibt der Take "
          "unverändert (weicher Übergang über eine Drittel-Oktave). Am Griff unten am Rand oder an der Linie ziehen, Umschalt = fein, Doppelklick = Standard. "
          "Standard: unten keine Grenze, oben 12 kHz. Darüber sind die Referenzen nicht mehr verlässlich, die Höhen des Takes bleiben deshalb, wie sie sind. "
          "Den rechten Begrenzer zum Beispiel auf 8 bis 10 kHz ziehen, wenn die Luft einer Stimme erhalten bleiben soll, den linken auf 100 bis 150 Hz, wenn "
          "der Nahbesprechungs-Bass eines Sprechers nicht angefasst werden soll.");
    body ("Lowcut: 24 dB/Oktave, ganz links aus. Standard 80 Hz, mit AUTO nach der Tonhöhe.");
    note ("Im Bild: grau die Referenz (das Ziel), blau der Eingang (die gelernte Klangform der Spur), orange die Korrektur (±12 dB über die Bildhöhe, die Mitte ist 0 dB). Grün läuft der Ausgang live mit: das, was tatsächlich herauskommt. Oben rechts zeigt EQ, ob die Feinabstimmung eingeschaltet ist; dann ist ihr Verlauf die helle Linie mit den Punkten 1 bis 5. Abgedunkelt ist der Bereich außerhalb der Begrenzer. Je näher der Ausgang an der grauen Referenz liegt, desto besser passt die Spur.");

    head ("DE-ESSER");
    body ("AN / AUS schaltet den De-Esser gezielt ab, zum Vergleich. Stärke: wie weit Zischlaute abgesenkt werden, bei 25 % etwa 3 bis 4 dB, "
          "bei 100 % etwa 6 bis 10 dB. Frequenz: ab hier sucht und bearbeitet der De-Esser die S-Laute (Standard 5,5 kHz). "
          "Tiefer bei dumpfen oder männlichen Stimmen, höher bei hellen.");
    body ("Erkannt wird ein Zischlaut daran, dass oberhalb der Frequenz mehr Energie liegt als im tiefen Stimmbereich darunter. Bei Vokalen ist es umgekehrt, "
          "deshalb greift der De-Esser nur bei S, Z, Sch, F und scharfen T zu und lässt helle Vokale in Ruhe.");
    body ("Rechts daneben zeigt die Anzeige, was der De-Esser gerade tut: der Balken hängt von oben herunter, so tief wie die aktuelle Absenkung (0 bis 12 dB), "
          "der Strich markiert die letzte Spitze. Daneben läuft ein Verlauf der letzten Sekunden: jeder Ausschlag ist ein erkannter S-Laut, der gedämpft wurde. "
          "Bleibt die Anzeige leer, erkennt er keine S-Laute, dann ist die Stärke zu niedrig oder die Frequenz passt nicht.");
    body ("Listen: spielt nur die tatsächlich entfernten Anteile ab (Differenz vor/nach der De-Esser-Absenkung). So hörst du, welche S-Laute und Stimmanteile abgezogen werden. Ohne Absenkung hörst du nichts. Danach wieder ausschalten.");
    note ("Klingen die S-Laute eines Takes ohnehin schwach oder lispelnd, den De-Esser ausschalten.");

    head ("EQ (FEINABSTIMMUNG)");
    body ("Optional: fünf frei einstellbare Glockenfilter nach dem Angleich. Damit lässt sich die Spur direkt im Plugin nachschärfen, ohne zusätzlichen EQ in der Slotkette. "
          "Standard: alle Bänder 0 dB, Feinabstimmung aus, der Klang bleibt dann unverändert.");
    body ("In der Anzeige ziehen: waagerecht die Frequenz, senkrecht die Verstärkung (±12 dB). Das Mausrad ändert die Güte des Bands, Doppelklick setzt es auf 0 dB. Umschalt macht Ziehen und Mausrad feiner. "
          "Ziehen schaltet die Feinabstimmung automatisch ein. Die Regler darunter stellen dieselben Werte als Zahl ein.");
    body ("Die Grundverstärkung berücksichtigt die geschätzte Lautheitswirkung der Feinabstimmung. Mit AUTO und vorhandenem Lernmitschnitt wird nach Änderungen erneut nachgemessen. Das Lautheitsziel gilt für diesen Mitschnitt.");
    body ("RESET unter dem AN/AUS-Schalter setzt alle fünf Bänder auf 0 dB, Standardfrequenzen und Q = 1 zurück. Der EQ bleibt an oder aus wie zuvor.");

    head ("LAUTHEIT");
    body ("Beim Lernen wird die integrierte Lautheit nach ITU-R BS.1770 / EBU R128 ermittelt. Frequenzen werden nach ihrer Lautheitswirkung gewichtet; "
          "Stille und ausreichend leise Abschnitte werden über absolute und relative Messschwellen ausgeblendet. Lautere Atmer können mitgemessen werden. "
          "In Stereo werden die Energien beider Kanäle addiert. Bei gleichem Signal links und rechts ist deshalb rund 3 dB weniger Verstärkung nötig als in Mono.");
    body ("Die Grundverstärkung kommt aus der gelernten Lautheit. Beispiel: Von −30 auf −23 LUFS sind ohne weitere Klang- oder Dynamikänderung ungefähr +7 dB nötig. "
          "Mit AUTO rechnet das Plugin den gespeicherten Lernmitschnitt durch die gesamte Kette einschließlich EQ, De-Esser, Leveler und Spitzenschutz, misst dessen Ausgang nach "
          "und korrigiert die Verstärkung innerhalb seiner Grenzen. Standardziel: −23 LUFS. AUTO aus: Die Lautheitswirkung wird nur vorausberechnet.");
    note ("Das Ziel gilt für den nachgemessenen Lernmitschnitt. Andere Sätze, spätere lautere oder leisere Abschnitte und andere Exportbereiche können abweichen. "
          "Eine repräsentative Passage mit normaler Sprache lernen. Einzelne Wörter dürfen unterschiedlich laut sein; −23 LUFS ist kein Spitzenpegel von −23 dBFS. "
          "Die grüne Prüfung beschreibt den Mitschnitt und die zugehörigen Einstellungen einschließlich der Abtastrate. Nach einem Abtastratenwechsel bitte neu lernen. Der begrenzte Mitschnitt behält die zuletzt aufgenommenen Samples.");
    body ("Leveler: fährt darüber hinaus sanft wie ein Fader und gleicht leise und laute Passagen an. Der Regler bestimmt den Bereich: "
          "0 % = aus, 100 % = bis zu ±10 dB. In ausreichend leisen Pausen hält er still. Er kann die integrierte Lautheit verändern; AUTO misst diese Wirkung am Lernmitschnitt nach. Bei jedem Wiedergabestart beginnt er neutral, genau wie beim Export. Die Grundverstärkung steht ab dem ersten Sample bereit.");
    body ("Messwert oben rechts in der Karte: die gemessene Lautheit des Ausgangs, integriert nach EBU R128, wie sie ein Loudness-Meter am Ende einer "
          "Passage anzeigt (ausreichend leise Abschnitte werden ausgeblendet). Die Messung beginnt bei jedem Wiedergabestart neu, ein Klick setzt sie sofort zurück. "
          "Orange heißt: innerhalb von ±1 LU um das Ziel. Erst nach einigen Sekunden Sprache ist der Wert aussagekräftig.");
    note ("Oben im Klangbild steht zusätzlich MOMENTAN: die Lautheit der letzten 0,4 Sekunden. Sie springt mit der Sprache und fällt in Pausen ab, zum Prüfen des Ziels taugt nur der integrierte Wert.");

    head ("Allgemein");
    body ("Doppelganger ist latenzfrei. Mit AUTO hält ein Spitzenschutz den Ausgang unter −1 dBFS (Sample-Spitze); ohne AUTO ist im Plugin kein Spitzenschutz aktiv.");
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

void IntegratedLoudness::set (float lufs, float seconds, float target)
{
    if (std::abs (lufs - value) < 0.05f && std::abs (target - goal) < 0.05f && (int) seconds == (int) secs) return;
    value = lufs; secs = seconds; goal = target;
    const int s = (int) seconds;
    setTooltip (juce::String::fromUTF8 ("Gemessene Lautheit des Ausgangs, integriert nach EBU R128 (ausreichend leise Abschnitte ausgeblendet), seit ")
        + juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2)
        + juce::String::fromUTF8 (" min. Beginnt bei jedem Wiedergabestart neu, Klick setzt sie sofort zurück."));
    repaint();
}

void IntegratedLoudness::paint (juce::Graphics& g)
{
    const bool has = value > -99.0f;
    const bool onTarget = has && std::abs (value - goal) <= 1.0f;
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (has ? (onTarget ? accent : text) : dim);
    const juce::String t = has ? juce::String (value, 1) + " LUFS" : juce::String::fromUTF8 ("– LUFS");
    g.drawText (t, getLocalBounds(), juce::Justification::centredRight);
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
    matchLo = s.getParameter ("matchLow");
    matchHi = s.getParameter ("matchHigh");
}

float FineNodes::xOfHz (float hz) const
{
    return getWidth() * std::log2 (hz / kFineMinHz) / std::log2 (kFineMaxHz / kFineMinHz);
}

float FineNodes::hzOfX (float x) const
{
    return kFineMinHz * std::pow (kFineMaxHz / kFineMinHz, juce::jlimit (0.0f, 1.0f, x / (float) getWidth()));
}

// Begrenzer greifen: an der Linie (±6 px) oder am Griff oben. Knoten der Feinabstimmung haben Vorrang.
int FineNodes::nearestLimiter (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 7.0f;
    for (int k = 0; k < 2; ++k)
    {
        const float x = juce::jlimit (3.0f, (float) getWidth() - 3.0f, xOfHz (valueOf (limiterParam (k))));
        const float d = std::abs (p.x - x) - (inGrip (p) ? 4.0f : 0.0f);
        if (d < bestD) { bestD = d; best = k; }
    }
    return best;
}

void FineNodes::setLimiterHz (int k, float hz)
{
    if (k == 0)
    {
        hz = hz < 45.0f ? dop::kMatchLoMin : juce::jlimit (dop::kMatchLoMin, dop::kMatchLoMax, hz);   // ganz links = keine Grenze
        hz = std::min (hz, valueOf (matchHi) / 1.5f);
    }
    else
    {
        hz = hz > 18000.0f ? dop::kMatchHiMax : juce::jlimit (dop::kMatchHiMin, dop::kMatchHiMax, hz);   // ganz rechts = keine Grenze
        hz = std::max (hz, valueOf (matchLo) * 1.5f);
    }
    setValue (limiterParam (k), hz);
}

void FineNodes::refresh (double rate)
{
    rate = dop::validSampleRate (rate);
    std::array<float, 18> values {};
    values[0] = on->getValue();
    values[16] = matchLo->getValue();
    values[17] = matchHi->getValue();
    for (size_t k = 0; k < bands.size(); ++k)
    {
        values[1 + 3 * k] = bands[k].hz->getValue();
        values[2 + 3 * k] = bands[k].db->getValue();
        values[3 + 3 * k] = bands[k].q->getValue();
    }
    if (! snapshotReady || values != lastValues || sampleRate != rate)
    { lastValues = values; sampleRate = rate; snapshotReady = true; repaint(); }
}

void FineNodes::mouseMove (const juce::MouseEvent& e)
{
    const int k = inGrip (e.position) && nearestLimiter (e.position) >= 0 ? -1 : nearestBand (e.position, 18.0f);   // Griffe oben haben Vorrang
    if (k < 0)
    {
        const int l = nearestLimiter (e.position);
        setMouseCursor (l >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        if (l < 0) { setTooltip ({}); return; }
        auto* p = limiterParam (l);
        setTooltip (juce::String::fromUTF8 (l == 0 ? "Angleich ab " : "Angleich bis ") + p->getCurrentValueAsText()
            + juce::String::fromUTF8 (l == 0 ? ". Darunter bleibt der Take unverändert. Ziehen: verschieben, Umschalt: fein, Doppelklick: ganz nach links."
                                             : ". Darüber bleibt der Take unverändert. Ziehen: verschieben, Umschalt: fein, Doppelklick: zurück auf 12 kHz."));
        return;
    }
    setMouseCursor (juce::MouseCursor::NormalCursor);
    const auto& b = bands[(size_t) k];
    setTooltip ("Band " + juce::String (k + 1) + ": " + juce::String (valueOf (b.hz), 0) + " Hz, "
        + juce::String (valueOf (b.db), 1) + " dB, Q " + juce::String (valueOf (b.q), 2)
        + juce::String::fromUTF8 (". Ziehen: Frequenz/Pegel. Mausrad: Q. Umschalt: fein. Doppelklick: 0 dB."));
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

    // Angleich-Bereich: ausserhalb abgedunkelt, Begrenzer als Linie mit Griff oben
    {
        const float xl = juce::jlimit (0.0f, w, xOfHz (valueOf (matchLo)));
        const float xr = juce::jlimit (0.0f, w, xOfHz (valueOf (matchHi)));
        g.setColour (juce::Colours::black.withAlpha (text.getPerceivedBrightness() < 0.5f ? 0.10f : 0.28f));   // heller Look: dezenter
        if (xl > 0.5f) g.fillRect (0.0f, 0.0f, xl, h);
        if (xr < w - 0.5f) g.fillRect (xr, 0.0f, w - xr, h);
        for (int k = 0; k < 2; ++k)
        {
            const float x = juce::jlimit (1.5f, w - 1.5f, k == 0 ? xl : xr);
            const bool hot = limiter == k;
            g.setColour (accent.withAlpha (hot ? 1.0f : 0.75f));
            g.drawLine (x, 18.0f, x, h, hot ? 2.0f : 1.4f);   // oben frei fuer die Messzeile
            const float tx = k == 0 ? x : x - 9.0f, ty = h - 15.0f;   // Griff unten am Rand
            g.fillRoundedRectangle (tx, ty, 9.0f, 15.0f, 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            for (float gy : { 4.5f, 7.5f, 10.5f }) g.drawHorizontalLine ((int) (ty + gy), tx + 2.0f, tx + 7.0f);
            g.setColour (text.withAlpha (0.85f));
            g.setFont (juce::FontOptions (9.5f));
            const auto label = limiterParam (k)->getCurrentValueAsText();
            if (k == 0) g.drawText (label, (int) x + 4, (int) h - 30, 70, 12, juce::Justification::centredLeft);
            else        g.drawText (label, (int) x - 74, (int) h - 30, 70, 12, juce::Justification::centredRight);
        }
    }

    // Mittellinie = 0 dB
    g.setColour (dim.withAlpha (0.35f));
    g.drawHorizontalLine ((int) (h * 0.5f), 0.0f, w);

    // Frequenzgang der Feinabstimmung als Summe der Baender
    std::array<dop::Biquad, dop::kFineBands> coefs {};
    if (on_) for (int k = 0; k < dop::kFineBands; ++k)
    {
        const auto& b = bands[(size_t) k];
        coefs[(size_t) k] = dop::fineCoefficient (sampleRate, valueOf (b.hz), valueOf (b.db), valueOf (b.q));
    }
    juce::Path curve;
    for (int i = 0; i <= 120; ++i)
    {
        const float f = kFineMinHz * std::pow (kFineMaxHz / kFineMinHz, (float) i / 120.0f);
        if (f >= sampleRate * 0.5) break;
        double sum = 0.0;
        if (on_)
            for (const auto& c : coefs) sum += dop::magnitudeDb (c, sampleRate, f);
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
    endDrag();
    if (! e.mods.isLeftButtonDown()) return;
    selected = inGrip (e.position) && nearestLimiter (e.position) >= 0 ? -1 : nearestBand (e.position, 18.0f);
    if (selected < 0)
    {
        limiter = nearestLimiter (e.position);
        if (limiter < 0) return;
        limiterParam (limiter)->beginChangeGesture();
        limiterX = juce::jlimit (0.0f, (float) getWidth(), xOfHz (valueOf (limiterParam (limiter))));
        dragOrigin = e.position;
        repaint();
        return;
    }
    const auto& b = bands[(size_t) selected];
    b.hz->beginChangeGesture();
    b.db->beginChangeGesture();
    on->beginChangeGesture();
    dragOrigin = e.position;
    dragNode = nodeAt (selected);
    dragging = true;
}

void FineNodes::mouseDrag (const juce::MouseEvent& e)
{
    if (limiter >= 0)
    {
        limiterX += (e.position.x - dragOrigin.x) * (e.mods.isShiftDown() ? 0.15f : 1.0f);
        dragOrigin = e.position;
        setLimiterHz (limiter, hzOfX (limiterX));
        mouseMove (e);
        repaint();
        return;
    }
    if (! dragging || selected < 0) return;
    const auto& b = bands[(size_t) selected];
    const auto position = dragNode + (e.position - dragOrigin) * (e.mods.isShiftDown() ? 0.15f : 1.0f);
    dragOrigin = e.position;
    dragNode = position;
    const float ratio = juce::jlimit (0.0f, 1.0f, position.x / (float) getWidth());
    const float hz = kFineMinHz * std::pow (kFineMaxHz / kFineMinHz, ratio);
    setValue (b.hz, juce::jlimit (kFineMinHz, kFineMaxHz, hz));
    setValue (b.db, dbOfY (position.y));
    if (! active()) setValue (on, 1.0f);   // Ziehen schaltet die Feinabstimmung ein
    repaint();
}

void FineNodes::mouseUp (const juce::MouseEvent&) { endDrag(); repaint(); }

void FineNodes::endDrag()
{
    if (limiter >= 0) { limiterParam (limiter)->endChangeGesture(); limiter = -1; }
    if (dragging && selected >= 0)
    {
        const auto& b = bands[(size_t) selected];
        b.hz->endChangeGesture();
        b.db->endChangeGesture();
        on->endChangeGesture();
    }
    dragging = false;
    selected = -1;
    repaint();
}

void FineNodes::mouseDoubleClick (const juce::MouseEvent& e)
{
    endDrag();
    const int k = inGrip (e.position) && nearestLimiter (e.position) >= 0 ? -1 : nearestBand (e.position, 18.0f);
    if (k < 0)
    {
        const int l = nearestLimiter (e.position);
        if (l < 0) return;
        auto* p = limiterParam (l);
        p->beginChangeGesture();
        setValue (p, l == 0 ? dop::kMatchLoDefault : dop::kMatchHiDefault);
        p->endChangeGesture();
        repaint();
        return;
    }
    const auto& b = bands[(size_t) k];
    b.db->beginChangeGesture();
    setValue (b.db, 0.0f);
    b.db->endChangeGesture();
    repaint();
}

void FineNodes::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int k = nearestBand (e.position, 18.0f);
    if (dragging || k < 0 || wheel.deltaY == 0.0f) return;
    const auto& b = bands[(size_t) k];
    const float q = juce::jlimit (0.3f, 8.0f, valueOf (b.q) * std::pow (2.0f, wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f) * (e.mods.isShiftDown() ? 0.3f : 2.0f)));
    b.q->beginChangeGesture();
    setValue (b.q, std::round (q * 100.0f) / 100.0f);
    b.q->endChangeGesture();
    if (! active()) { on->beginChangeGesture(); setValue (on, 1.0f); on->endChangeGesture(); }
    mouseMove (e);
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

    autoButton.setClickingTogglesState (true);
    autoButton.setTooltip (u8 ("Autopilot: stellt nach dem Lernen Angleichung, Lowcut und De-Esser ein und misst die ganze Kette am gespeicherten Lernmitschnitt nach."));
    autoAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "autopilot", autoButton);
    content.addAndMakeVisible (autoButton);

    registerBox.addItemList (DoppelgangerProcessor::registerNames(), 1);
    registerBox.setTooltip (u8 ("Welche Referenzstimme? Auto wählt nach der gelernten Tonhöhe."));
    registerAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "register", registerBox);
    content.addAndMakeVisible (registerBox);

    setupKnob (amountKnob, "amount", u8 ("ANGLEICHUNG"),
               u8 ("Wie stark die Klangfarbe an die Referenz angeglichen wird. Mit AUTO stellt das Plugin den Wert nach dem Lernen selbst ein: "
                   "so viel, dass im Mittel höchstens 1 dB Färbung von Mikrofon und Raum bleibt (45–85 %). Doppelklick: 60 %"), 60.0);
    setupKnob (lowcutKnob, "lowcut", u8 ("LOWCUT"), u8 ("Hochpass 24 dB/Oktave, ganz links aus. Mit AUTO nach der Tonhöhe gesetzt (80–110 Hz). Doppelklick: 80 Hz"), 80.0);
    setupKnob (deessKnob, "deess", u8 ("STÄRKE"), u8 ("Wie weit Zischlaute abgesenkt werden: 25 % ≈ 3 dB, 100 % ≈ 10 dB. "
                                                     "Mit AUTO nach dem Lernen aus den erkannten S-Lauten gesetzt (0 % = unauffällig, unbearbeitet). Doppelklick: 25 %"), 25.0);
    setupKnob (deessFreqKnob, "deessFreq", u8 ("FREQUENZ"),
               u8 ("Ab hier sucht und bearbeitet der De-Esser die S-Laute. Mit „Listen“ abhören. Doppelklick: 5,5 kHz"), 5500.0);
    setupKnob (levelerKnob, "leveler", u8 ("LEVELER"),
               u8 ("Langsamer Ausgleich leiser und lauter Passagen: 0 % = aus, 100 % = bis ±10 dB. Kann die integrierte Lautheit verändern; AUTO misst am Lernmitschnitt nach. Doppelklick: 50 %"), 50.0);
    setupKnob (targetKnob, "target", u8 ("ZIEL"), u8 ("Ziel für die integrierte Lautheit des Lernmitschnitts. Andere Abschnitte können abweichen. Doppelklick: −23 LUFS"), -23.0);

    deessOnButton.setClickingTogglesState (true);
    deessOnButton.setTooltip (u8 ("De-Esser an- und ausschalten, zum Vergleich"));
    deessOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "deessOn", deessOnButton);
    deessListenButton.setButtonText ("LISTEN");
    deessListenButton.setClickingTogglesState (true);
    deessListenButton.setTooltip (u8 ("Nur die tatsächlich entfernten Anteile hören. Ohne Absenkung bleibt es still. Für den normalen Klang LISTEN ausschalten."));
    deessListenAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "deessListen", deessListenButton);
    content.addAndMakeVisible (deessOnButton);
    content.addAndMakeVisible (deessListenButton);

    fineOnButton.setClickingTogglesState (true);
    fineOnButton.setTooltip (u8 ("EQ an- und ausschalten (Feinabstimmung nach dem Angleich). Aus: der Klang bleibt wie nach dem Angleich."));
    fineOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "fineOn", fineOnButton);
    content.addAndMakeVisible (fineOnButton);
    fineResetButton.setTooltip (u8 ("Alle fünf EQ-Bänder zurücksetzen: 0 dB, Standardfrequenzen und Q = 1. Der EQ bleibt an oder aus wie bisher."));
    fineResetButton.onClick = [this]
    {
        for (int k = 1; k <= dop::kFineBands; ++k)
            for (const auto* prefix : { "fineHz", "fineDb", "fineQ" })
                if (auto* parameter = proc.apvts.getParameter (juce::String (prefix) + juce::String (k)))
                {
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost (parameter->getDefaultValue());
                    parameter->endChangeGesture();
                }
        fineNodes.refresh (proc.getAnalysisSampleRate());
    };
    content.addAndMakeVisible (fineResetButton);
    content.addAndMakeVisible (fineNodes);
    intLoudness.onReset = [this] { proc.getEngine().resetIntegrated(); };
    content.addAndMakeVisible (intLoudness);
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
    deessListenButton.setButtonText (deessListenButton.getToggleState() ? "DIFF" : "LISTEN");
    fineOnButton.setButtonText (fineOnButton.getToggleState() ? "AN" : "AUS");
}

void DoppelgangerEditor::updateAutopilotReport()
{
    const auto& i = shown;
    autoLine = {}; autoState = 0;
    juce::StringArray tip { u8 ("Autopilot: stellt nach dem Lernen Angleichung, Lowcut und De-Esser ein und misst die ganze Kette am gespeicherten Lernmitschnitt nach.") };
    if (proc.isLearning() || ! i.profile.valid || i.needsRelearnForLoudness)
    {
        autoTip = tip.joinIntoString ("\n");
        if (autoButton.getTooltip() != autoTip) autoButton.setTooltip (autoTip);
        return;
    }
    if (! i.autopilot)
    {
        autoLine = u8 ("Autopilot aus: Einstellungen von Hand");
        tip.add (u8 ("Ausgeschaltet: Regler bleiben, wie sie sind, die Lautheit wird nur vorausberechnet (wie bis 1.7)."));
        autoTip = tip.joinIntoString ("\n");
        if (autoButton.getTooltip() != autoTip) autoButton.setTooltip (autoTip);
        return;
    }
    auto db1 = [] (float v) { return juce::String (v, 1).replace ("-", u8 ("−")).replace (".", ","); };
    juce::StringArray warnings;
    if (i.profile.clippedSamples > 0)
        warnings.add (u8 ("Eingang übersteuert: Aufnahme verzerrt"));
    if (i.profile.noiseSnrDb >= 0.0f && i.profile.noiseSnrDb < dop::autopilot::kNoiseWarnSnrDb)
        warnings.add (u8 ("Rauschen hörbar: vorher entrauschen"));
    if (i.calibration.ok && std::abs (i.calibration.outLufs - proc.apvts.getRawParameterValue ("target")->load()) > 0.5f)
        warnings.add (u8 ("Ziel nicht erreicht (") + db1 (i.calibration.outLufs) + u8 (" LUFS): Aufnahme extrem leise/laut"));
    if (i.calibration.ok && i.calibration.guardDb > dop::autopilot::kGuardWarnDb)
        warnings.add (u8 ("Sehr laute Spitzen: Spitzenschutz senkt bis ") + db1 (i.calibration.guardDb) + " dB");

    tip.add (u8 ("Angleichung ") + juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("amount")->load())) + u8 (" %, Lowcut ")
             + juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("lowcut")->load())) + " Hz.");
    tip.add (u8 ("Färbung durch Mikrofon und Raum: im Mittel ") + db1 (i.colourBeforeDb) + u8 (" dB, nach dem Angleich ") + db1 (i.colourAfterDb) + " dB.");
    if (i.calibration.ok)
        tip.add (u8 ("Lernmitschnitt nachgemessen: ") + db1 (i.calibration.outLufs) + u8 (" LUFS (Nachkorrektur ")
                 + (i.calibration.calibDb >= 0 ? "+" : "") + db1 (i.calibration.calibDb) + u8 (" dB), Spitzen ") + db1 (i.calibration.peakDb) + " dBFS"
                 + (i.calibration.truePeakDb > -99.0f ? u8 (" (True Peak ") + db1 (i.calibration.truePeakDb) + " dBTP)" : juce::String())
                 + (i.calibration.guardDb > 0.05f ? u8 (", Spitzenschutz bis ") + db1 (i.calibration.guardDb) + " dB." : u8 (", Spitzenschutz nicht nötig.")));
    tip.add (i.profile.noiseSnrDb >= 0.0f ? u8 ("Rauschabstand: ") + juce::String (juce::roundToInt (i.profile.noiseSnrDb)) + " dB."
                                          : u8 ("Rauschabstand: keine Pausen zum Messen."));
    {
        using DC = dop::autopilot::DeesserChoice;
        const auto& d = i.deesser;
        const juce::String n = juce::String (d.events) + u8 (" S-Laute erkannt");
        if (d.decision == DC::kApplied)
            tip.add (u8 ("De-Esser automatisch: ") + juce::String (juce::roundToInt (d.amount * 100.0f)) + u8 (" %, ")
                     + juce::String (d.freqHz / 1000.0f, 1).replace (".", ",") + " kHz (" + n + u8 ("). Zischlaute ")
                     + db1 (d.sharpnessBeforeDb) + u8 (" → ") + db1 (d.sharpnessAfterDb) + u8 (" dB gegenüber den Vokalen, Vokale ")
                     + db1 (-d.vowelCostDb) + " dB.");
        else if (d.decision == DC::kNotNeeded)
            tip.add (u8 ("De-Esser: S-Laute unauffällig (") + db1 (d.sharpnessBeforeDb) + u8 (" dB gegenüber den Vokalen, ") + n
                     + u8 ("), Stärke 0 %: die Stimme bleibt unbearbeitet."));
        else if (d.decision == DC::kInsufficient)
            tip.add (u8 ("De-Esser: Einstellung unverändert, ")
                     + (d.reason == DC::kNoVoicedSpeech ? u8 ("kein klarer Stimmklang (Flüstern oder Rauschen).")
                        : d.reason == DC::kTooMuchNoise ? u8 ("zu viel zischähnliches Geräusch für eine sichere Erkennung.")
                        : d.reason == DC::kVowelsAffected ? u8 ("jede Absenkung würde auch Vokale treffen.")
                        : u8 ("zu wenig sicher erkannte S-Laute (") + juce::String (d.events) + ")."));
    }
    if (i.profile.clippedSamples > 0)
        tip.add (u8 ("Übersteuerte Samples am Eingang: ") + juce::String (i.profile.clippedSamples) + u8 (". Das lässt sich nicht reparieren, besser neu aufnehmen."));
    for (auto& w : warnings) tip.add (u8 ("Hinweis: ") + w);

    if (i.calibrationPending)
        autoLine = u8 ("Autopilot misst nach …");
    else if (! warnings.isEmpty())
    {
        autoLine = warnings[0];
        autoState = 2;
    }
    else if (i.calibrationCurrent)
    {
        autoLine = u8 ("Autopilot: geprüft, ") + db1 (i.calibration.outLufs) + " LUFS";
        autoState = 1;
    }
    else if (! i.canRecalibrate && i.calibration.ok)
    {
        autoLine = u8 ("Geändert: Lautheit geschätzt, zum Prüfen neu lernen");
        tip.add (u8 ("Nach dem Laden eines Projekts liegt das gelernte Audio nicht mehr vor. Nach Regleränderungen wird die Lautheit deshalb nur vorausberechnet."));
    }
    else
        autoLine = u8 ("Autopilot: Lautheit vorausberechnet");
    autoTip = tip.joinIntoString ("\n");
    if (autoButton.getTooltip() != autoTip) autoButton.setTooltip (autoTip);
}

void DoppelgangerEditor::timerCallback()
{
    const bool visible = isShowing();
    proc.setLiveSpectrumEnabled (visible);
    const int interval = visible ? 40 : 200;
    if (getTimerInterval() != interval) startTimer (interval);
    if (! visible) return;
    fineNodes.refresh (proc.getAnalysisSampleRate());
    {
        auto& e = proc.getEngine();
        intLoudness.set (e.meterIntLufs.load(), e.meterIntSeconds.load(), proc.apvts.getRawParameterValue ("target")->load());
    }
    const auto oldHeadline = headline, oldStatus = statusText, oldAutoLine = autoLine;
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
    updateAutopilotReport();
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
        for (size_t i = 0; i < lo.size(); ++i)
            liveOut[i] += a * (lo[i] - liveOut[i]);
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
        || oldHeadline != headline || oldStatus != statusText || oldAutoLine != autoLine || oldCorrection != shown.correction
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
    intLoudness.accent = theme.accent; intLoudness.text = theme.text; intLoudness.dim = theme.dim; intLoudness.repaint();
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

    // Karte: Eingang lernen
    card (12.0f, 76.0f, (float) W - 24.0f, 72.0f, "EINGANG");
    g.setColour (theme.dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("STIMMLAGE", W - 166, 80, 140, 16, juce::Justification::centredLeft);
    if (headline.isNotEmpty())
    {
        g.setColour (theme.accent);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (headline, 296, autoLine.isNotEmpty() ? 88 : 94, W - 296 - 176, 22, juce::Justification::centredLeft);
    }
    g.setColour (shown.message.isNotEmpty() || shown.needsRelearnForLoudness ? theme.accent : theme.text);
    g.setFont (juce::FontOptions (12.0f));
    if (headline.isNotEmpty() && autoLine.isNotEmpty())
    {
        g.drawFittedText (statusText, 296, 112, W - 296 - 176, 15, juce::Justification::topLeft, 1, 0.85f);
        g.setColour (autoState == 2 ? theme.accent : autoState == 1 ? theme.out : theme.dim);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawFittedText (autoLine, 296, 128, W - 296 - 176, 15, juce::Justification::topLeft, 1, 0.85f);
    }
    else if (headline.isNotEmpty())
        g.drawFittedText (statusText, 296, 118, W - 296 - 176, 26, juce::Justification::topLeft, 2, 1.0f);
    else
        g.drawFittedText (statusText, 296, 98, W - 296 - 176, 44, juce::Justification::centredLeft, 3, 1.0f);

    // Karte: Klangbild
    card (12.0f, 158.0f, (float) W - 24.0f, 142.0f, "KLANGBILD");
    {
        // Legende: links jeweils eine Probe (Linie oder Flaeche), dahinter der Name in derselben Farbe
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        const float rowY = 162.0f, rowH = 16.0f;
        float x = 112.0f;
        auto legendItem = [&] (const juce::String& text, juce::Colour c, bool areaSwatch)
        {
            if (areaSwatch)
            {
                g.setColour (c.withAlpha (0.20f));
                g.fillRect (juce::Rectangle<float> (x, rowY + 4.0f, 12.0f, 8.0f));
                g.setColour (c);
                g.fillRect (juce::Rectangle<float> (x, rowY + 4.0f, 12.0f, 1.5f));
            }
            else
            {
                g.setColour (c);
                g.fillRect (juce::Rectangle<float> (x, rowY + 7.0f, 12.0f, 2.0f));
            }
            x += 16.0f;
            const float tw = g.getCurrentFont().getStringWidthFloat (text);
            g.setColour (c);
            g.drawText (text, juce::Rectangle<float> (x, rowY, tw + 2.0f, rowH), juce::Justification::centredLeft, false);
            x += tw + 14.0f;
        };
        legendItem ("REFERENZ", theme.dim, false);
        legendItem ("EINGANG (gelernt)", theme.take, false);
        legendItem (u8 ("KORREKTUR ±12 dB"), theme.accent, false);
        legendItem ("AUSGANG (live)", theme.out, true);

        // EQ-Anzeige ganz rechts: orange, wenn der EQ eingeschaltet ist, sonst dezent grau
        const bool eqOn = fineOnButton.getToggleState();
        const juce::Colour eqCol = eqOn ? theme.accent : theme.dim;
        const juce::Rectangle<float> pill (598.0f, rowY, 52.0f, rowH);
        g.setColour (eqCol.withAlpha (eqOn ? 0.22f : 0.08f));
        g.fillRoundedRectangle (pill, 4.0f);
        g.setColour (eqCol.withAlpha (eqOn ? 1.0f : 0.45f));
        g.drawRoundedRectangle (pill, 4.0f, 1.0f);
        g.setColour (eqCol);
        g.drawText (eqOn ? "EQ AN" : "EQ AUS", pill.toNearestInt(), juce::Justification::centred, false);
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
            // Ausgang: das Live-Spektrum nach dem Plugin, als Linie mit leichter Flaeche darunter
            juce::Path area, line;
            auto yv = [&] (float v) { return juce::jlimit (r.getY() + 2.0f, r.getBottom() - 2.0f, r.getCentreY() - (v - -6.0f) / 34.0f * (r.getHeight() * 0.5f - 4.0f)); };
            area.startNewSubPath (xOf (dop::kRefGridHz[0]), r.getBottom());
            for (int i = 0; i < dop::kRefGridPoints; ++i)
            {
                const float px = xOf (dop::kRefGridHz[i]), py = yv (liveOut[(size_t) i]);
                area.lineTo (px, py);
                if (i == 0) line.startNewSubPath (px, py); else line.lineTo (px, py);
            }
            area.lineTo (xOf (dop::kRefGridHz[dop::kRefGridPoints - 1]), r.getBottom());
            area.closeSubPath();
            g.setColour (theme.out.withAlpha (0.14f * liveAlpha));
            g.fillPath (area);
            g.setColour (theme.out.withAlpha (0.9f * liveAlpha));
            g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
        const juce::String m = u8 ("MOMENTAN ") + juce::String (e.meterOutLufs.load(), 1) + u8 (" LUFS   ·   GRUND ")
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
    card (12.0f, 450.0f, (float) W - 24.0f, 108.0f, "EQ");

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
    autoButton.setBounds (126, 313, 50, 18);

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
    fineResetButton.setBounds (24, 514, 70, 28);
    fineNodes.setBounds (22, 182, w - 44, 108);
    intLoudness.setBounds (580, 314, 80, 18);
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
