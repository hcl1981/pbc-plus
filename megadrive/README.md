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
| YM2612 + SN76489 über I2S | Ton über den **PWM-Lautsprecher**, synthetisiert auf Core 1 |
| Einstellungsmenü, SNES-Pad, USB-HID | entfallen |

**Stand:** läuft auf dem Gerät mit Ton bei 200 MHz in vollem Tempo; Sonic 2:
`47fps 100% CPU99% Z13 V27 S47`. Ohne Ton (in PBCemu abschaltbar) rund 55 %
Last. Mit Ton bei 300 MHz / 1,30 V startete das Gerät neu (Spannungswandler,
siehe unten), bei 200 MHz / 1,15 V nicht.

Die Oberfläche auf dem Gerät ist englisch (Menüs, Optionen, Meldungen, Hilfedatei auf dem Laufwerk).

## Benutzung

1. [`../dist/megadrive.uf2`](../dist/megadrive.uf2) flashen: BOOTSEL halten, USB anstecken, Datei auf
   das Laufwerk `RPI-RP2` kopieren.
2. **ROMs aufspielen:** beim Einschalten **A oder B** gedrückt halten. Das Gerät
   meldet sich als USB-Stick `PBC-ROMS`. ROMs (`.md`, `.bin`, `.gen`, `.smd`, bis
   5 MB) draufkopieren und das Laufwerk **auswerfen** – der PicoBoy startet von
   selbst neu. Beim allerersten Start (leeres Laufwerk) landet man automatisch
   in diesem Modus.
3. Ohne gedrückte Taste einschalten: Spieleliste. Hoch/Runter wählt,
   Links/Rechts blättert, **A** oder **Mitte** startet.
   Das Spiel, das gerade im Flash liegt, ist gelb markiert.
   Ganz oben steht immer **„Add games via USB“**: führt direkt in den
   USB-Stick-Modus, ohne Neustart mit gedrückter Taste. Beim Öffnen ist das
   erste Spiel markiert.
Beim ersten Start eines Spiels wird es in den ROM-Bereich des Flash
geschrieben (Fortschrittsbalken, bei 4 MB etwa eine halbe Minute) und danach
geprüft. Startet man dasselbe Spiel wieder, entfällt das.

### Steuerung im Spiel

Vor jedem Spielstart zeigt ein Kasten zwei Sekunden lang diese Belegung, wie
bei PicoBoyGB.

| Genesis | PicoBoy |
|---|---|
| Steuerkreuz | Joystick |
| A | A |
| B | B |
| C | Mitte |
| START | Mitte + B |

Mitte wird erst nach 40 ms als C gemeldet. So wird aus „Mitte, kurz danach B“
sicher START und kein versehentliches C. Solange Mitte gehalten wird, sind A und
B für die Kombinationen reserviert: **C gleichzeitig mit A oder B geht nicht.**

Zurück zur Spieleliste geht es mit RESET.

Mit `-DSHOW_FPS=1` gebaut, steht unter dem Bild z. B. `47fps 100% CPU99% Z13 V27 S47`:
gezeichnete Bilder pro Sekunde, Emulationstempo (100 % = Originalgeschwindigkeit),
wie viel der Zeit Kern 0 arbeitet statt auf das nächste Bild zu warten, davon
Z im Z80 und V im Warten auf Kern 1 bzw. beim selbst Zeichnen einer Zeile;
S ist der Anteil von Kern 1 für den Ton. Bleibt CPU deutlich unter 100 %, ist
Luft zum Heruntertakten. In PBCemu schaltet man die Anzeige in den
Einstellungen des Starters ein.

**Ton:** in PBCemu in den Optionen ein-/ausschaltbar, mit Lautstärke. Die
Einzel-Firmware hat kein Optionsmenü und spielt mit Lautstärke 8
(`-DAUDIO_VOLUME=0` schaltet den Ton ab).

**Absturzanzeige:** Läuft ein Kern auf einen Fehler, zeigt das Display
`CRASH on core …` mit PC, LR und CFSR, statt still neu zu starten. Die
Adressen lassen sich in `build/pbc_megadrive.elf` nachschlagen.

