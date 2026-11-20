# Änderungen

## Unveröffentlicht

- **Behoben:** Bei Stereo-Spuren rechnete die Nachmessung die Kette einkanalig mit 0,5·(L+R). Bei hohem Ziel und stark eingreifendem Spitzenschutz passte sie dann nicht zum Ausgang (z. B. Ziel −16, L=R: −16,0 gemeldet, −15,2 LUFS tatsächlich). Mitgeschnitten werden jetzt beide Kanäle getrennt; die Nachmessung rechnet Lautheit und gekoppelten Spitzenschutz wie der Stereo-Ausgang. Auch der gemeldete Spitzenschutz-Wert stimmt damit für Stereo.
- **Lernen endet nicht mehr zu früh für die Lautheit:** Automatisch aufgehört wird frühestens nach 20 statt nach etwa 13 Sekunden Sprache, und nur, wenn die letzten 6 Sekunden höchstens 1 LU vom Material davor abweichen. Vorher konnte der spätere Teil der Spur danebenliegen (105_Sheriff: −23,3 statt −23,0 LUFS; Folge mit Pegelsprung: −22,6). Jetzt −23,0 bzw. −23,1. Kurze Spuren mit weniger als 20 s Sprache: bis zum Ende abspielen und **Done** drücken.
- **Behoben:** Eine frisch gestartete Stereo-Instanz war in den ersten etwa 50 ms 3 dB zu laut (Grundverstärkung zunächst für Mono berechnet).
- **Behoben:** Wurde ein anderes Projekt geladen, während die Nachmessung noch lief, konnte deren Korrektur auf das neue Projekt wirken.
- Mono unverändert. In Stereo-Projekten aus älteren Versionen gilt die gespeicherte Nachmessung nicht mehr als „geprüft“; einmal neu lernen.

## 2.1 – 20. November 2026

- **Automatischer De-Esser (mit AUTO):** Nach dem Lernen werden Frequenz und Stärke aus sicher erkannten Zischlauten gewählt. Erkannt werden nur kurze, rauschartige Abschnitte nahe am Sprechpegel, in denen die Höhen überwiegen; helle Vokale, Atmer und Flüstern zählen nicht.
- Ziel ist eine natürliche Balance, keine feste Absenkung: Liegen die lauten S-Laute höchstens 12 dB unter den Vokalen, bleibt die Stärke bei 0 % (Stimme bitgenau unbearbeitet). Sonst die kleinste Stärke, die das erreicht, höchstens 60 %, und nur, solange Vokale um höchstens 0,3 dB berührt werden.
- Bei weniger als 8 sicheren S-Lauten (oder unter 0,4 s), Flüstern oder viel zischähnlichem Geräusch bleibt die Einstellung unverändert. Der Prüfbericht (Tooltip von AUTO) nennt Entscheidung und Grund.
- Danach misst der Autopilot die Lautheit mit den neuen Werten nach. Keine zusätzlichen Regler.
- Validierung mit hellen Stimmen, wenigen S-Lauten, Flüstern, Atmern und starken S-Lauten: [Prüfung](docs/DEESSER_AUTO_2.1.md).

[Release-Hinweise](docs/releases/2.1.md)

## 2.0 – 19. November 2026

- **Behoben:** Nach Speichern/Laden konnte eine veraltete Nachmessung als „geprüft“ gelten. Die Messung wird jetzt mit den Einstellungen gespeichert, für die sie gilt.
- **Behoben:** Nach einer Lowcut-Änderung wich die Nachmessung leicht ab (z. B. −23,0 gemeldet, −23,3 LUFS tatsächlich). Mitgeschnitten wird jetzt der Eingang vor dem Lowcut.
- **Prüfbericht:** zeigt zusätzlich den True Peak (dBTP). Der Spitzenschutz begrenzt weiterhin Sample-Spitzen auf −1 dBFS.
- Klang und Parameter unverändert. Projekte aus 1.8/1.9: einmal neu lernen, damit die Messung wieder als geprüft gilt.

[Release-Hinweise](docs/releases/2.0.md)

## 1.9 – 19. November 2026

- Die eingebaute Hilfe beschreibt den Autopiloten jetzt auch beim Weg durchs Plugin (Spitzenschutz am Ende), bei der Angleichung (mit AUTO 45–85 %, ohne AUTO 60 %) sowie bei Lautheit und EQ (Nachmessung durch die ganze Kette).
- Klang, Parameter und gespeicherte Projekte unverändert gegenüber 1.8.

[Release-Hinweise](docs/releases/1.9.md)

