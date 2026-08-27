# PicoBoyGB — die Arduino-Fassung

Der ursprüngliche Arduino-Sketch des Game-Boy-Emulators, aus dem die
SDK-Portierung in [`../picoboygb`](../picoboygb) hervorgegangen ist.

**Dieser Ordner wird nicht gebaut** und hat deshalb kein `build.sh`. Er liegt
zum Nachschlagen bei: der Emulator, die Anzeige, der Ton und das Menü sind in
beiden Fassungen derselbe Quelltext, und wer die SDK-Fassung liest, findet
hier die Vorlage.

## Was sich zwischen beiden unterscheidet

| Arduino-Fassung (hier) | SDK-Fassung (`../picoboygb`) |
|---|---|
| `setup()` / `loop()` | `main()` ruft `setup()` einmal, dann `loop()` |
| `millis()` / `delay()` | `to_ms_since_boot()` / `sleep_ms()` |
| `#include <Arduino.h>` | `#include <pico/stdlib.h>` |
| FatFS aus arduino-pico | mitgeliefertes ChaN FatFs R0.15 |
| FatFSUSB aus arduino-pico | TinyUSB-Massenspeicher |

Alles andere — `peanut_gb.h`, `st7789_picoboy.h`, `minigb_apu`, `audio_pwm.h`,
`gbcolors.h`, `splash.h` — ist unverändert dasselbe.

**Gepflegt wird die SDK-Fassung.** Wer den Emulator benutzen oder verändern
will, nimmt die.

## Bauen

Mit der Arduino-IDE, Board-Paket `arduino-pico` (Earle Philhower), Board
„Raspberry Pi Pico 2", Flash-Größe 16 MB.

## Lizenz

MIT. Die Herleitung über Peanut-GB, RP2040-GB und Pico-GB steht in
[`../picoboygb/HERKUNFT.md`](../picoboygb/HERKUNFT.md).
