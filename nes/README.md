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
| Ton über HDMI/I2S/PWM | Ton über den **PWM-Lautsprecher** (GP15), 22 222 Hz, an/aus und Lautstärke in den Optionen |
| Spielstände (Batterie-RAM) auf SD | **`<Spiel>.sav`** neben der ROM auf dem USB-Laufwerk, wie bei [`../gbc`](../gbc/) |
| Game Genie (Codes eintippen) | Codes in **`<Spiel>.cht`** neben der ROM |
| Einstellungen, Speicherstände, USB-HID | entfallen |

**Stand:** läuft auf dem Gerät, Ton klingt gut.

Die Oberfläche auf dem Gerät ist englisch (Menüs, Optionen, Meldungen, Hilfedatei auf dem Laufwerk).

## Benutzung

1. [`../dist/nes.uf2`](../dist/nes.uf2) flashen: BOOTSEL halten, USB anstecken,
   Datei auf das Laufwerk `RPI-RP2` kopieren.
2. **ROMs aufspielen:** beim Einschalten **A oder B** gedrückt halten. Das Gerät
   meldet sich als USB-Stick `PBC-ROMS`. `.nes`-Dateien draufkopieren und das Laufwerk
   **auswerfen** – der PicoBoy startet von selbst neu. Beim allerersten Start
   (leeres Laufwerk) landet man automatisch in diesem Modus.
3. Ohne gedrückte Taste einschalten: Spieleliste. Hoch/Runter wählt,
   Links/Rechts blättert, **A** oder **Mitte** startet.
   Ganz oben steht immer **„Add games via USB“**: führt direkt in den
   USB-Stick-Modus, ohne Neustart mit gedrückter Taste. Beim Öffnen ist das
   erste Spiel markiert.
4. **B** in der Spieleliste öffnet die Optionen: Ton an/aus und Lautstärke
   (1–12, Vorgabe 8). Beides bleibt gespeichert und lässt sich **nur hier**
   ändern, nicht im Spiel -- damit nichts versehentlich verstellt wird.

**Gemeinsames Laufwerk mit `megadrive`:** Beide Firmwares benutzen dieselbe
Flash-Aufteilung. NES- und Genesis-ROMs können also zusammen auf dem Laufwerk
liegen; jede Firmware listet nur ihre eigenen. Nach einem Wechsel der Firmware
wird das erste gestartete Spiel einmal neu in den ROM-Bereich geschrieben.

### Steuerung im Spiel

Vor jedem Spielstart zeigt ein Kasten zwei Sekunden lang diese Belegung, wie
bei PicoBoyGB.

| NES | PicoBoy |
|---|---|
| Steuerkreuz | Joystick |
| A | B |
| B | A |
| START | Mitte + B |
| SELECT | Mitte + A |

Mitte allein tut nichts, wie bei PicoBoyGB. Zurück zur Spieleliste geht es
mit RESET. In PBCemu tauscht die Zeile „B A“ in den Einstellungen des Spiels
A und B.

### Spielstände

Module mit Batterie (Bit im iNES-Kopf: Zelda, Final Fantasy, StarTropics …)
behalten die 8 KB RAM bei `$6000` in **`<Spielname>.sav`** neben der ROM, am PC
sichtbar und kopierbar. Geladen wird beim Start, geschrieben während des
Spiels, denn verlassen wird es mit RESET: Viermal pro Sekunde eine Prüfsumme
des RAM; hat es sich eine Sekunde lang nicht mehr geändert (oder ändert es sich
seit 20 s), wird die Datei geschrieben. **Nach dem Speichern im Spiel also kurz
warten**, bevor man RESET drückt. Unveränderte Flash-Sektoren bleiben stehen,
Kern 1 wird je geschriebenem Sektor in einer RAM-Schleife geparkt
(`src/flash_fs.c`, wie bei `../gbc`); das Spiel hält dabei etwa 50 ms an. Passt
die Datei nicht mehr auf das Laufwerk (jede Datei belegt mindestens 16 KB),
steht unten „NO ROOM FOR THE SAVE FILE“ und das Spiel läuft ohne Speichern
weiter.

### Game Genie

