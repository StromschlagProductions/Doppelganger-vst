# Änderungen

## 1.2 – 17. November 2026

Neue optionale **Feinabstimmung**: fünf frei einstellbare Glockenfilter nach dem Angleich. Das angeglichene Ergebnis lässt sich damit direkt im Plugin nachschärfen, ohne zusätzlichen EQ in der Slotkette. Anregung aus dem Beta-Test von Markus Hildebrandt (SOUNDJOB).

### Neu

- Fünf Bänder mit Frequenz (40 Hz bis 20 kHz), Verstärkung (±12 dB) und Güte (0,3 bis 8). Standard: 100 Hz, 300 Hz, 1 kHz, 3 kHz und 8 kHz, alle auf 0 dB, Feinabstimmung aus.
- Bedienung direkt im Klangbild: Punkt waagerecht ziehen ändert die Frequenz, senkrecht die Verstärkung. Das Mausrad ändert die Güte, Doppelklick setzt das Band auf 0 dB. Ziehen schaltet die Feinabstimmung ein. Darunter dieselben Werte als Regler mit Zahleneingabe.
- Die Grundverstärkung rechnet die Feinabstimmung mit ein: Die Spur bleibt auf der Ziel-Lautheit, der EQ ändert nur die Klangfarbe. Auf echten Sprachaufnahmen blieb der Ausgang dabei meist auf 0,1 dB genau, im ungünstigsten gemessenen Fall (±6 dB im Grundtonbereich einer Frauenstimme) auf 0,5 dB.
- Alle 16 neuen Werte sind automatisierbar und werden mit dem Projekt gespeichert.

### Behoben

- Die De-Esser-Anzeige zeigte im Ruhezustand ein fehlerhaftes Zeichen statt „S: –“.

### Kompatibilität

Die bisherigen neun Parameter-IDs, die Plugin-Kennung und das gespeicherte Format bleiben erhalten. Projekte aus älteren Versionen öffnen mit ausgeschalteter, flacher Feinabstimmung und klingen unverändert; ausgeschaltet oder mit allen Bändern auf 0 dB ist der Ausgang bitgleich mit 1.1.1. Das Fenster ist um die neue Karte höher geworden (Grundgröße 680 × 566 statt 680 × 450).

[Release-Hinweise](docs/releases/1.2.md)

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
