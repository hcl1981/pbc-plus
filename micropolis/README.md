# Micropolis für den PicoBoy Color Plus

Portierung von **Micropolis** — dem quelloffen gemachten Kern von *SimCity 1* —
auf den PicoBoy Color Plus (RP2350A, ST7789 240×280, Joystick + A/B).

Gebaut wird der **echte vendorierte Simulationskern** (`src/engine_micropolis.c`
zusammen mit `vendor/micropolis/sim/s_*.c`), nicht etwa ein Nachbau: dieselbe
Simulation, die 1989 im Original lief, nur ohne X11 und Tcl/Tk.

```bash
./build.sh          # baut nach ../dist/micropolis.uf2
./build.sh clean    # Bauordner vorher löschen
```

> **Eine offene Frage:** Der Vorgänger MicroCityPBC war ein Arduino-Sketch und
> benutzte das voreingestellte `SPI` von arduino-pico — die Display-Pins
> SCK/MOSI wurden deshalb nie aufgeschrieben. `src/config.h` nimmt **spi0,
> SCK=GP18, MOSI=GP19** an. Bleibt der Schirm schwarz, ist das die erste
> Stelle, die man gegen den Schaltplan halten sollte. Alles Übrige (CS=10,
> DC=8, RST=9, Beleuchtung=26, Tasten) stammt unmittelbar aus dem Sketch.

## Bedienung

| | |
|---|---|
| **Joystick** | Zeiger bewegen; an den Rändern scrollt die Karte |
| **A** auf der Karte | Werkzeugpalette öffnen |
| **B** auf der Karte | mit dem gewählten Werkzeug bauen |
| **Joystick** in der Palette | in zwei Richtungen wählen |
| **B** in der Palette | Eintrag übernehmen |
| **A** in der Palette | Palette wieder schließen |

Die Mitteltaste wird nicht benutzt. Der Zeiger zeigt die tatsächliche Grundfläche
des gewählten Werkzeugs. Solange die Palette offen ist, pausiert das Bauen.

Die Stadt **wird beim Einschalten automatisch geladen**; über die Palette lässt
sie sich speichern und laden.

## Aufbau

```
micropolis/
├── CMakeLists.txt          Baubeschreibung (RP2350)
├── build.sh                Bauskript
├── pico_sdk_import.cmake   findet das SDK
├── README.md  NOTICE.md  LICENSE
├── src/                    die Firmware
│   ├── main.c
│   ├── config.h
│   ├── st7789.{c,h}  input.{c,h}  save.{c,h}    Hardwareschicht
│   ├── render.{c,h}  ui.{c,h}                   Oberfläche und Zeichnen
│   ├── engine.h  engine_micropolis.c            Schnittstelle zum Kern
│   ├── micropolis_glue.c                        Anbindung des Kerns ohne Oberfläche
│   └── tiles.h  tiles_micropolis.c              960 Originalkacheln im Flash
├── tools/
│   └── convert_tiles.py    Micropolis-Kacheln → RGB565-Header
└── vendor/
    └── micropolis/         der Simulationskern (GPL-3.0) — siehe NOTICE.md
```

## Wie es zusammenspielt

```
main.c ── Bildschleife: Eingabe → Oberfläche → engine_tick → Zeichnen → Ausgabe
 │
 ├─ st7789.c   Displaytreiber (8-Bit-Befehle, 16-Bit-DMA fürs ganze Bild)
 ├─ input.c    Tasten über GPIO, Flankenerkennung und Wiederholung
 ├─ save.c     Stadtzustand in die obersten 32 KB des Flash
 ├─ render.c   Kacheln zeichnen, Zeiger, Statusleiste, RCI-Anzeige
 ├─ ui.c       Zeiger, Scrollen, Palette, Bauen, Speichern und Laden
 │
 ├─ engine.h   ◄── die Naht: render.c und ui.c rufen ausschließlich hierüber
 │   └─ engine_micropolis.c  umschließt den vendorierten s_*.c-Kern
 │
 └─ tiles.h    ◄── tile_get(id) → 256 Bildpunkte in RGB565
     └─ tiles_micropolis.c   960 Originalkacheln, rund 480 KB im Flash
```