Codes stehen in **`<Spielname>.cht`** neben der ROM (für `Contra (USA).nes`
also `Contra (USA).cht`), ein Code pro Zeile, 6 oder 8 Buchstaben, groß oder
klein; was nach einem Leerzeichen oder `#` folgt, ist Kommentar:

```
SXIOPO   unendlich Leben
AEKPTZGA # 8 Buchstaben: mit Vergleichsbyte
```

Bis zu 16 Codes. Beim Start steht drei Sekunden lang „3 CHEATS“ unter dem Bild
(„NO USABLE CHEATS“, wenn keiner gültig ist). Die Codes wirken wie das echte
Game Genie auf ihre **CPU-Adresse** und, bei 8 Buchstaben, nur solange dort
eine Bank mit dem Vergleichsbyte eingeblendet ist. Da die ROM im Flash liegt
und nicht verändert werden kann, zeigt der CPU-Kern die 2-KB-Seite mit einem
Code nach jedem Einblenden auf eine RAM-Kopie der ROM-Seite mit dem Code
darin (`Nes_Cpu::gg_remap` in `core/quicknes/nes_cpu.cpp`). Acht solche Kopien
werden behalten (16 KB Heap, nur mit Codes), so dass ein Bankwechsel hin und
zurück nichts kopiert; ohne Codes kostet es nichts. Im Link-Spiel werden keine
Codes geladen. Die Hilfedatei `NES.TXT` erklärt die Datei auch.

**Leistungsanzeige:** gezeichnete Bilder pro Sekunde, Emulationstempo (100 % =
Originalgeschwindigkeit) und Auslastung von Kern 0, z. B. `60fps 100% CPU71%`,
einmal pro Sekunde aufgefrischt, unter dem Bild. In PBCemu schaltet man sie in den Einstellungen des Starters ein;
die Einzel-Firmware zeigt sie, wenn mit `-DSHOW_FPS=1` gebaut.

## Bild

Das NES-Bild ist 256×240. Wie es aufs 240×280-Display kommt, ist der
Anzeigemodus; in PBCemu wählt man ihn je Gerät in den Einstellungen des
Starters, die Einzel-Firmware nutzt immer den ersten:

| Modus | Bild | Was passiert |
|---|---|---|
| Crop 1:1 | 240×240 | links und rechts je 8 Spalten weg – der Streifen, den ein Fernseher im Überscan versteckt und in dem viele Spiele Scroll-Reste zeigen; ohne Skalierung (Vorgabe) |
| Fit width | 240×225 | das ganze Bild, auf 15/16 verkleinert; wo sich zwei Pixel oder Zeilen einen Platz teilen, werden sie gemittelt |
| No overscan | 240×224 | wie Crop 1:1, zusätzlich oben und unten je 8 Zeilen weg, so viel wie ein Fernseher zeigte |

Kein Modus verzerrt das Bild. Es sitzt senkrecht mittig, der Rest bleibt schwarz.

QuickNES zeichnet abwechselnd in zwei Pixelpuffer. Kern 0 übergibt ein fertiges
Bild samt Palette an Kern 1. Der rechnet es Zeile für Zeile in BGR565 um und
schickt es per DMA, während Kern 0 schon das nächste Bild in den anderen Puffer
emuliert. Bei 150 MHz läuft der SPI mit 75 MHz; ein Bild braucht rund 12 ms.

## Ton

Ein DMA-Kanal, getaktet von einem DMA-Timer, holt alle 45 µs einen Sample aus
einem Ringpuffer und schreibt ihn in den PWM-Wert von GP15 (10 Bit, Träger
~146 kHz bei 150 MHz) – ohne Interrupt pro Sample; vorher lief dafür 22 222-mal
pro Sekunde ein Timer-Interrupt, zum Teil aus dem Flash. QuickNES erzeugt genau
diese Rate (22 222 Hz). Weil Bildtakt und Sample-Takt zwei getrennte Uhren
sind, wird der Füllstand bei jedem Bild geprüft und um einen Sample korrigiert;
hinter dem letzten Sample steht Stille, so hört man bei Rückstand nichts statt
alter Samples. Laute Spitzen werden weich begrenzt. Ist der Ton aus, steht der
DMA, und der Lautsprecher-Pin liegt auf Low (kein Ruhestrom).

