# PBC CPC

Amstrad-CPC-6128-Emulator für den **PicoBoy Color Plus**, nur als Teil von
PBCemu (`../pbcemu/build.sh` → `dist/pbcemu.uf2`); eine Einzel-Firmware gibt
es nicht.

Der Kern (`core/`) ist für den PicoBoy geschrieben (GPL-3.0-or-later): Gate
Array (Farben, Modi 0–2, Interrupt alle 52 Zeilen), CRTC 6845 zeilenweise,
PPI 8255 mit Tastatur und Joystick, 128 KB RAM mit dem Banking des 6128,
oberes und unteres ROM, AY-3-8912, Kassette (`.cdt`, in Echtzeit mit
Motorsteuerung), Diskettencontroller µPD765 mit Laufwerk A (`.dsk`) und
Snapshots (`.sna`). Der Z80 ist **redcode/Z80** (`core/redcode`,
LGPL-3.0-or-later); jeder Befehl dauert ganze Mikrosekunden, wie es die
Wartezyklen des Gate Array erzwingen. AY und Bandlogik teilt er sich mit dem
Spectrum (`../zx`).

**ROMs:** Betriebssystem, BASIC 1.1 und AMSDOS des 6128 sind eingebaut
(`roms/`). Sie gehören Amstrad und Locomotive Software (AMSDOS auch Digital
Research). Amstrad hat die Weitergabe mit Emulatoren erlaubt, solange die
Copyright-Meldungen bleiben und niemand Geld dafür nimmt; für die Teile von
Locomotive bräuchte es nach Amstrads eigener Aussage auch deren Zustimmung
(`roms/LICENSE.roms`). Sie stehen nicht unter der Lizenz dieses Ordners.

**Stand:** am PC mit freien Spielen getestet: Nibbler DX und Transversion
2024 von Diskette (Autostart, Musik), Red Sea von Kassette (in Echtzeit,
`|TAPE` und `RUN"` von selbst). Auf dem Gerät noch nicht.

## Spiele

| Endung | Was | Start |
|---|---|---|
| `.dsk` | Diskette (Standard- und erweitertes Format) | tippt `RUN"` mit dem ersten `.BAS`/`.BIN` des Inhaltsverzeichnisses, bei CP/M-Systemdisketten `|CPM` |
| `.cdt` | Kassette (TZX-Format) | tippt `|TAPE`, `RUN"` und eine Taste für „Press PLAY“ |
| `.sna` | Snapshot (64 oder 128 KB) | sofort |

`.dsk` und `.sna` gibt es auch bei MSX bzw. Spectrum; im Starter stehen
solche Dateien bei beiden, der jeweils falsche Emulator sagt das.

Läuft der Kassettenmotor, rechnet die Emulation so schnell sie kann, ohne
Ton, und zeichnet nur jedes 16. Bild; unten steht „LOADING“. Findet sich
auf einer Diskette kein Programm, bleibt BASIC stehen.

## Tasten

| PicoBoy | CPC |
|---|---|
| Joystick | Joystick (oder Cursortasten, oder QAOP, siehe Menü) |
| B | Feuer (Joystick: Feuer 1; Cursor: COPY; QAOP: SPACE) |
| A | SPACE |
| Mitte + B | RETURN |
| Mitte + A | Menü |

**Menü:** Joystick-Art, Neustart, Band zurückspulen (sonst Reset) und jede
Taste einmal drücken (RETURN, SPACE, ESC, COPY, DEL, SHIFT, CTRL, TAB, CAPS
LOCK, F0–F9, 0–9, A–Z).

## Anzeige

280 Zeilen: 40 Zeilen Rand über dem normalen Bildschirm, seine 200, 40
darunter (gemessen ab dem VSYNC, wie ein Monitor). Die Zeilen sind 640
Mode-2-Pixel breit. Modi (im Starter): *Fit width* (640 → 240, jedes
Panel-Pixel der Mittelwert der Pixel darunter, Vorgabe) und *Crop 1:1* (die
mittleren 240 Mode-1-Pixel). Mode 2 (80 Zeichen) ist so nur grob lesbar.

## Was fehlt

* CRTC-Tricks innerhalb einer Zeile (Rupture, Rasterstrahl-Splits mitten in
  der Zeile); der CRTC rechnet zeilenweise, horizontal wird das Bild
  mittig gesetzt statt nach R2.
* Kein CPC Plus/GX4000, keine Erweiterungs-ROMs, kein Laufwerk B.
* Geschriebenes auf Diskette geht verloren (das Abbild liegt im Flash).
* Kopierschutze, die die Zeitabläufe des Controllers prüfen.
* Die Einstellungen im Menü merkt er sich nicht.

## Aufbau

| Datei | Inhalt |
|---|---|
| `core/cpc.c` | die Maschine: Speicher und Banking, Gate Array, CRTC, PPI, Tastatur, AY, Zeilen, Autostart-Tippen, `.sna` |
| `core/fdc.c` | µPD765 und `.dsk` (Standard und erweitert), Inhaltsverzeichnis für den Autostart |
| `core/tape.c` | `.cdt` (aus `../zx`), Signal Flanke für Flanke |
| `core/ay8910.c` | AY-3-8912 (aus `../msx`) |
| `core/roms.S` | bindet die ROMs aus `roms/` ein |
| `src/main.c` | Start, Spielschleife, Menü, Schnellvorlauf beim Laden |
| `src/cpc_video.c` | die 280 Zeilen über einen Ring an Kern 1, der sie auf 240 Pixel umrechnet |
| `src/cpc_input.c` | Tasten → Joystick und Tastatur |
| übrige `src/`, `fatfs/` | Plattform aus `../zx` |

## Lizenz

GPL-3.0-or-later (`LICENSE`); `core/redcode` LGPL-3.0-or-later; die ROMs in
`roms/` gehören Amstrad und Locomotive Software (`roms/LICENSE.roms`);
Plattformteile aus PicoBoyGB MIT (`LICENSE.picoboygb`).
