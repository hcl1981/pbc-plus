# PBC MSX

MSX1-Emulator für den **PicoBoy Color Plus**, nur als Teil von PBCemu
(`../pbcemu/build.sh` → `dist/pbcemu.uf2`); eine Einzel-Firmware gibt es nicht.

Der Kern (`core/msx1`) ist für den PicoBoy geschrieben (GPL-3.0-or-later):
Videochip TMS9918A, Soundchip AY-3-8910, Konami-SCC, Slots, PPI 8255 und die
MegaROM-Mapper. Dazu kommen der Z80 **redcode/Z80** (`core/redcode`,
LGPL-3.0-or-later, derselbe wie im Master System) und **C-BIOS 0.29a** als
System-ROM (`core/cbios`, BSD-artig, `LICENSE.cbios`). Bis November 2026 lief
hier fMSX (MSX2), das nur nicht-kommerziell genutzt werden darf; davon ist
nichts übrig.

**Stand:** am PC mit C-BIOS und fünf freien MSX1-Spielen getestet (Tales of
Popolon, XSpelunker, Transball, The Menace from Triton, Westen House: Grafik,
Sprites, Ton); auf dem Gerät läuft Triton in Echtzeit (Benchmark: 1800 Bilder
in 30,0 s, CPU 76 %, der Rest Warten aufs Display). Link-Spiel und MegaROMs
auf dem Gerät noch nicht getestet.

## Spiele

`.rom` und `.mx1`: MSX1-Module. Einfache ROMs bis 64 KB (16, 32, 48 KB, auch
ab 0000h), MegaROMs mit den Mappern Konami, Konami SCC, ASCII 8 KB und ASCII
16 KB. Den Mapper errät der Kern aus den Bankumschaltbefehlen im ROM
(`LD (nnnn),A` auf die Register des jeweiligen Mappers); unter dem Bild steht
beim Start, welcher es geworden ist. C-BIOS hat kein BASIC und keine
Disketten; MSX2-Spiele laufen nicht.

## Tasten

| PicoBoy | MSX |
|---|---|
| Joystick | Joystick 1 und Cursortasten |
| B | Feuer 1 und SPACE |
| A | Feuer 2 und M |
| Center + B | F1 |
| Center + A | Menü: jede andere Taste einmal drücken |
| RESET | zurück zum Starter |

Im Link-Spiel hat Spieler 1 Joystick 1 und die Tastatur, Spieler 2 Joystick 2.
Wer das Menü öffnet, wählt; beide halten nach demselben Bild an, und die
gewählte Taste kommt auf beiden Geräten im selben Bild an.

## Aufbau

| Datei | Aufgabe |
|---|---|
| `core/msx1/msx1.c` | Maschine: Slots, 64 KB RAM, PPI (Slots, Tastaturmatrix), I/O, Mapper, Bild- und Tonablauf (228 Takte × 262 Zeilen, 60 Hz) |
| `core/msx1/tms9918.c` | Videochip: Grafik I/II, Multicolor, Text, 32 Sprites (4 pro Zeile, 5.-Sprite- und Kollisionsflag) |
| `core/msx1/ay8910.c` | PSG: 3 Töne, Rauschen, Hüllkurve; jedes Sample mittelt über seine Dauer |
| `core/msx1/scc.c` | Konami-SCC: 5 Wellenformkanäle |
| `src/main.c` | Start, Spielschleife, Menü, Link |
| `src/msx_video.c` | jede fertige Zeile geht über einen kleinen Ring an Kern 1 und von dort ans Display; kein Bildpuffer. Modi *Fit width* (256 → 240) und *Crop 1:1* |
| `src/msx_input.c` | Tasten → Joysticks und Tastaturmatrix |
| `src/rom_store.c` | Modul in den Flash, MegaROMs auf 2ⁿ Seiten gespiegelt |

Ton wird vor jedem PSG- oder SCC-Zugriff bis zum aktuellen Takt erzeugt; so
behalten Samples, die Spiele über die Lautstärkeregister abspielen, ihr
Timing. Das Display bekommt wie bei Mega Drive und PC Engine 75 MHz SPI aus
der USB-PLL. Der Speicher startet fest mit Nullen, nichts hängt von der Zeit
ab: Beide Geräte rechnen im Link-Spiel dasselbe.

## Lizenz

Gesamtwerk **GPL-3.0** (`LICENSE`); `core/redcode` LGPL-3.0-or-later,
`core/cbios` C-BIOS-Lizenz (BSD-artig), Plattformteile aus PicoBoyGB MIT
(`LICENSE.picoboygb`), FatFs eigene BSD-artige Lizenz.