## Mapper

QuickNES aus frank-nes kennt die Mapper 0–5, 7, 9–11, 15, 19, 21–26, 30,
32–34, 60, 66, 69–71, 73, 75, 78, 79, 85–89, 93, 94, 97, 113, 140, 152, 154,
156, 180, 184, 190, 193, 206, 207, 232, 240, 241, 244 und 246. Für den PicoBoy
neu geschrieben (nach dem NESdev-Wiki, Dateien `core/quicknes/mappers/`):

| Mapper | Platine | Beispiele |
|---|---|---|
| 16, 159 | Bandai FCG / LZ93D50 | Dragon Ball Z, SD Gundam (ohne EEPROM: kein Speichern) |
| 28 | Action 53 | Homebrew-Sammlungen (nur 8 KB CHR-RAM) |
| 48 | Taito TC0690 | The Flintstones: The Rescue of Dino & Hoppy, Don Doko Don 2 |
| 64 | Tengen RAMBO-1 | Klax, Shinobi, Rolling Thunder, Skull & Crossbones |
| 67 | Sunsoft-3 | Fantasy Zone II (Japan) |
| 118 | TxSROM (MMC3, Nametables aus den CHR-Bänken) | Armadillo, Pro Sport Hockey |
| 119 | TQROM (MMC3, 64 KB CHR-ROM und 8 KB CHR-RAM gemischt) | Pin-Bot, High Speed |
| 185 | CNROM mit CHR-Sperre (Kopierschutz) | B-Wings, Bird Week, Mighty Bomb Jack, Spy vs. Spy |
| 228 | Active Enterprises | Cheetahmen II |

48, 64, 118 und 119 sind Ableitungen der MMC3-Klasse; deren Bankfunktionen sind
dafür virtuell. Für 119 legt die PPU das CHR-RAM hinter das CHR-ROM in einen
gemeinsamen Adressraum samt Kachel-Cache (`open_chr`); ein Schreiben über
`$2007` landet nur dort, wo eine RAM-Bank eingeblendet ist. Für 185 liest
`$2007` bei gesperrtem CHR `$FF`; gesperrt ist es wie bei FCEUX, wenn die
Bits 0–1 null sind oder der Wert `$13` ist. Geprüft mit eigenen Testmodulen
auf dem PC (Bankumschaltung, Spiegelung, Zahl der IRQs, beim 119 auch das
Zeichnen aus RAM- und ROM-Kacheln), noch nicht mit den Spielen.
Nicht machbar ohne Umbau der PPU: 68 (Sunsoft-4, Nametables aus CHR-ROM)
und 13 (CPROM, 16 KB CHR-RAM in Bänken: das Schreiben über `$2007` ginge
durch die Bankumschaltung im heißen Pfad; dafür gibt es nur Videomation);
37 und 47 (MMC3-Sammelmodule) und Action 52 haben mehr CHR-ROM, als der
Kachel-Cache fasst.

## Speicher

