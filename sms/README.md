# PBC SMS/GG

Sega-Master-System- und Game-Gear-Emulator für den **PicoBoy Color Plus**
(RP2350A, 512 KB RAM, 16 MB Flash, ST7789 240×280, Joystick + A/B).

Grundlage ist SMS Plus (Charles MacDonald) in der Fassung von
[pico-smsplus](https://github.com/PicoPlus-devel/pico-smsplus). Das Drumherum
ist dasselbe wie bei [`../nes`](../nes/) und [`../c64`](../c64/):

| pico-smsplus | PBC SMS/GG |
|---|---|
| ROM von SD-Karte, in PSRAM oder Flash | ROM im **Flash**, SMS Plus blendet seine 16-KB-Seiten direkt per XIP ein |
| SD-Karten-Menü, USB-Laufwerk der SD-Karte | **USB-Stick-Modus** in eine FatFs-Partition im Flash (aus [PicoBoyGB](../picoboygb/)) |
| HDMI | **ST7789**: Master System 240 Pixel breit 1:1, Game Gear 1,5× auf 240×216, auf Kern 1 per DMA |
| Ton über HDMI/I2S | Ton über den **PWM-Lautsprecher** (GP15), 22 222 Hz, Timer auf Kern 1 |
| Spielstände, Batteriespeicher, SG-1000-Menü, zwei Controller | entfallen |

**Stand: gebaut, noch nicht auf dem Gerät getestet.**

Die Oberfläche auf dem Gerät ist englisch (Menüs, Optionen, Meldungen, Hilfedatei auf dem Laufwerk).

## Benutzung

1. [`../dist/sms.uf2`](../dist/sms.uf2) flashen: BOOTSEL halten, USB anstecken,
   Datei auf das Laufwerk `RPI-RP2` kopieren.
2. **ROMs aufspielen:** beim Einschalten **A oder B** gedrückt halten. Das Gerät
   meldet sich als USB-Stick `PBC-ROMS`. `.sms`- und `.gg`-Dateien draufkopieren
   und das Laufwerk **auswerfen** – der PicoBoy startet von selbst neu. Beim
   allerersten Start (leeres Laufwerk) landet man automatisch in diesem Modus.
3. Ohne gedrückte Taste einschalten: Spieleliste mit Master-System- und
   Game-Gear-Spielen zusammen; welches System emuliert wird, folgt aus der
   Dateiendung. Hoch/Runter wählt, Links/Rechts blättert, **A** oder **Mitte**
   startet.
   Ganz oben steht immer **„Add games via USB“**: führt direkt in den
   USB-Stick-Modus. Beim Öffnen ist das erste Spiel markiert.
4. **B** in der Spieleliste öffnet die Optionen: Ton an/aus (Vorgabe an) und
   Lautstärke (1–12, Vorgabe 8). Beides bleibt gespeichert und lässt sich nur
   hier ändern, nicht im Spiel.

Das Laufwerk teilt sich die Firmware mit `megadrive`, `nes` und `c64`; jede
listet nur ihre eigenen Dateien.

### Steuerung im Spiel

Vor jedem Spielstart zeigt ein Kasten zwei Sekunden lang diese Belegung, wie
bei PicoBoyGB.

| Master System / Game Gear | PicoBoy |
|---|---|
| Steuerkreuz | Joystick |
| Taste 1 | B |
| Taste 2 | A |
| START (Game Gear) / PAUSE (Master System) | Mitte + B |

Mitte allein tut nichts. Zurück zur Spieleliste geht es mit RESET.

## Bild

SMS Plus liefert jede Bildzeile als 8-Bit-Palettenindizes. Sie wird sofort mit
der Palette, die in genau dieser Zeile gilt, in Panel-Farben umgerechnet –
Palettenwechsel mitten im Bild bleiben also erhalten – und in einen von zwei
Bildpuffern geschrieben. Kern 1 schickt das fertige Bild, während Kern 0 das
nächste in den anderen Puffer emuliert.

* **Master System** (256×192, im 224-Zeilen-Modus mehr): Links und rechts fallen
  je 8 Spalten weg – die linken blenden viele Spiele ohnehin aus. Übrig bleiben
  240 Pixel, 1:1 und senkrecht mittig. Kern 1 schickt das Bild ohne
  Umrechnung in einem Stück per DMA.
* **Game Gear** (160×144): 1,5× auf 240×216. Aus je zwei Quellpixeln a und b
  werden drei: a, der Mittelwert von a und b, und b – waagerecht wie senkrecht.
  So fällt nichts weg und Kanten bleiben scharf.

Das ist jeweils die Vorgabe. Im Verbund (`../pbcemu`) lässt sich im Starter je
Gerät ein anderer **Anzeigemodus** wählen:

| Master System | | Game Gear | |
|---|---|---|---|
| Crop 1:1 | wie oben (Vorgabe) | Scale 1.5x | wie oben (Vorgabe) |
| Fit width | alle 256 Spalten auf 240, Höhe passend (180) | Original | 160×144, 1:1 |
| Zoom 1.25x | mittlere 192 Spalten auf 240, 1,25× hoch (240×240) | | |

Kein Modus verzerrt: waagerecht und senkrecht gilt derselbe Faktor.

Beim Skalieren wird das nächste Quellpixel genommen oder, wenn das Zielpixel
zwischen zwei fällt, deren Mittelwert. Master-System-Spalten werden schon auf
Kern 0 beim Umrechnen der Zeile skaliert (der Bildpuffer bleibt 240 breit),
alles andere auf Kern 1 beim Senden.

**Leistungsanzeige** (z. B. `60fps 100% CPU71%`: gezeichnete Bilder pro
Sekunde, Tempo gegenüber dem Original, Auslastung von Kern 0): im Verbund im
Starter einschaltbar, sonst über `SHOW_FPS`. Sie steht unter dem Bild, oder
links oben im Bild, wenn darunter kein Platz ist (Zoom 1.25x bei Spielen mit
224 Zeilen füllt alle 280).

## Ton

PSG (SN76489) und – bei japanischen Master-System-Spielen – der FM-Chip YM2413
laufen mit 22 222 Hz, der Rate des Lautsprecher-Timers aus `../nes`. Wie bei
`../c64` läuft dieser Timer auf Kern 1. Ein Gleichanteilfilter aus
pico-smsplus entfernt den Versatz des PSG, links und rechts werden gemittelt.

## Speicher

Zwei Bildpuffer in Panel-Farben (je 240×224×2 Byte) belegen 210 KB. Der Kern
holt sich über `malloc` Tile-Cache, Arbeitsspeicher, Batterie-RAM und den
FM-Chip, zusammen etwa 80 KB; dafür bleiben gut 200 KB Heap. Das ROM selbst
kostet kein RAM.

## Bauen

```bash
./build.sh                       # -> ../dist/sms.uf2
./build.sh clean
./build.sh -DCPU_MHZ=200 -DCPU_VREG=VREG_VOLTAGE_1_15
```

Werkzeuge und SDK wie bei allen Projekten dieses Repos, siehe
[`../docs/BAUEN.md`](../docs/BAUEN.md).

| Option | Vorgabe | Bedeutung |
|---|---|---|
| `CPU_MHZ` | 250 | Systemtakt (wie PicoBoyGB) |
| `CPU_VREG` | `VREG_VOLTAGE_1_25` | Kernspannung |
| `FLASH_MAX_FREQ_MHZ` | 88 | Obergrenze für den Flash-Takt |
| `PBC_SPI_HZ` | 62 500 000 | Display-SPI |
| `SHOW_FPS` | 0 | Leistungsanzeige (fps, Tempo, Last) in der Einzel-Firmware; im Verbund schaltet sie der Starter |

## Flash-Aufteilung (16 MB)

| Adresse | Größe | Inhalt |
|---|---|---|
| `0x000000` | 1 MB | Firmware (ca. 290 KB) |
| `0x100000` | 5 MB | ROM des laufenden Spiels (`src/rom_store.c`) |
| `0x600000` | ~9,9 MB | FatFs-Partition = USB-Stick (`src/flash_fs.h`) |
| `0xFF0000` | 4 KB | Info, welches Spiel im ROM-Bereich liegt |
| `0xFF3000` | 4 KB | Optionen (Ton, Lautstärke) |

## Absturzanzeige

Stürzt die Firmware ab, zeigt das Display „CRASH“ mit Kern (Core), PC, LR und
dem Fehlerregister CFSR. PC und LR lassen sich mit
`arm-none-eabi-addr2line -e build/pbc_sms.elf <Adresse>` einer Quellzeile
zuordnen.

## Aufbau

```
src/main.c          Startablauf, USB-Stick-Modus, Takt, Emulationsschleife
src/sms_video.c     Paletten- und Zeilenrückrufe des Kerns, Ausgabe auf Kern 1
src/input.c         Tasten -> SMS-/GG-Pad
src/audio_pwm.c     Ton: Ringpuffer, Timer (auf Kern 1), PWM auf GP15
src/display.c, menu.c, rom_store.c, settings.c   aus ../nes
src/flash_fs.*, usb_*.c, tusb_config.h, fatfs/   aus ../picoboygb
core/smsplus/       SMS Plus aus pico-smsplus
```

Am Kern geändert: `SMS_AUD_RATE` in `sms.h` lässt sich von außen setzen
(hier 22 222 Hz statt 44 100).

## Offene Punkte

* **Nicht auf Hardware geprüft.**
* **Kein Batteriespeicher.** Spiele mit Speicherfunktion (z. B. Phantasy Star)
  verlieren ihren Stand beim Ausschalten; das Batterie-RAM startet leer.
* Nur NTSC-Takt (60 Hz), kein zweiter Spieler, kein SG-1000 (`.sg`).

## Lizenzen

* Gesamtwerk: **GPL-3.0** wie pico-smsplus, siehe `LICENSE`. SMS Plus selbst
  steht unter GPL-2.0-or-later (Dateiköpfe in `core/smsplus/`).
* `src/flash_fs.*`, `src/usb_msc.c`, `src/usb_descriptors.c`,
  `src/tusb_config.h`, `src/font8x8.h`, `fatfs/diskio.c`: aus PicoBoyGB,
  **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
* `core/smsplus/emu2413/`: emu2413, © 2020 Mitsutaka Okazaki (Vermerk im Dateikopf; upstream steht emu2413 unter MIT).
