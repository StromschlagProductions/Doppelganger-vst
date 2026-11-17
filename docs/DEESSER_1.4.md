# De-Esser 1.4: Messung an echter Sprache

## Anlass

Im Praxistest war mit LISTEN (Differenz vor/nach dem De-Esser) zu hören, dass der De-Esser in Version 1.3 nicht nur Zischlaute bearbeitet, sondern auch den übrigen Inhalt im Höhenbereich. Gleichzeitig senkte er die Zischlaute selbst bei Standardstärke kaum ab.

## Was sich geändert hat

| | 1.3 | 1.4 |
|---|---|---|
| Zischband | Hochpass 12 dB/Oktave ab der Frequenz | Hochpass 24 dB/Oktave ab der Frequenz, oben bei 12 kHz begrenzt |
| Vergleich | Zischband gegen Gesamtsignal | Zischband gegen tiefen Stimmbereich (Tiefpass 24 dB/Oktave bei höchstens 3 kHz) |
| Schwelle | −8,5 dB (Standard) bis −16 dB | −1,5 dB (Standard) bis −6 dB |
| Größte Absenkung | 3 dB (Standard) bis 12 dB | 5,75 dB (Standard) bis 14 dB |
| Erkennung Rückfall | 50 ms | 4 ms |
| Absenkung Rückfall | 50 ms | 15 ms |

Bei Vokalen liegt im tiefen Stimmbereich viel mehr Energie als im Zischband, bei S, Z, Sch, F und scharfen T ist es umgekehrt. Der neue Vergleich trennt beides deutlich besser. Die kürzeren Rückfallzeiten verhindern, dass nach einem S der folgende Vokal mit abgesenkt wird.

## Messung

Drei Sprachaufnahmen (eine Männerstimme, zwei Frauenstimmen), 48 kHz, De-Esser-Frequenz 5,5 kHz, ohne Angleich-EQ. Ausgewertet wurde in 10-ms-Abschnitten der Pegel oberhalb von 4,5 kHz mit und ohne De-Esser.

- **Zischlaut:** Abschnitt mit mehr Energie oberhalb von 4,5 kHz als unterhalb von 3 kHz.
- **Stimmhaft:** Abschnitt mit mindestens 18 dB mehr Energie unterhalb von 3 kHz, mindestens 50 ms von einem Zischlaut entfernt.

| Aufnahme | Stärke | Zischlaute: mittlere Absenkung 1.3 → 1.4 | Stimmhaft: Anteil > 1 dB abgesenkt 1.3 → 1.4 |
|---|---|---|---|
| Mann | 25 % | −0,5 dB → **−2,7 dB** | 1,6 % → **0,6 %** |
| Mann | 100 % | −2,7 dB → **−6,3 dB** | 14,8 % → **1,1 %** |
| Frau 1 | 25 % | −0,3 dB → **−4,4 dB** | 0,7 % → **0,5 %** |
| Frau 1 | 100 % | −3,2 dB → **−9,6 dB** | 11,8 % → **2,1 %** |
| Frau 2 | 25 % | −0,2 dB → **−4,1 dB** | 1,0 % → **0,1 %** |
| Frau 2 | 100 % | −2,4 dB → **−9,3 dB** | 8,9 % → **0,8 %** |

Im Mittel über alle stimmhaften Abschnitte liegt die Absenkung in 1.4 unter 0,1 dB.

Der Regressionstest `deesserSelectivity` hält dieses Verhalten fest: Ein heller Vokal mit Obertönen bis 9 kHz darf auch bei voller Stärke nicht abgesenkt werden, ein Zischlaut mit Stimmanteil muss bei Standardstärke um mehr als 3,2 dB sinken. Version 1.3 besteht diesen Test nicht.
