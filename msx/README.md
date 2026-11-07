# PBC MSX

MSX1- und MSX2-Emulator für den **PicoBoy Color Plus**, nur als Teil von PBCemu
(`../pbcemu/build.sh` → `dist/pbcemu.uf2`); eine Einzel-Firmware gibt es nicht.

Der Kern (`core/msx1`) ist für den PicoBoy geschrieben (GPL-3.0-or-later):
Videochip TMS9918A, Soundchip AY-3-8910, Konami-SCC, Slots, PPI 8255 und die
MegaROM-Mapper; für MSX2 der Videochip V9938 (`v9938.c`), Memory-Mapper mit
128 KB, die Uhr RP5C01 und Diskettenlaufwerk A:. Dazu kommen der Z80 **redcode/Z80** (`core/redcode`,
LGPL-3.0-or-later, derselbe wie im Master System) und **C-BIOS 0.29a** als
System-ROM für MSX1 und MSX2, samt seinem Disk-ROM C-DISK (`core/cbios`,
BSD-artig, `LICENSE.cbios`). Bis November 2026 lief
hier fMSX (MSX2), das nur nicht-kommerziell genutzt werden darf; davon ist
nichts übrig.

**Stand:** am PC mit C-BIOS und fünf freien MSX1-Spielen getestet (Tales of
Popolon, XSpelunker, Transball, The Menace from Triton, Westen House: Grafik,
Sprites, Ton); auf dem Gerät läuft Triton in Echtzeit (Benchmark: 1800 Bilder
in 30,0 s, CPU 76 %, der Rest Warten aufs Display). Link-Spiel und MegaROMs
auf dem Gerät noch nicht getestet. MSX2 am PC mit den ROMs aus der
fMSX-Distribution getestet (Logo, BASIC 2.1, Diskette lesen und schreiben,
SCREEN 4 bis 8 mit LINE, CIRCLE, PAINT und Sprites); mit C-BIOS am PC eine
selbst gebaute Bootdiskette (Bootsektor lädt per PHYDIO nach) und Triton als
Modul. Auf dem Gerät ist MSX2 noch nicht getestet.

## Spiele

`.rom` und `.mx1`: MSX1-Module. Einfache ROMs bis 64 KB (16, 32, 48 KB, auch
ab 0000h), MegaROMs mit den Mappern Konami, Konami SCC, ASCII 8 KB und ASCII
16 KB. Den Mapper errät der Kern aus den Bankumschaltbefehlen im ROM
(`LD (nnnn),A` auf die Register des jeweiligen Mappers); unter dem Bild steht
beim Start, welcher es geworden ist. C-BIOS hat kein BASIC und keine
Disketten.

Ein `.rom`, dessen Name einen bekannten MSX2-Titel enthält (Metal Gear,
Space Manbow, Vampire Killer, Usas, Aleste 2, Psycho World, Undeadline,
Super Laydock, Zanac EX, Golvellius II), startet von selbst auf dem MSX2:
Das Modul selbst verrät nicht, für welchen Rechner es ist. Andere
MSX2-Module brauchen weiter die Endung `.mx2`.

**MSX2:** `.mx2` (Module, wie oben) und `.dsk` (Disketten-Abbilder, 360 oder
720 KB) laufen auf einem MSX2. Eingebaut ist C-BIOS für MSX2 mit C-DISK:
Module laufen damit, Disketten nur, wenn sie über ihren Bootsektor selbst
starten (viele Spiele). BASIC und MSX-DOS gibt es damit nicht; Disketten mit
`AUTOEXEC.BAS`, BASIC-Ladern oder MSX-DOS erkennt der Emulator am Bootsektor
und sagt, was fehlt. Für sie gehören die originalen System-ROMs ins
Hauptverzeichnis des USB-Laufwerks: `msx2.rom` (Haupt-ROM, 32 KB),
`msx2ext.rom` (Sub-ROM, 16 KB) und für Disketten `disk.rom` (16 KB). Liegt
dieser Satz vollständig dort, wird er statt C-BIOS benutzt (nie gemischt).
Die Spieleliste zeigt sie nicht; unter dem Bild steht beim Start „MSX2“
bzw. „MSX2 C-BIOS“. Das Disk-ROM (und die ROMs vom Laufwerk) kopiert der
Emulator beim Start in den Flash hinter das Modul. Die Maschine:
Haupt-ROM in Slot 0, Module in Slot 1, Slot 3 erweitert mit Sub-ROM in 3-0
und 3-1, RAM in 3-2 und Disk-ROM in 3-3. Diskettenzugriffe laufen nicht über
einen nachgebildeten Controller, sondern über die Einsprünge, die jedes
Disk-ROM gleich hat (DSKIO, DSKCHG, GETDPB, …); sie lesen und schreiben die
`.dsk`-Datei direkt. Spiele, die den Controller selbst ansprechen (manche
Kopierschutze), laufen deshalb nicht. Die Befehle des V9938 (Kopieren,
Füllen, Linien) sind sofort fertig statt nach ihrer echten Laufzeit.

