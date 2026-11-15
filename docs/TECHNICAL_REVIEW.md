# Technische Überarbeitung – Ausgangsbefunde und Architektur

Ausgangspunkt: `main` bei `46077b0d7c0c739cc2cfaec4b1ee4a27382d39d3` (Version 1.1).
JUCE bleibt auf 8.0.9; Hersteller-/Plugin-Codes, Parameter-IDs und XML-Struktur bleiben erhalten.

## Bestätigte Befunde

- `livePos` veröffentlichte lediglich eine Position. Die Float-Ringpuffer wurden während des GUI-Lesens weiterhin überschrieben: C++-Data-Race, auch ohne einen sichtbaren Absturz.
- Beim 20-kHz-Anzeigepunkt und 22.050 Hz konnte `k0` bereits größer als 2048 sein. Die Ersatzgewichtung mit einem Element las anschließend hinter dem FFT-Leistungspuffer.
- Live-Samples wurden auch ohne Editor erfasst; bei gestopptem Transport analysierte der Editor denselben alten Frame immer wieder.
- Der Prozessor pollte stets mit 30 Hz. Dieser Timer verarbeitet tatsächlich Lernen, Konvergenz und EQ-Neuberechnung. Ihn ersatzlos zu entfernen wäre falsch.
- Beide EQ-Bänke verarbeiteten bei Überblendungen auch vollständig neutrale Filter. Neue EQ-Anforderungen konnten laufende Überblendungen abrupt abbrechen. Das Kopieren von TDF-II-Zuständen zwischen unterschiedlichen Koeffizienten konnte insbesondere in einer neutralen Zielbank einen falschen Restimpuls erzeugen.
- `prepareToPlay`, Timer und State-Callbacks waren nicht insgesamt serialisiert. `currentFs`, Konvergenz-/Designzustand und Lernwechsel hatten zusätzliche Nebenläufigkeitsrisiken. Wiederholtes Lernen konnte verspätet geschriebene Samples der vorherigen Aufnahme übernehmen.
- Gespeicherte Profilzahlen wurden ohne Prüfung auf NaN, Inf oder plausible Werte akzeptiert; State-Laden beendete eine laufende Lernaufnahme nicht.
- CMake baute nur `test_engine`; CTest hatte **keinen** registrierten Test. `test_learn` fehlte. Beide vorhandenen Programme waren manuelle Dateiverarbeiter, keine automatischen Regressionstests. Fehlende Argumente/Dateien verursachten Abstürze.

## Erhaltener Signal- und Datenfluss

Audio: Eingang → Lowcut → Lernabgriff → Match-EQ mit 30-ms-Überblendung → De-Esser → Grundverstärkung → Leveler → Ausgang. Mono und Stereo bleiben latenzfrei. Referenzen sind die drei unveränderten eingebetteten Zielkurven; eine frei aufnehmbare Referenzstimme war im Ausgangscode nicht implementiert. Learn analysiert die aktuelle Sprecherspur.

- Audio-Thread: DSP, feste Lernblöcke, bei angeforderter Anzeige feste Spektralframes, atomare Meter. Kein Heap-Aufbau, kein blockierender Mutex, keine FFT oder Profiloptimierung.
- Message-Thread: Lernanalyse, Konvergenzprüfung, EQ-Entwurf, UI/FFT. Lernen benötigt weiterhin die Host-Nachrichtenschleife, aber keinen Editor.
- Host-Threads: Prepare, State-Lesen und State-Laden. Eine gemeinsame rekursive `stateLock` serialisiert diese mit den nicht-echtzeitkritischen Prozessoraufgaben. Der Audio-Callback erwirbt diese Sperre nie. Der Host muss wie üblich Prepare/Reset gegen den Audio-Callback serialisieren.
- Filterübergabe: kurze bestehende SpinLock für den Produzenten, ausschließlich **ein TryLock-Versuch** im Audio-Callback; bei belegter Sperre wird die vorige Bank weiterverwendet.

## Eigentum und C++-Speichermodell

`AudioTransport.h` enthält eine feste Single-Producer-/Single-Consumer-Queue. Release-Publikation überträgt einen fertig geschriebenen Slot an den Consumer. Dessen Acquire-Lesen sieht die vollständigen Daten. Erst nach abgeschlossener Kopie gibt der Consumer den Slot per Release zurück; der Producer prüft mit Acquire. Deshalb wird kein Slot gleichzeitig gelesen und geschrieben. Es gibt weder konkurrierendes Zurücksetzen der Indizes noch Überschreiben belegter Slots, kein CAS-Retry und keine Allokation. Die Index- und Generationsatomics müssen zur Übersetzungszeit lockfrei sein.

