# PBC NES

NES-Emulator für den **PicoBoy Color Plus** (RP2350A, 512 KB RAM, 16 MB Flash,
ST7789 240×280, Joystick + A/B).

Grundlage ist der QuickNES-Kern (Shay Green) in der Fassung von
[frank-nes](https://github.com/rh1tech/frank-nes). Das Drumherum ist dasselbe
wie bei [`../megadrive`](../megadrive/):

| frank-nes | PBC NES |
|---|---|
| ROM von SD-Karte, in PSRAM | ROM im **Flash**, QuickNES liest PRG direkt per XIP |
| SD-Karten-Browser | **USB-Stick-Modus** in eine FatFs-Partition im Flash (aus [PicoBoyGB](../picoboygb/)) |
| HDMI/VGA/TV | **ST7789**: 240×240 Pixel **1:1**, auf Kern 1 per DMA gesendet |
| Ton über HDMI/I2S/PWM | **kein Ton**: die APU läuft mit niedriger Rate mit, die Samples werden verworfen |
| Einstellungen, Spielstände, Game Genie, USB-HID | entfallen |

**Stand: gebaut, noch nicht auf dem Gerät getestet.**

## Benutzung

1. [`../dist/nes.uf2`](../dist/nes.uf2) flashen: BOOTSEL halten, USB anstecken,
   Datei auf das Laufwerk `RPI-RP2` kopieren.
2. **ROMs aufspielen:** beim Einschalten **A oder B** gedrückt halten. Das Gerät
   meldet sich als USB-Stick. `.nes`-Dateien draufkopieren und das Laufwerk
   **auswerfen** – der PicoBoy startet von selbst neu. Beim allerersten Start
   (leeres Laufwerk) landet man automatisch in diesem Modus.
3. Ohne gedrückte Taste einschalten: Spieleliste. Hoch/Runter wählt,
   Links/Rechts blättert, **A** oder **Mitte** startet.

**Gemeinsames Laufwerk mit `megadrive`:** Beide Firmwares benutzen dieselbe
Flash-Aufteilung. NES- und Genesis-ROMs können also zusammen auf dem Laufwerk
liegen; jede Firmware listet nur ihre eigenen. Nach einem Wechsel der Firmware
wird das erste gestartete Spiel einmal neu in den ROM-Bereich geschrieben.

### Steuerung im Spiel

| NES | PicoBoy |
|---|---|
| Steuerkreuz | Joystick |
| A | A |
| B | B |
| START | Mitte + B |
| SELECT | Mitte + A (1 s halten = zurück zur Spieleliste) |

Mitte allein tut nichts, wie bei PicoBoyGB. Unter dem Bild stehen gezeichnete
Bilder pro Sekunde, Emulationstempo und die Auslastung von Kern 0.

## Bild

Das NES-Bild ist 256×240. Links und rechts fallen je 8 Spalten weg – der Streifen,
den ein echter Fernseher im Überscan ohnehin versteckt und in dem viele Spiele
Scroll-Reste zeigen. Übrig bleiben 240×240, ohne Skalierung, senkrecht mittig.

QuickNES zeichnet abwechselnd in zwei Pixelpuffer. Kern 0 übergibt ein fertiges
Bild samt Palette an Kern 1. Der rechnet es Zeile für Zeile in BGR565 um und
schickt es per DMA, während Kern 0 schon das nächste Bild in den anderen Puffer
emuliert. Bei 250 MHz läuft der SPI mit 62,5 MHz; ein Bild braucht rund 15 ms.

## Speicher

QuickNES dekodiert das CHR-ROM einmal in einen **Kachel-Cache** im RAM, doppelt
so groß wie das CHR-ROM. Nach Abzug von Firmware und Pixelpuffern bleiben etwa
320 KB Heap. Damit laufen Spiele bis **128 KB CHR-ROM** (z. B. Super Mario Bros. 3)
und alle Spiele mit CHR-RAM (z. B. Zelda, Mega Man 2). Spiele mit 256 KB CHR-ROM
(z. B. Kirby's Adventure) passen nicht; sie melden „Spiel nicht ladbar“ statt
abzustürzen.

## Bauen

```bash
./build.sh                       # -> ../dist/nes.uf2
./build.sh clean
./build.sh -DCPU_MHZ=200 -DCPU_VREG=VREG_VOLTAGE_1_10
```

Werkzeuge und SDK wie bei allen Projekten dieses Repos, siehe
[`../docs/BAUEN.md`](../docs/BAUEN.md).

| Option | Vorgabe | Bedeutung |
|---|---|---|
| `CPU_MHZ` | 250 | Systemtakt (wie PicoBoyGB) |
| `CPU_VREG` | `VREG_VOLTAGE_1_25` | Kernspannung |
| `FLASH_MAX_FREQ_MHZ` | 88 | Obergrenze für den Flash-Takt |
| `PBC_SPI_HZ` | 62 500 000 | Display-SPI |
| `SHOW_FPS` | 1 | Anzeige unter dem Bild |

Das NES braucht viel weniger Rechenleistung als das Mega Drive. Zeigt die
Anzeige wenig CPU-Last, lohnt ein niedrigerer Takt für die Batterie.

## Flash-Aufteilung (16 MB)

| Adresse | Größe | Inhalt |
|---|---|---|
| `0x000000` | 1 MB | Firmware (ca. 170 KB) |
| `0x100000` | 5 MB | ROM des laufenden Spiels (`src/rom_store.c`) |
| `0x600000` | ~9,9 MB | FatFs-Partition = USB-Stick (`src/flash_fs.h`) |
| `0xFF0000` | 4 KB | Info, welches Spiel im ROM-Bereich liegt |

## Aufbau

```
src/main.c        Startablauf, USB-Stick-Modus, Takt, Emulationsschleife
src/video.c       Palette, Ausgabe 240x240 auf Kern 1
src/input.c       Tastenbelegung (Mitte als Modifier)
src/display.c, menu.c, rom_store.c   aus ../megadrive (rom_store ohne Byte-Tausch)
src/flash_fs.*, usb_*.c, tusb_config.h, fatfs/   aus ../picoboygb
core/quicknes/    QuickNES-Kern aus frank-nes, unverändert
core/emu2413/     YM2413-Emulation für den VRC7-Mapper, aus frank-nes
```

Der Kern ist unverändert. Mit `PICO_MALLOC_PANIC=0` darf `malloc` scheitern,
damit QuickNES fehlenden Speicher selbst melden kann.

## Offene Punkte

* **Nicht auf Hardware geprüft.**
* **Keine Batteriespeicher.** Spiele mit Speicherfunktion (Zelda u. a.) verlieren
  ihren Stand beim Ausschalten. QuickNES hält das Cartridge-RAM, es müsste nur
  beim Verlassen in den Flash geschrieben werden.
* Kein Ton, nur NTSC-Takt, kein zweiter Spieler.

## Lizenzen

* Gesamtwerk: **GPL-3.0** wie frank-nes, siehe `LICENSE`.
* `core/quicknes/`: QuickNES, **LGPL-2.1-or-later** (Dateiköpfe); die
  Kapselung `quicknes.cpp/.h` von frank-nes steht unter MIT.
* `core/emu2413/`: emu2413 von Mitsutaka Okazaki, VRC7-Anpassung von xodnizel,
  über QuickNES_Core und frank-nes übernommen.
* `src/flash_fs.*`, `src/usb_msc.c`, `src/usb_descriptors.c`,
  `src/tusb_config.h`, `src/font8x8.h`, `fatfs/diskio.c`: aus PicoBoyGB,
  **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
