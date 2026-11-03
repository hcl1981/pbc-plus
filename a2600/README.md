# PBC 2600

Atari-2600-Emulator für den **PicoBoy Color Plus**, nur als Teil von
PBCemu (`../pbcemu/build.sh` → `dist/pbcemu.uf2`); eine Einzel-Firmware
gibt es nicht.

Der Kern (`core/`) ist für den PicoBoy geschrieben (GPL-3.0-or-later), nach
Ataris *Stella Programmer's Guide*: CPU 6507 mit allen offiziellen und den
stabilen inoffiziellen Befehlen, TIA (Bild und Ton), RIOT 6532 (128 Byte RAM,
Timer, Joysticks, Konsolenschalter) und die Bankumschaltung der Module.

**Stand:** am PC mit fünf freien Homebrew-Spielen getestet (Pipes, Clock,
Hello, Pickup laufen; Phazer ist ein unfertiges Tutorial und hängt). Auf dem
Gerät läuft Pipes in Echtzeit mit Display und Ton (Benchmark: 1800 Bilder in
30,0 s, CPU 62 %), ebenso ein PAL-Test-ROM (312 Zeilen, 49,86 Hz) und Kaz's
Knickers (16 KB, F6). Link-Spiel auf dem Gerät noch nicht getestet.

**Farben:** NTSC mit der Palette von Stella (Tabelle in `core/tia.c`), PAL
berechnet (Farbton und Helligkeit nach der PAL-Farbfolge des TIA).