## 1.8 – 19. November 2026

- **Autopilot (AUTO, Standard an):** Nach dem Lernen stellt das Plugin Angleichung (45–85 %, so dass höchstens 1 dB Färbung bleibt) und Lowcut (80–110 Hz nach der Tonhöhe) selbst ein.
- **Lautheit nachgemessen:** Die gelernte Passage wird im Hintergrund durch die ganze Kette gerechnet. Die Verschiebung durch De-Esser, Leveler und EQ wird ausgeglichen (±6 dB, in Tests auf ±0,01 LU genau).
- **Spitzenschutz bei −1 dBFS** mit AUTO: latenzfrei, Kanäle gekoppelt, in der Nachmessung enthalten.
- **Prüfbericht** im EINGANG: „geprüft“ oder ein Hinweis bei Übersteuerung, Rauschen, sehr lauten Spitzen oder unerreichbarem Ziel. Alle Messwerte als Tooltip auf AUTO.
- **Leise Aufnahmen** werden zuverlässig gelernt (relative statt fester Schwellen). Die Grundverstärkung reicht bis 40 dB.
- **Leveler beginnt bei jedem Wiedergabestart neutral**, wie beim Export.
- AUTO aus = Verhalten wie 1.7.

[Release-Hinweise](docs/releases/1.8.md)

## 1.7 – 18. November 2026

- **RESET unter dem EQ-Schalter:** Setzt alle fünf EQ-Bänder auf ihre Parameter-Standardwerte zurück: 0 dB, Frequenzen 100/300/1000/3000/8000 Hz und Q = 1. Der EQ-Ein-/Aus-Zustand bleibt erhalten. Die DAW wird über die Parameteränderungen benachrichtigt; Regler und Knoten folgen direkt.
- Die helle und dunkle Oberfläche sind mit Version 1.7 neu aufgenommen. Die übrigen Einstellungen und die Signalverarbeitung bleiben erhalten; neu lernen ist nicht nötig.

[Release-Hinweise](docs/releases/1.7.md)

## 1.6 – 18. November 2026

- **Klangbild klarer beschriftet:** Legende mit Farbmustern: REFERENZ (grau), EINGANG (gelernt) (blau), KORREKTUR ±12 dB (orange), AUSGANG (live) (grün).
- **Live-Eingang entfernt:** Die zappelnde Eingangslinie entfällt; die Korrektur richtet sich ohnehin nur nach dem Gelernten. Der Ausgang ist dafür deutlich sichtbar.
- **EQ statt Feinabstimmung:** Die untere Karte heißt EQ, oben rechts im Klangbild zeigt EQ AN (orange) oder EQ AUS (grau) den Zustand.
- Die Karte zum Lernen heißt EINGANG statt STIMME. Hilfe und README sind angepasst.
- Klang, Parameter und gespeicherte Projekte unverändert gegenüber 1.5.

[Release-Hinweise](docs/releases/1.6.md)

## 1.5 – 17. November 2026

- Einfache öffentliche Versionsnummern: Plugin-Anzeige, VST3-Metadaten, Release-Tag und ZIP-Name verwenden dieselbe Nummer. Klang und Funktionen wie 1.4.1.

[Release-Hinweise](docs/releases/1.5.md)

## 1.4.1 – 17. November 2026

- Lautheitsanzeige und Leveler summieren die unabhängig K-gewichteten Kanalenergien nach BS.1770. Der bisherige Stereo-Mittelwert unterschätzte die Lautheit bei gleichem L/R-Signal um rund 3,01 LU.
- Anlernen misst die Energie vor dem Stereo-Downmix separat pro Kanal. Gegenphasige oder unterschiedlich laute Anteile verschwinden damit nicht mehr aus der Lautheitsmessung.
- Lernprofile speichern ihre Kanalzahl. Die Grundverstärkung berücksichtigt die aktuelle Mono-/Stereo-Konfiguration; Kanalzahl und Pegelanpassung bleiben auch nach Speichern/Laden und einem Layoutwechsel konsistent.
- Regressionen prüfen Mono, Dual-Mono, energiegleiches Stereo, einzelne aktive Kanäle, Gegenphase und asymmetrisches Stereo. LISTEN, De-Esser und Angleich-Bereich aus 1.4 bleiben erhalten.

[Release-Hinweise](docs/releases/1.4.1.md)

## 1.4 – 17. November 2026

Nach Rückmeldungen aus dem Praxistest: Angleich-Bereich mit Begrenzern, ein De-Esser, der nur auf Zischlaute reagiert, und eine prüfbare Lautheitsanzeige.

