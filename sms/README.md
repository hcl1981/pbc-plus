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
| Ton über HDMI/I2S | Ton über den **PWM-Lautsprecher** (GP15), 45 454 Hz, Timer auf Kern 1 |
| Batteriespeicher | als **`<Spielname>.sav`** neben der ROM auf dem Laufwerk, wie bei `../gbc` |
| Spielstände (Savestates), SG-1000-Menü | entfallen; zwei Controller nur im Link-Spiel von PBCemu ([`../docs/LINKSPIEL.md`](../docs/LINKSPIEL.md)) |

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

Mitte allein tut nichts. Zurück zur Spieleliste geht es mit RESET. In
PBCemu tauscht die Zeile „B A“ in den Einstellungen des Spiels die Tasten 1
und 2.

### Batteriespeicher

Master-System- und Game-Gear-Module mit Batterie (z. B. Phantasy Star,
Golden Axe Warrior, Shining Force Gaiden) blenden bis zu 32 KB RAM über
8000h–BFFFh ein. Es landet als **`<Spielname>.sav`** neben der ROM auf dem
Laufwerk, am PC sichtbar und kopierbar: 16 KB, 32 KB sobald ein Spiel die
obere Hälfte benutzt. Geladen wird beim Start. Gespeichert wird während des
Spiels, denn verlassen wird es mit RESET: Zweimal pro Sekunde prüft eine
Prüfsumme das RAM; die Datei wird geschrieben, sobald es sich eine Sekunde
lang nicht mehr geändert hat (oder seit 20 s ständig ändert). Unveränderte
Flash-Sektoren bleiben stehen, Kern 1 wird je geändertem Sektor angehalten
(`src/flash_fs.c`, wie bei `../gbc`): das Spiel stockt dabei etwa 50 ms je
4 KB. **Nach dem Speichern im Spiel kurz warten, bevor man RESET drückt.**
Ist das Laufwerk voll, steht vier Sekunden lang „SAVE FAILED: DRIVE FULL?“
im Bild. Im Link-Spiel wird weder geladen noch gespeichert: beide Geräte
müssen dasselbe RAM haben, und das Stocken risse die Verbindung. SG-1000 und ColecoVision haben keinen Batteriespeicher.

## Bild

SMS Plus liefert jede Bildzeile als 8-Bit-Palettenindizes. Kern 0 legt sie so,
wie sie sind, in einen von zwei Bildpuffern, dazu die Palette, die in genau
dieser Zeile gilt – neu abgelegt nur, wenn sie sich seit der letzten Zeile
geändert hat. Palettenwechsel mitten im Bild bleiben also erhalten. Kern 1
rechnet das fertige Bild Zeile für Zeile in Panel-Farben um und schickt es,
während Kern 0 das nächste in den anderen Puffer emuliert. Bis November 2026
rechnete Kern 0 jedes Pixel selbst um (gut 5 % seiner Zeit); das Ergebnis
ist Pixel für Pixel dasselbe (auf dem PC mit allen Anzeigemodi,
Statuszeile, Scanlines und Batteriewarnung verglichen).

* **Master System** (256×192, im 224-Zeilen-Modus mehr): Links und rechts fallen
  je 8 Spalten weg – die linken blenden viele Spiele ohnehin aus. Übrig bleiben
  240 Pixel, 1:1 und senkrecht mittig.
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
zwischen zwei fällt, deren Mittelwert. Alles auf Kern 1 beim Senden:
Master-System-Spalten beim Umrechnen der Zeile, dann der Rest.

**Leistungsanzeige** (z. B. `60fps 100% CPU71%`: gezeichnete Bilder pro
Sekunde, Tempo gegenüber dem Original, Auslastung von Kern 0): im Verbund im
Starter einschaltbar, sonst über `SHOW_FPS`. Sie steht unter dem Bild, oder
links oben im Bild, wenn darunter kein Platz ist (Zoom 1.25x bei Spielen mit
224 Zeilen füllt alle 280).

## Ton

PSG (SN76489) läuft mit 45 454 Hz (bis November 2026: 22 222 Hz, dumpfer), der Rate des
Lautsprecher-Timers aus `../nes`. Wie bei
`../c64` läuft dieser Timer auf Kern 1. Ein Gleichanteilfilter aus
pico-smsplus entfernt den Versatz des PSG, links und rechts werden gemittelt.

**Kein FM-Ton:** pico-smsplus brachte die FM-Einheit (YM2413, emu2413) mit,
schaltete sie aber nie ein (`sms.use_fm` stand fest auf 0); auch japanische
Spiele spielten nur den PSG-Ton. Seit November 2026 ist sie ganz entfernt
(gut 11 KB Flash, etwa 10 KB RAM); die Ports F0–F2 verhalten sich wie
vorher, Spielstände behalten ihre 64 Byte dafür (Nullen).

Die Samples eines Bildes entstehen nicht mehr in 262 kleinen Stücken nach
jeder Zeile, sondern erst, wenn das Spiel einen Tonchip beschreibt (PSG,
Stereo-Port des Game Gear, AY): dann die bis zur laufenden Zeile, mit
den Registern, wie sie bis dahin waren; der Rest am Ende des Bildes
(`sms_sound_sync()` in `sms.c`). Das sind dieselben Samples, auch für die
Sprachausgabe über die Lautstärke-Register. Einen stummen PSG-Kanal
(Lautstärke 15) rechnet `sn76496.c` nicht mehr Sample für Sample durch,
sondern stellt seinen Zähler in einem Schritt dorthin, wo er sonst gelandet
wäre; bei Periode 0 oder 1 waren das mehrere Durchläufe je Sample.

## Speicher

