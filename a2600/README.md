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
30,0 s, CPU 62 %). Link-Spiel auf dem Gerät noch nicht getestet.

## Spiele

`.a26`: Module bis 512 KB. Die Bankumschaltung errät der Kern aus Größe und
Inhalt: 2 KB, 4 KB, F8 (8 KB), F6 (16 KB), F4 (32 KB), jeweils auch mit
Superchip-RAM, E0 (Parker Brothers), FE (Activision) und 3F (Tigervision).
Paddles, Keypads und andere Controller gibt es nicht, nur Joysticks.

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
