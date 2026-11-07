# mdhost — der Mega-Drive-Kern auf dem PC

Ein kleines Kommandozeilenprogramm, um Bildfehler ohne Flashen zu finden.
Es lässt **dieselben Kernquellen** wie die Firmware laufen (68000, Z80,
VDP, YM2612-Timer) und rechnet jedes Bild genauso wie das Gerät, mit der
Palette pro Zeile aus `src/video.c`. Mittendrin wechselnde Paletten
(Wasser, Rasterfarben) sehen also aus wie auf dem PicoBoy.

Die paar ARM-Assembler-Stücke ersetzt `host.c` durch C (Speicherzugriffe,
Z80-Speicher) bzw. nimmt die C-Fassung, die im Kern schon steckt
(VDP-Zeilenroutinen). Die Bildschleife folgt `emulation_loop()` in
`src/main.c`; ändert sich die, `host.c` mitziehen.

**Nicht abgebildet:** das Zusammenspiel der beiden Kerne (Kern 1 rechnet
Zeilen später um) und der Ton. Fehler, die nur daran liegen, sieht man nur
auf dem Gerät.

## Bauen und benutzen

```bash
./build.sh                                   # -> ./mdhost (gcc)
./mdhost SPIEL.md 1500 -d 1400:10 -o /tmp/bilder
./mdhost "Sonic 2.md" 1510 -z 0x0D01 -d 1500 -o /tmp/bilder -p
```

| Option | Bedeutung |
|---|---|
| `ROM FRAMES` | ROM-Datei und Anzahl Bilder (60 = eine Sekunde) |
| `-d FROM[:EVERY]` | Bilder ab FROM als `fNNNNN.ppm` speichern, jedes EVERY-te (sonst nur das letzte) |
| `-o DIR` | Ordner für die Bilder (Vorgabe: aktueller) |
| `-z ZONE` | Sonic 2: START drücken, bis ein Level läuft, dieses Level/diesen Akt laden (`0x0D01` Chemical Plant 2, `0x0F00` Aquatic Ruin 1), dann nach rechts laufen und springen. Ohne `-z`: 20 Sekunden lang alle zwei Sekunden START |
| `-k DATEI` | Pad 1 aus einer Datei mit Zeilen `BILD TASTEN` (U D L R A B C S, `-` für keine), jede gilt bis zur nächsten; z. B. Sonic 2 zu zweit: `900 D`, `910 -`, `960 S`, `966 -`, `1150 S`, `1156 -` |
| `-s` | mit `-z`: 100 Bilder nach Levelbeginn in den Bonuslevel (Tunnel) wechseln. Mit `MDHOST_TIME=1` davor gibt es die Rechenzeit jedes Bildes aus |
| `-p` | je gespeichertem Bild ausgeben, ab welcher Zeile wie viele der 64 Farben umgelenkt sind (Palettenwechsel mitten im Bild) |

**Zwei Kerne vergleichen:** `MDHOST_TRACE=DATEI` schreibt vor jedem
68000-Befehl PC, SR, alle Register und die Taktzahl (19 Wörter). Zwei
Fassungen (z. B. `CPU=old OUT=mdhost-old ./build.sh` mit dem früheren Kern)
laufen lassen und die Spuren mit `cmp` vergleichen; so wurde Musashi 4 gegen
den alten Kern geprüft.

Gefunden damit: die Interrupt-Reihenfolge und die halbe Palettenzeile
an der Wasseroberfläche in Sonic 2 (siehe `../../README.md`). 1500 Bilder
dauern weniger als eine Sekunde.

## Zwei Stände vergleichen (`cmp/`)

Für Optimierungen, die nichts ändern dürfen. `hostcmp` lässt den Kern wie
`mdhost` laufen, dazu den Ton wie auf dem Gerät (Kern 0 protokolliert,
`src/ym_core1.c` spielt in die zweite YM2612-Kopie und den PSG ein) und
den Skalierer (`src/video.c`), und gibt je Bild Prüfsummen aus: Bild am
Display, VDP-Puffer, YM2612, PSG, Lautsprecher, 68000- und Z80-RAM,
VRAM/CRAM/VSRAM/Register und den CPU-Zustand nach jedem Zeitabschnitt.

```bash
rsync -a --exclude 'build*' megadrive/ /tmp/alt/   # Stand vorher sichern
# ... ändern ...
mkdir /tmp/v && cd /tmp/v && .../tools/host/cmp/cmp.sh /tmp/alt 6000
```

`cmp.sh` baut beide Stände (`hb.sh`) und vergleicht Sonic 2, Streets of
Rage 2 und Gunstar Heroes (`ROMS=Ordner`) mit Eingaben, Zufallstests für
die VDP-Ports (`-F`) und den Zeilenzeichner (`-L`), den Skalierer
(`vtest.c`) und die YM2612-Synthese (`ymfuzz.c`); jede Zeile muss `SAME`
sagen.

Der PC-Bau nimmt die C-Fassung der VDP-Zeilenroutinen. Für die
ARM-Assembler-Teile und um zu zählen, was ein Kern tut: `ab.sh STAND BIN`
baut `hostcmp` für den Cortex-M33 mit den Schaltern der Firmware,
`armrun.py` (Python mit `unicorn`, `pyelftools`) lässt es darauf laufen und
gibt Befehle pro Bild nach Teil aus (68000, Z80, Zeilen, Skalierer, Ton
auf Kern 1; `HC_DETAIL=N` je Funktion) und eine Prüfsumme über alle Bilder.
`armbench.sh BIN...` macht das für die drei Spiele. Flash-Wartezeiten sind
nicht nachgebildet. Am PC und auf dem M33 kommen dieselben Prüfsummen heraus
(`HC_FOLD=1`).
