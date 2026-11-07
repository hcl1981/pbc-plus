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
`|TAPE` und `RUN"` von selbst). Auf dem Gerät noch nicht. Link-Spiel: am PC
geprüft, dass zwei Läufe mit denselben Tasten Bild für Bild dasselbe RAM
haben; mit zwei Geräten noch nicht.

## Spiele

| Endung | Was | Start |
|---|---|---|
| `.dsk` | Diskette (Standard- und erweitertes Format) | tippt `RUN"` mit dem ersten `.BAS`/`.BIN` des Inhaltsverzeichnisses, bei CP/M-Systemdisketten `|CPM` |
| `.cdt` | Kassette (TZX-Format) | tippt `|TAPE`, `RUN"` und eine Taste für „Press PLAY“ |
| `.sna` | Snapshot (64 oder 128 KB) | sofort |

`.dsk` und `.sna` gibt es auch bei MSX bzw. Spectrum; der Starter schaut
in solche Dateien hinein und zeigt sie nur beim passenden System (CPC-Dateien
beginnen mit „MV - SNA“, „MV - CPC“ oder „EXTENDED“).

Läuft der Kassettenmotor, rechnet die Emulation so schnell sie kann, ohne
Ton, und zeichnet nur jedes 16. Bild; unten steht „LOADING“. Findet sich
auf einer Diskette kein Programm, bleibt BASIC stehen.

## Tasten

| PicoBoy | CPC |
|---|---|
| Joystick | Joystick (oder Cursortasten, oder QAOP, siehe Menü) |
| B | Feuer (Joystick: Feuer 1; Cursor: COPY; QAOP: SPACE) |
| A | SPACE |
| Mitte + B | RETURN (andere Taste: Einstellungen des Spiels im Starter oder Menü) |
| Mitte + A | Menü |

**Menü** (Mitte + A, wie bei C64, ZX und MSX aufgebaut): „Swap disk“ (bei
Diskettenspielen) oder „Rewind tape“ (bei Bändern), „Settings“ (Joystick-Art
und Taste auf Mitte + B; gelten sofort und werden beim Spiel gespeichert,
außer im Link-Spiel), „Press a key“ und ganz unten „Back to game“ (oder A).
Kein Neustart-Punkt mehr: RESET und das Spiel neu starten. Im Einzelnen:
**Diskette wechseln** („Swap disk“: alle CPC-Disketten des Laufwerks; die
gewählte wird in den Flash kopiert und eingelegt, ohne Neustart; im
Link-Spiel nur, wenn beide sie haben, beide kopieren Stück für Stück im
Gleichschritt), welche
Taste auf Mitte + B liegt („Center + B: …“, bis zum Ende des Spiels; im
Link-Spiel für beide; dauerhaft je Spiel in dessen Einstellungen im Starter,
Mitte in der Spieleliste, außer im Link-Spiel: dort beginnt es mit RETURN;
dort merkt sich der Starter auch die Joystick-Art je Spiel) und jede Taste einmal drücken (RETURN, SPACE, ESC,
COPY, DEL, SHIFT, CTRL, TAB, CAPS LOCK, F0–F9, 0–9, A–Z).

## Link-Spiel

Zwei PicoBoys am USB-C-Kabel spielen zu zweit (`../docs/LINKSPIEL.md`):
beide rechnen im Gleichschritt und tauschen jedes Bild ihre Tasten.
Spieler 2 ist **Joystick 1** des CPC, auf der Tastaturzeile 6 (Tasten 6 5
R T für die Richtungen, B = Feuer 1 = F, A = Feuer 2 = G); Mitte + B
(RETURN oder die Taste aus dem Menü) und Tasten aus dem Menü wirken bei beiden. Spieler 1 behält die
Joystick-Art aus dem Menü. Öffnet einer das Menü, hält es beide an; seine
Wahl geht über das Kabel, so dass beide sie im selben Bild ausführen. Die
Maschine hat weder Uhr noch Zufall, das Spiel liegt auf beiden im Flash und
Diskettenschreiben wird verworfen: beide laufen gleich. Zum Vergleich geht
das ganze RAM (128 KB) in die Prüfsumme, die ein Auseinanderlaufen meldet
(„LINK OUT OF SYNC“). Nur Spiele, die einen zweiten Joystick kennen, sind
zu zweit spielbar.

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

## Aufbau

| Datei | Inhalt |
|---|---|
| `core/cpc.c` | die Maschine: Speicher und Banking, Gate Array, CRTC, PPI, Tastatur, AY, Zeilen, Autostart-Tippen, `.sna` |
| `core/fdc.c` | µPD765 und `.dsk` (Standard und erweitert), Inhaltsverzeichnis für den Autostart |
| `core/tape.c` | `.cdt` (aus `../zx`), Signal Flanke für Flanke |
| `core/ay8910.c` | AY-3-8912 (aus `../msx`) |
| `core/roms.S` | bindet die ROMs aus `roms/` ein |
| `src/main.c` | Start, Spielschleife, Menü, Schnellvorlauf beim Laden, Link-Spiel |
| `src/cpc_video.c` | der Kern zeichnet die 280 Zeilen direkt in einen Ring, Kern 1 rechnet sie auf 240 Pixel um |
| `src/cpc_input.c` | Tasten → Joystick und Tastatur, im Link-Spiel Spieler 2 auf Joystick 1 |
| übrige `src/`, `fatfs/` | Plattform aus `../zx` |

Für die Geschwindigkeit laufen Betriebssystem, BASIC und AMSDOS aus
Kopien im RAM (48 KB) statt aus dem Flash, ebenso die Ein- und Ausgabe
(IN/OUT) samt AY und die Tabellen des AY. Ändert sich eine Farbe, entsteht
die Tabelle Byte → acht Pixel aus 16 fertigen Wörtern. *Fit width* mittelt
mit gepackten Farben (eine Tabelle, eine Multiplikation je Pixel; dasselbe
Ergebnis, für alle Farbkombinationen geprüft).

## Lizenz

GPL-3.0-or-later (`LICENSE`); `core/redcode` LGPL-3.0-or-later; die ROMs in
`roms/` gehören Amstrad und Locomotive Software (`roms/LICENSE.roms`);
Plattformteile aus PicoBoyGB MIT (`LICENSE.picoboygb`).
