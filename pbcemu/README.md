# PBCemu — alle Emulatoren in einer Firmware

Ein Starter mit Gerätewahl und Spieleliste vor allen Emulatoren dieses Repos:
Atari 2600, ZX Spectrum, ColecoVision, Commodore 64, NES, SG-1000, MSX,
Amstrad CPC, Master System, Atari 7800, PC Engine, Mega Drive, Game Gear und
Game Boy (Color). Fertig als **`dist/pbcemu.uf2`**,
eine Datei für alles. Zwei Geräte am USB-C-Kabel spielen Atari 2600, C64,
NES, SG-1000, MSX, ZX Spectrum, Amstrad CPC, Master System, PC Engine und Mega
Drive zu zweit
([`../docs/LINKSPIEL.md`](../docs/LINKSPIEL.md)); im Karussell tragen diese
Konsolen die goldene Plakette **LINK**.
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
2. **Geräte:** Joystick links/rechts blättert durch die Systeme, **B** öffnet
   die Spieleliste. Unter jedem Gerät steht, wie viele Spiele es gibt.
   **Mitte** öffnet die Einstellungen des Geräts (siehe unten). Die letzte
   Seite, **Add Games**, schaltet den USB-Laufwerk-Modus ein. Oben in der
   Mitte steht die Seite (`1 / 16`), oben rechts der Ladestand der
   Batterien in vier Balken (grün, ab zwei Balken gelb, dann rot), bei
   Betrieb über das Kabel (Schalter auf USB) ein Blitz mit „USB“.
3. **Favoriten:** die erste Seite, mit dem goldenen Stern; dort beginnt der
   Starter immer. **B** öffnet die Favoriten aller Systeme in einer Liste,
   alphabetisch, rechts in jeder Zeile das System. Ein Spiel wird Favorit
   über **Mitte** in seiner Spieleliste und den Schalter **Favorite**; in
   den Listen trägt es dann einen kleinen Stern. Bis zu 40 Favoriten;
   gelöschte oder umbenannte Dateien fallen aus der Liste.
4. **Spiele:** hoch/runter wählt, links/rechts blättert seitenweise, **B**
   startet, **A** geht zurück, **Mitte** öffnet die Optionen. Beim C64 sind
   das zusätzlich Joystick-Port, Programm auf der Diskette und die Taste auf
   Mitte + B, je Spiel.
5. **Energiesparen:** 30 Sekunden ohne Tastendruck dunkelt der Starter das
   Display ab, nach weiteren zwei Minuten geht die Beleuchtung aus, beides
   weich in einer Sekunde. Solange es dunkel ist, zeichnet der Starter
   nichts, läuft mit 60 statt 150 MHz und schaltet den Ton ab; alle 2,5 s
   laufen die Status-LEDs rot, gelb, grün durch, jede in 0,15 s ein- und in
   0,35 s ausgeblendet, so dass sie ineinander übergehen. Ein Link-Partner, der ein Spiel startet, weckt ihn auch. Eine Taste macht
   es in 150 ms wieder hell und bewirkt sonst nichts. In den Spielen nicht.
6. Vor dem Start zeigt der Starter die Tastenbelegung (zwei Sekunden, B
   überspringt). Zurück zur Liste: **RESET**.

### Einstellungen (Mitte)

| Zeile | Wirkung |
|---|---|
| Favorite | nur bei Spielen: in die Favoriten aufnehmen oder entfernen |
| Sound / Volume | Ton und Lautstärke, für alle Systeme |
| Brightness | Helligkeit in acht Stufen, für Starter und alle Systeme (Vorgabe Stufe 6 = die bisherige); wirkt sofort. Die Emulatoren bekommen sie im Übergabe-Datensatz |
| Display | Anzeigemodus des Systems (gemerkt je System) |
| Show FPS | blendet im Spiel Bilder/s, Tempo in % des Originals und Auslastung von Kern 0 ein, z. B. `60fps 100% CPU71%` |
| Joystick / Program | nur bei C64-Spielen: Port und Programm auf der Diskette |
| Mitte + B | nur bei C64-Spielen: welche Taste dort liegt (Vorgabe SPACE). Mitte + A öffnet im Spiel die Diskettenauswahl, siehe `../c64/README.md` |
| True drive | nur bei C64-Spielen: echte 1541-Emulation für Spiele mit eigenem Schnelllader; braucht `1541.rom` auf dem Laufwerk, siehe `../c64/README.md` |

Anzeigemodi (der erste ist jeweils die Vorgabe):

