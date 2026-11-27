# Prüfung der ursprünglichen Referenzaufnahmen

27. November 2026. Ergänzung zur [Lernstabilitätsprüfung](LEARNING_STABILITY_AUDIT_2.3.md), nachdem die drei Referenz-WAVs nachgereicht wurden. Geprüfter Plugin-Code: `a130fd4e803fa097d7a72b34d4a25737f5e798ac`. Keine Änderung der Zielkurven oder der Klangverarbeitung. Die Audiodateien werden nicht im Repository abgelegt.

## Die Dateien stimmen mit den eingebauten Zielen überein

Alle drei Dateien sind 48-kHz-Mono-WAVs mit 24-Bit-PCM. Der vorhandene Generator erzeugt daraus **alle 128 gespeicherten Werte je Referenz exakt auf die verwendeten drei Nachkommastellen**. Die maximale Differenz vor Rundung ist jeweils kleiner als 0,0005 dB. Es liegt keine Verwechslung oder abweichende Referenzversion vor.

| Datei | Dauer | erkannte Sprache nach Default-Lowcut | Median-Grundfrequenz | AUTO-Register der vollständigen Datei |
| --- | ---: | ---: | ---: | --- |
| referenz_tief.wav | 13,27 s | 12,65 s | 103,67 Hz | Tief |
| referenz_mittel.wav | 8,77 s | 8,30 s | 210,53 Hz | Mittel |
| referenz_hoch.wav | 7,55 s | 7,32 s | 242,42 Hz | Hoch |

Die Samplespitzen liegen zwischen −1,54 und −1,75 dBFS, die vierfach oversampleten Spitzen zwischen −1,50 und −1,74 dBTP. Es gibt keine Samples mit Betrag ≥0,999. Daraus lässt sich keine Aussage über eine mögliche frühere Dynamikbearbeitung ableiten.

Der vorhandene Bandbreitendetektor liefert bei allen Dateien etwa **16,02 kHz**. Das entspricht der im Plugin gespeicherten Grenze. Eine 48-kHz-Datei kann eine geringere Nutzbandbreite enthalten; die Messung allein identifiziert keinen bestimmten Codec oder Bearbeitungsschritt.

## Das Matching erkennt die vollständigen Referenzen wieder

Die ganze Datei wurde mit dem unveränderten Learner ausgewertet und anschließend mit ihrem zugehörigen Zielregister verglichen. Der tatsächlich gefittete Match-EQ ist über 100–8000 Hz praktisch neutral: mittlerer Betrag 0,035 dB (Tief), 0,000 dB (Mittel) und 0,014 dB (Hoch). Die größte lokale Abweichung beträgt 0,35 dB bei etwa 101 Hz in Tief, nahe dem Default-Lowcut.

Das bestätigt die Konsistenz von Referenzerzeugung und Matching für diese Dateien. Es bewertet weder den subjektiven Klang noch die Eignung derselben Zielkurve für andere Stimmen. Gemessen wird der Match-EQ, nicht der Frequenzgang der gesamten Kette einschließlich Lowcut und Dynamik.

## Die Referenzen sind kurze Stichproben

Alle drei Dateien sind kürzer als die normale AUTO-Lernstrecke mit ungefähr 20–30 Sekunden erkannter Sprache. Beim einmaligen Abspielen erreicht deshalb keine Datei den regulären automatischen Lernabschluss. Die gespeicherte Referenz wird vom separaten Generator erstellt; dessen Erzeugung setzt diesen AUTO-Abschluss nicht voraus.

Für die Textabhängigkeit wurden fünf Sekunden lange Ausschnitte im Abstand von einer Sekunde ausgewertet, zusätzlich jeweils die letzten fünf Sekunden. Diese Fenster überlappen und sind keine unabhängigen Aufnahmen. Bei festgehaltenem Zielregister unterscheiden sich ihre gefitteten EQs von der ganzen Datei im Median der mittleren absoluten Frequenzabstände um 0,62 dB (Tief), 0,64 dB (Mittel) und 0,39 dB (Hoch), ausgewertet über 100–8000 Hz. Einzelne Fenster weichen stärker ab.

Die Referenzkurven enthalten somit auch die individuelle Stimme, Artikulation und Lautverteilung des jeweiligen kurzen Textes. Längere, abwechslungsreiche Aufnahmen **derselben** Referenzstimme würden erlauben, die Repräsentativität zu prüfen. Einfaches Wiederholen der vorhandenen Datei liefert keine neue Lautverteilung. Diese Messungen allein zeigen nicht, welche längere Zielkurve richtiger wäre; deshalb wurden die vorhandenen Ziele beibehalten.

## Registerwechsel jetzt mit echter Sprache nachgewiesen

Bei `referenz_mittel.wav` ergibt die gesamte Datei 210,53 Hz und damit Mittel. Die letzten fünf Sekunden, **3,76954–8,76954 s**, ergeben dagegen 241,21 Hz und damit Hoch. Der Ausschnitt enthält 4,63 Sekunden erkannte Sprache und überschreitet somit die vorhandene Mindestdauer von vier Sekunden für manuelles Beenden.

