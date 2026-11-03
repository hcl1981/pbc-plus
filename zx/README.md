# PBC ZX

ZX-Spectrum-Emulator (48K und 128K) für den **PicoBoy Color Plus**, nur als
Teil von PBCemu (`../pbcemu/build.sh` → `dist/pbcemu.uf2`); eine
Einzel-Firmware gibt es nicht.

Der Kern (`core/`) ist für den PicoBoy geschrieben (GPL-3.0-or-later): ULA
(Bild zeilenweise, Rand pro Zeile, Tastatur, Beeper, EAR-Eingang), 48 KB
oder 128 KB RAM mit dem Paging des 128K, der AY-3-8912 des 128K, Kempston-
Joystick, Floating Bus, Band (`.tap`, `.tzx`) und Snapshots (`.z80`
Version 1–3, `.sna`). Dazu kommt der Z80 **redcode/Z80** (`core/redcode`,
LGPL-3.0-or-later, derselbe wie in MSX und Master System). Den AY teilt er
sich mit dem MSX (`core/ay8910.c`).

**ROMs:** Die originalen ROMs des 48K und des 128K sind eingebaut
(`roms/`). Sie gehören Amstrad, das ihre Weitergabe mit Emulatoren erlaubt
hat, solange die Copyright-Meldungen bleiben und niemand Geld dafür nimmt
(`roms/LICENSE.roms`). Sie stehen deshalb nicht unter der Lizenz dieses
Ordners.

**Stand:** am PC mit drei freien Spielen der Mojon Twins getestet (Tenebra
Macabre, Ninjajar! 128K, Horace Goes to the Tower als `.tzx`), dazu ein
selbst gebautes Band, das in Echtzeit lädt, und Snapshots (`.sna`, `.z80`
v1 und v3, 48K und 128K). Auf dem Gerät: Ninjajar! läuft in Echtzeit (1800
Bilder in 36,0 s, alle gezeichnet), das Echtzeit-Band lädt gut dreimal so
schnell wie ein echter Spectrum (9,6 s Band in 3,0 s), ein `.z80`-Snapshot
startet. Spiele mit eigenem Schnelllader (Turbo-Blöcke in `.tzx`) sind noch
nicht getestet.

## Spiele

| Endung | Was | Maschine |
|---|---|---|
| `.tap` | Band | 128K, startet über den Tape Loader |
| `.tzx` | Band, auch mit eigenem Schnelllader | 128K, wie `.tap` |
| `.z80` | Snapshot, Version 1–3 | wie im Snapshot (48K oder 128K) |
| `.sna` | Snapshot | 48K (49179 Bytes) oder 128K |

Bänder starten von selbst: beim 128K wählt der Emulator im Menü „Tape
Loader“, beim 48K tippt er `LOAD ""`. Standard-Blöcke lädt er sofort (die
Laderoutine LD-BYTES im ROM ist abgefangen); alles andere, etwa
Schnelllader, spielt das Band in Echtzeit ab. Dann läuft die Emulation so
schnell sie kann, ohne Ton, und zeichnet nur jedes 16. Bild; unten steht
„LOADING“. Läuft ein Spiel auf dem 128K nicht, startet es das Menü als 48K
neu.

## Tasten

| PicoBoy | Spectrum |
|---|---|
| Joystick | Joystick (Kempston, Sinclair, Cursor oder QAOP, siehe Menü) |
| B | Feuer |
| A | SPACE |
| Mitte + B | ENTER |
| Mitte + A | Menü |

**Menü:** Joystick-Art (Kempston an Port 1Fh, Sinclair = Tasten 6–0, Cursor
= 5–8 und 0, QAOP mit SPACE als Feuer), Neustart als 48K oder 128K, Band
zurückspulen (bei Snapshots: Reset) und jede Taste einmal drücken (ENTER,
SPACE, BREAK, DELETE, CAPS SHIFT, SYMBOL SHIFT, 0–9, A–Z). Die Wahl gilt bis
zum Verlassen des Spiels.

## Anzeige

Das Bild füllt die Höhe des Panels: 44 Zeilen Rand, die 192 des
Bildschirms, 44 Zeilen Rand, so sieht man die Ladestreifen. Modi (im
Starter): *Fit width* (256 → 240 Spalten, Vorgabe) und *Crop 1:1* (je 8
Spalten links und rechts weg). Die Statuszeile liegt im unteren Rand.

## Was fehlt

* **Speicher-Contention:** Der Z80 läuft so schnell wie ohne ULA-Zugriffe.
  Das merken nur zeitgenaue Effekte (Multicolor, Rand-Effekte mitten in der
  Zeile) und manche Schnelllader.
* `.tzx`-Blöcke 18h (CSW) und 19h (Generalized Data) werden übersprungen,
  Sprünge (23h) und Aufrufe (26h) nicht ausgeführt.
* Kein +2A/+3 (Diskette), kein Interface 1, kein Link-Spiel.
* Die Einstellungen im Menü (Joystick, Maschine) merkt er sich nicht.

## Aufbau

| Datei | Inhalt |
|---|---|
| `core/zx.c` | die Maschine: Speicher und Paging, ULA-Zeilen, Ports, Beeper, AY, LD-BYTES-Trap, Snapshots |
| `core/tape.c` | `.tap`/`.tzx`: Standard-Blöcke für den Trap, sonst das Signal Flanke für Flanke |
| `core/ay8910.c` | AY-3-8912 (aus `../msx`) |
| `core/roms.S` | bindet die drei ROMs aus `roms/` ein |
| `core/redcode` | Z80 (redcode, gepatcht wie in `../msx`) |
| `src/main.c` | Start, Spielschleife, Menü, Schnellvorlauf beim Laden |
| `src/zx_video.c` | die 280 Zeilen über einen Ring an Kern 1, der sie umrechnet und die Statuszeile einblendet |
| `src/zx_input.c` | Tasten → Joystick und Tastatur |
| übrige `src/`, `fatfs/` | Plattform aus `../msx` |

## Lizenz

GPL-3.0-or-later (`LICENSE`); `core/redcode` LGPL-3.0-or-later; die ROMs in
`roms/` gehören Amstrad (`roms/LICENSE.roms`); Plattformteile aus PicoBoyGB
MIT (`LICENSE.picoboygb`).