| System | Modi |
|---|---|
| Atari 2600 | Scale 1.5x (160 → 240 Pixel breit) |
| ColecoVision | Crop 1:1, Fit width, Zoom 1.25x (wie SG-1000) |
| Atari 7800 | Fit width (320 → 240) |
| ZX Spectrum | Fit width (256 → 240, mit Rand oben und unten), Crop 1:1 |
| Commodore 64 | Standard (320×240-Ausschnitt verkleinert), Full border (ganzer Rand), No border (nur 320×200) |
| NES | Crop 1:1 (240×240), Fit width (240×225), No overscan (240×224) |
| SG-1000 | Crop 1:1, Fit width, Zoom 1.25x |
| MSX | Fit width (256 → 240), Crop 1:1 |
| Amstrad CPC | Fit width (640 Mode-2-Pixel → 240), Crop 1:1 (die mittleren 240 Mode-1-Pixel) |
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
| Atari 2600 | `.a26` |
| ColecoVision | `.col`, auch `.rom` und `.bin` (erkannt an AA 55 / 55 AA am Anfang); BIOS `coleco.rom` ins Hauptverzeichnis, sonst ein eigenes Mini-BIOS, siehe `../sms` |
| Atari 7800 | `.a78` (mit oder ohne Kopf), siehe `../a2600` |
| ZX Spectrum | `.tap` `.tzx` (Bänder, starten von selbst) `.z80` `.sna` (Snapshots); die ROMs sind eingebaut, siehe `../zx` |
| Commodore 64 | `.d64` `.prg` `.crt` (eigene ROMs: `kernal.rom`, `basic.rom`, siehe `../c64`) |
| NES | `.nes` |
| SG-1000 | `.sg` |
| MSX | `.rom` `.mx1` (MSX1-Module; C-BIOS ist eingebaut), `.mx2` (MSX2-Module) und `.dsk` (MSX2-Disketten); eingebaut ist C-BIOS (Module, selbst startende Disketten), für BASIC und MSX-DOS `msx2.rom`, `msx2ext.rom` und `disk.rom` ins Hauptverzeichnis, siehe `../msx` |
| Amstrad CPC | `.dsk` (Disketten) `.cdt` (Kassetten) `.sna` (Snapshots); starten von selbst, die ROMs sind eingebaut, siehe `../cpc` |
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
überspringen ihre eigene Liste, den USB-Modus und die Tastenübersicht.
Ladebildschirm und Meldungen zeichnen sie mit den Schriften des Starters
(`../common/pbc_loading.c`): Der Starter legt bei jedem Start die Adresse
einer kleinen Tabelle (`pbc_shared_t`) in Scratch-Register 2, statt dass
jeder Emulator rund 25 KB Schriften mitbringt. Sonst
sind sie unverändert; ohne `PBC_APP_OFFSET` gebaut, sind sie weiterhin
eigenständige Firmwares (`dist/nes.uf2` usw.).

### Flash-Aufteilung

| Adresse | Inhalt |
|---|---|
| `0x000000` | Starter (268 KB vorgesehen, ca. 261 KB belegt: Schriften und Gerätebilder) |
| `0x043000` … | Mega Drive, Master System/Game Gear/SG-1000, NES, PC Engine, Game Boy Color, C64, MSX, Atari 2600, ZX Spectrum, Amstrad CPC — dicht gepackt, auf 4 KB gerundet (zusammen ca. 1,7 MB, bis `0x200000`) |
| `0x200000` | ROM des laufenden Spiels (5 MB) |
| `0x700000` | FatFs = USB-Laufwerk `PBC-ROMS`, ca. 9 MB (Einzel-Firmwares: ab `0x600000`) |
| `0xFF0000` | gehört den Einzel-Firmwares (welches Spiel in *ihrem* ROM-Bereich liegt); der Starter löscht es, weil dort jetzt Emulatoren liegen |
| `0xFF1000`–`0xFF5000` | Einstellungen der Emulatoren (C64: Port und Programm je Spiel) |
| `0xFF6000` | Übergabe-Datensatz |
| `0xFF7000` | Einstellungen des Starters (Ton, Lautstärke, Helligkeit, Anzeigemodus je System, FPS-Anzeige) |
| `0xFF8000` | welches Spiel im ROM-Bereich des Verbunds liegt |
| `0xFF9000` | Favoriten (bis 40: Dateiname und System, das System als Prüfsumme seines Namens, damit neue Systeme nichts verschieben) |

## Oberfläche

