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
| Spielstände, Batteriespeicher, SG-1000-Menü | entfallen; zwei Controller nur im Link-Spiel von PBCemu ([`../docs/LINKSPIEL.md`](../docs/LINKSPIEL.md)) |

**Stand: gebaut, noch nicht auf dem Gerät getestet.**

Die Oberfläche auf dem Gerät ist englisch (Menüs, Optionen, Meldungen, Hilfedatei auf dem Laufwerk).

## Benutzung

1. [`../dist/sms.uf2`](../dist/sms.uf2) flashen: BOOTSEL halten, USB anstecken,
   Datei auf das Laufwerk `RPI-RP2` kopieren.
2. **ROMs aufspielen:** beim Einschalten **A oder B** gedrückt halten. Das Gerät
   meldet sich als USB-Stick `PBC-ROMS`. `.sms`-, `.gg`- und `.sg`-Dateien (SG-1000) draufkopieren
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
FM-Chip, zusammen etwa 80 KB; dafür bleiben gut 140 KB Heap. Das ROM selbst
kostet kein RAM.

### Was im RAM läuft

Das ROM wird direkt aus dem Flash gelesen; Flash und Emulatorcode teilen sich
einen Cache von nur 16 KB. Der Kern läuft deshalb aus dem RAM (rund 80 KB):
Z80 (redcode/Z80 samt Befehlstabellen, siehe unten), Speicher- und
Port-Zugriffe, VDP, Zeichenroutinen, PSG und die Zeilenausgabe in
`src/sms_video.c`. Im übernommenen RP2350-Port war das
abgeschaltet (`in_ram` in `core/smsplus/shared.h` war leer), alles lief aus
dem Flash.

Die 64-KB-Tabelle `lut.h`, aus der für jedes deckende Sprite-Pixel an
zufälliger Stelle gelesen wurde, ist durch eine Rechnung von fünf Zeilen
ersetzt (`sprite_over_bg()` in `render.c`, für alle 65536 Eingaben gleiches
Ergebnis, auf dem PC geprüft). Die kleinen Tabellen der FM-Klangerzeugung
(YM2413) liegen ebenfalls im RAM.

Nebenbei behoben: Doppelt große Sprites (Register 1, Bit 0) lasen in der
unteren Hälfte die falsche Kachel, 8×16-Sprites dabei sogar über das Ende der
64-Byte-Kachel hinaus (`core/smsplus/render.c`).

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
| `CPU_MHZ` | 150 | Systemtakt (Grundtakt, wie C64 und NES) |
| `CPU_VREG` | `VREG_VOLTAGE_1_10` | Kernspannung (bei 150 MHz ohnehin die Grundeinstellung) |
| `FLASH_MAX_FREQ_MHZ` | 88 | Obergrenze für den Flash-Takt |
| `PBC_SPI_HZ` | 75 000 000 | Display-SPI: 150 MHz / 2 = 75 MHz (über dem Datenblattwert, läuft auf dem Gerät sauber) |
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
* Nur NTSC-Takt (60 Hz). SG-1000 (`.sg`) bisher nur am PC getestet.

## ColecoVision (nur in PBCemu)

Die ColecoVision ist fast ein SG-1000 (Z80, TMS9918A, SN76489) und läuft als
eigener Modus im Kern (`TYPE_COLECO`): BIOS bei 0000h, 1 KB RAM achtfach
über 6000h–7FFFh, das Modul ab 8000h (bis 32 KB), Ports nach A7–A5, der
Interrupt des Videochips am NMI, der Controller mit Joystick- und
Ziffernfeld-Modus. Im Starter ist sie ein eigenes System; Module heißen
`.col`, oft auch `.rom` oder `.bin` – der Starter erkennt sie an ihren ersten
Bytes (AA 55 oder 55 AA) und zeigt sie nur bei der ColecoVision.

**BIOS:** `coleco.rom` (8 KB) ins Hauptverzeichnis des Laufwerks, es gehört
Coleco und liegt nicht bei. Fehlt es, nimmt der Emulator ein eigenes
Mini-BIOS: es startet das Modul an der Adresse in seinem Kopf und leitet NMI
und RST-Vektoren wie das echte weiter. Das reicht für Spiele, die keine
BIOS-Routinen aufrufen; die meisten älteren brauchen das echte.

| PicoBoy | ColecoVision |
|---|---|
| Joystick | Joystick |
| B / A | linker / rechter Feuerknopf |
| Mitte + Hoch/Rechts/Runter/Links | Ziffern 1, 2, 3, 4 |
| Mitte + B + Hoch/Rechts/Runter/Links | Ziffern 5, 6, 7, 8 |
| Mitte + A + Hoch/Rechts/Runter/Links | 9, 0, `*`, `#` |

Eine Ziffer zählt, sobald die Tasten drei Bilder lang (50 ms) gleich
bleiben; solange Mitte gedrückt ist, bewegt der Joystick nichts.

Am PC mit dem echten BIOS und fünf Homebrews getestet (Space Trainer,
Breakout, Slither, Frogs and Flies, Kaboom), auf dem Gerät mit Breakout:
volle 60 Bilder/s, Kern 0 zu 76 % ausgelastet. Ohne
BIOS laufen sie nicht. Zusatzcontroller (Lenkrad, Roller Controller, Super
Action) fehlen.

## Lizenzen

* Gesamtwerk: **GPL-3.0** wie pico-smsplus, siehe `LICENSE`. SMS Plus selbst
  steht unter GPL-2.0-or-later (Dateiköpfe in `core/smsplus/`).
* `src/flash_fs.*`, `src/usb_msc.c`, `src/usb_descriptors.c`,
  `src/tusb_config.h`, `src/font8x8.h`, `fatfs/diskio.c`: aus PicoBoyGB,
  **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
* `core/redcode/`: Z80-Kern **redcode/Z80** von Manuel Sainz de Baranda y Goñi
  mit Headern seiner Bibliothek Zeta, **LGPL-3.0-or-later**; für den PicoBoy
  geändert von `tools/redcode_patch.py`, Einzelheiten in `core/redcode/README.md`.
  Er ersetzt seit November 2026 den Z80-Kern aus MAME (Juergen Buchmueller),
  der nur für nicht-kommerzielle Zwecke frei war; `core/smsplus/z80_redcode.c`
  bietet SMS Plus dessen alte Schnittstelle.
* `core/smsplus/sn76496.[ch]`: PSG (SN76489), seit November 2026 eigener Code
  für den PicoBoy (GPL-2.0-or-later); er ersetzt den SN76496 aus dem alten
  MAME (Nicola Salmoria) und behält Schnittstelle und Pegel. Das Rauschen
  folgt jetzt dem Sega-Chip (16-Bit-Register, Abgriffe 0 und 3).
* `core/smsplus/emu2413/`: emu2413, © 2020 Mitsutaka Okazaki (Vermerk im Dateikopf; upstream steht emu2413 unter MIT).
