# Doom-WAD in eine flashbare Datei umwandeln

Die Doom-Firmware `../doom-usb-mp.uf2` bringt **keine Level mit**. Die stecken
in einer WAD-Datei, die getrennt an eine eigene Stelle im Flash geschrieben
wird. Dieses Werkzeug macht aus deiner WAD die passende `.uf2`.

Du brauchst eine eigene Kopie von `DOOM.WAD` oder `DOOM2.WAD` — etwa aus einer
gekauften Fassung oder von Steam. **Die WADs liegen hier nicht bei und dürfen
nicht weitergegeben werden**; das Werkzeug darfst du weitergeben, die
Spieldaten nicht.

## Windows

Die Datei **`bin\whd_gen-windows-x86_64.exe`** doppelklicken. Es öffnet sich
ein Dateiauswahl-Fenster; dort die WAD auswählen. Die fertige `.uf2` liegt
danach **neben der WAD**.

Alternativ die WAD-Datei mit der Maus **auf die .exe ziehen**.

Es muss nichts nachinstalliert werden — der Dialog benutzt nur eingebaute
Windows-Funktionen.

## Linux

```bash
bin/whd_gen-linux-x86_64  DOOM.WAD
```

Daraus wird `DOOM.uf2` im selben Ordner. Das Binary ist statisch gebunden und
braucht keine weiteren Pakete.

Falls die Datei nicht ausführbar ist:

```bash
chmod +x bin/whd_gen-linux-x86_64
```

## macOS

Für macOS liegt **kein fertiges Programm bei** — es muss auf einem Mac gebaut
werden:

```bash
./build-whd_gen.sh
```

## Eigener Ausgabename

```bash
bin/whd_gen-linux-x86_64  DOOM.WAD  MEIN-NAME.uf2  -no-super-tiny
```

* Endet der Zielname auf **`.uf2`**, entsteht eine flashfertige Datei für die
  Adresse `0x10048000`.
* Endet er auf **`.whd`**, wird nur das rohe Zwischenformat geschrieben.
* Gibt es die `.uf2` schon, wird sie **nicht überschrieben** — es wird `_1`,
  `_2` … angehängt.
* `-no-super-tiny` ist wichtig und wird bei der Kurzform automatisch gesetzt.
  Es passt zur hier mitgelieferten Firmware, die die Vollversion mit drei
  Episoden unterstützt.

## Aufs Gerät bringen

Zwei Dateien, zwei Adressen — die Firmware und die WAD überschneiden sich
nicht, du kannst sie also unabhängig voneinander flashen:

| Datei | Inhalt | Adresse |
|---|---|---|
| `../doom-usb-mp.uf2` | das Spiel | `0x10000000` |
| deine `DOOM.uf2` | die Level | `0x10048000` |

**Beim ersten Mal muss der Flash komplett gelöscht werden.** Reines
Drag&Drop genügt nicht, wenn vorher MicroPython oder ein anderes Programm
darauf war: es überschreibt nur die eigenen Blöcke, alte Reste bleiben stehen.
Das Gerät bootet dann zwar Doom, behält bei einem Neustart aber das alte Bild
auf dem Display.

```bash
picotool erase -a                 # kann bei 16 MB über zwei Minuten dauern
picotool load ../doom-usb-mp.uf2  # das Spiel
picotool load DOOM.uf2            # die Level
picotool reboot
```

Danach einmal **wirklich stromlos machen** — USB abziehen, kurz warten,
wieder einstecken.

Später reicht Drag&Drop: BOOTSEL gedrückt halten, USB anstecken, Datei auf das
erscheinende Laufwerk kopieren.

## Zwischen Doom 1 und Doom 2 wechseln

Dieselbe Firmware spielt beides — welcher Modus gilt, erkennt sie zur Laufzeit
an der WAD, samt Super-Shotgun bei Doom 2. Beide WAD-Dateien liegen aber an
derselben Adresse, es passt also immer nur **eine** aufs Gerät. Zum Umschalten
genügt es, die andere zu laden; die Firmware bleibt liegen:

```bash
picotool load DOOM2.uf2
picotool reboot
```

## Wenn es klemmt

**Zu große WADs passen nicht.** Getestet ist das Werkzeug vor allem mit den
Original-WADs von Doom 1 und Doom 2. Andere können funktionieren, sind aber
nicht garantiert — in 16 MB Flash geht nicht alles.

**`picotool erase -a` bricht ab.** Dann zusätzlich den oberen Bereich löschen:

```bash
picotool erase -r 0x10600000 0x11000000
```

**Das Display bleibt schwarz.** Meist ist der Flash nicht vollständig gelöscht
worden — siehe oben. Ansonsten prüft `picotool info -a`, ob überhaupt
`rp2350-arm-s` gemeldet wird.

## Was sonst noch hier liegt

| Datei | Wofür |
|---|---|
| `bin/` | die fertigen Programme für Linux und Windows |
| `LIESMICH.md` | die ausführlichere Urfassung dieser Anleitung |
| `wad2uf2.py` | optionaler Python-Aufsatz mit Dateidialog — **nicht nötig** |
| `build-whd_gen.sh` | baut das Programm selbst, etwa für macOS |
| `whd_gen_compat.h` | wird nur beim Selberbauen gebraucht |
