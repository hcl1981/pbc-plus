# PBCemu — alle Emulatoren in einer Firmware

Ein Starter mit Gerätewahl und Spieleliste vor allen Emulatoren dieses Repos:
Commodore 64, NES, Master System, PC Engine, Mega Drive, Game Gear und
Game Boy (Color). Fertig als **`dist/pbcemu.uf2`**, eine Datei für alles.
Der alte PicoBoyGB-Emulator ist nicht dabei; Game-Boy-Spiele laufen im
Game-Boy-Color-Emulator.

**Stand:** baut; die Oberfläche ist auf dem PC in einer Vorschau geprüft
(gleicher Zeichencode, Bilder des Bildpuffers). Auf dem Gerät noch nicht
getestet, auch nicht der Sprung vom Starter in die Emulatoren.

## Bedienung

1. `dist/pbcemu.uf2` flashen (BOOTSEL halten, USB anstecken, Datei auf
   `RPI-RP2` kopieren). Das USB-Laufwerk liegt im Verbund weiter hinten im
   Flash als bei den Einzel-Firmwares (siehe unten), damit Mega-Drive-Spiele
   bis 5 MB passen. **Beim ersten Start wird es deshalb neu formatiert**; die
   Spiele danach wieder draufkopieren. Dasselbe beim Zurückwechseln auf eine
   Einzel-Firmware.
2. **Geräte:** Joystick links/rechts blättert durch die Systeme, **A** öffnet
   die Spieleliste. Unter jedem Gerät steht, wie viele Spiele es gibt.
   **Mitte** öffnet die Einstellungen des Geräts (siehe unten). Die letzte
   Seite, **Add Games**, schaltet den USB-Laufwerk-Modus ein.
3. **Spiele:** hoch/runter wählt, links/rechts blättert seitenweise, **A**
   startet, **B** geht zurück, **Mitte** öffnet die Optionen. Beim C64 sind
   das zusätzlich Joystick-Port und Programm auf der Diskette, je Spiel.
4. Vor dem Start zeigt der Starter die Tastenbelegung (zwei Sekunden, A
   überspringt). Zurück zur Liste: **RESET**.

### Einstellungen (Mitte)

| Zeile | Wirkung |
|---|---|
| Sound / Volume | Ton und Lautstärke, für alle Systeme außer Mega Drive (der Emulator hat keinen Ton) |
| Display | Anzeigemodus des Systems (gemerkt je System) |
| Show FPS | blendet im Spiel Bilder/s, Tempo in % des Originals und Auslastung von Kern 0 ein, z. B. `60fps 100% CPU71%` |
| Joystick / Program | nur bei C64-Spielen: Port und Programm auf der Diskette |

Anzeigemodi (der erste ist jeweils die Vorgabe):

| System | Modi |
|---|---|
| Commodore 64 | Standard (320×240-Ausschnitt verkleinert), Full border (ganzer Rand), No border (nur 320×200) |
| NES | Crop 1:1 (240×240), Fit width (240×225), No overscan (240×224) |
| Master System | Crop 1:1, Fit width (240×180), Zoom 1.25x |
| PC Engine | Standard, Crop 1:1 |
| Mega Drive | Fit width, Crop 1:1 |
| Game Gear | Scale 1.5x (240×216), Original (160×144) |
| Game Boy Color | Scale 1.5x (240×216), Original (160×144) |

Alle Modi behalten das Seitenverhältnis (keine verzerrten Vollbild-Modi).
Einstellen lässt sich das nur hier im Starter, nicht während eines Spiels.

USB-Laufwerk-Modus auch: **A** oder **B** beim Einschalten halten. Dateien
nach Endung:

| System | Endungen |
|---|---|
| Commodore 64 | `.d64` `.prg` (eigene ROMs: `kernal.rom`, `basic.rom`, siehe `../c64`) |
| NES | `.nes` |
| Master System | `.sms` |
| PC Engine | `.pce` |
| Mega Drive | `.md` `.gen` `.smd` `.bin` (bis 5 MB, mit Super Street Fighter II) |
| Game Gear | `.gg` |
| Game Boy Color | `.gbc` `.gb` |