QuickNES dekodiert das CHR-ROM einmal in einen **Kachel-Cache** im RAM, doppelt
so groß wie das CHR-ROM. Nach Abzug von Firmware und Pixelpuffern bleiben etwa
328 KB Heap. Damit laufen Spiele bis **128 KB CHR-ROM** (z. B. Super Mario Bros. 3)
und alle Spiele mit CHR-RAM (z. B. Zelda, Mega Man 2). Spiele mit 256 KB CHR-ROM
(z. B. Kirby's Adventure) passen nicht; sie melden „Cannot load game“ statt
abzustürzen.

Was pro Befehl, Zeile oder Sample läuft, liegt im RAM (`NES_RAM` in
`blargg_common.h`, zusammen 18 KB): neben `Nes_Cpu::run` die Speicher- und
I/O-Zugriffe der CPU (`cpu_read`/`cpu_write`, PPU- und APU-Register), der
Zeilenzeichner (`Nes_Ppu_Rendering`), die Tonkanäle samt `Blip_Buffer` und
die Mapper MMC1/MMC3 (001/004); dazu die Taktzahltabelle der CPU. Aus dem
Flash konkurrierte das mit dem Spiel-ROM um den 16-KB-XIP-Cache. Den Platz
dafür geben Puffer frei, die nur beim Laden gebraucht werden: FatFs im
„tiny“-Modus (die Dateien teilen den Sektorpuffer des Laufwerks), und
Formatieren, ROM-Prüfung und USB-Stick-Modus holen ihren Puffer vom Heap.
Der Heap ist dadurch sogar etwas größer als vorher.

## Bauen

```bash
./build.sh                       # -> ../dist/nes.uf2
./build.sh clean
./build.sh -DCPU_MHZ=200 -DCPU_VREG=VREG_VOLTAGE_1_15
```

Werkzeuge und SDK wie bei allen Projekten dieses Repos, siehe
[`../docs/BAUEN.md`](../docs/BAUEN.md).

| Option | Vorgabe | Bedeutung |
|---|---|---|
| `CPU_MHZ` | 150 | Systemtakt (Grundtakt, wie C64 und Master System) |
| `CPU_VREG` | `VREG_VOLTAGE_1_10` | Kernspannung (bei 150 MHz ohnehin die Grundeinstellung) |
| `FLASH_MAX_FREQ_MHZ` | 88 | Obergrenze für den Flash-Takt |
| `PBC_SPI_HZ` | 75 000 000 | Display-SPI: 150 MHz / 2 = 75 MHz (über dem Datenblattwert, läuft auf dem Gerät sauber) |
| `SHOW_FPS` | 0 | Leistungsanzeige in der Einzel-Firmware (in PBCemu: Einstellung im Starter) |

Das NES braucht viel weniger Rechenleistung als das Mega Drive. Zeigt die
Anzeige wenig CPU-Last, lohnt ein niedrigerer Takt für die Batterie.

## Flash-Aufteilung (16 MB)

| Adresse | Größe | Inhalt |
|---|---|---|
| `0x000000` | 1 MB | Firmware (ca. 170 KB) |
| `0x100000` | 5 MB | ROM des laufenden Spiels (`src/rom_store.c`) |
| `0x600000` | ~9,9 MB | FatFs-Partition = USB-Stick (`src/flash_fs.h`) |
| `0xFF0000` | 4 KB | Info, welches Spiel im ROM-Bereich liegt |
| `0xFF1000` | 4 KB | Optionen (Ton an/aus, Lautstärke) |

## Aufbau

```
src/main.c        Startablauf, USB-Stick-Modus, Takt, Emulationsschleife, Spielstände, Game-Genie-Datei
src/video.c       Palette, Ausgabe 240x240 auf Kern 1
src/audio_pwm.c   Ton: Ringpuffer, Timer, PWM auf GP15
src/settings.c    Optionen, im Flash gespeichert
src/input.c       Tastenbelegung (Mitte als Modifier)
src/display.c, menu.c, rom_store.c   aus ../megadrive (rom_store ohne Byte-Tausch)
src/flash_fs.*, usb_*.c, tusb_config.h, fatfs/   aus ../picoboygb
core/quicknes/    QuickNES-Kern aus frank-nes; entfernt: das Übernehmen von Sprites aus dem Vorbild (HDMI-Notlösung, löschte das Blinken); neu: die Mapper oben, gemischtes CHR-ROM/-RAM und abschaltbares CHR in der PPU, qnes_get_battery_ram(), Game Genie über RAM-Seiten im CPU-Kern (qnes_set_game_genie(), eigener Decoder statt des fehlerhaften in nes_util.cpp)
core/emu2413/     YM2413-Emulation für den VRC7-Mapper, aus frank-nes
```

Sonst ist der Kern unverändert. Mit `PICO_MALLOC_PANIC=0` darf `malloc` scheitern,
damit QuickNES fehlenden Speicher selbst melden kann.

## Offene Punkte

* Spielstände noch nicht auf dem Gerät geprüft. Im Link-Spiel werden sie
  weder geladen noch gespeichert (beide Geräte brauchen dasselbe RAM).
* Game Genie: Codes auf mehr als acht verschiedenen 2-KB-Seiten zugleich
  greifen nicht alle (es gibt acht Kopien). Auf dem Gerät nicht geprüft.
* Nur NTSC-Takt, kein zweiter Spieler.

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