Der tatsächliche JUCE-Processor bestätigt diese Zuordnung. Damit ist die im ersten Bericht nur als synthetischer Randfall behandelte Empfindlichkeit nun an einem echten Sprachbeispiel reproduziert. Bei Tief und Hoch bleiben die getesteten Fünf-Sekunden-Fenster im ursprünglichen Register.

Der Registerwechsel hat im konkreten Fünf-Sekunden-Beispiel eine relevante Folge: Mit festem Ziel Mittel beträgt die vorgeschlagene Anpassungsstärke 45 %, mit AUTO-Ziel Hoch 75 %. Die resultierenden Match-EQ-Antworten unterscheiden sich im Mittel um **2,68 dB**, lokal bis **8,61 dB bei etwa 157 Hz**. Diese Zahl umfasst die veränderte Referenz **und** die dadurch veränderte Autopilot-Stärke; sie isoliert keinen reinen Referenzwechsel bei gleicher Stärke. Mit festem Register Mittel unterscheidet sich derselbe Ausschnitt von der ganzen Mittel-Datei nur um durchschnittlich 0,56 dB, lokal maximal 1,19 dB.

Dies ist ein nachgewiesener Effekt einer **kurz und manuell beendeten** Lernphase. Ein Registerwechsel nach einem regulären AUTO-Abschluss mit 20–30 Sekunden verschiedener Sprache ist damit nicht nachgewiesen. Die vorhandenen kurzen Dateien können diese Frage nicht beantworten.

## Empfehlung

Die eingebauten Kurven sind korrekt aus den gelieferten Dateien erzeugt. Die bisherige Abstimmung kann beibehalten werden. Die Quellen eignen sich als konkrete klangliche Vorlagen; drei einzelne kurze Stimmen begründen keinen allgemein gültigen Sollklang für alle Hörspielstimmen.

Für dieselbe Sprecher-/Aufnahmesituation möglichst einmal aus ausreichend abwechslungsreicher Sprache lernen und das Profil wiederverwenden. Falls kurze Ausschnitte separat gelernt werden müssen, kann eine feste Stimmlage den nachgewiesenen Registerwechsel vermeiden. Die Wahl orientiert sich am repräsentativen Material der jeweiligen Stimme. Die Zuordnung der drei Referenzdateien ist keine automatische Vorgabe für andere Sprecher.

Für eine technische Verbesserung der automatischen Registerwahl sollte als nächstes längeres Material derselben Mittel-Sprecherin und weiterer Stimmen nahe 160/225 Hz geprüft werden. Eine verlässlichere Zuordnung könnte mit einem gespeicherten Sprecherregister oder einer ausgewiesenen Unsicherheit arbeiten. Ein pauschales Verschieben der Grenze oder Ersetzen aller Zielkurven ist aus diesem Einzelbeispiel nicht ableitbar.

Für den Nachweis, wie gut die Referenzen die eigenen Aufnahmen angleichen, fehlen weiterhin mehrere eigene Takes pro Sprecher und Aufnahmequelle. Diese Prüfung verifiziert die Referenzen selbst und einen konkreten Auswahl-Randfall.

## Messdaten und Wiederholung

[reference-recordings-summary.json](learning-stability/reference-recordings-summary.json) enthält Dateiprüfsummen, technische Messwerte, sämtliche Fünf-Sekunden-Vergleiche und vier Kontrollen mit dem echten Processor. Dort gilt: Register 0 = Tief, 1 = Mittel, 2 = Hoch. Profile, Pitch, Register und ausgewertete Laufzeit stimmen in allen vier Kontrollen mit dem Kernprüfer überein; Korrekturabweichungen bleiben unter 0,00000002 dB.

Das Werkzeug [check_references.py](../tools/learning-stability/check_references.py) verwendet lokale WAV-Dateien, erzeugt PCM nur im gewählten Arbeitsordner und ruft den vorhandenen Kernprüfer auf. Es lädt nichts hoch und schreibt `Source/ReferenceCurves.h` nicht neu. Aufbau von Kernprüfer und optionalem Processor-Prüfer siehe [Werkzeuganleitung](../tools/learning-stability/README.md). Zusätzlich benötigt Python das Paket SoundFile.

```sh
python3 tools/learning-stability/check_references.py work/reference-audit --tief /pfad/referenz_tief.wav --mittel /pfad/referenz_mittel.wav --hoch /pfad/referenz_hoch.wav --analyser work/stability/analyse
```

Mit `--processor /pfad/stability_processor` werden die drei vollständigen Dateien und das letzte Fünf-Sekunden-Fenster von Mittel zusätzlich durch den echten Processor geprüft. Die Summary enthält ausschließlich Messwerte und Prüfsummen, keine Audiodaten oder Transkripte.
