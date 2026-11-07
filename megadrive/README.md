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

**Link-Spiel** (nur in PBCemu, `../docs/LINKSPIEL.md`): zwei Geräte am
USB-C-Kabel, Controller 1 und 2. Vor jedem Bild tauschen sie ihre Pads, das
startende Gerät gibt den Takt vor. Die Emulation ist dafür deterministisch:
Ton an/aus ändert nur die Ausgabe (die YM2612-Timer laufen immer auf Kern 0),
das Auslassen von Bildern nur das Zeichnen. Einzige Ausnahme war das
Sprite-Overflow-Flag des VDP, das beim Zeichnen auf Kern 1 entsteht; im
Link-Spiel liest es sich als 0. Auf zwei Geräten noch nicht getestet.

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
| A | B |
| B | A |
| C | Mitte |
| START | Mitte + B |

Mitte wird erst nach 40 ms als C gemeldet. So wird aus „Mitte, kurz danach B“
sicher START und kein versehentliches C. Solange Mitte gehalten wird, sind A und
B für die Kombinationen reserviert: **C gleichzeitig mit A oder B geht nicht.**

In PBCemu lässt sich die Verteilung je Spiel ändern: In den Einstellungen des
Spiels (Mitte in der Spieleliste) legt die Zeile **B A Mitte** fest, welche
der Tasten A, B, C auf PicoBoy-B, -A und -Mitte liegt (alle sechs
Verteilungen). Die 40-ms-Wartezeit gilt dann für die Taste auf Mitte.

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
| `0x600000` | ~9,9 MB | FatFs-Partition = USB-Stick (`src/flash_fs.h`), auch die `.sav`-Dateien |
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
src/sram.c        Batterie-SRAM: Kopf lesen, .sav laden und während des Spiels schreiben
src/ym_core1.c    Ton auf Core 1: Schreibprotokoll abspielen, mischen, auf 22 kHz
src/ym2612_synth.c  zweite Übersetzung von core/sound/ym2612.c für Core 1
src/audio_pwm.c   PWM-Lautsprecher per DMA, wie in ../sms
tools/m68k_gen.py       erzeugt die 68000-Befehlsroutinen und ihre Tabelle (Musashi)
tools/m68k_hot.py       wählt die 68000-Befehlsroutinen für den RAM aus
tools/host/       mdhost: der Kern als PC-Programm, für Bildfehler (eigene README);
                  cmp/: zwei Stände bitgenau vergleichen, Befehle zählen
