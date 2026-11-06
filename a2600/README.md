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
Joysticks und Paddles; Keypads und andere Controller gibt es nicht.

**Paddles** steuert man durch Drehen des PicoBoy in der Bildebene, wie ein
Lenkrad (Beschleunigungssensor STK8BA58, `src/tilt.c`): ±35° vom Halten beim
Umschalten aus ist der ganze Weg, rechts herum dreht das Paddle im
Uhrzeigersinn. B oder A ist sein Knopf. Ob ein Spiel Paddles will, erkennt
der Emulator selbst: Liest es nach dem Entladen der Kondensatoren (VBLANK
Bit 7) 20 Bilder lang immer wieder INPT0–3, schaltet er um und zeigt
„PADDLE: TURN THE PICOBOY“ (ohne Sensor „PADDLE: JOYSTICK“, dann dreht der
Joystick links/rechts). Eine Liste bekannter Spiele (Prüfsummen) braucht es
so nicht, und Joystick-Spiele lesen INPT0–3 nicht. Die Ladezeit bis INPT
Bit 7 kommt aus der Stellung; der ganze Weg reicht vom Ende des Entladens
bis zum nächsten (meist das sichtbare Bild), so dass jedes Spiel seinen
ganzen Bereich bekommt. Im Link-Spiel ist Spieler 2 Paddle 1, seine
Stellung läuft in den oberen Bits des Link-Worts mit; allein ist Paddle 1
nicht angeschlossen. Paddle 2 und 3 (Warlords zu viert) gibt es nicht.

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
48 KB, SuperGame-Bankumschaltung (mit RAM oder zusätzlicher Bank bei
4000h) und, wenn der Kopf es sagt, die von Absolute (F-18 Hornet: Bank 0
oder 1 bei 4000h) und Activision (Double Dragon, Rampage: eine von acht
16-KB-Bänken bei A000h, gewählt durch Schreiben nach FF80h–FF8Fh; geprüft
mit eigenen Testmodulen, noch nicht mit den Spielen). Ein BIOS braucht es
nicht: das Spiel startet an seinem Reset-Vektor. PAL-Module (laut Kopf)
laufen mit 313 Zeilen und 50 Hz.

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
XM und YM2151 (z. B. 1942), die Zeitabläufe von MARIA innerhalb der Zeile.

| PicoBoy | Atari 7800 |
|---|---|
| Joystick | Joystick |
| B / A | linker / rechter Knopf |
| Center + B | RESET |
| Center + A | SELECT |
| Center + Runter | PAUSE |

## Vectrex

`.vec`-Module laufen auf einem nachgebildeten **GCE Vectrex** (`core/vectrex.c`,
für dieses Gerät geschrieben): der Motorola 6809 mit allen Befehlen und
Adressierungsarten, der VIA 6522 (Timer 1 mit PB7, Timer 2, Schieberegister
auf CB2, CA2/CB2), die Analogtechnik, die den Strahl führt (DAC an Port A,
Multiplexer an Port B, X/Y-Integratoren mit RAMP, ZERO und BLANK,
Helligkeit, der Joystick-Vergleicher), und der AY-3-8910 hinter Port A (aus
`../msx`). Der Strahl wird Takt für Takt verfolgt (ruhige Abschnitte, in
denen kein Timer abläuft und der Strahl geradeaus läuft, in einem Schritt);
jede beleuchtete Strecke wird eine Linie. Kern 0 rechnet ein Bild (30000
Takte, 50 Hz) und gibt die Linien an Kern 1, der sie in ein Helligkeitsbild
über das ganze Display zeichnet (240 × 280, hochkant wie der Vectrex), das
je Bild auf die Hälfte nachglüht, mit leichtem Leuchten um den Strahl. Zum
Display gehen nur Zeilen mit Licht und die, die gerade dunkel wurden.
Modul und System-ROM liegen im RAM. Das System-ROM mit Mine Storm ist
eingebaut wie bei vecx (`LICENSE.vectrex`).

| PicoBoy | Vectrex |
|---|---|
| Joystick | Joystick (am Anschlag) |
| B | Knopf 4 |
| A | Knopf 3 |
| Mitte + B | Knopf 2 |
| Mitte + A | Knopf 1 |

Am PC geprüft: Startbild, Mine Storm, Scramble, Berzerk, Star Castle,
Spike; auf dem Gerät volle Geschwindigkeit (50 Bilder/s). Kein Link-Spiel,
keine Farbfolien.

## Tasten

| PicoBoy | Atari 2600 |
|---|---|
| Joystick | Joystick |
| B oder A | Feuerknopf |
| PicoBoy drehen | Paddle (bei Paddle-Spielen, siehe oben), B oder A sein Knopf |
| Center + B | RESET-Schalter |
| Center + A | SELECT-Schalter |
| RESET | zurück zum Starter |

Im Link-Spiel hat Spieler 1 den linken, Spieler 2 den rechten Joystick; die
Konsolenschalter können beide drücken.

Die Schwierigkeitsschalter (links, rechts: A oder B) und beim 2600 Farbe
oder Schwarzweiß stellt man je Spiel in dessen Einstellungen im Starter
(Mitte in der Spieleliste); Vorgabe B, B, Farbe. Im Link-Spiel gelten die
Vorgaben.

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
| `src/tilt.c` | Beschleunigungssensor über I2C0 (GP20/21), Drehung in der Bildebene für die Paddles |

Das Display bekommt wie bei Mega Drive, PC Engine und MSX 75 MHz SPI aus der
USB-PLL. Der Speicher startet fest mit Nullen, nichts hängt von der Zeit ab:
Beide Geräte rechnen im Link-Spiel dasselbe.

## Lizenz

Gesamtwerk **GPL-3.0** (`LICENSE`); Plattformteile aus PicoBoyGB MIT
(`LICENSE.picoboygb`), FatFs eigene BSD-artige Lizenz.
