# Spout für den PicoBoy Color Plus

Portierung des Höhlenflugspiels **Spout** (Kuni, 2002–2006; Unix-Fassung
spout-1.4 von Nick White, 2010, MIT) auf den PicoBoy Color Plus
(RP2350, Pico SDK, C/CMake).

Ziel des Spiels: so hoch wie möglich kommen. Das Schiff fällt, der Schub
stößt einen Sandstrahl aus, der einen nach oben drückt. Die Körner prallen
von den Höhlenwänden ab und **zerstören sie**: grauer Fels hält drei Treffer
aus. Alle 128 Höhenmeter versperrt eine rote Sperrmauer den Weg — sie muss
weggeschossen werden — und es gibt Zeitbonus.

## Bedienung

| | |
|---|---|
| **B** | Schub |
| **Links / Rechts** | drehen |
| **A** | Pause |
| **A** oder **B** | im Pausenbild weiter |
| **A** oder **B** | Start bzw. nach „game over" zurück zum Titel |

Beim Einschalten gehaltene Tasten:

| | |
|---|---|
| **B** | Ton aus (schließt einen hängenden Tonpfad als Ursache aus) |
| **A** | Debugzeile: Bildzeit, Rechenzeit, Zahl der Körner |
| **Hoch** | dunkles Farbschema |

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

Erzeugt `build/spout.uf2` (Spiel) und `build/spout_selftest.uf2`
(Testfirmware für Display, Tasten, Ton und Bildzeitmessung).

Optional nach der Messung auf Seite 5 der Testfirmware:
`-DPBC_SPI_HZ=80000000` verkürzt die Bildzeit von ~14,8 auf ~11,5 ms.

## Flashen

BOOTSEL gedrückt halten, USB einstecken, loslassen; das Gerät meldet sich
als `RPI-RP2`.

```bash
picotool info -a                 # muss rp2350-arm-s melden
picotool erase -a                # Pflicht, wenn vorher MicroPython o.ä. drauf war
picotool load -x build/spout.uf2
```

Danach einmal wirklich stromlos machen.

## Prüfen ohne Gerät

```bash
cd tools && make -f Makefile.host
./host_test 30000 0xC0FFEE            # normaler Lauf
./host_test 60000 0x1234 --fuzz       # Zufallseingaben
./host_test 80000 0xBEEF --nodeath    # tief, bis in die engsten Höhlenstufen
./host_test 4000 0xBEEF --shots /tmp/s  # PPM-Bilder ausgeben
```

Der Host-Test baut Spielkern **und** Renderer nativ mit
`-fsanitize=address,undefined`, spielt sie streifenweise durch wie das Gerät
und prüft nach jedem Bild die Kornbuchhaltung (`spout_selfcheck`).

## Aufbau

```
src/game.c          Spiellogik, portiert aus spout-1.4, plattformfrei
src/render.c        Bildaufbau 240x280 RGB565, zeilenweise
src/palette.h       Zellbyte -> Farbe (eine einzige Stelle)
src/pbc/            Gerät: ST7789, Tasten, PWM/DMA-Ton, Flash, LEDs, Panikbild
selftest/           geräteseitige Testfirmware
src/font*.{c,h}     Bitmapschriften, je Größe einzeln gerastert
tools/              Host-Test und die Generatoren für Schrift und Sinustabelle
doc/PORT.md         was gegenüber der Vorlage geändert wurde und warum
```

Lizenz: MIT, siehe `COPYING`. Die Schriftdaten sind aus DejaVu Sans
gerastert (Bitstream-Vera-Lizenz, `doc/LICENSE.DejaVu`).
