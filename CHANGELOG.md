# Änderungen

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