Der Starter zeichnet in einen Bildpuffer im RAM (zweimal 240×280 Pixel) und
schickt ihn per DMA ans Display, während schon das nächste Bild entsteht. So
gibt es weiche Kanten, Überblendungen, gleitendes Blättern und weiches
Scrollen. Ein Bild braucht etwa 30 ms zum Display; danach richten sich alle
Animationen. Beim Einschalten bleibt die Beleuchtung aus, bis das erste Bild
ganz auf dem Display ist, und blendet dann in 150 ms auf (`gfx_present`).
Nach dem Einschalten und nach RES (nicht nach dem USB-Laufwerk-Modus) läuft
eine Startanimation von 1,5 s: „PBC“ und „emu“ gleiten von beiden Seiten herein,
ein Lichtschimmer läuft darüber, dann steigt der Schriftzug auf und das
Karussell erscheint. Jede Taste überspringt sie.
Das gewählte Gerät schwebt: Es hebt und senkt sich alle 3,2 s um 3 Pixel (in
Viertelpixeln, die nächste Zeile teilweise gezeichnet), sein Schatten bleibt
am Boden und wird blasser und schmaler. Dafür nimmt `make_assets.py` den
Kontaktschatten aus den Bildern (schwarze, höchstens halb deckende Pixel;
er hätte beim Anheben eine Lücke in Form des Geräts gezeigt) und merkt sich
Mitte, Unterkante und Breite des Geräts; der Starter zeichnet dort einen
weichen runden Schatten (`gfx_glow` in Schwarz).

**Ton** (`src/sound.c`, nur mit Sound an, in der eingestellten Lautstärke):
ein Klicken beim Ändern von Sound und Volume (so hört man die Lautstärke), ein glockenartiges „Pling“ (E6, H6, E7, jeweils abklingend), wenn
der Schimmer der Startanimation losläuft. Jeder Klang wird in einen Puffer
gerechnet und per DMA einmal abgespielt. Zwischen den Klängen liegt der
Ausgang auf 0 (in der Mitte gehalten, pfiff das Rauschen der Versorgung im
Lautsprecher); jeder Klang gleitet in 10 ms zur Mitte und danach zurück,
damit nichts knackt. Der Schriftzug sind drei
Bilder aus `make_assets.py` (Lato Black, 50 px).

* **Gerätebilder** stammen aus einem mit Google Gemini erzeugten Sprite-Sheet
  (`art/sheet.jpeg`, Prompt siehe unten). `tools/import_icons.py` stellt die
  acht Zellen vor dem Magenta frei, schneidet sie zu, setzt sie auf eine
  gemeinsame Grundlinie mit einheitlichem Schatten und legt sie als
  `art/*.png` ab. Das MSX (ein MSX2) kommt aus `art/sheet_msx.jpeg`, der
  Atari 2600 aus `art/sheet_extra.jpeg`, das SG-1000 aus `art/sg1000.jpeg`,
  der ZX Spectrum aus `art/zx.jpeg`, der Amstrad CPC aus `art/cpc.jpeg`
  (Aufrufe in `tools/import_icons.py`). Fehlt ein Bild dort, nimmt `make_assets.py` die eigene
  Zeichnung aus `tools/devices.py` (gleiche Perspektive, keine Logos); so
  entsteht auch der Stern der Favoriten.
* **Schrift:** Lato (SIL Open Font License, `LICENSE.lato`) in vier Größen,
  geglättet mit 16 Stufen, Codepage 437 wie die Dateinamen aus FatFs (Umlaute).
  Die große Titelschrift zeigt nur Systemnamen und hat deshalb nur ASCII
  (10 statt 26 KB).
* **Batterie:** GP29 (ADC 3) hinter einem Teiler 1:3 misst die Spannung der
  zwei AAA-Zellen, auf USB rund 5 V (`src/battery.c`, viermal pro Sekunde,
  geglättet). Über 3,8 V gilt als USB; die Balken stehen bei 2,85 / 2,65 /
  2,45 / 2,25 V, mit 40 mV Hysterese. Akkus (NiMH, 1,2 V je Zelle) zeigen
  deshalb auch voll nur zwei bis drei Balken.
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
src/ui.c          Karussell, Spieleliste, Favoriten, Optionen, Tastenkarte, Meldungen
src/gfx.c         Bildpuffer, Formen mit Kantenglättung, Schrift, Bilder
src/devices.c     die Systeme: Name, Endungen, Emulator, Farbe, Bild, Tasten
src/games.c       Spiele auf dem Laufwerk zählen und auflisten
src/prefs.c       Ton und Lautstärke (0xFF7000)
src/favs.c        Favoriten (0xFF9000)
src/battery.c     Batteriespannung, USB-Betrieb
src/sound.c       Klicken und „Pling“ des Starters
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
