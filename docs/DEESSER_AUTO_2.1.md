# Automatischer De-Esser 2.1: Verfahren und Prüfung

## Auftrag

Frequenz und Stärke konservativ anhand ausreichend sicher erkannter Zischlaute wählen. Bereits unauffällige Stimmen möglichst unverändert lassen. Bei unzureichendem Lernmaterial keine vermeintlich präzise Einstellung erzwingen. Die Entscheidung steht nach dem Lernen fest, danach wird die Lautheit neu nachgemessen. Keine zusätzlichen Regler.

## Verfahren

Analysiert wird die gelernte Passage. Der De-Esser sieht dabei dasselbe Signal wie im Betrieb: nach Lowcut, Angleich-EQ und EQ.

1. **Abschnitte von etwa 10 ms** (FFT 20 ms, halbe Überlappung) werden gemessen nach Pegel, Zischband (4,5–12 kHz) gegen tiefen Stimmbereich (80 Hz–3 kHz) und spektraler Flachheit im Zischband.
2. **Zischlaut-Kandidat** ist ein Abschnitt nur, wenn alle drei Bedingungen gelten:
   - Die Höhen überwiegen.
   - Das Zischband ist rauschartig (Flachheit mindestens 0,3). Obertöne heller Vokale sind harmonisch und fallen heraus.
   - Der Pegel liegt höchstens 30 dB unter dem Sprechpegel, also dem 90. Perzentil. Leise Atmer fallen heraus.
3. **Zischlaut** ist eine zusammenhängende Folge von Kandidaten mit 30–350 ms Dauer. Längeres ist Flüstern oder Dauerrauschen.
4. **Vokale** sind Abschnitte mit mindestens 6 dB mehr Energie unten als im Zischband, nahe am Sprechpegel, mindestens 50 ms von einem Zischlaut entfernt und **stimmhaft**. Stimmhaft heißt hier: normierte Autokorrelation über 40 ms im Bereich 70–400 Hz von mindestens 0,5.
   - Der Abstand von 6 dB statt 18 dB lässt natürlich helle Stimmen als Vokale gelten.
   - Die Autokorrelation erkennt Stimmklang unabhängig von Tonhöhe und Helligkeit. Ein zunächst verwendetes Flachheitsmaß im tiefen Bereich konnte bei Männerstimmen (120 Hz) die Obertöne nicht auflösen und wurde ersetzt.
5. **Ausreichend** ist das Material bei mindestens 8 Zischlauten mit zusammen mindestens 0,4 s. Höchstens 30 % der Sprache dürfen zischähnlich sein, und mindestens 10 % müssen stimmhafte Vokale sein. Sonst bleibt die Einstellung unverändert.
6. **Frequenz:** Gemitteltes, terzgeglättetes Spektrum der Zischlaute, Beginn der Hauptenergie (3 dB unter dem Maximum), auf 4–8 kHz begrenzt.
7. **Schärfe:** Pegel im Zischband der lauteren Hälfte der Zischlaute gegen den mittleren Pegel der Vokale.
   - Nach den klassischen relativen Lautstärken der Sprachlaute (Fletcher) liegt /s/ etwa 13–16 dB unter einem offenen Vokal.
   - **Bis −12 dB** gilt die Stimme als unauffällig: Stärke 0 %, und der De-Esser arbeitet bitgenau neutral.
8. **Stärke:** Die Kandidaten 5, 10, 15 … 60 % werden durch die echte Kette gerechnet. Gewählt wird die kleinste Stärke, bei der die Schärfe höchstens −12 dB beträgt. Abgebrochen wird, sobald Vokale im Zischband um mehr als 0,3 dB abgesenkt würden.

## Prüfung (synthetische, sprachähnliche Signale)

Die Signale stammen aus `tests/SpeechSynth.h`. Silben bestehen aus Obertönen mit Formanten und leichtem Vibrato. Zischlaute sind bandbegrenztes Rauschen, auf einen Effektivwert relativ zum Vokal normiert. Dazu kommen Pausen, wahlweise Atmer und Flüstern. 20 s, 25 Zischlaute, sofern nicht anders angegeben. Alle Fälle sind als Regressionstest `deesserAutopilotTests` hinterlegt.

