# CI-Konfiguration und Actions-Kontingent

Stand: 2026-11-16. Anlass: Das GitHub-Konto hatte nur noch rund 188 von 2.000 enthaltenen Actions-Minuten.
Windows-Läufe zählen doppelt, deshalb läuft der Windows-Build nicht mehr bei jedem Commit.
Alle bisherigen Prüfungen (Windows-Build, Regressionstests, pluginval Level 8, ASan/UBSan, TSan) bleiben erhalten.

## Wer läuft wann

| Workflow | Runner | Läuft bei | Läuft nicht bei |
|---|---|---|---|
| `build.yml` Windows-VST3 bauen (Build, Regressionstests, pluginval 8, ZIP) | Windows (Minuten x2) | **Run workflow** von Hand; Pull Request beim Öffnen oder bei „Ready for review“ (keine Drafts) | Push auf `main`, jeder weitere Push in einen PR, reine Doku-Änderungen |
| `tests.yml` DSP und Speichersicherheit (ASan/UBSan, TSan) | Linux (ohne JUCE, billig) | Push auf `main`, Öffnen/Ready/Reopen eines PR und weitere Pushes in den PR, aber nur bei Änderungen an `Source/`, `tests/`, `CMakeLists.txt`, `VERSION`; außerdem von Hand | Doku-Änderungen, README, `docs/` |

Weitere Maßnahmen:

- `concurrency`: Ein neuer Lauf derselben Ref bricht den überholten ab. Ausnahme: manuell gestartete Läufe (damit ein Release-Lauf nicht abgebrochen wird).
- Die Windows-Laufzeitgrenze wurde von 45 auf 30 Minuten gesenkt, damit ein hängender Lauf nicht lange Minuten frisst.
- Build-Artefakte werden nur 7 Tage aufbewahrt.
- Beim Release-Lauf wird vor dem Bauen geprüft, ob `docs/releases/<VERSION>.md` existiert und ob auf `main` gestartet wurde. Fehlt etwas, bricht der Lauf nach Sekunden ab statt nach dem vollen Build.

## Entwicklung und Abnahme

1. Während der Entwicklung lokal bauen und testen (die Befehle stehen in `docs/TECHNICAL_REVIEW.md`):
   `cmake -S . -B build/tests -DDOPPELGANGER_TESTS=ON && cmake --build build/tests && ctest --test-dir build/tests --output-on-failure`
2. Zur Abnahme **einmal** den Windows-Lauf starten: GitHub > Actions > „Windows-VST3 bauen“ > „Run workflow“, Branch `main`.
   Ohne Haken bei **publish** wird nur gebaut und geprüft (Artefakt zum Herunterladen), es entsteht kein Release.
3. Erst wenn das grün ist und das Plugin gehört wurde: denselben Lauf mit Haken **publish** starten. Das veröffentlicht das Release `v<VERSION>` mit ZIP und setzt den README-Download-Link („latest“) auf die neue Version.

Es wird nie automatisch ein Release oder Merge ausgelöst.

## Hinweis zum Kontingent

Ein Windows-Lauf kostet etwa das Doppelte seiner Laufzeit in abgerechneten Minuten. Bei 188 verbleibenden Minuten reichen also nur wenige vollständige Abnahmeläufe. Der Zähler setzt sich mit dem neuen Abrechnungszeitraum zurück.

Verbindliches öffentliches Versionsschema: `docs/VERSIONING.md` (ohne Build-Suffix).

Seit 2.3 prüft ein zusätzlicher Linux-Lauf den echten JUCE-Prozessor mit ThreadSanitizer (`processor_concurrency`). Der günstige Linux-Workflow reagiert auf `synchronize`; der Windows-Build bleibt zur Begrenzung der Kosten bei seiner bisherigen manuellen/Ready-Abnahme.