**Was fehlt:** HMOVE wirkt sofort und ganz, egal wann in der Zeile es kommt.
Tricks mit HMOVE mitten in der Zeile oder in Takt 73/74 (etwa die 96 Pixel
breiten Titelbilder aus batari Basics „Titlescreen“-Kernel, Kaz's Knickers)
erscheinen deshalb als Streifen.

## Spiele

`.a26`: Module bis 512 KB. Die Bankumschaltung errät der Kern aus Größe und
Inhalt: 2 KB, 4 KB, F8 (8 KB), F6 (16 KB), F4 (32 KB), jeweils auch mit
Superchip-RAM, E0 (Parker Brothers), FE (Activision) und 3F (Tigervision).
Paddles, Keypads und andere Controller gibt es nicht, nur Joysticks.

**PAL und NTSC** erkennt der Emulator an der Zeilenzahl, die das Spiel
erzeugt (312 statt 262 pro Bild, 30 Bilder lang): PAL läuft dann mit 50 Hz,
PAL-Farben und einem tiefer liegenden Bildausschnitt (Zeilen 42 bis 281). Die
Umschaltung hängt nur von der Emulation ab, im Link-Spiel schalten beide
Geräte im selben Bild. SECAM gibt es nicht.

## Atari 7800

Der Emulator spielt auch **Atari-7800-Module** (`.a78`, mit oder ohne den
128 Byte langen Kopf); im Starter ist der 7800 ein eigenes System. Der
Kern `core/a7800.c` benutzt denselben 6502 (zweimal übersetzt), den TIA für
Ton und Knöpfe und dieselben Joystick-Bits. Neu ist MARIA: Display-List-List,
Display-Lists mit direkter und indirekter (Zeichen-)Grafik, die Modi 160A/B
und 320A–D, acht Paletten, Kangaroo- und Holey-DMA, die DMA-Zeit, die der
CPU pro Zeile fehlt, und die Display-List-Interrupts als NMI (wenn MARIA den
markierten Eintrag holt). Dazu RIOT (mit dem Sofort-Schritt des Timers nach
dem Schreiben, auf den sich 7800basic verlässt), 4 KB RAM, Module flach bis
48 KB und SuperGame-Bankumschaltung (mit RAM oder zusätzlicher Bank bei
4000h). Ein BIOS braucht es nicht: das Spiel startet an seinem Reset-Vektor.
PAL-Module (laut Kopf) laufen mit 313 Zeilen und 50 Hz.

**POKEY** (`core/pokey.c`): hat das Modul laut Kopf einen (bei 4000h, 450h
oder 440h), kommt sein Ton zum TIA dazu. Vier Kanäle mit 64-kHz-, 15-kHz-
oder 1,79-MHz-Takt, 16-Bit-Paare, die Polynomzähler (4, 5, 9, 17 Bit),
Nur-Lautstärke, die beiden Hochpassfilter, STIMER und RANDOM; gerechnet
wird von Ereignis zu Ereignis und über jede halbe Zeile gemittelt. Es
fehlen die Timer-Interrupts, die serielle Schnittstelle, Paddles und
Tastatur. Geprüft mit eigenen Testmodulen gegen die Formeln des
Datenblatts (reine Töne in allen Taktarten, 16 Bit, Rauschen), noch nicht
mit einem Spiel: die Demo von Black Forest meldet zwar einen POKEY an,
spielt aber keine Musik.

Gezeigt werden 240 der 243 Zeilen, 320 Pixel (die 160er-Modi doppelt) auf
240 verkleinert (aus vier Pixeln drei, das mittlere gemittelt).

**Stand:** am PC mit zwei freien Homebrews getestet: Side Bounce (7800basic,
Ton) und die Demo von Black Forest (272 KB SuperGame); auf dem Gerät Side
Bounce mit vollen 60 Bildern/s, Kern 0 zu 69 % ausgelastet. **Was fehlt:**
XM und YM2151 (z. B. 1942), Activision-
und Absolute-Bankumschaltung, die Zeitabläufe von MARIA innerhalb der Zeile.

| PicoBoy | Atari 7800 |
|---|---|
| Joystick | Joystick |
| B / A | linker / rechter Knopf |
| Center + B | RESET |
| Center + A | SELECT |
| Center + Runter | PAUSE |

## Tasten

| PicoBoy | Atari 2600 |
|---|---|
| Joystick | Joystick |
| B oder A | Feuerknopf |
| Center + B | RESET-Schalter |
| Center + A | SELECT-Schalter |
| RESET | zurück zum Starter |

Im Link-Spiel hat Spieler 1 den linken, Spieler 2 den rechten Joystick; die
Konsolenschalter können beide drücken.

## Aufbau

| Datei | Aufgabe |
|---|---|
| `core/cpu6502.c` | 6507: jeder Buszyklus ist ein Zugriff, auch die Blindzugriffe, damit die TIA die Schreibzugriffe am richtigen Takt sieht |
| `core/a2600.c` | Maschine: Adressdekodierung, RIOT (Timer wird erst beim Lesen ausgerechnet), Bankumschaltung, Bildablauf (76 CPU-Takte pro Zeile, WSYNC springt ans Zeilenende); der CPU-Kern wird hier mitübersetzt, damit Befehle aus dem ROM ohne Umweg gelesen werden |
| `core/a7800.c` | Atari 7800: MARIA, RIOT, Speicher, Bankumschaltung; der 6502 ein zweites Mal unter eigenen Namen |
| `core/pokey.c` | POKEY-Ton für 7800-Module, die einen haben |
| `core/tia.c` | TIA: Spielfeld, zwei Spieler, zwei Raketen, Ball, Kollisionen, HMOVE, vertikale Verzögerung; das Bild wird bis zum aktuellen Takt nachgezeichnet, bevor ein Register sich ändert. Zwei Tonkanäle mit allen AUDC-Formen |
| `src/main.c` | Start, Spielschleife, Link |
| `src/a26_video.c` | jede fertige Zeile geht als Farbindizes über einen Ring (halbes Bild) an Kern 1; der rechnet 160 → 240 Pixel (a, Mittelwert, b) und schickt sie ans Display. Gezeigt werden die Zeilen 26 bis 265 |
| `src/rom_store.c` | Modul in den Flash |

Das Display bekommt wie bei Mega Drive, PC Engine und MSX 75 MHz SPI aus der
USB-PLL. Der Speicher startet fest mit Nullen, nichts hängt von der Zeit ab:
Beide Geräte rechnen im Link-Spiel dasselbe.

## Lizenz

Gesamtwerk **GPL-3.0** (`LICENSE`); Plattformteile aus PicoBoyGB MIT
(`LICENSE.picoboygb`), FatFs eigene BSD-artige Lizenz.