| Fall (48 kHz) | Entscheidung | Erkannt | Frequenz | Stärke | Schärfe vorher → nachher | Vokale |
|---|---|---|---|---|---|---|
| Mann 120 Hz, S −18 dB | nicht nötig | 25 | – | 0 % | −19,1 dB | 0,00 dB |
| Mann, S −14 dB | nicht nötig | 25 | – | 0 % | −15,1 dB | 0,00 dB |
| Mann hell, S −18 / −14 dB | nicht nötig | 25 | – | 0 % | −19,0 / −15,1 dB | 0,00 dB |
| Frau 210 Hz, dunkel und hell, S −18 / −14 dB | nicht nötig | 25 | – | 0 % | −18,9 … −15,2 dB | 0,00 dB |
| Mann, S −10 dB | angewandt | 25 | 4,4 kHz | 5 % | −11,3 → −14,5 dB | 0,00 dB |
| Mann, S −8 dB | angewandt | 25 | 4,4 kHz | 5 % | −9,3 → −12,6 dB | 0,00 dB |
| Mann, S −6 dB | angewandt | 25 | 4,4 kHz | 20 % | −7,4 → −12,2 dB | 0,01 dB |
| Frau hell, S −6 dB | angewandt | 25 | 4,4 kHz | 20 % | −7,4 → −12,2 dB | 0,01 dB |
| Mann, S −3 dB | angewandt | 25 | 4,4 kHz | 50 % | −4,4 → −12,2 dB | 0,01 dB |
| Mann, S 0 dB | angewandt (Obergrenze) | 25 | 4,4 kHz | 60 % | −1,4 → −10,1 dB | 0,01 dB |
| Mann, S +6 dB | angewandt (Obergrenze) | 25 | 4,4 kHz | 60 % | +4,4 → −4,3 dB | 0,01 dB |
| S-Schwerpunkt 4,5 / 6,5 / 8,5 kHz | angewandt | 25 | 4,0 / 4,4 / 5,7 kHz | 60 % | | 0,01 dB |
| Mann, S −6 dB, mit Atmern | angewandt (wie ohne) | 25 | 4,4 kHz | 20 % | −7,4 → −12,2 dB | 0,01 dB |
| Helle Frau ohne S-Laute | unverändert: zu wenig S-Laute | 0 | | | | |
| Wenige scharfe S-Laute (2 in 20 s) | unverändert: zu wenig S-Laute | 2 | | | | |
| Nur 4 s Lernmaterial | unverändert: zu wenig S-Laute | 4 | | | | |
| Flüstern, S −12 / 0 dB | unverändert: kein Stimmklang | 25 | | | | |

Bei 44,1 und 96 kHz ergeben sich dieselben Entscheidungen; die Stärke bei S −6 dB liegt bei 15–25 %.

Weitere geprüfte Eigenschaften:
- Mehr Schärfe führt nie zu weniger Stärke.
- Helle und dunkle Stimmen mit gleichen S-Lauten bekommen dieselbe Stärke (höchstens 10 Prozentpunkte Abstand).
- Atmer werden nicht als Zischlaute gezählt und ändern die Entscheidung nicht.
- Bei Stärke 0 % ist der De-Esser bitgenau neutral.
- Ein Abbruch liefert keine Entscheidung.

**Im Prozessor** (`deesserAutopilotProcessor`):
- Nach dem Lernen werden Stärke und Frequenz gesetzt, danach wird die Lautheit mit diesen Werten auf −23,0 LUFS geprüft.
- Speichern und Laden erhalten Bericht und Einstellung.
- Unauffällige Stimmen bekommen 0 %, die Frequenz bleibt dabei unverändert.
- Bei Flüstern bleiben 25 % und 5,5 kHz.
- Mit AUTO aus findet keine Analyse statt.

## Grenzen

Alle Werte stammen aus synthetischen Signalen. Ob der Zielwert von −12 dB und die Erkennungsschwellen zu echten Sprechern passen, muss die Abnahme mit Originalmaterial zeigen. Die Messgrößen stehen dafür im Prüfbericht (Tooltip von AUTO): erkannte S-Laute, Frequenz, Stärke, Schärfe vorher und nachher sowie die Wirkung auf Vokale. Echte Zischlaute mit Stimmanteil (Z, stimmhaftes S) werden vom tiefen Bereich mitgeprägt und eher seltener als Zischlaut erkannt. Das ist konservativ: im Zweifel weniger Bearbeitung.
