# Mono-/Stereo-Lautheit – Prüfung für 1.4.1

## Ursache

Für L/R gilt nach BS.1770 `P = P_L + P_R`. Der bisherige Code verwendete `(P_L + P_R) / 2`. Das betrifft Momentanwert, integrierte Anzeige und die Messung, auf der der Leveler beruht. Bei Dual-Mono beträgt die Differenz `10 log10(2) = 3,0103 LU`. Ein energiegleich auf beide Kanäle verteiltes Signal ist dagegen so laut wie das Mono-Signal.

Die bisherige Lern-Lautheit wurde aus dem arithmetischen Stereo-Downmix gemessen. Das ist bei unterschiedlichen oder teilweise gegenphasigen Signalen zusätzlich falsch: Das Quadrat einer Summe enthält Kreuzterme. Für die Lautheit müssen die Kanäle unabhängig K-gewichtet und ihre Leistungen addiert werden.

## Umsetzung

- Ausgangs-/Eingangsmessung und Leveler verwenden die Summenleistung. De-Esser-Erkennung und Spektralanalyse behalten ihre jeweiligen Aufgaben.
- Beim Lernen werden die K-gewichteten Kanalenergien nach dem Lowcut in einem zusätzlichen festen Puffer übertragen. Kein Lock, keine Speicherreservierung im Audio-Callback. Neue Lernsitzungen setzen die zugehörigen Filterzustände im Audio-Thread zurück.
- Das Lernprofil enthält die gemessene Gesamtlautheit und `loudnessChannels`. Intern wird daraus eine Leistung pro Kanal für die EQ-Schätzung abgeleitet; für die Grundverstärkung wird die aktuelle Kanalzahl wieder berücksichtigt. Damit wird der Stereo-Faktor weder vergessen noch doppelt verrechnet.
- Layoutwechsel beginnen eine neue integrierte Messung. Wie bei verändertem Routing/Eingangspegel sollte danach neu gelernt werden: Der Host kann beim Formatwechsel zusätzlich einen eigenen Pan-Law-/Downmix-Gain anwenden.
- Alte Profile ohne Kanalfeld werden als bisherige Mono-Äquivalent-Messung gelesen. Für korrekte neue Lernwerte erneut lernen; ihre frühere Stereo-Phaseninformation lässt sich aus dem alten Profil nicht rekonstruieren.

## Automatische Regressionen

Fünf Sample-Rates (22.050, 44.100, 48.000, 96.000, 192.000 Hz): Mono, gleiche/gegenphasige L/R-Signale, energiegleiches Stereo, links oder rechts allein und asymmetrische Pegel. Geprüft werden Momentan- und integrierte Messung sowie die Pegelanpassung mit Leveler aus/an. Die integrierte Histogramm-Anzeige hat eine Auflösung von 0,05 LU; ihre Vergleiche berücksichtigen diese Quantisierung. Der Momentanwert muss den 3,0103-LU-Unterschied auf 0,001 LU treffen.

JUCE-Prozessortests prüfen echte Mono-/Stereo-Busse, unabhängige Lern-Lautheit bei teilweise gegenphasigem Stereo, Kanalmetadaten, State-Rundreise und Laden eines Stereo-Profils in einer Mono-Instanz. Die bestehende Allokationsüberwachung bleibt auch beim Lernen aktiv.

## Unabhängige Referenzmessung

FFmpeg 7.1.5, Filter `ebur128`, synthetisches Sprachsignal mit acht Sekunden Signal und zwei Sekunden Pause, 48 kHz. Sieben Kanalvarianten jeweils unbearbeitet und mit gelerntem Grundpegel auf −23 LUFS: 14 exportierte Audiodateien. Der Grundpegel wird hier aus einem Mono-Lernprofil und den bekannten Kanalverstärkungen abgeleitet; die separaten Prozessortests prüfen das tatsächliche Stereo-Anlernen. Der Leveler ist aus, um die Grundverstärkung separat zu prüfen.

| Signal | Plugin unbearbeitet | FFmpeg unbearbeitet | FFmpeg nach Pegelanpassung |
|---|---:|---:|---:|
| Mono | −24,202 | −24,2 | −23,1 |
| Dual-Mono | −21,189 | −21,2 | −23,1 |
| Energiegleiches Stereo | −24,202 | −24,2 | −23,1 |
| Nur links | −24,202 | −24,2 | −23,1 |
| Nur rechts | −24,202 | −24,2 | −23,1 |
| Gegenphasiges Dual-Mono | −21,189 | −21,2 | −23,1 |
| Links 1, rechts −0,5 | −23,233 | −23,2 | −23,1 |

Alle Werte in LUFS. Die größte Differenz zur auf 0,1 LU gerundeten FFmpeg-Anzeige beträgt 0,041 LU. Alle normalisierten Varianten erreichen laut FFmpeg −23,1 LUFS. Das ist eine Prüfung mit synthetischem Material, kein Hörtest in Studio One. Bei wechselnder Sprache beeinflussen EQ, De-Esser und Leveler den tatsächlichen integrierten Endwert; das Ziel ist keine harte nachgeschaltete Normalisierung.

Reproduzieren (FFmpeg im Suchpfad, DSP-Test-Build bereits konfiguriert):

```sh
cmake --build build/tests --target loudness_reference
mkdir -p build/loudness-reference
./build/tests/loudness_reference build/loudness-reference > build/loudness-reference/measurements.tsv
python3 tests/check_loudness_reference.py build/loudness-reference
```

Der Prüfer verlangt alle 14 Fälle, höchstens 0,12 LU Messdifferenz und höchstens 0,2 LU Abweichung vom Ziel. Die JSON-Ergebnisse werden neben den Audiodateien gespeichert. Die regulären CTests benötigen FFmpeg nicht.
