# PicoBoyGB — die SDK-Fassung

Game-Boy-Emulator (DMG) für den **PicoBoy Color Plus** (RP2350A), gebaut mit dem
schlichten **Raspberry Pi Pico SDK** statt mit der Arduino-IDE.

Das ist die Portierung des Arduino-Sketches in
[`../picoboygb-arduino`](../picoboygb-arduino). Emulator, Anzeige, Ton,
Spielstände und Menü sind derselbe Quelltext — ersetzt wurde nur, was am
Arduino-Kern hing:

| Arduino-Fassung | SDK-Fassung (dieser Ordner) |
|---|---|
| `setup()` / `loop()` als Laufzeitgerüst | `main()` ruft `setup()` einmal, danach `loop()` endlos |
| `millis()` / `delay()` | `to_ms_since_boot()` / `sleep_ms()` (Zwischenschicht in `main.cpp`) |
| `#include <Arduino.h>` | `#include <pico/stdlib.h>` |
| **FatFS** aus arduino-pico | mitgeliefertes **ChaN FatFs R0.15** (`fatfs/`) auf einem Flash-Bereich |
| **FatFSUSB** aus arduino-pico | **TinyUSB**-Massenspeicher (`src/usb_*.c`) |

Alles andere — `peanut_gb.h`, `st7789_picoboy.h`, `minigb_apu`, `audio_pwm.h`,
`gbcolors.h`, `splash.h` … — ist unverändert übernommen.

**Gepflegt wird diese Fassung.** Der Arduino-Sketch liegt nur noch zum
Nachschlagen daneben.

## Bauen

```bash
./build.sh          # baut nach ../dist/picoboygb.uf2
./build.sh clean    # Bauordner vorher löschen
```

Das Skript holt Pico SDK 2.1.0 nach `~/.cache/pbcp-sdks` (gemeinsam mit den
anderen Projekten), falls `PICO_SDK_PATH` nicht gesetzt ist. Von Hand:

```bash
cmake -S . -B build -G Ninja \
  -DPICO_SDK_PATH=/pfad/zum/pico-sdk \
  -DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=pico2
cmake --build build -j
# -> build/picoboygb.uf2
```

Gebraucht werden `gcc-arm-none-eabi`, `cmake`, `git` und `python3`; `ninja` ist
optional. Zum Flashen über USB braucht `picotool` zusätzlich
`libusb-1.0-0-dev`.

## Aufteilung des Flash (16 MB)

| Adresse | Größe | Inhalt |
|---|---|---|
| `0x000000` | 1 MB | Firmware (dieser Bau ist rund 120 KB groß) |
| `0x100000` | 2 MB | Bereich für das laufende ROM (`rom.h`) |
| `0x300000` | 12 MB | FatFs-Partition → der USB-Stick (`src/flash_fs.h`) |
| `0xFF0000` | 64 KB | Spielstände (`main.cpp`) |

Ein FatFs-Sektor entspricht genau einem Flash-Sektor von 4096 Byte. Dadurch
wird aus jedem USB-Schreibvorgang unmittelbar ein `flash_range_erase` plus
`flash_range_program` — ohne den Umweg über Lesen, Ändern, Zurückschreiben.

## Benutzung

1. `picoboygb.uf2` einmal flashen: BOOTSEL halten, USB anstecken, Datei
   hinüberkopieren.
2. **ROMs aufspielen:** **Mitte** halten und **RESET** drücken → das Gerät
   meldet sich als USB-Stick namens `PICOBOYGB`. Dort `.gb`-Dateien ablegen,
   danach **auswerfen**.
3. **RESET** *ohne* Mitte drücken → das Menü auf dem Gerät listet die ROMs auf.
   Hoch/Runter bewegt, **A** oder **Mitte** wählt aus, Links/Rechts schaltet
   den Ton um.

ROMs liegen dem Repo nicht bei.

### Steuerung im Spiel

| Game Boy | PicoBoy |
|---|---|
| Steuerkreuz | Joystick |
| A | Taste A |
| B | Taste B |
| START | Mitte + B |
| SELECT | Mitte + A |

## Aufbau

```
picoboygb/
├── CMakeLists.txt
├── pico_sdk_import.cmake
├── build.sh
├── src/
│   ├── main.cpp              portierter Sketch (Emulator, Menü, Dateisystem, USB)
│   ├── flash_fs.{h,c}        Blockgerät im Flash für die FatFs-Partition
│   ├── usb_msc.c             Rückrufe des TinyUSB-Massenspeichers
│   ├── usb_descriptors.c     USB-Deskriptoren für Gerät und Massenspeicher
│   ├── tusb_config.h         TinyUSB-Einstellungen (nur Gerät, nur Massenspeicher)
│   └── *.h / minigb_apu.cpp  unverändert aus der Arduino-Fassung übernommen
└── fatfs/                    mitgeliefertes ChaN FatFs R0.15 + eigenes diskio.c/ffconf.h
```

## Lizenz

MIT. Die Herleitung über Peanut-GB, RP2040-GB und Pico-GB steht in
[`HERKUNFT.md`](HERKUNFT.md).