## Tasten

| PicoBoy | MSX |
|---|---|
| Joystick | Joystick 1 und Cursortasten |
| B | Feuer 1 und SPACE |
| A | Feuer 2 und M |
| Center + B | F1 |
| Center + A | Menü: „Swap disk“ (bei Diskettenspielen), „Press a key“ (jede Taste einmal drücken), „Back to game“ |
| RESET | zurück zum Starter |

**Diskettenwechsel:** Bei einem Diskettenspiel steht oben im Menü „Swap
disk“: alle MSX-Disketten des Laufwerks (CPC-Disketten mit derselben Endung
bleiben draußen). Die gewählte kommt sofort in Laufwerk A:, DSKCHG meldet den
Wechsel. Im Link-Spiel wechseln beide nur, wenn der ganze Inhalt der Datei auf
beiden gleich ist (die Disketten werden beschrieben).

Im Link-Spiel hat Spieler 1 Joystick 1 und die Tastatur, Spieler 2 Joystick 2.
Wer das Menü öffnet, wählt; beide halten nach demselben Bild an, und die
gewählte Taste kommt auf beiden Geräten im selben Bild an.

## Aufbau

| Datei | Aufgabe |
|---|---|
| `core/msx1/msx1.c` | Maschine: Slots, 64 KB RAM, PPI (Slots, Tastaturmatrix), I/O, Mapper, Bild- und Tonablauf (228 Takte × 262 Zeilen, 60 Hz) |
| `core/msx1/tms9918.c` | Videochip: Grafik I/II, Multicolor, Text, 32 Sprites (4 pro Zeile, 5.-Sprite- und Kollisionsflag) |
| `core/msx1/v9938.c` | MSX2-Videochip: alle Modi bis Grafik 7 und Text 80, Sprite-Modus 2, Palette, Scrolling, Zeileninterrupt, Befehle; Zeilen als RGB565 |
| `core/msx1/ay8910.c` | PSG: 3 Töne, Rauschen, Hüllkurve; jedes Sample mittelt über seine Dauer, Strecken ohne hörbare Änderung in einem Schritt |
| `core/msx1/scc.c` | Konami-SCC: 5 Wellenformkanäle |
| `src/main.c` | Start, Spielschleife, Menü, Link; MSX2: C-BIOS oder System-ROMs vom Laufwerk, Diskette |
| `src/msx_video.c` | der Kern zeichnet jede Zeile roh (Farbindizes bzw. RGB565) direkt in einen kleinen Ring, Kern 1 rechnet sie um und schickt sie ans Display; kein Bildpuffer. Modi *Fit width* (256 → 240) und *Crop 1:1* |
| `src/msx_input.c` | Tasten → Joysticks und Tastaturmatrix |
| `src/rom_store.c` | Modul in den Flash, MegaROMs auf 2ⁿ Seiten gespiegelt |

Ton wird vor jedem PSG- oder SCC-Zugriff bis zum aktuellen Takt erzeugt; so
behalten Samples, die Spiele über die Lautstärkeregister abspielen, ihr
Timing. Das Display bekommt wie bei Mega Drive und PC Engine 75 MHz SPI aus
der USB-PLL. Der Speicher startet fest mit Nullen, nichts hängt von der Zeit
ab: Beide Geräte rechnen im Link-Spiel dasselbe.

Für die Geschwindigkeit: Beim MSX1 laufen das Haupt-ROM und einfache
Module bis 32 KB aus Kopien in den sonst freien oberen 64 KB des RAM
(MegaROMs bleiben im Flash). Die Port-Zugriffe (VDP, PSG, SCC, Slots), die
Befehle des V9938 und die Tabellen des PSG liegen im RAM; Schreiben ins RAM
geht über eine Seitentabelle direkt. Die Sprites des TMS9918 löschen nur die
Spalten, die sie bedecken.

## Lizenz

Gesamtwerk **GPL-3.0** (`LICENSE`); `core/redcode` LGPL-3.0-or-later,
`core/cbios` C-BIOS-Lizenz (BSD-artig), Plattformteile aus PicoBoyGB MIT
(`LICENSE.picoboygb`), FatFs eigene BSD-artige Lizenz.
