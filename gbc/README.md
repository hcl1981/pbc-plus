# PBC Game Boy Color

Game-Boy- und Game-Boy-Color-Emulator für den **PicoBoy Color Plus** (RP2350A,
512 KB RAM, 16 MB Flash, ST7789 240×280, Joystick + A/B).

Grundlage ist Peanut-GB (Mahyar Koshkouei) mit der Game-Boy-Color-Erweiterung
aus tvecera's `gbc-rtc-fix`-Zweig, in der Fassung von
[PicoPlus-devel/pico-peanutGB](https://github.com/PicoPlus-devel/pico-peanutGB).
Der Ton kommt von minigb_apu wie in [`../picoboygb`](../picoboygb/), das
Drumherum ist dasselbe wie bei [`../nes`](../nes/) und [`../c64`](../c64/).

[`../picoboygb`](../picoboygb/) bleibt daneben bestehen: Es kann nur den
klassischen Game Boy (DMG), dafür mit Paletten-Auswahl.

Die Oberfläche auf dem Gerät ist englisch (Menüs, Optionen, Meldungen, Hilfedatei auf dem Laufwerk).

**Stand: gebaut, noch nicht auf dem Gerät getestet.**

## Benutzung

1. [`../dist/gbc.uf2`](../dist/gbc.uf2) flashen: BOOTSEL halten, USB anstecken,
   Datei auf das Laufwerk `RPI-RP2` kopieren.
2. **ROMs aufspielen:** beim Einschalten **A oder B** gedrückt halten. Das Gerät
   meldet sich als USB-Stick `PBC-ROMS`. `.gb`- und `.gbc`-Dateien draufkopieren
   und das Laufwerk **auswerfen** – der PicoBoy startet von selbst neu.
3. Ohne gedrückte Taste einschalten: Spieleliste. Hoch/Runter wählt,
   Links/Rechts blättert, **A** oder **Mitte** startet.
   Ganz oben steht immer **„Add games via USB“**: führt direkt in den
   USB-Stick-Modus, ohne Neustart mit gedrückter Taste. Beim Öffnen ist das
   erste Spiel markiert.
4. **B** in der Spieleliste öffnet die Optionen: Ton an/aus (Vorgabe an) und
   Lautstärke (1–12, Vorgabe 8). Beides bleibt gespeichert und lässt sich
   **nur hier** ändern, nicht im Spiel.

Das Laufwerk teilt sich die Firmware mit `megadrive`, `nes` und `c64`; jede
listet nur ihre eigenen Dateien.

### Steuerung im Spiel

Vor jedem Spielstart zeigt ein Kasten zwei Sekunden lang diese Belegung.

| Game Boy | PicoBoy |
|---|---|
| Steuerkreuz | Joystick |
| A | A |
| B | B |
| START | Mitte + B |
| SELECT | Mitte + A |

Wie bei PicoBoyGB. Zurück zur Spieleliste geht es mit RESET.

## Bild

160×144 wird auf **240×216** vergrößert (Faktor 1,5), mittig: Aus je zwei
Quellpixeln a, b werden drei – a, der Mittelwert von a und b, b –,
waagerecht und senkrecht. So wird keine Zeile einfach verdoppelt und das Bild
bleibt so scharf wie möglich.

* **Game-Boy-Color-Spiele** in ihren eigenen Farben (15 Bit, Paletten pro Zeile).
* **Klassische Game-Boy-Spiele** in einer festen, gut lesbaren Grünpalette.

Der Emulator zeichnet jede Zeile direkt in einen 160×144-Puffer. Davon gibt es
zwei: Kern 0 füllt den einen, Kern 1 vergrößert und sendet den anderen per DMA.
Ist Kern 1 mit dem vorigen Bild noch nicht fertig, wird das neue verworfen statt
halb überschrieben.

**Anzeigemodi** (im Verbund `pbcemu` in den Einstellungen des Starters
wählbar; die Einzel-Firmware nutzt immer Modus 0):

| Nr. | Name | Bild |
|---|---|---|
| 0 | Scale 1.5x | 240×216, wie oben (Vorgabe) |
| 1 | Original | 160×144 Pixel 1:1, mittig |

Ein verzerrender Vollbildmodus fehlt bewusst. Der Display-SPI-Takt ist
clk_peri geteilt durch eine gerade Zahl; bei 200 MHz wären das nur 50 MHz
(ein Bild in 1,5-fach knapp 17 ms). Im Spiel wird USB nicht gebraucht, also
läuft die USB-PLL dann mit 150 MHz und versorgt clk_peri allein: 75 MHz SPI,
gut 11 ms pro Bild, unabhängig vom Systemtakt.

**Leistungsanzeige:** fps, Tempo in % des Originals und Auslastung von
Kern 0, z. B. `60fps 100% CPU71%`, einmal pro Sekunde, unter dem Bild. Im Verbund schaltet sie
der Starter ein, in der Einzel-Firmware `SHOW_FPS=1`.

## Ton

Ein DMA-Kanal, getaktet von einem DMA-Timer, holt alle 45 µs einen Sample
aus einem Ringpuffer und schreibt ihn in den PWM-Wert von GP15 – ohne
Interrupt pro Sample (vorher feuerte ein Timer 22 222-mal pro Sekunde und
lief dabei teils aus dem Flash). minigb_apu rechnet mit genau dieser Rate
(22 222 Hz, 372 Samples pro Game-Boy-Bild); seine Routinen pro Sample liegen
im RAM.

Weitere Kleinigkeiten fürs Tempo: Die Farbpalette von Game-Boy-Color-Spielen
wird pro Zeile nur noch dort umgerechnet, wo sich ein Eintrag geändert hat,
und `gb_run_frame()` bleibt als eigene Funktion im RAM, statt in die
Hauptschleife im Flash eingebaut zu werden.

## Spielstände

Spiele mit Batterie-RAM (Pokémon, Zelda …) speichern in eine Datei
**`<Spielname>.sav`** neben der ROM auf dem USB-Laufwerk – am PC sichtbar und
kopierbar.

Ein Spiel wird mit RESET verlassen, deshalb wird während des Spiels
geschrieben: sobald sich das Cartridge-RAM eine Sekunde lang nicht mehr
geändert hat (oder seit 20 Sekunden ständig ändert). Unveränderte Flash-Sektoren
werden übersprungen; für jeden geänderten wird Kern 1 kurz geparkt (wie beim
C64), das Spiel steht dabei etwa 50 ms pro 4-KB-Sektor still. **Nach dem
Speichern im Spiel also kurz warten**, bevor man RESET drückt.

Die Echtzeituhr mancher Spiele (MBC3, z. B. Pokémon Gold/Silber) läuft nicht
weiter, wenn das Gerät aus ist.

## Absturzanzeige

Stürzt die Firmware ab, zeigt das Display „CRASH“ mit Kern, PC, LR und CFSR
(wie beim C64). PC und LR lassen sich mit
`arm-none-eabi-addr2line -e build/pbc_gbc.elf <Adresse>` zuordnen.

## Bauen

```bash
./build.sh                       # -> ../dist/gbc.uf2
./build.sh clean
./build.sh -DCPU_MHZ=300 -DCPU_VREG=VREG_VOLTAGE_1_30
```

Werkzeuge und SDK wie bei allen Projekten dieses Repos, siehe
[`../docs/BAUEN.md`](../docs/BAUEN.md).

| Option | Vorgabe | Bedeutung |
|---|---|---|
| `CPU_MHZ` | 200 | Systemtakt. Vorher 250 MHz bei 1,25 V; damit stürzte das Gerät mit Ton gelegentlich ab, wie zuvor die PC Engine |
| `CPU_VREG` | `VREG_VOLTAGE_1_15` | Kernspannung |
| `FLASH_MAX_FREQ_MHZ` | 100 | Obergrenze für den Flash-Takt (bei 200 MHz: 100 MHz, wie beim Mega Drive) |
| `PBC_SPI_HZ` | 75 000 000 | Display-SPI, im Spiel aus der USB-PLL (150 MHz / 2) |
| `SHOW_FPS` | 0 | Leistungsanzeige der Einzel-Firmware (im Verbund: Starter) |

## Flash-Aufteilung (16 MB)

| Adresse | Größe | Inhalt |
|---|---|---|
| `0x000000` | 1 MB | Firmware (ca. 80 KB) |
| `0x100000` | 5 MB | ROM des laufenden Spiels (`src/rom_store.c`) |
| `0x600000` | ~9,9 MB | FatFs-Partition = USB-Stick, auch die `.sav`-Dateien (`src/flash_fs.h`) |
| `0xFF0000` | 4 KB | Info, welches Spiel im ROM-Bereich liegt |
| `0xFF5000` | 4 KB | Optionen (Ton, Lautstärke) |

## Speicher

Statisch belegt sind rund 360 KB: zwei Bildpuffer (2 × 45 KB), der
Emulatorzustand mit Game-Boy-Color-RAM (49 KB), Cartridge-RAM bis 128 KB
(MBC5) und Bank 0 der ROM als Kopie im RAM (16 KB, wird ständig gelesen).

## Aufbau

```
src/main.c        Startablauf, USB-Stick-Modus, Peanut-GB-Anbindung,
                  Spielstände, Absturzanzeige, Emulationsschleife
src/video.c       Doppelpuffer, Vergrößerung 1,5x, Ausgabe auf Kern 1
src/audio_pwm.c   Ton (DMA, wie in ../sms)
src/flash_fs.c    Flash-Laufwerk; überspringt unveränderte Sektoren,
                  parkt Kern 1 beim Schreiben
src/input.c, menu.c, rom_store.c, settings.c, display.c   aus ../nes
src/usb_*.c, tusb_config.h, fatfs/                         aus ../picoboygb
core/peanut_gb.h  Peanut-GB mit GBC-Erweiterung, aus pico-peanutGB; geändert: Kopfbytes für ROM-/RAM-Größe werden geprüft statt hinter Tabellen zu lesen, 64-KB-Speicherstände (0x05) erkannt
core/minigb_apu.* aus ../picoboygb
```

## Offene Punkte

* **Nicht auf Hardware geprüft.**
* Echtzeituhr (MBC3) läuft beim Ausschalten nicht weiter.
* Game-Boy-Color-Spiele im doppelten Takt brauchen mehr Rechenzeit; laut
  pico-peanutGB haben manche Farbspiele Bild- oder Tonfehler.
* Keine Paletten-Auswahl für klassische Spiele (dafür gibt es `../picoboygb`).

## Lizenzen

* Gesamtwerk: **GPL-3.0**, siehe `LICENSE`. `core/peanut_gb.h` stammt aus dem
  GPL-3.0-Projekt pico-peanutGB; der Dateikopf trägt die MIT-Lizenz von
  Peanut-GB (Mahyar Koshkouei, Teile aus SameBoy von Lior Halphon). Um
  sicherzugehen, gilt für den Ordner die GPL-3.0.
* `core/minigb_apu.*`, `src/flash_fs.*`, `src/usb_msc.c`,
  `src/usb_descriptors.c`, `src/tusb_config.h`, `src/font8x8.h`,
  `fatfs/diskio.c`: aus PicoBoyGB, **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
