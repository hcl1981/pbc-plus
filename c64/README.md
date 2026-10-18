# PBC C64

Commodore-64-Emulator für den **PicoBoy Color Plus** (RP2350A, 512 KB RAM,
16 MB Flash, ST7789 240×280, Joystick + A/B).

Grundlage ist Frodo4 (Christian Bauer) in der Portierung von
[frank-c64](https://github.com/rh1tech/frank-c64), im zeilenbasierten Modus wie
dort. Das Drumherum ist dasselbe wie bei [`../nes`](../nes/) und
[`../megadrive`](../megadrive/):

| frank-c64 | PBC C64 |
|---|---|
| Disk-Images von SD-Karte | **USB-Stick-Modus** in eine FatFs-Partition im Flash (aus [PicoBoyGB](../picoboygb/)); Frodo benutzt das Image direkt von dort, **auch schreibend** (Spielstände) |
| PS/2- oder USB-Tastatur | keine Tastatur: **`LOAD"*",8,1`** wird nach dem Start automatisch getippt, danach bei Bedarf `RUN` |
| HDMI/VGA 320×240 | **ST7789**: 320×240 auf 240×180 verkleinert (gemittelt), auf Kern 1 per DMA |
| SID über I2S/PWM | SID über den **PWM-Lautsprecher** (GP15), 22 222 Hz, **per Vorgabe aus** |
| F9 tauscht die Joystick-Ports | **Joystick-Port pro Spiel** in den Optionen, gespeichert |
| Diskettenwahl, Cartridges, Bandlaufwerk | entfallen |

**Stand:** läuft auf dem Gerät, Kern 0 unter 30 % Last bei 300 MHz – daher jetzt 200 MHz.
Der Ton stürzte nach kurzer Zeit ab; sein Timer läuft deshalb jetzt auf Kern 1
(noch nicht auf dem Gerät geprüft) und ist per Vorgabe aus.

Die Oberfläche auf dem Gerät ist englisch (Menüs, Optionen, Meldungen, Hilfedatei auf dem Laufwerk).

## Benutzung

1. [`../dist/c64.uf2`](../dist/c64.uf2) flashen: BOOTSEL halten, USB anstecken,
   Datei auf das Laufwerk `RPI-RP2` kopieren.
2. **Spiele aufspielen:** beim Einschalten **A oder B** gedrückt halten. Das
   Gerät meldet sich als USB-Stick `PBC-ROMS`. `.d64`-Disk-Images (oder
   `.prg`-Programme) draufkopieren und das Laufwerk **auswerfen** – der PicoBoy
   startet von selbst neu.
3. Ohne gedrückte Taste einschalten: Spieleliste. Hoch/Runter wählt,
   Links/Rechts blättert, **A** oder **Mitte** startet.
   Ganz oben steht immer **„Add games via USB“**: führt direkt in den
   USB-Stick-Modus, ohne Neustart mit gedrückter Taste. Beim Öffnen ist das
   erste Spiel markiert.
4. **B** öffnet die Optionen für das markierte Spiel:
   * **Joystick-Port 1 oder 2** (pro Spiel gespeichert, Vorgabe 2),
   * **Datei**: welches Programm von der Diskette geladen wird – `*` (die
     erste, Vorgabe) oder eines aus dem Inhaltsverzeichnis (pro Spiel
     gespeichert),
   * Ton an/aus (Vorgabe aus) und Lautstärke (für alle Spiele).

   Geändert werden kann das nur hier, nicht im Spiel.

Das Laufwerk teilt sich der C64 mit `megadrive` und `nes`; jede Firmware listet
nur ihre eigenen Dateien.

### Was beim Start passiert

Es gibt keine Tastatur, also tippt der Emulator selbst:

1. Sobald BASIC nach dem Einschalten bereit ist, wird das Image als Laufwerk 8
   eingehängt und `LOAD"*",8,1` eingegeben – oder `LOAD"NAME",8,1` für die in
   den Optionen gewählte Datei. Der Tastaturpuffer des C64 fasst nur 10
   Zeichen; längere Befehle werden stückweise getippt, sobald der Editor den
   Puffer geleert hat.
2. Kehrt BASIC danach zu `READY.` zurück, hat sich das Programm nicht selbst
   gestartet – dann folgt `RUN`. Startet es sich selbst, wird nichts weiter
   getippt.

Bei `.prg`-Dateien wird das Programm direkt in den Speicher geladen und mit
`RUN` gestartet.

Erkannt wird „BASIC ist bereit“ daran, dass der C64 in der Warteschleife des
KERNAL auf eine Taste wartet (`$E5CD`–`$E5D5`).

### Steuerung im Spiel

Vor jedem Spielstart zeigt ein Kasten zwei Sekunden lang diese Belegung, wie
bei PicoBoyGB.

| C64 | PicoBoy |
|---|---|
| Joystick (Port aus den Optionen) | Joystick |
| Feuer | B |
| RETURN | A |
| Leertaste | Mitte + A |

Zurück zur Spieleliste geht es mit RESET. Mit `-DSHOW_FPS=1` gebaut, stehen
unter dem Bild gezeichnete Bilder pro Sekunde, Emulationstempo (100 % = 50
Bilder/s, PAL) und die Auslastung von Kern 0.

## Bild

Frodo zeichnet 384×272 Pixel samt Rahmen. Gezeigt wird derselbe Ausschnitt wie
bei frank-c64 auf HDMI: 320×240, ab Spalte 32 und Zeile 16. Das wird auf
240×180 verkleinert, genau wie beim Mega Drive: Von je vier Pixeln bleiben die
äußeren, die mittleren werden gemittelt, waagerecht und senkrecht. So bleibt
die 8-Pixel-Schrift des C64 lesbar.

Kern 1 rechnet das Bild nach jedem Frame um und schickt es per DMA, während
Kern 0 schon den nächsten emuliert. Weil Kern 1 schneller von oben liest, als
der VIC neu zeichnet, reicht ein einziger Bildpuffer.

## Ton

SID über den Lautsprecher an GP15, nach dem Verfahren aus `../nes`: Ein Timer
holt alle 45 µs einen Sample aus einem Ringpuffer und schreibt ihn in den
PWM-Wert. Anders als beim NES läuft dieser Timer auf **Kern 1**: Beim C64
reicht die Aufrufkette auf Kern 0 tief (CPU → Laufwerk → FatFs), und ein
Interrupt 22 000-mal pro Sekunde könnte dort den 4-KB-Stack sprengen. Mit Ton
an stürzte die erste Fassung reproduzierbar nach kurzer Zeit ab, daher ist der
Ton per Vorgabe aus.

## Absturzanzeige

Es gibt keine serielle Konsole. Stürzt die Firmware ab, zeigt das Display
„CRASH“ mit Kern (Core), PC, LR und dem Fehlerregister CFSR. PC und LR lassen sich
mit `arm-none-eabi-addr2line -e build/pbc_c64.elf <Adresse>` einer
Quellzeile zuordnen.

## Disketten und Grenzen

* **Spielstände auf Diskette gehen.** Das USB-Laufwerk liegt im selben Flash,
  aus dem beide Kerne ihren Code ausführen, und während ein Sektor gelöscht
  wird, ist der Flash nicht lesbar. Deshalb wird für jeden Sektor Kern 1 kurz
  in einer RAM-Schleife geparkt (multicore lockout) und Kern 0 sperrt seine
  Interrupts – das Spiel steht dabei für etwa 50 ms pro 4-KB-Sektor still.
  FatFs puffert Schreibzugriffe; damit ein Spielstand ein RESET übersteht,
  werden gepufferte Daten nach einer halben Sekunde ohne weitere
  Schreibzugriffe durchgeschrieben. **Nach dem Speichern also kurz warten**,
  bevor man RESET drückt oder ausschaltet. Fällt der Strom mitten im Schreiben
  aus, kann das Image beschädigt werden.
* **Keine echte 1541.** Wie bei frank-c64 läuft das Laufwerk auf DOS-Ebene
  (Frodo-IEC-Emulation), nicht als eigene 6502-CPU – das spart Rechenzeit und
  lädt schnell. Spiele mit eigenem Schnelllader, der direkt mit dem Laufwerk
  spricht, oder mit Kopierschutz laden so nicht.
* Nur Laufwerk 8, kein Diskettenwechsel: Spiele über mehrere Disketten gehen
  nicht.
* Keine Cartridges (`.crt`), kein Bandlaufwerk.

## Bauen

```bash
./build.sh                       # -> ../dist/c64.uf2
./build.sh clean
./build.sh -DCPU_MHZ=336 -DCPU_VREG=VREG_VOLTAGE_1_35
```

Werkzeuge und SDK wie bei allen Projekten dieses Repos, siehe
[`../docs/BAUEN.md`](../docs/BAUEN.md). Frodo4 braucht C++20.

| Option | Vorgabe | Bedeutung |
|---|---|---|
| `CPU_MHZ` | 200 | Systemtakt (bei 300 MHz lag die Last unter 30 %) |
| `CPU_VREG` | `VREG_VOLTAGE_1_15` | Kernspannung |
| `FLASH_MAX_FREQ_MHZ` | 88 | Obergrenze für den Flash-Takt |
| `PBC_SPI_HZ` | 62 500 000 | Display-SPI |
| `SHOW_FPS` | 0 | Anzeige unter dem Bild (fps, Tempo, Last) |

## Flash-Aufteilung (16 MB)

| Adresse | Größe | Inhalt |
|---|---|---|
| `0x000000` | 1 MB | Firmware (ca. 280 KB) |
| `0x600000` | ~9,9 MB | FatFs-Partition = USB-Stick (`src/flash_fs.h`) |
| `0xFF2000` | 4 KB | Optionen: Ton, Lautstärke, Joystick-Port und Datei je Spiel (bis 150 Spiele, `src/settings.c`) |

Der ROM-Bereich ab `0x100000`, den `megadrive` und `nes` benutzen, bleibt
unberührt: Der C64 liest die Images direkt aus dem Dateisystem.

## Aufbau

```
src/main.c          Startablauf, USB-Stick-Modus, Takt, Autostart, Emulationsschleife
src/c64_video.c     Skalierung 320x240 -> 240x180, Ausgabe auf Kern 1
src/c64_input.c     Tasten -> Joystick-Port, RETURN, Leertaste
src/c64_audio.c     SID-Samples -> Lautsprecher
src/flash_fs.c      Flash-Laufwerk; parkt Kern 1 beim Schreiben
src/settings.c      Optionen im Flash, Joystick-Port und Datei je Spiel
src/d64_dir.c       Inhaltsverzeichnis einer .d64 für die Dateiauswahl
src/menu.c          Spieleliste und Optionen
src/audio_pwm.c, display.c, input.c   aus ../nes
src/flash_fs.*, usb_*.c, tusb_config.h, fatfs/   aus ../picoboygb
core/               Frodo4 aus frank-c64 (zeilenbasierte Fassung)
core/rp2350/        frank-c64-Anbindung, angepasst:
                    Display ohne HDMI, SID mit 22 222 Hz, verzögertes f_sync
                    beim Schreiben, c64_get_pc() für den Autostart
```

## Lizenzen

* Gesamtwerk: **GPL-2.0-or-later** wie frank-c64 und Frodo4, siehe `LICENSE`.
* `src/flash_fs.*`, `src/usb_msc.c`, `src/usb_descriptors.c`,
  `src/tusb_config.h`, `src/font8x8.h`, `fatfs/diskio.c`: aus PicoBoyGB,
  **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
* Die eingebauten C64-ROMs (BASIC, KERNAL, Zeichensatz, 1541) sind die
  Original-ROMs von Commodore und unverändert aus frank-c64 übernommen. Ihre
  Rechte liegen heute bei Cloanto; sie stehen **nicht** unter der GPL. Siehe
  `../docs/LIZENZEN.md`.
