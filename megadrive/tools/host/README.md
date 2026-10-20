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
| `-p` | je gespeichertem Bild ausgeben, ab welcher Zeile wie viele der 64 Farben umgelenkt sind (Palettenwechsel mitten im Bild) |

Gefunden damit: die Interrupt-Reihenfolge und die halbe Palettenzeile
an der Wasseroberfläche in Sonic 2 (siehe `../../README.md`). 1500 Bilder
dauern weniger als eine Sekunde.