Startet das Gerät von selbst neu (Spannungseinbruch o. Ä.), sagt der Starter
das beim nächsten Start mit dem Grund, statt kommentarlos in den USB-Modus zu
gehen.

## Wie es funktioniert

Der Starter liegt am Anfang des Flash, die Emulatoren dahinter, jeder an seine
eigene Adresse gelinkt (`../common/pbc_multi.cmake`). Wird ein Spiel gewählt,
schreibt der Starter einen Übergabe-Datensatz (Spiel, Ton, Lautstärke,
Anzeigemodus, FPS-Anzeige,
`../common/pbc_launch.h`) in den Flash, legt die Adresse des Emulators in die
Scratch-Register des Watchdogs und startet den Chip neu. Gleich nach dem
Neustart, bevor er irgendetwas anderes anfasst, springt er hinein: Interrupts
aus und gelöscht, Vektortabelle, Stack und Stack-Grenze des Emulators setzen,
Reset-Handler aufrufen. Dessen Laufzeitstart setzt die Peripherie zurück und
stellt die Takte neu ein, als wäre er allein auf dem Chip. Jeder spätere
Neustart (RESET, „zurück zur Liste“, Ausschalten) landet wieder im Starter.

Im Verbund übernehmen die Emulatoren das Spiel aus dem Datensatz und
überspringen ihre eigene Liste, den USB-Modus und die Tastenübersicht. Sonst
sind sie unverändert; ohne `PBC_APP_OFFSET` gebaut, sind sie weiterhin
eigenständige Firmwares (`dist/nes.uf2` usw.).

### Flash-Aufteilung

| Adresse | Inhalt |
|---|---|
| `0x000000` | Starter (256 KB vorgesehen, ca. 180 KB belegt) |
| `0x040000` … | Mega Drive, Master System/Game Gear, NES, PC Engine, Game Boy Color, C64 — dicht gepackt, auf 4 KB gerundet (zusammen ca. 1,4 MB) |
| `0x200000` | ROM des laufenden Spiels (5 MB) |
| `0x700000` | FatFs = USB-Laufwerk `PBC-ROMS`, ca. 9 MB (Einzel-Firmwares: ab `0x600000`) |
| `0xFF0000` | gehört den Einzel-Firmwares (welches Spiel in *ihrem* ROM-Bereich liegt); der Starter löscht es, weil dort jetzt Emulatoren liegen |
| `0xFF1000`–`0xFF5000` | Einstellungen der Emulatoren (C64: Port und Programm je Spiel) |
| `0xFF6000` | Übergabe-Datensatz |
| `0xFF7000` | Einstellungen des Starters (Ton, Lautstärke, Anzeigemodus je System, FPS-Anzeige) |
| `0xFF8000` | welches Spiel im ROM-Bereich des Verbunds liegt |

## Oberfläche

Der Starter zeichnet in einen Bildpuffer im RAM (zweimal 240×280 Pixel) und
schickt ihn per DMA ans Display, während schon das nächste Bild entsteht. So
gibt es weiche Kanten, Überblendungen, gleitendes Blättern und weiches
Scrollen. Ein Bild braucht etwa 30 ms zum Display; danach richten sich alle
Animationen.

* **Gerätebilder** stammen aus einem mit Google Gemini erzeugten Sprite-Sheet
  (`art/sheet.jpeg`, Prompt siehe unten). `tools/import_icons.py` stellt die
  acht Zellen vor dem Magenta frei, schneidet sie zu, setzt sie auf eine
  gemeinsame Grundlinie mit einheitlichem Schatten und legt sie als
  `art/*.png` ab. Fehlt ein Bild dort, nimmt `make_assets.py` die eigene
  Zeichnung aus `tools/devices.py` (gleiche Perspektive, keine Logos).
