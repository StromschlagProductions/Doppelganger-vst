# Reproduzierbare Screenshots

Die PNGs werden aus der echten JUCE-Oberfläche erzeugt, einschließlich der aus `VERSION`
kompilierten Versionsanzeige. Keine nachträglich ersetzten Versionsziffern.

```sh
cmake -S . -B build
cmake --build build --config Release --target DoppelgangerScreenshot
# Windows:
build/DoppelgangerScreenshot_artefacts/Release/DoppelgangerScreenshot.exe <absoluter-Pfad-zum-docs-Ordner>
```

Unter Linux benötigt das Werkzeug eine X11-Sitzung, z. B.
`xvfb-run -a build/DoppelgangerScreenshot_artefacts/Release/DoppelgangerScreenshot "$PWD/docs"`;
der ausführbare Name hat dort kein `.exe`. Es läuft als normale JUCE-Anwendung mit echter
Message-Loop, legt den Editor kurz auf den Desktop (der Editor aktualisiert Kurven,
Knopfbeschriftungen und die Live-Anzeige nur, solange er sichtbar ist), lässt ihn etwa
1,5 Sekunden mit laufendem Signal einschwingen und schreibt die Aufnahmen mit zweifacher
Pixelauflösung. Theme und Beispielparameter betreffen ausschließlich die
lokale Screenshot-Instanz, keine Plugin-Defaults oder gespeicherten Nutzerprojekte.

Vor einer Versionsveröffentlichung die Screenshots neu erzeugen und die sichtbare
Versionszeile mit `VERSION` abgleichen. Das Ziel ist vom normalen Build ausgenommen.

Doppelganger lernt für die Aufnahme ein selbst erzeugtes synthetisches, sprachähnliches
Testsignal (gleitende Grundtonhöhe um 120 Hz mit Obertönen, drei wandernde Vokal-Formanten,
etwas Atemrauschen, Silbenrhythmus) über seine öffentliche Lernfunktion. Die dargestellten Kurven sind echte Ergebnisse
dieser Beispielinstanz, keine Messungen einer Nutzeraufnahme.

Das Werkzeug schreibt außerdem `docs/screenshots.json` mit der einkompilierten Version
und den erzeugten PNG-Dateien. Diesen Manifest-Eintrag nicht manuell hochsetzen.
Vor Veröffentlichung prüft der Workflow die Übereinstimmung mit `VERSION` und das
Vorhandensein aller aufgeführten Bilder. Das Manifest wird als Release-Asset angehängt,
damit die Homepage künftige Aufnahmen aus genau dem freigegebenen Release-Tag laden kann.
