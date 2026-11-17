# Prüfung der Feinabstimmung in Version 1.2

Ausgangspunkt: `d154ef8`, fünf optionale Glockenfilter nach dem Match-EQ. Prüfung und Korrekturen auf `codex/eq-review`.

## Bedienung

Bereits vorhanden: Ziehen am Knoten für Frequenz und Gain, Mausrad über dem Knoten für Q (0,3–8), Doppelklick für 0 dB. Q ist die Güte: höher bedeutet schmaleres Band. Die Zahlenregler darunter bearbeiten dieselben automatisierbaren Parameter.

Ergänzt: Umschalt verfeinert Ziehen und Mausrad. Das Band springt beim außermittigen Anklicken nicht mehr zum Mauszeiger. Ein Tooltip zeigt Bandnummer, Frequenz, Gain und Q. Scrollrichtung berücksichtigt die JUCE-Meldung zur natürlichen/umgekehrten Scrollrichtung. Host-Gesten werden beim Doppelklick und Schließen während einer Drag-Geste zuverlässig beendet; unnötige Q-Gesten bei reinem Frequenz-/Gain-Ziehen entfallen.

Dies orientiert sich an vertrauter grafischer EQ-Bedienung. Es ist kein vollständiger Pro-Q-Nachbau: fünf Glockenfilter, keine dynamischen Bänder, kein Band-Solo, kein Linear-Phase-Modus und keine zusätzlichen Filtertypen.

## Bestätigte Fehler

1. **Instabile Filter oberhalb von Nyquist.** Reproduktion mit 22.050 Hz, Bandfrequenz 20 kHz, +12 dB, Q=1 und einem 0,01-Impuls: bisher maximaler endlicher Float-Wert ca. 2,49e38, danach 21.374 nicht-endliche Samples in einer Sekunde. Mit Begrenzung auf 0,45 × Sample-Rate: Peak 0,0121426, keine nicht-endlichen Samples. Derselbe Koeffizienten-Generator wird in Audio, Anzeige und Lautheitsabschätzung verwendet. Frequenzen ab Nyquist werden nicht als realer Frequenzgang gezeichnet.
2. **Alte Filter nach Sample-Rate-Wechsel.** Gleiche Parameter führten nach Prepare nicht zur Neuberechnung der Feinfilter. Prepare invalidiert jetzt den Entwurf; Regression vergleicht wiederverwendete und frische Engine nach Rate-Wechsel samplegenau.
3. **Harte Koeffizienten-/Bypass-Umschaltung.** Neue Parameter ersetzten direkt die Filterkoeffizienten; Bypass übersprang sie sofort. Jetzt laufen zwei Bänke mit 30 ms Überblendung, der neueste zusätzliche Wunsch wird bis zum Ende der laufenden Überblendung vorgemerkt. Zustände gehören jeweils zur passenden Bank; bei dauerhaft neutralem EQ entfallen Filteroperationen weiterhin.
4. **Testlücke bei alten Projekten.** Der bisherige Legacy-Test prüfte nur eine frische Instanz. Ein zusätzlicher Test lädt ein altes Projekt in eine bereits bearbeitete Instanz. JUCE ergänzt fehlende Parameter bereits mit Defaults; dafür ist keine zusätzliche State-Migration notwendig.
5. **Anzeige fest auf 48 kHz.** Die grafische Fein-EQ-Kurve verwendet nun die tatsächliche Sample-Rate und dieselbe Begrenzung wie das Audio. Unveränderte Parameter lösen keinen zusätzlichen periodischen Repaint der Knotenkomponente mehr aus.

## Grenzen und bewusste Entscheidungen

Die Lautheitskompensation ist weiterhin eine spektrale Schätzung aus dem gelernten Profil. Sie ist keine Garantie für exakt gleiche gehörte Lautheit bei beliebiger EQ-Einstellung oder verändertem Eingangsmaterial. Die bisherige automatische Kompensation bleibt erhalten.

Die Filter bleiben hinter dem Lernabgriff: Feinabstimmung verändert nicht das zum Lernen gesammelte Signal. Parameter-IDs, XML-State-Format, Hersteller-/Plugin-Kennung und Referenzkurven bleiben erhalten. Version bleibt 1.2; diese Prüfung veröffentlicht kein neues Release.

Keine pauschale Umstellung auf andere Filtertypen oder eine Nachbildung aller Pro-Q-Funktionen. Keine Änderung an den sparsamen CI-Auslösern oder am manuell freizugebenden Release-Ablauf.

## Validierung

Zusätzlich zu den vorhandenen Tests: niedrige Sample-Rates mit Hochfrequenzbändern, endliche und begrenzte Impulsantworten, Rate-Wechsel, kontinuierliche Überblendung bei erneuter Anforderung, neutraler Bypass nach abgeschlossener Überblendung und alte Projekte in bereits editierten Instanzen. JUCE-Mausereignisse testen Q mit Mausrad, umgekehrte Scrollrichtung, Klick ohne Positionssprung und Doppelklick.

Tatsächliche abschließende Build-/Test-Ergebnisse werden im Pull Request festgehalten. Ein DAW-Hörtest mit Maus/Trackpad bleibt ergänzend sinnvoll, insbesondere schnelle Änderungen bei tiefen Frequenzen, hoher Güte und extremen Gain-Werten. Eine 30-ms-Überblendung begrenzt harte Sprünge, beweist aber für sich allein keine Unhörbarkeit aller Übergänge.
