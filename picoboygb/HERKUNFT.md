# Herkunft und Lizenz von PicoBoyGB

PicoBoyGB ist eine Portierung, keine Neuentwicklung. Die Kette lässt sich
lückenlos zurückverfolgen, und **jedes Glied darin steht unter MIT**. Damit ist
MIT auch für diese Portierung die richtige und einzige mögliche Wahl — eine
freizügigere Lizenz wäre nicht zulässig, eine strengere (etwa GPL) wäre zwar
erlaubt, aber gegenüber dem Ursprung unnötig.

## Die Kette

| Stufe | Projekt | Autor | Lizenz |
|---|---|---|---|
| 1 | **Peanut-GB** — der Emulatorkern (`peanut_gb.h`) | Mahyar Koshkouei | MIT |
| 2 | **MiniGBS** — Grundlage der Tonerzeugung | Alex Baines | MIT |
| 3 | **minigb_apu** — MiniGBS eingepasst in Peanut-GB | deltabeard (Peanut-GB) | MIT |
| 4 | **RP2040-GB** — Peanut-GB auf dem RP2040 | deltabeard | MIT |
| 5 | **Pico-GB** — RP2040-GB auf Pico + ILI9225 + SD + I2S | Vincent Mistler (YouMakeTech) | MIT |
| 6 | **PicoBoyGB** — dieses Projekt | | MIT |

Stufe 5 ist der unmittelbare Vorgänger. Der Beleg dafür steckt noch im
Quelltext: `gbcolors.h` trägt „Copyright (c) 2022 Vincent Mistler" über der
MIT-Lizenz, und der Kopf von `PicoBoyGB.ino` nennt die Umbauten von Stufe 5
auf Stufe 6:

```
ILI9225 -> ST7789      anderes Display
SD-Karte -> Flash      ROMs liegen im Flash statt auf Karte
I2S -> PWM-Piezo       anderer Tonausgang
RP2040 -> RP2350       anderer Chip
```

## Weitere mitgelieferte Bestandteile

| Datei / Ordner | Herkunft | Lizenz |
|---|---|---|
| `fatfs/` | ChaN FatFs R0.15 | eigene BSD-artige Ein-Klausel-Lizenz (siehe `fatfs/ff.h`) |
| `src/hedley.h` | Evan Nemerson | Public Domain (CC0) |
| `src/font8x8.h` | — | Public Domain |
| TinyUSB (aus dem Pico SDK) | Ha Thach | MIT |

Alles davon ist mit MIT verträglich. Die FatFs-Lizenz verlangt lediglich, dass
ihr Urheberrechtsvermerk erhalten bleibt — er steht unverändert in den Dateien
unter `fatfs/`.

## Was daraus folgt

* Die Datei `LICENSE` in diesem Ordner ist MIT.
* Rechteinhaber ist Jan Schulz; der Vermerk steht in `LICENSE`.
* Die Urheberrechtsvermerke in `src/peanut_gb.h`, `src/gbcolors.h`,
  `src/minigb_apu.*` und unter `fatfs/` müssen erhalten bleiben. Sie sind
  unverändert vorhanden — beim Aufräumen bitte nicht entfernen.
