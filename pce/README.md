# PBC PC Engine

PC-Engine-/TurboGrafx-16-Emulator für den **PicoBoy Color Plus** (RP2350A,
512 KB RAM, 16 MB Flash, ST7789 240×280, Joystick + A/B).

Grundlage ist der Kern **pce-go** aus [retro-go](https://github.com/ducalex/retro-go)
(ducalex) in der Fassung von
[pico-pcePlus](https://github.com/PicoPlus-devel/pico-pcePlus). Das Drumherum
ist dasselbe wie bei [`../nes`](../nes/) und [`../c64`](../c64/):

| pico-pcePlus | PBC PC Engine |
|---|---|
| HuCards von SD-Karte | **USB-Stick-Modus** in eine FatFs-Partition im Flash (aus [PicoBoyGB](../picoboygb/)); die Karte liegt im ROM-Bereich, pce-go blendet ihre Bänke direkt per XIP ein |
| HDMI 320×240 | **ST7789**: auf 240 Pixel Breite beschnitten bzw. verkleinert, auf Kern 1 per DMA |
| Ton über HDMI/I2S | **PWM-Lautsprecher** (GP15), 22 222 Hz, per DMA |
| CD-ROM², SuperGrafx | entfallen (bräuchten PSRAM) |
| Menü mit Spielständen, Einstellungen, Controller-Test | entfallen |

Die Oberfläche auf dem Gerät ist englisch (Menüs, Optionen, Meldungen, Hilfedatei auf dem Laufwerk).

**Stand: gebaut, noch nicht auf dem Gerät getestet.**

## Benutzung

1. [`../dist/pce.uf2`](../dist/pce.uf2) flashen: BOOTSEL halten, USB anstecken,
   Datei auf das Laufwerk `RPI-RP2` kopieren.
2. **Spiele aufspielen:** in der Spieleliste ganz oben **„Add games via USB“**
   wählen oder beim Einschalten **A oder B** halten. Das Gerät meldet sich als
   USB-Stick `PBC-ROMS`. HuCard-Abbilder (`.pce`) draufkopieren und das
   Laufwerk **auswerfen** – der PicoBoy startet von selbst neu.
3. Spieleliste: Hoch/Runter wählt, Links/Rechts blättert, **A** oder **Mitte**
   startet. Beim Öffnen ist das erste Spiel markiert.
4. **B** öffnet die Optionen: Ton an/aus (Vorgabe an) und Lautstärke (1–12,
   Vorgabe 8). Beides bleibt gespeichert und lässt sich nur hier ändern, nicht
   im Spiel.

Beim ersten Start einer Karte wird sie in den ROM-Bereich des Flash
geschrieben und danach geprüft. Das Laufwerk teilt sich die Firmware mit den
anderen Emulatoren; jede listet nur ihre eigenen Dateien.

### Steuerung im Spiel

Vor jedem Spielstart zeigt ein Kasten zwei Sekunden lang diese Belegung.

| PC Engine | PicoBoy |
|---|---|
| Steuerkreuz | Joystick |
| I | B |
| II | A |
| RUN | Mitte + B |
| SELECT | Mitte + A |

Mitte allein tut nichts. Zurück zur Spieleliste geht es mit RESET.

## Bild

pce-go zeichnet 8-Bit-Farben (GGGRRRBB, feste Palette) in einen Bildpuffer mit
368 Byte Zeilenlänge (352 Pixel plus Rand). Die PC Engine wechselt die Breite je
nach Spiel – meist 256 Pixel, manche 320, 336 oder 352 – bei 224 bis 242 Zeilen:

| Breite | Darstellung |
|---|---|
| bis 240 | mittig, 1:1 |
| 241–256 | 1:1, außen beschnitten (bei 256 je 8 Spalten) |
| breiter | auf 240 verkleinert, die Zeilen im selben Verhältnis (336×224 → 240×160), damit nichts verzerrt; liegt ein Ausgabepixel oder eine Ausgabezeile zwischen zwei Quellpixeln bzw. -zeilen, werden beide gemittelt (wie beim Mega Drive) |

Sonst werden die Zeilen 1:1 gezeigt; das Bild steht senkrecht mittig. Ändert ein Spiel die
Auflösung, wird der Bildschirm einmal schwarz gelöscht.

Das ist Anzeigemodus 0 („Standard“). Im Verbund (`../pbcemu`) lässt sich je
System ein anderer wählen; die Einzel-Firmware zeigt immer Modus 0:

| Modus | Darstellung |
|---|---|
| 0 Standard | wie oben |
| 1 Crop 1:1 | nie skaliert, bei breiteren Zeilen die mittleren 240 Spalten |

**Leistungsanzeige** („60fps 100% CPU71%“: gezeichnete Bilder/s, Tempo in %
des Originals, Auslastung von Kern 0): im Verbund per Option im Starter, in
der Einzel-Firmware mit `SHOW_FPS=1`. Steht unter dem Bild.

pce-go bekommt statt des Zeilenpuffers von pico-pcePlus einen ganzen
Bildpuffer, zwei davon im Wechsel: Kern 0 emuliert ins eine Bild, Kern 1 rechnet
das andere in BGR565 um und schickt es per DMA.

## Ton

Wie bei pico-pcePlus sample-genau: Vor jedem Schreibzugriff auf den PSG ruft
pce-go `osd_psg_sync()` mit der Zyklusposition im Bild auf, und die Samples bis
dahin werden noch mit den alten Registern erzeugt. Am Bildende wird der Rest
aufgefüllt. Die Zyklusposition zählt im aktuellen CPU-Takt (113 Zyklen je
Zeile bei 1,79 MHz, 455 nach `CSH` bei 7,16 MHz, was fast jedes Spiel
einschaltet); die Bildlänge folgt deshalb `PCE.Timer.cycles_per_line`. Vorher
war sie fest auf 263 × 113, und bei 7,16 MHz landete jeder PSG-Zugriff ab
etwa Zeile 65 am Bildende – Sprache und Samples (DDA) klangen verzerrt.
Der Kern rechnet stereo, die Ausgabe mischt auf mono herunter.
Abgespielt wird über `audio_pwm`: Ein DMA-Kanal, getaktet von einem
DMA-Timer, schreibt alle 45 µs ein Sample aus dem Ringpuffer in den
PWM-Vergleichswert. Vorher lief dafür 22 222-mal pro Sekunde ein
Timer-Interrupt auf Kern 1, dessen SDK-Code aus dem Flash kam.

## US-HuCards

TurboGrafx-16-Karten sind bitweise gespiegelt gespeichert. pce-go entschlüsselt
sie beim Laden im Speicher – das geht im Flash nicht. `rom_store.c` wendet
deshalb dieselbe Regel wie pce-go (bekannte CRC von *Legend of Hero Tonma* oder
ein unmögliches Byte `$1FFF` in der ersten Bank) schon beim Schreiben in den
Flash an. Im Kern ist die Entschlüsselung für ROMs im Flash abgeschaltet.

## Bauen

```bash
./build.sh                       # -> ../dist/pce.uf2
./build.sh clean
./build.sh -DCPU_MHZ=250 -DCPU_VREG=VREG_VOLTAGE_1_25
```

Werkzeuge und SDK wie bei allen Projekten dieses Repos, siehe
[`../docs/BAUEN.md`](../docs/BAUEN.md).

| Option | Vorgabe | Bedeutung |
|---|---|---|
| `CPU_MHZ` | 200 | Systemtakt |
| `CPU_VREG` | `VREG_VOLTAGE_1_15` | Kernspannung |
| `FLASH_MAX_FREQ_MHZ` | 100 | Obergrenze für den Flash-Takt (bei 200 MHz: 100 MHz, wie beim Mega Drive) |
| `PBC_SPI_HZ` | 75 000 000 | Display-SPI. Im Spiel kommt `clk_peri` aus der USB-PLL (150 MHz, USB wird dann nicht gebraucht), also 75 MHz bei jedem Systemtakt; vorher ergaben 200 MHz nur 50 MHz, zu wenig für 60 Bilder/s |
| `SHOW_FPS` | 0 | Leistungsanzeige (fps, Tempo, Last) in der Einzel-Firmware; im Verbund schaltet sie der Starter |

**Warum 200 MHz:** pico-pcePlus erreicht 60 Bilder/s bei 252 MHz und liest
das ROM wie hier aus dem Flash. Zuerst lief PBC PCE mit 300 MHz bei 1,30 V.
Damit startete das Gerät bei Out Run mit Ton nach einiger Zeit neu, vermutlich
weil die Versorgung einbrach. Deshalb zuerst 250 MHz bei 1,25 V wie NES, SMS und
GBC. Auch damit startete es neu, nur viel später; nun 200 MHz bei 1,15 V
wie beim C64. Startet das Gerät trotzdem ungewollt neu, zeigt es beim nächsten Start
„UNEXPECTED RESTART“ mit dem Grund (z. B. „Supply voltage dropped“) und geht
nicht in den USB-Modus, auch wenn gerade A oder B gedrückt ist.

## Speicher

Firmware 117 KB. Statisch belegt sind 297 KB, davon 178 KB für die beiden
Bildpuffer. Der Zeilenzeichner (`render_lines` mit `draw_tiles`) liegt im RAM;
GCC hatte das `static inline` in den Flash ausgelagert, wo es mit dem
Spiel-ROM um den 16-KB-XIP-Cache konkurrierte. Vom Heap (etwa 216 KB) nimmt sich pce-go rund 80 KB: 8 KB
Arbeitsspeicher, 64 KB VRAM und eine 8-KB-Leerseite.

## Flash-Aufteilung (16 MB)

| Adresse | Größe | Inhalt |
|---|---|---|
| `0x000000` | 1 MB | Firmware |
| `0x100000` | 5 MB | HuCard des laufenden Spiels (`src/rom_store.c`); Street Fighter II (2,5 MB) passt |
| `0x600000` | ~9,9 MB | FatFs-Partition = USB-Stick (`src/flash_fs.h`) |
| `0xFF0000` | 4 KB | Info, welches Spiel im ROM-Bereich liegt |
| `0xFF4000` | 4 KB | Optionen (Ton, Lautstärke) |

## Aufbau

```
src/main.c        Startablauf, USB-Stick-Modus, Takt, Absturzanzeige, Emulationsschleife
src/pce_glue.c    Rückrufe für pce-go: Bildpuffer, Ton (sample-genau), Speicher
src/video.c       Anzeigemodi (Beschnitt/Skalierung), Ausgabe auf Kern 1, zwei Bildpuffer
src/rom_store.c   HuCard in den Flash schreiben (Kopierer-Header, US-Entschlüsselung)
src/input.c       Tastenbelegung (Mitte als Modifier)
src/audio_pwm.c   PWM-Lautsprecher per DMA, wie in ../sms
src/display.c, menu.c, settings.c   aus ../nes
src/flash_fs.*, usb_*.c, tusb_config.h, fatfs/   aus ../picoboygb
core/pce-go/      pce-go aus pico-pcePlus
```

Änderungen am Kern: In `LoadCard()` (`core/pce-go/pce-go.c`) werden ROMs im
Flash weder entschlüsselt noch freigegeben. `cd.c` und `cd_chd.c` werden nur
mitgebaut, damit der Kern bindet; ohne eingelegte CD werden sie nie betreten.

## Offene Punkte

* **Nicht auf Hardware geprüft.**
* Keine CD-ROM²- und SuperGrafx-Spiele (`.sgx` wird nicht gelistet).
* Keine Spielstände (HuCards haben keinen Batteriespeicher; Speicherstände wie
  in pico-pcePlus sind nicht übernommen).
* Kein Mehrspieler (Multitap).

## Lizenzen

* Gesamtwerk: **GPL-3.0** wie pico-pcePlus, siehe `LICENSE`.
* `core/pce-go/`: pce-go aus retro-go, **GPL-2.0** (siehe `core/pce-go/COPYING`
  und `CREDITS`), unverändert so übernommen, wie pico-pcePlus es unter GPL-3.0
  verbreitet.
* `src/flash_fs.*`, `src/usb_msc.c`, `src/usb_descriptors.c`,
  `src/tusb_config.h`, `src/font8x8.h`, `fatfs/diskio.c`: aus PicoBoyGB,
  **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
