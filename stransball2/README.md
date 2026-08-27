# Super Transball 2 für den PicoBoy Color Plus

Portierung von **Super Transball 2** von Santiago Ontañón (2002–2005) auf den
PicoBoy Color Plus (RP2350, Pico SDK, C/CMake).

Ein Thrust-artiges Höhlenflugspiel: finde die Kugel, greife sie mit dem
Traktorstrahl und bringe sie nach oben aus dem Level heraus. Gegen dich
arbeiten Schwerkraft, knapper Treibstoff, Kanonen, Panzer, Türen und Schalter.

## Lizenz — bitte beachten

Die Vorlage steht unter **GPL-2.0**. Diese Portierung steht damit ebenfalls
unter GPL-2.0: wer die Firmware weitergibt, muss den Quelltext mitliefern oder
anbieten. Das ist anders als bei den MIT-/BSD-lizenzierten Ports. Der volle
Lizenztext liegt als `COPYING` bei.

## Bedienung

| | |
|---|---|
| **Links / Rechts** | Schiff drehen |
| **B** | Schub |
| **A** | Traktorstrahl (Kugel greifen und halten) |
| **Mitte** | feuern |
| **Links / Rechts** im Titel | Level wählen |
| **A** oder **B** | starten, Schiff wählen, weiter |

Beim Einschalten gehaltene Tasten: **B** = Ton aus, **A** = Debugzeile.

Drei Schiffe stehen zur Wahl, wie im Original: SHADOW RUNNER (schnell, schwache
Waffe), V-PANTHER 2 (ausgewogen), X-TERMINATOR (starke Waffe, träge).

## Bauen

Am einfachsten:

```bash
./build.sh          # baut und legt die .uf2 in ../dist/ ab
./build.sh clean    # Bauordner vorher löschen
```

Das Skript holt bei Bedarf das Pico SDK, baut, sammelt das Ergebnis ein und
prüft die erzeugte Datei. Von Hand geht es weiterhin:

```bash
cmake -B build -DPICO_SDK_PATH=$HOME/pico/pico-sdk \
      -DCMAKE_BUILD_TYPE=MinSizeRel -Dpicotool_DIR=/usr/local/lib/cmake/picotool
cmake --build build -j
```

> `picotool_DIR` zeigt auf eine bereits gebaute picotool-Installation. Wer
> keine hat, lässt den Schalter weg und setzt stattdessen
> `PICOTOOL_FETCH_FROM_GIT_PATH=~/.cache/pbc_plus_picotool` — dann baut das
> SDK sich picotool einmalig selbst. Genau das macht `build.sh`.

Erzeugt `build/stransball2.uf2`.

Die Spieldaten liegen als Originaldateien in `data_src/` (Kacheln als PCX,
Level als Text). Nach Änderungen daran:

```bash
python3 tools/mkdata.py    # Kacheln, Masken, Level -> src/data_*.c
python3 tools/mktrig.py    # Winkeltabellen -> src/trig.h
```

Auf dem Gerät läuft weder ein Bilddecoder noch ein Dateisystem.

## Prüfen ohne Gerät

```bash
cd tools && make -f Makefile.host
./host_test 4000 0x77 --level 9        # ein bestimmter Level
./host_test 20000 0xAB --fuzz          # Zufallseingaben
./host_test 900 0xBEEF --shots /tmp/s  # PPM-Bilder ausgeben
```

Baut mit `-fsanitize=address,undefined`; `stb_selfcheck()` läuft nach jedem Bild.

## Aufbau

```
data_src/           Originaldaten: tiles.pcx, tiles-mask.pcx, 14 Level
tools/mkdata.py     wandelt sie nach src/data_*.c
src/game.c          Spiellogik, Kollision und Renderer, plattformfrei
src/pbc/            Gerät: ST7789, Tasten, PWM/DMA-Ton, Flash, LEDs, Panikbild
doc/PORT.md         was gegenüber der Vorlage geändert wurde und warum
```
