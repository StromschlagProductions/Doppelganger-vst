# DOPPELGANGER – Audio Drama Voice Matcher

VST3-Insert-Effekt für Hörspiel-Produktion. Er gleicht die Klangfarbe einer Sprecherspur an eine feste Referenzstimme
an und bringt sie gleich auf die richtige Lautheit: Mikrofon und Raum werden auf den Standard des Projekts gebracht,
ohne dass man jede Spur mit mehreren Plugins einzeln nachbearbeitet.

**⬇ Download (Windows · VST3 · Mono/Stereo):** https://github.com/StromschlagProductions/Doppelganger-vst/releases/latest

Installation: ZIP entpacken und den Ordner `Doppelganger.vst3` nach `C:\Program Files\Common Files\VST3` kopieren.

**Status:** Entwicklung (Version 0.2). Die Klangfunktion ist fertig und getestet, die Oberfläche ist noch eine
schlichte Testversion. Das endgültige Design folgt.

## Warum es dieses Plugin gibt

Bisher lag auf jeder Sprecherspur eine Kette aus Match-EQ, Leveler/Kompressor und De-Esser, jede Spur einzeln
angeglichen an eine männliche oder weibliche Vorlage. DOPPELGANGER kennt diese Vorlagen fest und erledigt alles in
einem Plugin: auf die Spur ziehen, einmal lernen lassen, fertig.

– Lars Eickstädt, Stromschlag Audio

## Bedienung

1. Plugin auf die Sprecherspur legen (mono oder stereo).
2. **Lernen** drücken und die Spur abspielen. Nach 30 Sekunden Sprache hört das Plugin selbst auf, man kann aber
   auch vorher **Fertig** drücken (mindestens 4 Sekunden Sprache).
3. Das Plugin erkennt die Tonhöhe und wählt bei **Stimmlage: Auto** die passende Referenz (unter 160 Hz Mann,
   bis 225 Hz Frau mittel, darüber Frau hoch). Das Gelernte wird mit dem Projekt gespeichert.

## Regler (alle automatisierbar)

| Regler | Wirkung |
|---|---|
| **Angleichung** | Wie stark die Klangfarbe an die Referenz angeglichen wird. 100 % = ganz, Standard 60 %: Mikrofon und Raum werden angeglichen, die eigene Stimmfarbe bleibt. Der reine Pegelunterschied wird dabei nie angeglichen, nur die Klangform; Korrekturen sind auf ±9 dB begrenzt. |
| **Stimmlage** | Auto, Tief (Mann), Mittel (Frau), Hoch (Frau). |
| **Lowcut** | 24 dB/Oktave, 20–200 Hz (ganz links = aus), Standard 80 Hz. |
| **De-Esser** | 0 % = aus. Senkt nur die Zischlaute ab (dynamischer Höhen-Shelf ab 4,5 kHz). Bei 25 % etwa 3 dB, bei 100 % etwa 10 dB. Bei Takes mit schwachen S-Lauten (lispelnd) auf 0 % stellen. |
| **Leveler** | Die Lautheit wird immer auf das Ziel gebracht (integrierte Lautheit nach EBU R128, gleitend über ca. 20 s). Der Regler bestimmt zusätzlich, wie stark Schwankungen innerhalb der Sätze ausgeglichen werden: 0 % = Dynamik bleibt wie gesprochen, 100 % = sehr gleichmäßig. In Pausen wird nicht nachgeregelt. |
| **Ziel-Lautheit** | −30 bis −14 LUFS, Standard −23 LUFS. |

Am Ende sitzt immer ein Limiter (−1 dBTP). Latenz: 74 Samples bei 48 kHz (ca. 1,5 ms), die Studio One automatisch ausgleicht.

## Referenzen

Die Zielkurven der drei Referenzstimmen sind fest im Plugin hinterlegt (`Source/ReferenceCurves.h`), die
Audiodateien selbst werden nicht mitgeliefert. Erzeugt werden die Kurven mit:

```
python3 tools/make_reference_curves.py tief=MANN.wav mittel=FRAU1.wav hoch=FRAU2.wav
```

Die Referenzen enden bei ca. 16 kHz (Codec-Grenze). Darüber korrigiert das Plugin nicht, die Höhen der Takes bleiben erhalten.

## Bauen und testen

- Windows-VST3: wird bei jeder Änderung automatisch von GitHub Actions gebaut, mit pluginval geprüft und als Release veröffentlicht.
- Engine-Test ohne JUCE: `g++ -std=c++17 -O2 -o test_engine tests/test_engine.cpp`