* **Schrift:** Lato (SIL Open Font License, `LICENSE.lato`) in vier Größen,
  geglättet mit 16 Stufen, Codepage 437 wie die Dateinamen aus FatFs (Umlaute).
* `tools/make_assets.py` macht aus beidem `src/assets.c` (Bilder auf ≤ 255
  Farben mit Alpha, lauflängenkodiert; zusammen ca. 90 KB). Die erzeugten
  Dateien liegen im Repo; zum Bauen der Firmware braucht man Pillow und Lato
  nicht.

### Prompt für die Gerätebilder

So entstand `art/sheet.jpeg` (Google Gemini). Neu erzeugen, dann
`python3 tools/import_icons.py art/sheet.jpeg art` und `make_assets.py`:

```
Create a single image: a sprite sheet of 8 product illustrations arranged in a
4 x 2 grid, each cell exactly the same size (landscape, 25:16 aspect ratio),
on a solid flat magenta background (#FF00FF), with generous empty space
between the cells. No text, no labels, no logos, no brand names anywhere.
Style (identical for all 8): modern flat vector illustration with subtle
shading, like a premium app icon set. All devices seen from the same angle:
front view, slightly from above (about 20 degrees). Light from the upper
left, one soft blurred floor shadow under each object, no outlines.
Cells: 1 NES front-loader with controller, 2 Sega Master System with pad,
3 Sega Mega Drive (model 1) with 3-button pad, 4 Sega Game Gear,
5 NEC PC Engine (white) with pad, 6 Game Boy Color in grape purple,
7 Commodore 64 breadbox with joystick, 8 blue USB stick with a white arrow.
```

## Bauen

```bash
./build.sh            # alle Emulatoren für den Verbund + Starter -> ../dist/pbcemu.uf2
./build.sh clean
python3 tools/make_assets.py .     # nach Änderungen an Bildern oder Schrift
```

`build.sh` baut jeden Emulator in seinen Ordner `build-multi/`, gelinkt an die
nächste freie Adresse, schreibt die Adressen nach `build/gen/pbc_apps.h`, baut
den Starter damit und verpackt alles mit `tools/pack_uf2.py`. Reicht der
Platz bis 2 MB nicht, bricht es mit einer Meldung ab.

## Aufbau

```
src/main.c        Start, Sprung in den Emulator, Übergabe, USB-Modus
src/ui.c          Gerätekarussell, Spieleliste, Optionen, Tastenkarte, Meldungen
src/gfx.c         Bildpuffer, Formen mit Kantenglättung, Schrift, Bilder
src/devices.c     die Systeme: Name, Endungen, Emulator, Farbe, Bild, Tasten
src/games.c       Spiele auf dem Laufwerk zählen und auflisten
src/prefs.c       Ton und Lautstärke (0xFF7000)
src/c64/          Port/Programm je C64-Spiel, Disk-Verzeichnis (aus ../c64)
src/display.c, input.c, flash_fs.*, usb_*.c, fatfs/   wie in den Emulatoren
art/              Gerätebilder (sheet.jpeg -> *.png)
tools/            import_icons.py, devices.py, make_assets.py, pack_uf2.py
```

## Lizenzen

* Starter: **GPL-3.0-or-later**, siehe `LICENSE` (enthält aus `../c64`
  stammenden Code unter GPL-2.0-or-later).
* `src/flash_fs.*`, `src/usb_msc.c`, `src/usb_descriptors.c`,
  `src/tusb_config.h`, `src/font8x8.h`, `fatfs/diskio.c`: aus PicoBoyGB,
  **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
* Schrift in `src/assets.c`: Lato, **SIL Open Font License 1.1**, `LICENSE.lato`.
* `dist/pbcemu.uf2` enthält außerdem alle Emulatoren, jeden unter seiner
  eigenen Lizenz (Mega Drive AGPL-3.0, C64 GPL-2.0+, die übrigen GPL-3.0);
  die Quellen liegen in den jeweiligen Ordnern.
