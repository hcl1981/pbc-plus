# PBC Mega Drive

Sega-Genesis-/Mega-Drive-Emulator für den **PicoBoy Color Plus** (RP2350A,
512 KB RAM, 16 MB Flash, ST7789 240×280, Joystick + A/B).

Grundlage ist der Emulatorkern von
[frank-genesis](https://github.com/rh1tech/frank-genesis) (Gwenesis-basiert).
Der braucht eigentlich 8 MB PSRAM, HDMI, SD-Karte und I2S-Ton – nichts davon hat
der PBC+. Deshalb ist hier alles drumherum ersetzt:

| frank-genesis | PBC Mega Drive |
|---|---|
| ROM im PSRAM (4 MB) | ROM im **Flash**, wird direkt per XIP ausgeführt |
| ROMs von SD-Karte | ROMs per **USB-Stick-Modus** in eine FatFs-Partition im Flash (aus [PicoBoyGB](../picoboygb/)) |
| HDMI 320×240 | **ST7789**: Bild auf 240 Pixel Breite verkleinert, wird schon während des Zeichnens per DMA gesendet |
| YM2612 + SN76489 über I2S | **kein Ton**: Klangerzeugung abgeschaltet, Z80 und YM2612-Timer laufen weiter |
| Einstellungsmenü, SNES-Pad, USB-HID | entfallen |

**Stand:** läuft auf dem Gerät mit 101 % Tempo bei 300 MHz (~34 gezeichnete Bilder/s).
Ein Versuch mit Ton über den PWM-Lautsprecher lief langsamer und instabil und
wurde wieder entfernt.

## Benutzung

1. [`../dist/megadrive.uf2`](../dist/megadrive.uf2) flashen: BOOTSEL halten, USB anstecken, Datei auf
   das Laufwerk `RPI-RP2` kopieren.
2. **ROMs aufspielen:** beim Einschalten **A oder B** gedrückt halten. Das Gerät
   meldet sich als USB-Stick `PBC-MD`. ROMs (`.md`, `.bin`, `.gen`, `.smd`, bis
   5 MB) draufkopieren und das Laufwerk **auswerfen** – der PicoBoy startet von
   selbst neu. Beim allerersten Start (leeres Laufwerk) landet man automatisch
   in diesem Modus.
3. Ohne gedrückte Taste einschalten: Spieleliste. Hoch/Runter wählt,
   Links/Rechts blättert, **A** oder **Mitte** startet.
   Das Spiel, das gerade im Flash liegt, ist gelb markiert.
Beim ersten Start eines Spiels wird es in den ROM-Bereich des Flash
geschrieben (Fortschrittsbalken, bei 4 MB etwa eine halbe Minute) und danach
geprüft. Startet man dasselbe Spiel wieder, entfällt das.

### Steuerung im Spiel

| Genesis | PicoBoy |
|---|---|
| Steuerkreuz | Joystick |
| A | A |
| B | B |
| C | Mitte |
| START | Mitte + B |
| SELECT | Mitte + A (1 s halten = zurück zur Spieleliste) |

Mitte wird erst nach 40 ms als C gemeldet. So wird aus „Mitte, kurz danach B“
sicher START und kein versehentliches C. Solange Mitte gehalten wird, sind A und
B für die Kombinationen reserviert: **C gleichzeitig mit A oder B geht nicht.**

Unter dem Bild steht z. B. `34fps 101% CPU93%`: gezeichnete Bilder pro
Sekunde, Emulationstempo (100 % = Originalgeschwindigkeit) und wie viel der
Zeit Kern 0 arbeitet statt auf das nächste Bild zu warten. Bleibt CPU deutlich
unter 100 %, ist Luft zum Heruntertakten.

## Bauen

```bash
./build.sh                       # -> ../dist/megadrive.uf2
./build.sh clean                 # Bauordner neu anlegen
./build.sh -DCPU_MHZ=336 -DCPU_VREG=VREG_VOLTAGE_1_35
```

Werkzeuge und SDK wie bei allen Projekten dieses Repos, siehe
[`../docs/BAUEN.md`](../docs/BAUEN.md). Gebaut wird als Release (`-O2`/`-O3`)
statt der sonst üblichen MinSizeRel-Vorgabe.

### Stellschrauben (CMake-Optionen)

| Option | Vorgabe | Bedeutung |
|---|---|---|
| `CPU_MHZ` | 300 | Systemtakt |
| `CPU_VREG` | `VREG_VOLTAGE_1_30` | Kernspannung. Über 1,30 V wird die Sperre des RP2350 aufgehoben |
| `FLASH_MAX_FREQ_MHZ` | 88 | Obergrenze für den Flash-Takt (bei 300 MHz: 75 MHz) |
| `PBC_SPI_HZ` | 62 500 000 | Display-SPI. Der Teiler hängt vom Systemtakt ab: bei 300 MHz werden daraus 50 MHz, mit `75000000` 75 MHz |
| `SHOW_FPS` | 1 | fps-Anzeige unter dem Bild |
| `Z80_SLICE_LINES` | 16 | Z80 alle N Zeilen ausführen |

**Vorsicht beim Takt:** Der Spannungswandler des PBC+ ist klein (150–250 mA).
PicoBoyGB läuft mit 250 MHz bei 1,25 V. Höhere Werte erst am USB-Kabel
ausprobieren, dann mit Batterien.

## Flash-Aufteilung (16 MB)

| Adresse | Größe | Inhalt |
|---|---|---|
| `0x000000` | 1 MB | Firmware (ca. 650 KB) |
| `0x100000` | 5 MB | ROM des laufenden Spiels, byte-getauscht (`src/rom_store.c`) |
| `0x600000` | ~9,9 MB | FatFs-Partition = USB-Stick (`src/flash_fs.h`) |
| `0xFF0000` | 4 KB | Info, welches Spiel im ROM-Bereich liegt |

Ein neues Firmware-UF2 lässt ROM-Bereich und USB-Laufwerk unangetastet.

## Aufbau

```
src/main.c        Startablauf, USB-Stick-Modus, Takt, Emulationsschleife
src/video.c       Palette, Skalierer, zeilenweise Ausgabe auf Core 1
src/display.c     ST7789-Treiber (DMA-Strom + Text), wie in ../spout und ../noiz2sa
src/input.c       Tastenbelegung mit Mitte als C/Modifier
src/menu.c        Spieleliste, Meldungen, Fortschrittsbalken
src/rom_store.c   ROM aus FatFs in den Flash schreiben (inkl. .smd), prüfen
src/HDMI.h        Ersatz für den HDMI-Header, den der VDP-Kern einbindet
src/flash_fs.*, usb_*.c, tusb_config.h, fatfs/   aus ../picoboygb
core/             Emulatorkern aus frank-genesis
```

Änderungen am Kern sind auf zwei Stellen beschränkt, beide hinter
`PBC_NO_AUDIO`: `ym2612_run()` erzeugt keine Samples mehr, lässt aber Timer A/B
weiterlaufen (Soundtreiber fragen deren Überlauf-Bits ab), und
`gwenesis_SN76489_run()` zählt nur noch die Zeit weiter.

### Wie das Bild aufs Display kommt

Die VDP zeichnet 8-Bit-Palettenindizes in einen 320×240-Puffer – mit der
Zeilenbreite des aktuellen Modus (256 oder 320) als Schrittweite. Core 0
emuliert, Core 1 zeichnet die jeweils vorige Zeile (wie in frank-genesis). Sobald
die Quellzeilen einer Ausgabezeile fertig sind, rechnet Core 1 sie in BGR565 um
und schickt sie per DMA. Das ganze Bild liegt nie als RGB im RAM.

| Modus | Quelle | Ausgabe | Faktor |
|---|---|---|---|
| H40 NTSC | 320×224 | 240×168 | ¾ |
| H40 PAL | 320×240 | 240×180 | ¾ |
| H32 NTSC | 256×224 | 240×210 | 15/16 |
| H32 PAL | 256×240 | 240×225 | 15/16 |

Wegfallende Pixel werden nicht einfach ausgelassen. Die beiden Quellpixel bzw.
-zeilen, die sich ein Ausgabepixel teilen, werden gemittelt, damit kleine
Schrift lesbar bleibt. Palettenwechsel mitten im Bild (Rasterfarben) werden pro
Zeile berücksichtigt.

Ist die Emulation mehr als ein Bild im Rückstand, wird das Zeichnen
übersprungen (höchstens drei Bilder am Stück). Die Emulation selbst läuft
immer vollständig.

## Offene Punkte

* **Kein Ton.** Ein Versuch (PWM per DMA, 53 kHz) lief langsamer und instabil.
* **Keine Spielstände.** Der Kern emuliert kein batteriegepuffertes
  Cartridge-SRAM. Spiele mit Speicherfunktion verlieren ihren Stand beim
  Ausschalten.
* Kein Mapper für Super Street Fighter II (> 4 MB).
* Kein 6-Button-Pad, kein zweiter Spieler.

## Lizenzen

* Emulatorkern (`core/`) und damit das Gesamtwerk: **AGPL-3.0** wie
  frank-genesis, siehe `LICENSE`.
* `src/flash_fs.*`, `src/usb_msc.c`, `src/usb_descriptors.c`,
  `src/tusb_config.h`, `src/font8x8.h`, `fatfs/diskio.c`: aus PicoBoyGB,
  **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
