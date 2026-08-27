# Noiz2sa für den PicoBoy Color Plus

Portierung des abstrakten Kugelhagel-Schützen **noiz2sa 0.52** von Kenta Cho
(ABA Games, BSD) auf den PicoBoy Color Plus (RP2350, Pico SDK, C/CMake).

Ziel: dem Sperrfeuer ausweichen. Das Schiff überlebt die Berührung mit einem
Gegnerkörper, nur die Geschosse sind tödlich. Grüne Sterne sind Bonus — wer sie
in Folge einsammelt, steigert die Wertung (oben rechts). Zerstörte Gegner
löschen die Geschosse in ihrer Umgebung und lassen sie als Bonus zurück.

## Bedienung

| | |
|---|---|
| **Joystick** | Schiff bewegen |
| **B** | Feuer |
| **A** | langsam bewegen (feines Ausweichen) |
| **Links / Rechts** | im Titel die Stufe wählen |
| **A** oder **B** | starten, nach „game over" zurück zum Titel |

Einen Pausenknopf gibt es nicht — siehe `doc/PORT.md`, Abschnitt 6.

Beim Einschalten gehaltene Tasten: **B** = Ton aus, **A** = Debugzeile.

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

Erzeugt `build/noiz2sa.uf2`.

Die Sperrfeuermuster liegen als BulletML in `data_bml_src/`. Wer sie ändert
oder eigene hinzufügt, ruft danach

```bash
python3 tools/mkbml.py     # data_bml_src/*.xml -> src/bml_data.c
```

und baut neu. Ein XML-Parser läuft **nicht** auf dem Gerät.

## Flashen

BOOTSEL halten, USB einstecken, loslassen.

Dann die `.uf2` einfach auf das Laufwerk ziehen. Wer lieber `picotool` nimmt:

```bash
picotool info -a                 # muss rp2350-arm-s melden
picotool load -x build/noiz2sa.uf2
```

Danach einmal stromlos machen, sonst steht auf dem Display womöglich noch das
Bild des Vorgängers.

## Prüfen ohne Gerät

```bash
cd tools && make -f Makefile.host
./bml_test 2400                       # alle 73 Muster durchspielen
./host_test 30000 0x77 --stage 9      # Spiel, harte Stufe
./host_test 40000 0xAB --fuzz         # Zufallseingaben
./host_test 3000 0xBEEF --shots /tmp/s  # PPM-Bilder ausgeben
```

Beides baut mit `-fsanitize=address,undefined`.

## Aufbau

```
data_bml_src/       die 73 Sperrfeuermuster als BulletML (aus der Vorlage)
tools/mkbml.py      übersetzt sie nach src/bml_data.c
src/bml.c           führt den Bytecode aus (ersetzt libBulletML)
src/game.c          Spiellogik, portiert, plattformfrei
src/render.c        Bildaufbau 240x280 RGB565, zeilenweise
src/pbc/            Gerät: ST7789, Tasten, PWM/DMA-Ton, Flash, LEDs, Panikbild
doc/PORT.md         was gegenüber der Vorlage geändert wurde und warum
```

Lizenz: BSD, siehe `COPYING`. Schriftdaten aus DejaVu Sans
(`doc/LICENSE.DejaVu`).