Die beiden Nahtstellen `engine.h` und `tiles.h` sind der eigentliche Kniff:
`render.c` und `ui.c` müssen nie wissen, was dahinter steckt.

## Speicher (520 KB SRAM, kein PSRAM)

| Block | Größe |
|---|---|
| Bildpuffer (240×280 in RGB565) | rund 131 KB |
| Micropolis-Karten (120×100) | rund 24 KB |
| Teilkarten der Simulation und Zustand | rund 33 KB |
| Zwischenpuffer beim Speichern | 64 KB * |
| Stapel, Heap, BSS | der Rest |

\* Der Speicherpuffer wird nur während des Speicherns und Ladens belegt.
Die Kacheln (rund 480 KB) und der Spielstand liegen im Flash, nicht im SRAM.

## Wie der Kern portiert wurde

Der ganze X11-, Tcl- und Tk-Unterbau ist herausgetrennt: Die Oberflächen- und
systemabhängigen Einbindungen in `sim.h` stehen hinter `#ifndef HEADLESS` und
werden durch `headers/headless_shims.h` ersetzt — Attrappen-Typen, damit die
oberflächennahen Strukturen in `view.h` weiterhin übersetzen.

Die rund 90 Symbole aus Oberfläche, Ton, Sprites und Diagrammen, die der Kern
erwartet, liefert `src/micropolis_glue.c`: 34 Modellvariablen, echte
Speicherverwaltung und im Übrigen wirkungslose Haken. Darüber setzt
`src/engine_micropolis.c` die Schnittstelle aus `engine.h`.

Vendoriert sind `s_alloc`, `s_sim`, `s_zone`, `s_traf`, `s_power`, `s_eval`,
`s_scan`, `s_disast`, `s_gen`, `s_init`, `s_msg`, `s_fileio` und `rand*`, dazu
die Werkzeugschicht `w_tool.c` (Werkzeuge, Grundflächen 3×3, 4×4 und 6×6,
Kosten) und `w_con.c` (Straßen, Schienen und Leitungen verbinden sich von
selbst).

**Auf dem Wirtssystem nachgewiesen** (`tools/host_sim_test.c`): Der Kern
übersetzt ohne Oberfläche fehlerfrei, erzeugt eine Stadt und lässt `SimFrame()`
absturzfrei laufen, wobei die RCI-Nachfrage reagiert. Bauen zieht über `Spend`
Geld ab, die Grundflächen stimmen, Strom breitet sich von den Kraftwerken aus,
und bei `SimSpeed=3` steigt etwa die Wohnnachfrage einer versorgten Zone über
fünf simulierte Jahre auf +31.

Die Kacheln entstanden aus dem originalen `tiles.xpm` (16×15360 Bildpunkte =
960 Kacheln) über `tools/convert_tiles.py` — ein
`const uint16_t[960][256]`-Feld im Flash.

Gespeichert wird im Format von Micropolis selbst: `engine_state_blob` und
`engine_load_blob` schreiben sechs Verlaufsfelder, `MiscHis` und die Karte —
zusammen 27120 Byte in der `.cty`-Anordnung des Originals. Ein Durchlauf aus
Speichern, Zurücksetzen und Laden auf dem Wirtssystem stellt Kontostand, Jahr
und die vollständige Karte exakt wieder her.

## Was noch offen ist

* **Ton auf GP15** und **schnelleres Scrollen**.
* `s_fileio.c` bindet weiterhin `fopen`, `fread` und `fwrite` von newlib ein.
  Ohne dahinterliegendes Dateisystem tun die nichts. Speichern und Laden geht
  daran vorbei über `save.c`, deshalb fällt es im Betrieb nicht auf — es
  bliebe nur zu beachten, falls die vendorierte Dateiausgabe je unmittelbar
  benutzt werden soll.

## Lizenz

GPL-3.0, wie Micropolis. Siehe `LICENSE`; zur Namensverwendung außerdem
`vendor/micropolis/MicropolisPublicNameLicense.md` und `NOTICE.md`.