### Neu

- **Angleich-Bereich:** Zwei Begrenzer im Klangbild, wie man es vom Match EQ in Ozone kennt. Angeglichen wird nur dazwischen, außerhalb bleibt der Take unverändert (weicher Übergang über eine Drittel-Oktave). Ziehen am Griff unten oder an der Linie, Umschalt = fein, Doppelklick = Standard. Neue Parameter „Angleich ab“ (20 Hz – 2 kHz) und „Angleich bis“ (1 – 20 kHz), automatisierbar.
- **Standard oben 12 kHz:** Die Referenzen sind oberhalb davon nicht mehr verlässlich. Bisher zog die Korrektur dort die Höhen des Takes mit herunter; jetzt bleiben sie, wie sie sind.
- **Gemessene Lautheit:** In der Karte LAUTHEIT steht die Lautheit des Ausgangs, integriert nach EBU R128 (Pausen ausgeblendet). Sie beginnt bei jedem Wiedergabestart neu, Klick setzt sie zurück, orange = innerhalb ±1 LU um das Ziel. Der Momentanwert oben im Klangbild heißt jetzt MOMENTAN.

### Verbessert

- **De-Esser:** Die Erkennung vergleicht jetzt das Zischband (steiler Hochpass ab der eingestellten Frequenz, oben bei 12 kHz begrenzt) mit dem tiefen Stimmbereich statt mit dem Gesamtsignal und reagiert schneller. Gemessen an drei Sprachaufnahmen: Zischlaute bei Standardstärke 2,7–4,4 dB abgesenkt (vorher 0,2–0,5 dB), bei voller Stärke 6,3–9,6 dB (vorher 2,4–3,2 dB). Stimmhafte Abschnitte werden bei voller Stärke nur noch in 0,8–2,1 % der Zeit um mehr als 1 dB abgesenkt (vorher 9–15 %). Einzelheiten in [docs/DEESSER_1.4.md](docs/DEESSER_1.4.md).

### Kompatibilität

Bestehende Projekte öffnen mit dem neuen Angleich-Bereich (unten keine Grenze, oben 12 kHz) und dem neuen De-Esser. Oberhalb von 12 kHz und bei den S-Lauten klingen sie deshalb etwas anders als mit 1.3. Wer den alten Höhenverlauf möchte, zieht den rechten Begrenzer ganz nach rechts. Alle bisherigen Parameter-IDs bleiben erhalten.

[Release-Hinweise](docs/releases/1.4.md)

## 1.3 – 17. November 2026

- LISTEN spielt die tatsächlich entfernten De-Esser-Anteile ab (Differenz vor/nach Absenkung). Aktiv zeigt der Knopf DIFF.
- Monitorwechsel über 10 ms geglättet; Grundverstärkung und Leveler wirken identisch auf Normal- und Differenzpfad.
- Die De-Esser-Freigabe endet bei exakter Neutralstellung, sodass ohne Absenkung kein Rest durch die Differenzüberwachung hörbar bleibt.
- Regressionen bei fünf Sample-Rates prüfen Signalrekonstruktion und Stille ohne Absenkung.


## 1.2 – 17. November 2026

Neue optionale **Feinabstimmung**: fünf frei einstellbare Glockenfilter nach dem Angleich. Das angeglichene Ergebnis lässt sich damit direkt im Plugin nachschärfen, ohne zusätzlichen EQ in der Slotkette. Anregung aus dem Beta-Test von Markus Hildebrandt (SOUNDJOB).

### Neu

- Fünf Bänder mit Frequenz (40 Hz bis 20 kHz), Verstärkung (±12 dB) und Güte (0,3 bis 8). Standard: 100 Hz, 300 Hz, 1 kHz, 3 kHz und 8 kHz, alle auf 0 dB, Feinabstimmung aus.
- Bedienung direkt im Klangbild: Punkt waagerecht ziehen ändert die Frequenz, senkrecht die Verstärkung. Das Mausrad ändert die Güte, Doppelklick setzt das Band auf 0 dB. Ziehen schaltet die Feinabstimmung ein. Darunter dieselben Werte als Regler mit Zahleneingabe.
- Die Grundverstärkung rechnet die Feinabstimmung mit ein: Die Spur bleibt auf der Ziel-Lautheit, der EQ ändert nur die Klangfarbe. Auf echten Sprachaufnahmen blieb der Ausgang dabei meist auf 0,1 dB genau, im ungünstigsten gemessenen Fall (±6 dB im Grundtonbereich einer Frauenstimme) auf 0,5 dB.
- Alle 16 neuen Werte sind automatisierbar und werden mit dem Projekt gespeichert.

