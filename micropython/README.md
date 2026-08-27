# MicroPython für den PicoBoy Color Plus

MicroPython mit einem eigenen Board `PBC_PLUS` und zwei C-Modulen, die
Display und Hardware zugänglich machen. Danach reicht ein `import pbc`, um mit
wenigen Zeilen etwas auf den Schirm zu bringen.

```bash
./build.sh              # -> ../dist/micropython-PBC_PLUS.uf2
```

## Warum hier so wenig liegt

In diesem Ordner steckt **nur die Board-Portierung** — 38 Dateien. MicroPython
selbst ist unverändertes Upstream und wird deshalb nicht mitgeliefert, sondern
beim ersten Bauen geholt (Version `v1.28.0`, nach `~/.cache/pbcp-sdks`).

Das hält dieses Repo klein und macht sofort sichtbar, was an der Portierung
wirklich eigen ist. Wer MicroPython schon ausgecheckt hat, setzt
`MICROPYTHON_PATH` und spart sich das Herunterladen.

## Was das Board mitbringt

**`pbc_display`** — ST7789 240×280, Zeichenprimitive und Schriften in drei
Größen. Bilder werden als PNG geladen (`png_decode.c`, `idat_stream.c`), also
ohne vorherige Umwandlung in ein Sonderformat.

**`pbc_hw`** — Tasten (abfragen, „war gedrückt", Rückrufe bei Tastendruck),
Beschleunigungssensor (roh und in g), Akkuspannung, RGB-LED und Tonausgabe.

**`pbc.py`** — die Python-Schicht darüber. Neben den Zeichenbefehlen gibt es
`Canvas` (ein `framebuf.FrameBuffer` mit Zusatzfunktionen), `Sprite` samt
Hintergrundwiederherstellung, `load_image()`, ein Programm-Menü (`menu()`) und
`help()`.

Dazu `png.py` und `turtle.py` — Letzteres eine Turtle-Grafik zum Einstieg.

```python
import pbc

pbc.clear(pbc.BLACK)
pbc.text24("Hallo", 20, 100, pbc.WHITE)
pbc.rect(10, 10, 220, 260, pbc.RED)

if pbc.pressed_a():
    pbc.led_green(True)
```

Die vollständige Aufstellung steht in [`PBC_PLUS/README.md`](PBC_PLUS/README.md)
— die ist bewusst auf Englisch gehalten, weil sich der Board-Ordner so
unverändert bei MicroPython einreichen ließe.

## Flashen

BOOTSEL gedrückt halten, USB anstecken, `micropython-PBC_PLUS.uf2` auf das
erscheinende Laufwerk kopieren. Danach meldet sich das Gerät als serielle
Konsole (REPL) — etwa mit `mpremote` oder Thonny.

Eigene Programme lassen sich per `mpremote cp` auf das Gerät kopieren; `menu()`
listet sie zum Starten auf.

## Lizenz

MIT — sowohl MicroPython selbst als auch diese Board-Portierung. Siehe
`LICENSE`.