src/HDMI.h        Ersatz für den HDMI-Header, den der VDP-Kern einbindet
src/flash_fs.*, usb_*.c, tusb_config.h, fatfs/   aus ../picoboygb
core/             Emulatorkern aus frank-genesis
```

### Änderungen am Kern

Fast alles betrifft die Geschwindigkeit. Der Flash hat nur 16 KB Cache, und
aus ihm kommen Spiel-ROM und Emulatorcode zugleich; alles, was pro Befehl
oder pro Sample gebraucht wird, liegt deshalb im RAM.

- **68000 seit November 2026:** Musashi 4.10 (MIT) statt Eke-Ekes Fassung
  von Musashi 3.32, siehe Lizenzen und `core/cpus/musashi/README.md`. Die
  Optimierungen unten für den alten Kern gelten genauso: Befehlstabelle
  gepackt im RAM (42 KB, jetzt von `tools/m68k_gen.py` beim Erzeugen der
  Routinen), `m68k_run()` und die meistbenutzten Routinen im RAM, ROM und
  RAM direkt gelesen. Am PC Befehl für Befehl mit dem alten Kern verglichen
  (144 Millionen Befehle aus Sonic 1–3, auch die Takte gleich). Auf dem
  Gerät (PBCemu, Sonic 2, 30 s ohne Eingabe, Benchmark-Build): CPU 60,0 % →
  57,1 %; Z80, Grafikchip und Ton unverändert.
- **68000 (alter Kern):** Die Befehlstabelle (Routine je Opcode) und die
  Taktzahltabelle lagen mit zusammen 320 KB im Flash; jeder Befehl verfehlte
  meist den Cache. Die Taktzahl hängt nur von der Routine ab, und viele
  32er-Abschnitte der Tabelle wiederholen sich; ein Skript packte beides
  verlustfrei in drei Tabellen mit zusammen 42 KB im RAM, ebenso
  `m68k_run()`. Nebenbei gefunden: Die alte Tabelle endete bei $EFBF,
  Line-F-Opcodes lasen dahinter einen falschen Sprungzeiger.
- **Z80 seit November 2026:** redcode/Z80 statt des Kerns von Marat
  Fayzullin (der nur nicht-kommerziell frei ist), siehe Lizenzen;
  Speicherzugriffe in C in `core/sound/z80inst.c` statt `z80_mem_opt.S`.
  Auf dem Gerät gemessen (PBCemu, 30 s Spiel ohne Eingabe, Benchmark-Build):
  CPU 58,4 % → 59,9 %, davon Z80 12,0 % → 13,7 %; 68000 und Grafikchip
  unverändert, Ton auf Kern 1 42,7 % → 43,1 %. Opcodes kommen direkt aus dem
  Z80-RAM, Code und Tabellen liegen im RAM, in PBCemu laufen auch die
  IX/IY-Befehle über `switch`. Der Rest ist redcodes Interrupt-Prüfung nach
  jedem Befehl. Das Folgende gilt für den alten Kern.
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
  zuerst) und Adressierung (Register, (An), (An)+, -(An) zuerst) aus: 48 KB
  in beiden Firmwares (`M68K_HOT1`), weitere 30 KB nur in PBCemu
  (`M68K_HOT2`). Bis November 2026 waren es 17 + 30 KB; der neue YM2612 gab
  das RAM für mehr frei (Benchmark: CPU 58,1 % → 56,2 %). Ändert sich der
  Kern, das Skript mit einer Bau-.elf neu laufen lassen.
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
- **YM2612 seit November 2026:** eigener Code statt MAME fm.c / Genesis Plus
  GX (siehe Lizenzen). Am PC mit dem alten verglichen: Einzeltests (alle
  Algorithmen, Rückkopplung, Hüllkurven, SSG-EG, LFO, Kanal-3-Modus) und die
  YM2612-Schreibzugriffe aus Sonic 1–3, Kanal für Kanal; bis auf Rundung in
  fast stillen Ausklängen dieselben Samples. Bewusst anders: Die
  Tonhöhenmodulation folgt den dokumentierten Tiefen (Cent) statt der
  Bit-Tabelle des Chips, die Rückkopplung von Operator 1 rechnet mit voller
  Genauigkeit, und der DAC (Kanal 6) springt wie im Chip, statt geglättet zu
  werden. Auf dem Gerät (PBCemu, Sonic 2, 30 s, Benchmark-Build): Ton auf
  Kern 1 42,8 % → 32,7 %. Die großen Tabellen des alten fallen weg; das
  RAM nutzen jetzt mehr 68000-Routinen (unten). Frei bleiben in der
  Einzel-Firmware 40 KB, in PBCemu 29 KB.
- **Weniger Last auf beiden Kernen (November 2026):** alles bitgenau, am
  PC und auf einem emulierten Cortex-M33 mit `tools/host/cmp/` gegen den
  Stand davor geprüft (Bild, Ton beider Chips, RAM, CPU-Zustand nach jedem
  Zeitabschnitt; Sonic 2, Streets of Rage 2, Gunstar Heroes je 6000
  Bilder mit Eingaben, dazu Zufallstests für VDP, Zeilenzeichner,
  Skalierer, YM2612). Befehle pro Bild im Spiel (M33-Emulation, ohne
  Flash-Wartezeiten): Kern 1 (Zeilen, Skalierer, Ton) rund −30 %, Kern 0
  (68000, Z80) je nach Spiel −4 bis −25 %; zusammen Sonic 2 4,2 → 3,1 Mio.,
  SoR2 3,9 → 2,7 Mio. Auf dem Gerät noch nicht gemessen.
  * Zeilenzeichner (`gwenesis_vdp_line_opt.S`): Ebene A, B und Fenster
    machen die acht Pixel einer Kachelzeile als zwei Wörter mit den
    DSP-Befehlen (USUB8/SEL/UQSUB8) statt Byte für Byte: −35 %.
  * Skalierer (`video.c`): eine Schleife je Modus statt zweier
    Index-Tabellen; gemittelt wird nur, wo zwei Quellpixel zusammenfallen:
    −45 %.
  * 68000: Warteschleifen aus `tst`/`btst`/`cmpi` auf eine RAM-Adresse und
    `Bcc.s` zurück sowie `DBF Dn,*` laufen bis zum Ende des Zeitabschnitts
    in einem Schritt (`m68ki_poll_skip`, erst wenn die Schleife einmal ganz
    lief). Innerhalb eines Abschnitts ändert nichts anderes das RAM;
    Register, Flags und Takt am Abschnittsende bleiben gleich, nur eine
    Spur Befehl für Befehl (`MDHOST_TRACE`) sieht weniger Befehle. SoR2 im
    Spiel: 68000 −48 %, Sonic 2 je nach Szene −10 bis −46 %.
  * YM2612 auf Kern 1: Dämpfung je Operator nur noch bei Änderung
    berechnet, Timer nur im CSM-Betrieb pro Sample, Tabellen für Hüllkurve,
    halbe Sinuswelle und Exponent, Frequenzberechnung im RAM: −11 bis
    −14 %. Die Kopie auf Kern 0 führt nur noch die Register 24–27 (Timer).
  * Kern 1 rechnet pro Aufruf höchstens 8 Samples je Chip, gibt das Bild
    in Stücken an den Lautsprecher und schaut auch zwischen Ton und
    Skalierer nach einer zu zeichnenden Zeile; so sollte Kern 0 seltener
    selbst zeichnen (V). Wie weit beide Chips beim Abarbeiten des
    Protokolls stehen, bleibt dabei genau gleich: Das Protokoll ist nicht
    zeitlich sortiert (der Z80 läuft nach dem 68000), ein kleinerer Schritt
    allein hätte den Ton verändert.
  * DMA in den VRAM ohne Neuladen der Register pro Byte; im RAM jetzt
    auch die Zugriffe des 68000 auf VDP, I/O und Z80-Bus, `z80_run`, die
    Palette, das Protokoll an Kern 1, die Schleife von Kern 1 und die
    Display-Aufrufe darin.
  * Flash −5 KB, freies RAM 17,7 → 18,6 KB (PBCemu-Bau). `-O3` für den
    Z80 brachte nichts.

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

## Spielstände

Spiele mit Batterie-SRAM (Phantasy Star II–IV, Shining Force, Sonic 3 …)
speichern in **`<Spielname>.sav`** neben der ROM auf dem USB-Laufwerk, wie
beim Game Boy Color (`src/sram.c`). Ob es SRAM gibt, steht im Kopf der
Cartridge (`RA` bei `$1B0`, Art bei `$1B2`, Anfang und Ende bei `$1B4`/`$1B8`,
meist `$200001`–`$203FFF`, also 8 KB in den ungeraden Bytes). Die Datei
enthält nur die benutzten Bytes, höchstens 8 KB. Liegt das ROM unterhalb
des SRAM, ist es immer sichtbar, sonst erst, wenn das Spiel 1 nach `$A130F1`
schreibt (Phantasy Star IV, 3 MB).

Ein Spiel wird mit RESET verlassen, deshalb wird während des Spiels
geschrieben: sobald sich das SRAM eine Sekunde lang nicht mehr geändert hat
(oder seit 20 Sekunden ständig ändert). Kern 1 wartet dabei im RAM, das
Spiel steht etwa 50 ms pro geändertem 4-KB-Sektor still; unveränderte
Sektoren werden übersprungen. **Nach dem Speichern im Spiel kurz warten**,
bevor man RESET drückt. Ist das Laufwerk voll, wird nicht gespeichert, das
Spiel läuft weiter. Im **Link-Spiel** startet das SRAM auf beiden Geräten
leer und wird nicht gespeichert, damit beide Emulationen gleich bleiben.

Damit das wenig Flash kostet (der Verbund hat kaum Platz): Die Datei wird
einmal auf ihre Länge gebracht, danach schreibt `sram.c` ihre Sektoren
direkt, ohne FatFs' Schreibcode; den FIL teilt es sich mit `rom_store.c`;
statt der Multicore-Sperre des SDK hält eine eigene kleine Schleife Kern 1
an. Der 68000 liest ROM unter 2 MB weiter direkt, darüber entscheidet eine
kleine Funktion im RAM zwischen ROM und Bus (`m68kcpu.h`), damit jeder
der vielen eingebauten Lesezugriffe so kurz bleibt wie vorher. Auf dem PC
geprüft mit eigenen Testmodulen (lesen/schreiben als Byte, Wort, Langwort,
`$A130F1`, Laden und Speichern über FatFs, volles Laufwerk); **auf dem
Gerät und mit echten Spielen noch nicht getestet.**

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
* **Kein EEPROM.** Spiele, die mit einem seriellen EEPROM speichern (z. B.
  NBA Jam, Wonder Boy in Monster World, Evander Holyfield's Boxing),
  verlieren ihren Stand; nur Batterie-SRAM wird gespeichert (siehe oben).
* Kein 6-Button-Pad, kein zweiter Spieler.

## Lizenzen

* Emulatorkern (`core/`) und damit das Gesamtwerk: **AGPL-3.0** wie
  frank-genesis, siehe `LICENSE`.
* `src/flash_fs.*`, `src/usb_msc.c`, `src/usb_descriptors.c`,
  `src/tusb_config.h`, `src/font8x8.h`, `fatfs/diskio.c`: aus PicoBoyGB,
  **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
* `core/cpus/redcode/`: Z80-Kern **redcode/Z80** von Manuel Sainz de Baranda
  y Goñi mit Headern seiner Bibliothek Zeta, **LGPL-3.0-or-later**; für den
  PicoBoy geändert von `tools/redcode_patch.py` (`core/cpus/redcode/README.md`).
  Er ersetzt seit November 2026 den Z80-Kern von Marat Fayzullin, der nur für
  nicht-kommerzielle Zwecke frei war.
* `core/sound/gwenesis_sn76489.*`: PSG, im November 2026 für diese Portierung
  neu geschrieben (GPL-3.0-or-later wie die übrige Tonausgabe); er ersetzt den
  PSG nach Maxim mit Änderungen aus Genesis Plus GX. Schnittstelle, Zeitablauf
  und Lautstärke sind gleich geblieben (am PC verglichen).
* `core/cpus/musashi/`: 68000-Kern **Musashi 4.10** von Karl Stenerud,
  **MIT**; für den 68000 und das Mega Drive angepasst, Routinen erzeugt von
  `tools/m68k_gen.py` (`core/cpus/musashi/README.md`). Er ersetzt seit
  November 2026 Musashi 3.32 in Eke-Ekes Fassung aus Genesis Plus GX, dessen
  Lizenz keine kommerzielle Nutzung erlaubt.
* `core/sound/ym2612.*`: YM2612, im November 2026 für diese Portierung neu
  geschrieben (GPL-3.0-or-later) nach Yamahas und Segas Unterlagen und den
  veröffentlichten Messungen am Chip; er ersetzt den YM2612 nach MAME fm.c mit
  Änderungen aus Genesis Plus GX (Lizenz ohne kommerzielle Nutzung).
  Schnittstelle und Pegel sind gleich geblieben. Damit enthält der
  Mega-Drive-Emulator keinen Code mehr, der nur nicht-kommerziell frei ist.