- Bedienung im Klangbild: Umschalt beim Ziehen oder Mausrad stellt feiner ein, ein Tooltip zeigt Frequenz, Pegel und Güte des Bands. Ein Band springt beim Anklicken neben dem Punkt nicht mehr zum Mauszeiger, die Scrollrichtung des Systems wird beachtet.
- Hilfe und README beschreiben ausführlicher, was das Plugin macht und wofür es gedacht ist: Angleichen an neutrale Sprecherprofile aus einem Berliner Profi-Studio.

### Behoben

- Bei niedrigen Abtastraten (z. B. 22.050 Hz) konnte ein Feinabstimmungsband nahe 20 kHz instabil werden. Die Bandfrequenz ist jetzt auf 45 % der Abtastrate begrenzt, Audio, Anzeige und Lautheitsrechnung nutzen denselben Filterentwurf.
- Nach einem Wechsel der Abtastrate rechnet die Feinabstimmung ihre Filter neu, auch wenn die Regler unverändert sind.
- Änderungen an der Feinabstimmung und das Ein-/Ausschalten werden über 30 ms überblendet, statt hart umzuschalten.
- Die Anzeige der Feinabstimmung rechnet mit der tatsächlichen Abtastrate statt fest mit 48 kHz.
- Die De-Esser-Anzeige zeigte im Ruhezustand ein fehlerhaftes Zeichen statt „S: –“.

### Kompatibilität

Die bisherigen neun Parameter-IDs, die Plugin-Kennung und das gespeicherte Format bleiben erhalten. Projekte aus älteren Versionen öffnen mit ausgeschalteter, flacher Feinabstimmung und klingen unverändert; ausgeschaltet oder mit allen Bändern auf 0 dB ist der Ausgang bitgleich mit 1.1.1. Das Fenster ist um die neue Karte höher geworden (Grundgröße 680 × 566 statt 680 × 450).

[Release-Hinweise](docs/releases/1.2.md) · [Prüfbericht Feinabstimmung](docs/EQ_REVIEW_1.2.md)

## 1.1.1 – 15. November 2026

Technische Qualitätsüberarbeitung ohne Änderung der grundlegenden Klangzielsetzung oder Bedienoberfläche.

### Behoben

- Konkurrierende Zugriffe zwischen Audio-Thread und Live-Spektrumanzeige sowie mögliche FFT-Zugriffe außerhalb des Puffers bei niedrigen Abtastraten.
- Vermischung verspäteter Samples zwischen Lernaufnahmen; unerkannte Lernpuffer-Überläufe; unzureichend geprüfte Profilwerte und konkurrierende State-/Prepare-/Timer-Arbeit.
- Abrupt unterbrochene EQ-Überblendungen, unpassende übernommene Filterzustände und Rundungsreste in neutralen Filtern.
- Abstürze der manuellen Testprogramme bei fehlenden Argumenten oder Dateien.

### Verbessert

- Neutrale EQ-Bänke werden als Durchleitung behandelt; redundante DSP-Berechnungen entfallen.
- Keine Live-Erfassung bei unsichtbarem Editor, keine FFTs auf unveränderten Daten und weniger unnötige Repaints. Reduzierter Prozessor-Timer bei Instanzen ohne Profil und laufendes Lernen.
- Beide vorhandenen Testprogramme sind in CMake/CTest eingebunden. Neue DSP-, FFT-, Lern-, State-, Übergangs- und Nebenläufigkeitstests laufen in CI; ASan, UBSan und TSan ergänzen die Prüfung.
- Windows-VST3-Build und pluginval Level 8 bleiben erhalten. Releases enthalten versionsbezogene Hinweise und Installationsschritte.

### Kompatibilität

Plugin-/Herstellerkennungen, Parameter-IDs, Referenzkurven und XML-State-Struktur bleiben erhalten. JUCE bleibt auf 8.0.9. Kein GUI-Redesign und keine neuen Audioeffekte.

[Release-Hinweise](docs/releases/1.1.1.md) · [Messwerte und vollständiger Qualitätsbericht](docs/QUALITY_REPORT_1.1.1.md) · [Architektur und Testbefehle](docs/TECHNICAL_REVIEW.md)

## 1.1

Bisheriger veröffentlichter Stand: De-Esser-Anzeige mit Verlauf und automatisches Lernende bei stabilem Klangbild. Die zugehörigen Downloads bleiben in den älteren GitHub-Releases erhalten.