**Anzeigemodi** (in PBCemu wählbar, die Einzel-Firmware nutzt „Fit width“):

| Modus | Bild |
|---|---|
| Fit width | Breite auf 240 skaliert (H40 ¾, H32 15/16, gemittelt), Seitenverhältnis bleibt |
| Crop 1:1 | Pixel 1:1, die mittleren 240 Pixel jeder Zeile (H40 verliert links/rechts je 40) |

## Bauen

```bash
./build.sh                       # -> ../dist/megadrive.uf2
./build.sh clean                 # Bauordner neu anlegen
./build.sh -DCPU_MHZ=250 -DCPU_VREG=VREG_VOLTAGE_1_25
```

Werkzeuge und SDK wie bei allen Projekten dieses Repos, siehe
[`../docs/BAUEN.md`](../docs/BAUEN.md). Gebaut wird als Release (`-O2`/`-O3`)
statt der sonst üblichen MinSizeRel-Vorgabe.

### Stellschrauben (CMake-Optionen)

| Option | Vorgabe | Bedeutung |
|---|---|---|
| `CPU_MHZ` | 200 | Systemtakt (unter 150 wird heruntergetaktet) |
| `CPU_VREG` | `VREG_VOLTAGE_1_15` | Kernspannung. Über 1,30 V wird die Sperre des RP2350 aufgehoben |
| `FLASH_MAX_FREQ_MHZ` | 100 | Obergrenze für den Flash-Takt (bei 200 und 300 MHz: 100 MHz) |
| `PBC_SPI_HZ` | 75 000 000 | Display-SPI. Im Spiel kommt `clk_peri` aus der USB-PLL (150 MHz, USB wird dann nicht gebraucht), daher 75 MHz bei jedem Systemtakt (über dem Datenblattwert, läuft auf dem Gerät sauber) |
| `SHOW_FPS` | 0 | Anzeige unter dem Bild (fps, Tempo, Last) |
| `AUDIO_VOLUME` | 8 | Lautstärke der Einzel-Firmware, 0 = aus |
| `TEST_SONIC2_ZONE` | -1 | Nur zum Testen: drückt START, bis ein Level läuft, und startet Sonic 2 in dieser Zone/diesem Akt (z. B. `0x0D01` = Chemical Plant 2, `0x0F00` = Aquatic Ruin 1, `0x1000` = Spezialstufe nach 100 Bildern im ersten Level). Stört andere Spiele |
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
src/ym_core1.c    Ton auf Core 1: Schreibprotokoll abspielen, mischen, auf 22 kHz
src/ym2612_synth.c  zweite Übersetzung von core/sound/ym2612.c für Core 1
src/audio_pwm.c   PWM-Lautsprecher per DMA, wie in ../sms
tools/m68k_dispatch.py  packt die 68000-Befehlstabellen für den RAM
tools/m68k_hot.py       wählt die 68000-Befehlsroutinen für den RAM aus
tools/host/       mdhost: der Kern als PC-Programm, für Bildfehler (eigene README)
src/HDMI.h        Ersatz für den HDMI-Header, den der VDP-Kern einbindet
src/flash_fs.*, usb_*.c, tusb_config.h, fatfs/   aus ../picoboygb
core/             Emulatorkern aus frank-genesis
```

### Änderungen am Kern

Fast alles betrifft die Geschwindigkeit. Der Flash hat nur 16 KB Cache, und
aus ihm kommen Spiel-ROM und Emulatorcode zugleich; alles, was pro Befehl
oder pro Sample gebraucht wird, liegt deshalb im RAM.

- **68000:** Die Befehlstabelle (Routine je Opcode) und die Taktzahltabelle
  lagen mit zusammen 320 KB im Flash; jeder Befehl verfehlte meist den Cache.
  Die Taktzahl hängt nur von der Routine ab, und viele 32er-Abschnitte der
  Tabelle wiederholen sich. `tools/m68k_dispatch.py` packt beides deshalb
  verlustfrei in drei Tabellen mit zusammen 42 KB
  (`core/cpus/M68K/m68ki_dispatch.h`), die im RAM liegen, ebenso
  `m68k_run()`. Nebenbei gefunden: Die alte Tabelle endete bei $EFBF,
  Line-F-Opcodes lasen dahinter einen falschen Sprungzeiger.
- **Z80:** Der Assembler-Kern aus frank-genesis war ein Zwischenstand. Er
  verglich jeden Opcode der Reihe nach mit bis zu 240 Werten und gab beim
  ersten fehlenden an den C-Kern im Flash ab. Jetzt läuft nur der C-Kern,
  samt Tabellen im RAM (`PBC_Z80_IN_RAM`). Z-Anteil bei Sonic 2: 61 % → 8 %.
- **ROM-Zwischenspeicher entfernt:** frank-genesis kopierte ROM-Seiten (68000)
  und 32-KB-Bänke (Z80) aus dem PSRAM in den RAM. Aus dem Flash ist das nur
  eine Kopie des XIP-Caches; die Z80-Bänke kosteten bei jedem Wechsel 32 KB
  Kopie, und bei SSF2 hätten die ROM-Seiten nach einem Bankwechsel veraltet
  sein können. Beide lesen jetzt direkt; das macht 96 KB RAM frei.
- **68000-Befehlsroutinen:** Alle 1700 passen nicht in den RAM.
  `tools/m68k_hot.py` wählt nach Befehlsart (Move, Sprünge, Vergleiche, …
  zuerst) und Adressierung (Register, (An), (An)+, -(An) zuerst) aus: 17 KB
  in beiden Firmwares (`M68K_HOT1`), weitere 30 KB nur in PBCemu
  (`M68K_HOT2`), wo mehr RAM frei ist. Ändert sich der Kern, das Skript mit
  einer Bau-.elf neu laufen lassen.
- **VDP:** `gwenesis_vdp_render_line()` und `gwenesis_vdp_write_memory_16()`
  im RAM.
- **VBlank-Interrupt:** kommt jetzt wie in Genesis Plus GX 788 Takte nach
  Beginn von Zeile 224 statt an ihrem Anfang. Vorher wurden ein
  Zeilen-Interrupt aus Zeile 223 und der VBlank-Interrupt gleichzeitig
  ausgelöst, und der VBlank-Handler lief zuerst. Sonic 2 (Aquatic Ruin)
  lädt bei Wasser unterhalb des Bildes in Zeile 223 die Unterwasserpalette
  und im VBlank die normale; in falscher Reihenfolge blieb das nächste Bild
  „unter Wasser“, im Wechsel mit normalen Bildern (Flackern).
- **Palette mitten im Bild:** Jede Zeile bekommt die Palette vom
  Zeilenanfang (wie Genesis Plus GX), nicht vom Zeilenende. Eine
  Palettenübertragung, die über mehrere Zeilen läuft und dabei lauter
  verschiedene Einträge schreibt, zählt als ein Wechsel. Sonic 2 schreibt
  die 64 Unterwasserfarben über zwei Zeilen; die erste zeigte halb alte,
  halb neue Farben, eine lila Linie an der Wasseroberfläche. Schreibt ein
  Spiel denselben Eintrag Zeile für Zeile neu (Farbverlauf), bleibt jede
  Zeile ein eigener Wechsel.
  Zu Beginn der Bildpause friert `video_palette_freeze()` die Farbtabelle
  für die Zeilen ein, die Kern 1 noch umrechnet: Sonic 2 lädt die
  Unterwasserfarben auch bei Wasser unterhalb des Bildes, in der
  Bildpause, und die letzten Zeilen bekamen sie sonst ab (eine springende
  lila Linie unten im Bild).
- **PSG-Zeitstempel vom Z80:** `z80_mem_opt.S` rechnete die restlichen
  Z80-Takte mal 2 statt mal 15 in Master-Takte um.
- **Display-Takt:** Der SPI-Takt ist `clk_peri` geteilt durch eine gerade
  Zahl, und `clk_peri` folgt dem Systemtakt; bei 200 MHz blieben 50 MHz.
  Im Spiel versorgt deshalb die USB-PLL mit 150 MHz `clk_peri` allein
  (`display_clock()`): 75 MHz bei jedem Systemtakt.
- **Ton per DMA:** Ein DMA-Kanal mit DMA-Timer schiebt alle 45 µs ein Sample
  in den PWM-Vergleichswert; vorher lief dafür 22222-mal pro Sekunde ein
  Timer-Interrupt, zum Teil aus dem Flash.
- **Region:** Die Firmware lädt das ROM nicht über `load_cartridge()`, die
  Regionserkennung des Kerns lief deshalb nie, und jede Konsole war
  NTSC-USA. Jetzt wählt `set_region()` sie nach dem Header (USA vor Europa
  vor Japan). Reine PAL-Spiele wie Sonic 3 (Europe) laufen so auf einer
  PAL-Konsole: PAL-Bit im VDP-Status, 313 Zeilen und 50 Bilder pro Sekunde.
  Die Zeilenzahl richtet sich nach der Region, nicht mehr nach dem
  240-Zeilen-Modus des Spiels.
- **Ton:** Ist er aus, erzeugt `ym2612_run()` keine Samples, lässt aber Timer
  A/B weiterlaufen (Soundtreiber fragen deren Überlauf-Bits ab), und
  `gwenesis_SN76489_run()` zählt nur die Zeit weiter. Ist er an, schreibt
  Kern 0 jeden YM2612-/PSG-Zugriff mit Zeitstempel in einen Ring
  (`PBC_YM_CORE1`); seine eigene YM2612-Kopie führt nur Timer und Status.
  Kern 1 spielt den Ring zwischen den Zeilen in kleinen Schritten in eine
  zweite Kopie (`src/ym2612_synth.c`) und den PSG ein, mittelt die 53 kHz
  des Chips auf 22 kHz und gibt jedes Bild an den Lautsprecher. Die
  Synthese und ihre kleinen Tabellen liegen im RAM.

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

## Super Street Fighter II (5 MB)

SSF2 ist das einzige Mega-Drive-Spiel über 4 MB. Die Konsole sieht nur 4 MB,
aufgeteilt in acht Fenster zu 512 KB; über die Register `$A130F3`–`$A130FF`
legt das Spiel eine der zehn Bänke in die Fenster 1–7. Benutzt wird das für
das oberste Megabyte (`$300000`), in das es die Bänke 8 und 9 holt.

Der 68K-Kern liest das ROM an vielen handgeschriebenen Stellen als
`ROM_DATA + Adresse`. Eine Bank-Tabelle in jedem Zugriff würde jedes Spiel
bremsen. Deshalb erledigt das die Adressübersetzung des RP2350 (QMI
ATRANS, `src/ssf2.c`): `ROM_DATA` zeigt so in den XIP-Bereich, dass 0–3 MB
in Übersetzungsfenster 1 fallen (fest, Bänke 0–5) und 3–4 MB in Fenster 2,
dessen Flash-Basis bei jeder Umschaltung umgestellt wird. Das ist ein
Registerzugriff, kein Kopieren. Danach wird der XIP-Cache verworfen, weil er
nach virtuellen Adressen arbeitet. Aktiv nur bei ROMs über 4 MB; alle anderen
Spiele behalten die normale Abbildung. Grenze: Die Fenster 1–5 bleiben fest;
schaltet ein Spiel dort um, wird das ignoriert. SSF2 tut das nicht.

**Nicht getestet** (kein ROM zur Hand) — weder auf dem PC noch auf dem Gerät.

## Offene Punkte

* **Kein Ton.** Ein Versuch (PWM per DMA, 53 kHz) lief langsamer und instabil.
* **Keine Spielstände.** Der Kern emuliert kein batteriegepuffertes
  Cartridge-SRAM. Spiele mit Speicherfunktion verlieren ihren Stand beim
  Ausschalten.
* Kein 6-Button-Pad, kein zweiter Spieler.

## Lizenzen

* Emulatorkern (`core/`) und damit das Gesamtwerk: **AGPL-3.0** wie
  frank-genesis, siehe `LICENSE`.
* `src/flash_fs.*`, `src/usb_msc.c`, `src/usb_descriptors.c`,
  `src/tusb_config.h`, `src/font8x8.h`, `fatfs/diskio.c`: aus PicoBoyGB,
  **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
