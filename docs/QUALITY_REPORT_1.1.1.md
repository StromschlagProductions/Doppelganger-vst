# Qualitätsbericht – Doppelganger 1.1.1

Prüfstand des überarbeiteten Audio-Codes: `c36f04f` vom 15. November 2026. Für die Veröffentlichung wurden anschließend Versionsnummer, Dokumentation und Release-Beschreibung ergänzt; der geprüfte Audio-Code bleibt unverändert.

Die Live-Spektrumanzeige konnte gleichzeitig überschriebene Audiosamples lesen und bei niedrigen Abtastraten außerhalb ihres FFT-Puffers zugreifen. Jetzt werden vollständige Frames mit eindeutigem Speichereigentum übergeben; Frequenzen ab Nyquist werden sicher behandelt. Lernaufnahme, Zustandsverwaltung und EQ-Übergänge sind robuster, und die bisher unvollständige Testeinbindung wurde durch automatische Regressionen ergänzt.

## Ausgangszustand und bestätigte Probleme

- Audio-/GUI-Data-Race in den Live-Ringpuffern und ungültiger FFT-Startindex unter anderem bei 22.050 Hz bestätigt.
- Live-Erfassung ohne Editor, wiederholte FFTs auf alten Daten und unnötige Repaints bestätigt.
- Neutrale EQ-Bänke durchliefen alle 26 Filter. Schnelle Änderungen konnten eine Überblendung abrupt abbrechen; kopierte Filterzustände passten nicht zuverlässig zu neuen Koeffizienten.
- Prepare-, Timer- und State-Arbeit waren nicht insgesamt serialisiert. Verspätete Lernblöcke konnten einer neuen Aufnahme zugerechnet werden. Geladene Profilzahlen wurden nicht auf ungültige Werte geprüft.
- CMake baute nur eines der beiden manuellen Testprogramme; CTest enthielt keine Tests. Beide Programme konnten bei fehlenden Dateien/Argumenten abstürzen.
- Große Lernberechnungen lagen bereits außerhalb des Audio-Threads. Auch die bestehende Filterbank-Übergabe verwendete dort bereits einen nicht blockierenden TryLock. Diese sinnvollen Eigenschaften bleiben erhalten.

## Änderung und Funktionserhalt

Feste SPSC-Queues übertragen Live-Frames und Lernblöcke ohne blockierenden Audio-Mutex, Heap-Aufbau oder konkurrierendes Überschreiben. Aufnahmegenerationen trennen alte und neue Lernvorgänge. Bei Lernpuffer-Überlauf wird mit einer Meldung abgebrochen und ein vorhandenes Profil erhalten. State-Laden beendet eine laufende Aufnahme; beschädigte Profile werden abgefangen, ältere Profile ohne Lautheitsfelder bleiben lesbar.

Neutrale EQ-Bänke werden umgangen. Überblendungen bleiben 30 ms lang und werden bei neuen Anforderungen kontinuierlich beendet; anschließend folgt die neueste angeforderte Bank. Null-dB-Filter sind ausdrücklich exakte Durchleitungen, auch bei Compileroptimierung mit zusammengefassten Gleitkommaoperationen.

Live-Erfassung entfällt bei geschlossenem/unsichtbarem Editor; die Anzeige verarbeitet nur neue Frames. Ohne Profil und Lernen sinkt der Prozessor-Timer auf 5 Hz. Mit Profil oder laufendem Lernen bleiben ungefähr 30 Hz erhalten, um Lernen ohne Editor und die bisherige Reaktionszeit auf Automation zu erhalten. Die DSP-Kette wird bei Stille nicht pauschal abgeschaltet, damit Zustände korrekt ausklingen.

Alle neun Parameter-IDs, Hersteller-/Plugin-Codes, Referenzkurven, Assets und die XML-State-Struktur bleiben erhalten. Die Veröffentlichungsfassung trägt die Versionsnummer 1.1.1. Kein GUI-Redesign, keine neuen Effekte und kein JUCE-Upgrade (weiterhin 8.0.9). Der Ausgangscode verwendet drei fest eingebettete Referenzkurven; Learn analysiert die aktuelle Sprecherspur.

## Tatsächlich ausgeführte Validierung