Zwei Bildpuffer mit Palettenindizes (je 256×224 Byte) und die Paletten der
Zeilen belegen 140 KB (bis November 2026 in Panel-Farben 210 KB). Der Kern
holt sich über `malloc` Tile-Cache, Arbeitsspeicher und Batterie-RAM,
zusammen etwa 80 KB; dafür bleiben gut 210 KB Heap. Das ROM selbst
kostet kein RAM – außer bei SG-1000 und ColecoVision: deren Modul (höchstens
48 bzw. 32 KB) wird nach dem Start in den Heap kopiert, den diese Systeme
ohne Tile-Cache kaum brauchen (`sms_cart_to_ram()`, mit 16 KB Reserve).

### Was im RAM läuft

Das ROM wird direkt aus dem Flash gelesen; Flash und Emulatorcode teilen sich
einen Cache von nur 16 KB. Der Kern läuft deshalb aus dem RAM (rund 80 KB):
Z80 (redcode/Z80 samt Befehlstabellen, siehe unten) mit seiner Anbindung
(`z80_execute`, IN/OUT in `z80_redcode.c`), Speicher- und Port-Zugriffe,
VDP, Zeichenroutinen (auch die des Game Gear, die bis November 2026 im
Flash lag), PSG, die Zeilenausgabe in `src/sms_video.c` und die Warte-
schleifen von Kern 1 auf den Display-DMA (`src/display.c`): aus dem Flash
holte Kern 1 dort ständig Befehle über denselben XIP-Cache, aus dem Kern 0
das ROM liest. Im übernommenen RP2350-Port war das
abgeschaltet (`in_ram` in `core/smsplus/shared.h` war leer), alles lief aus
dem Flash.

Die 64-KB-Tabelle `lut.h`, aus der für jedes deckende Sprite-Pixel an
zufälliger Stelle gelesen wurde, ist durch eine Rechnung von fünf Zeilen
ersetzt (`sprite_over_bg()` in `render.c`, für alle 65536 Eingaben gleiches
Ergebnis, auf dem PC geprüft).

Der Hintergrund im Mode 4 schreibt seine Pixel wortweise auch an ungerade
Adressen (der Cortex-M33 kann das mit LDR/STR; `ALIGN_DWORD` in `render.c`
ist aus) und fragt den Kachel-Cache bei einem Treffer ohne Funktionsaufruf
ab, bei angeschnittenen Spalten einmal statt je Pixel.

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
| `0x600000` | ~9,9 MB | FatFs-Partition = USB-Stick, auch die `.sav`-Dateien (`src/flash_fs.h`) |
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
(hier 45 454 Hz statt 44 100); `sms.save` merkt sich in Bit 1, ob ein
Spiel die obere Hälfte des Batterie-RAMs benutzt hat (`sms.c`).

## Offene Punkte

* **Nicht auf Hardware geprüft.**
* Batteriespeicher nur am PC geprüft (Ablauf und Mapper mit eigenen
  Testmodulen), nicht mit echten Spielen auf dem Gerät.
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
BIOS-Routinen aufrufen; die meisten älteren brauchen das echte (Donkey Kong
etwa ruft sie gleich im ersten Bild). Der Rest des Mini-BIOS ist HALT: ruft
ein Spiel eine Routine, bleibt es dort stehen, und unten auf dem Bildschirm
steht „GAME NEEDS COLECO.ROM“ statt nur Schwarz.

**Super Game Module** (Opcode Games): viele neuere Homebrews brauchen es. Es
schaltet sich ein, sobald ein Spiel seine Ports benutzt, ohne Einstellung:
24 KB RAM über 2000h–7FFFh, solange Bit 0 von Port 53h gesetzt ist; 8 KB RAM
statt des BIOS, solange Bit 1 von Port 7Fh gelöscht ist (Spiele schreiben
0Dh, 0Fh holt das BIOS zurück); ein AY-3-8910 an den Ports 50h (Register),
51h (schreiben) und 52h (lesen), mit halbem CPU-Takt (1,79 MHz), zum PSG
dazugemischt. Der AY ist eine Kopie von `../msx/core/msx1/ay8910.c`
(`core/smsplus/ay8910.c`, eigener Code, GPL-3.0-or-later). Die 32 KB RAM
holt sich nur der ColecoVision-Modus vom Heap, der dort reichlich frei ist
(kein Tile-Cache). Am PC mit einem eigenen Testmodul geprüft (RAM oben und
unten, BIOS zurück, AY-Register lesen, 440-Hz-Ton); mit echten SGM-Spielen
noch nicht.

| PicoBoy | ColecoVision |
|---|---|
| Joystick | Joystick |
| B / A | linker / rechter Feuerknopf |
| Mitte + Hoch/Rechts/Runter/Links | Ziffern 1, 2, 3, 4 |
| Mitte + B + Hoch/Rechts/Runter/Links | Ziffern 5, 6, 7, 8 |
| Mitte + A + Hoch/Rechts/Runter/Links | 9, 0, `*`, `#` |

Eine Ziffer zählt, sobald die Tasten drei Bilder lang (50 ms) gleich
bleiben; solange Mitte gedrückt ist, bewegt der Joystick nichts. Nach dem
Loslassen bleibt die Ziffer noch eine halbe Sekunde gedrückt: Manche Spiele
fragen das Ziffernfeld nur selten ab – Donkey Kong nach dem letzten Leben
etwa alle Drittelsekunde –, sonst ging ein kurzer Druck auf `*` oder `#`
verloren (das Spiel schien zu hängen).

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
* `core/smsplus/ay8910.[ch]`: AY-3-8910 fürs Super Game Module, Kopie aus
  `../msx` (eigener Code für den PicoBoy, GPL-3.0-or-later).