Live-Frames enthalten Ein-/Ausgang, Sample-Rate, Ringposition und Generationsnummer. Ein privater Audio-Ring erzeugt nach dem ersten vollständigen Fenster alle 1024 Samples einen 4096er-Frame. Die GUI übernimmt höchstens drei bereitstehende Frames und analysiert nur den neuesten davon. Bei voller Queue entfallen neue Anzeigeframes. Öffnen, Schließen und Prepare ändern die Generation; veraltete Frames werden verworfen. Ohne sichtbaren Editor entfällt die Erfassung. Die GUI-FFT arbeitet ausschließlich mit ihrem eigenen Snapshot und stellt Frequenzen ab Nyquist am unteren Anzeigerand dar.

Lernblöcke tragen die Aufnahmegeneration. Stop/Neustart/State-Laden invalidiert alte Blöcke, ohne eine laufende Queue zurückzusetzen. Bei Überlauf bleibt ein vorheriges Profil erhalten; eine verständliche Meldung verlangt erneutes Lernen. Bei manuellem Done kann höchstens der gerade in Arbeit befindliche Audioblock fehlen; er wird niemals einer neuen Aufnahme zugeordnet.

## Konservative Optimierungen

- Exakt neutrale EQ-Bänke sind Durchleitungen. Neue Zielbanken beginnen mit gelöschten, zu ihren Koeffizienten passenden Zuständen. Die alte Bank klingt während 30 ms weiter aus.
- Während einer Überblendung wird nur die neueste zusätzliche Bank vorgemerkt. Es gibt keinen abrupten Sprung auf eine halb eingeblendete Bank. Reset übernimmt die neueste gewünschte Korrektur.
- Grundverstärkungsfaktor wird nur beim vorhandenen 16-Sample-Steuertakt neu berechnet. Zeitkonstanten werden beim Prepare vorbereitet. De-Esser-Peak-dB werden einmal pro Block statt pro Sample umgerechnet.
- Ohne Profil und ohne Lernen arbeitet der Prozessor mit 5 Hz. Mit Profil oder Lernen bleiben etwa 30 Hz erhalten, damit die vorhandene Reaktionszeit von EQ-Automation nicht verschlechtert wird. Ein vollständig ereignisgesteuerter Ersatz wurde bewusst nicht eingeführt: JUCE-AsyncUpdater aus dem Audio-Thread kann OS-Nachrichten/Allokationen auslösen.
- Unsichtbare Editoren überspringen Analyse/Zeichnen und prüfen Sichtbarkeit mit 5 Hz; sichtbare mit 25 Hz. Repaints entfallen, wenn Kurven, Texte, gerundete Meter und Animationen gleich bleiben.
- Keine aggressive Stilleschaltung der gesamten DSP-Kette: Filter- und Hüllkurvenzustände müssen korrekt ausklingen. Keine Dependency-Upgrades oder GUI-Neugestaltung.

## Reproduzierbare Prüfungen

```sh
cmake -S . -B build/tests -DDOPPELGANGER_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests --parallel 2
ctest --test-dir build/tests --output-on-failure

cmake -S . -B build/asan -DDOPPELGANGER_TESTS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDOPPELGANGER_SANITIZER=address,undefined
cmake --build build/asan --parallel 2
ctest --test-dir build/asan --output-on-failure

cmake -S . -B build/tsan -DDOPPELGANGER_TESTS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDOPPELGANGER_SANITIZER=thread
cmake --build build/tsan --target test_transport --parallel 2
ctest --test-dir build/tsan -R concurrent_transport --output-on-failure

cmake -S . -B build/plugin -DDOPPELGANGER_PLUGIN_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/plugin --config Release --parallel 2
ctest --test-dir build/plugin -C Release --output-on-failure
```

Linux-Prozessortests mit Editor benötigen einen X-Server, z. B. `xvfb-run`. Windows benötigt keinen X-Server. Die Tests verwenden explizite Prüfungen, die auch mit `NDEBUG` aktiv sind. Fehlende Dateien/Argumente müssen kontrolliert einen Fehlercode liefern; CTest darf einen Absturz nicht als erwarteten Fehler akzeptieren.

`benchmark_engine` misst 10.000 Stereo-Blöcke à 256 Samples bei 48 kHz nach 1000 Aufwärmblöcken, getrennt für neutrale und aktive Korrektur. Durchschnitt, P99 und größter beobachteter Wert sind Mikrosekunden pro Block. Für den Vorhervergleich dieselbe Benchmarkquelle mit dem Ausgangsheader bauen. Dies sind Engine-Messungen, keine DAW-Gesamtlast oder garantierten Echtzeitgrenzen.

Tatsächlich ausgeführte Prüfungen und Messwerte werden im Abschlussbericht dokumentiert.