- Unveränderte manuelle Engine-/Lerntests mit synthetischem Sprachsignal, vollständiger Linux-VST3-Referenzbuild und pluginval Level 8 erfolgreich.
- Finaler Linux-VST3-Build und **8/8 CTest-Prüfungen erfolgreich**. Enthalten sind DSP/FFT, Lernen, FIFO-Parallelität, State-Rundreisen, Legacy-/beschädigte Profile, Überlauf, Editor-Lebensdauer sowie parallele Host- und Audio-Aufrufe mit zwei unabhängigen Prozessorinstanzen.
- Sample-Rates 22.050 / 44.100 / 48.000 / 96.000 / 192.000 Hz; zusätzlich 8 kHz und weitere niedrige Raten im FFT-Test. Mono/Stereo, nullgroße, kleine, wechselnde und übergroße Blöcke bis 32.769 Samples; Stille, sehr kleine Pegel und ungültige Eingangszahlen.
- Schnelle und wiederholte Filteranforderungen unterbrechen die vorhandene Überblendung im Test nicht; die anschließende stationäre Filterverstärkung wird quantitativ geprüft.
- Keine beobachteten C++-new/new[]-Allokationen innerhalb der geprüften Audio-Callbacks.
- **ASan/UBSan: 7/7 erfolgreich**, lokal und in [GitHub Actions](https://github.com/StromschlagProductions/Doppelganger-vst/actions/runs/43763107379). Sanitizer-Fehler führen zum Abbruch.
- **TSan-Queue-Stresstest erfolgreich**, lokal und in derselben CI. Zusätzlich bestand ein lokaler TSan-Prozessortest mit instrumentiertem Plugin-/Testcode und JUCE Core; die übrigen JUCE-Module waren dabei Release-Bibliotheken. Eine anfängliche Meldung bei nicht instrumentiertem JUCE-SpinLock verschwand nach Einbeziehung des Synchronisierungscodes, ohne Unterdrückungsregel oder Änderung der Sperre.
- Finaler Linux-pluginval 1.0.4, Level 8, Seed 12345: erfolgreich mit den fünf gewünschten Abtastraten und Blockgrößen 1 / 7 / 31 / 64 / 512 / 8193.
- **Finaler Windows-x64-VST3-Build, 8/8 CTest-Prüfungen und pluginval Level 8 erfolgreich**: [GitHub-Actions-Lauf für c36f04f](https://github.com/StromschlagProductions/Doppelganger-vst/actions/runs/43763107410). Dort ist das Testartefakt **Doppelganger-VST3-Windows** verfügbar. Der Release-Schritt wurde ausdrücklich übersprungen.
- Der erste CI-Durchlauf mit GCC 15 deckte winzige Rundungsreste in einer mathematisch neutralen Filterstufe auf. Die Ursache wurde mit FMA-Optimierung reproduziert und durch exakte Null-dB-Koeffizienten behoben; die strengen Tests wurden beibehalten.

## Klang und Messwerte

Der 16-sekündige synthetische 48-kHz-Vergleich über Lernen und anschließende Verarbeitung liefert auch im finalen Stand **768.000 exakt identische Ausgabesamples** zum Ausgangsstand. Das ist ein reproduzierbarer Referenzfall, keine Garantie für beliebige Sprachaufnahmen. Die gezielt reparierten Umschaltvorgänge dürfen sich in ihrer kurzen Übergangsphase unterscheiden.

Engine-Benchmark: GCC 14.2, -O3 -DNDEBUG, Linux-Cloud auf Xeon Platinum 8573C, 48 kHz stereo, 256 Samples, 1000 Aufwärmblöcke plus 10.000 Messblöcke. Fünf abwechselnde Vorher-/Nachher-Läufe; Median der jeweiligen Kennzahl:

| EQ | Mittlere Blockzeit vorher | Nachher | P99 vorher | Nachher |
|---|---:|---:|---:|---:|
| Neutral | 46,808 µs | 13,122 µs | 109,486 µs | 33,966 µs |
| Aktiv | 44,786 µs | 44,224 µs | 105,597 µs | 114,811 µs |

Damit ist der neutrale Engine-Pfad in diesem Benchmark rund 72 % schneller. Für aktive Korrektur ist keine belastbare Beschleunigung nachgewiesen. Größte beobachtete Einzelzeiten: neutral 6248,936 → 2944,974 µs, aktiv 2894,976 → 6207,487 µs. Diese schwankenden Cloud-Zeitspitzen enthalten Betriebssystem-/Scheduling-Einflüsse und belegen keine garantierte Echtzeitgrenze. GUI-, Lern- und DAW-Gesamtlast sowie Speicherverbrauch wurden nicht separat quantifiziert; dort werden nur strukturelle Verbesserungen behauptet. Benchmarkquelle: tests/benchmark_engine.cpp.

## Verbleibende Prüfung in der DAW

Ein Hörtest mit echten Sprachaufnahmen und eine Messung in der konkreten Windows-DAW waren hier nicht möglich. Bitte dort insbesondere alte Projekte/States laden, Learn bei geschlossenem Fenster ausführen, Learn/Done/Clear wiederholen, speichern und neu öffnen sowie schnelle Angleichungsänderungen und mehrere Instanzen mit kleinem Audiopuffer testen. Auf Klicks, Aussetzer, korrekte Profilwiederherstellung und flüssige Anzeige achten.

Lernen benötigt weiterhin die Host-Nachrichtenschleife. Bei längerer Blockade oder sehr schnellem Offline-Durchlauf kann der feste Lernpuffer überlaufen; das wird nun erkannt statt stillschweigend in das Profil übernommen. Die Leistung eines beliebigen Hosts ist durch diese Tests nicht garantiert.

## Git und Nachvollziehbarkeit

Branch: `codex/doppelganger-quality-improvements`, ausgehend von `main` bei `46077b0`.

- `24e4347`: sichere Datenübergabe, Lern-/State-Verwaltung und EQ-Übergänge.
- `0b5c92d`: Regressionen, CMake und CI.
- `eb90aef`: exakte Null-dB-Filter bei FMA.
- `c36f04f`: zusätzliche Übergangs- und parallele Instanztests.

[Architektur, Befunde und reproduzierbare Testbefehle](https://github.com/StromschlagProductions/Doppelganger-vst/blob/main/docs/TECHNICAL_REVIEW.md).

Der technische Stand wurde zunächst ohne Merge oder Veröffentlichung geprüft. Die anschließende Veröffentlichung von Version 1.1.1 wurde vom Eigentümer ausdrücklich freigegeben. PR-Builds erzeugen Testartefakte; die Veröffentlichung aus main erfolgt erst nach Windows-Build, Regressionstests und pluginval. Der aktuelle Veröffentlichungsstand ist unter [GitHub Releases](https://github.com/StromschlagProductions/Doppelganger-vst/releases/latest) dokumentiert. Repository-Einstellungen und Zugriffsrechte werden nicht verändert.

