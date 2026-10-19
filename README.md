# PBC+ — Spiele und Firmware für den PicoBoy Color Plus

Portierungen, Emulatoren und Firmware für den **PicoBoy Color Plus**:
RP2350A, 512 KB RAM, 16 MB Flash, ST7789 240×280, Joystick + A/B/Mitte, Piezo.

**Nur spielen?** Die fertigen Abbilder liegen in [`dist/`](dist/). BOOTSEL
gedrückt halten, USB anstecken, die gewünschte `.uf2` auf das erscheinende
Laufwerk kopieren. Es passt immer nur **eine** Firmware aufs Gerät.

**Selbst bauen?** Siehe [`docs/BAUEN.md`](docs/BAUEN.md).

```bash
./build_all.sh              # alles bauen
cd spout && ./build.sh      # oder ein einzelnes Projekt
```

## Die Projekte

| Ordner | Was es ist | Fertiges Abbild |
|---|---|---|
| [`noiz2sa`](noiz2sa/) | Abstrakter Kugelhagel-Schütze von Kenta Cho. Ausweichen, Sterne sammeln, Wertung steigern. | `noiz2sa.uf2` |
| [`spout`](spout/) | Höhlenflug. Der Sandstrahl treibt an **und** zerstört den Fels. | `spout.uf2` |
| [`stransball2`](stransball2/) | Super Transball 2. Kugel mit dem Traktorstrahl greifen und aus dem Level fliegen. | `stransball2.uf2` |
| [`micropolis`](micropolis/) | Micropolis — der quelloffene Kern von SimCity 1. | `micropolis.uf2` |
| [`tyrian`](tyrian/) | OpenTyrian. Firmware plus 8,9 MB Spieldaten in einer Datei. | `tyrian-komplett.uf2` |
| [`picoboygb`](picoboygb/) | Game-Boy-Emulator. ROMs kommen per USB-Stick-Modus aufs Gerät. | `picoboygb.uf2` |
| [`pbcemu`](pbcemu/) | **PBCemu**: alle Emulatoren unten (C64, NES, Master System, PC Engine, Mega Drive, Game Gear, Game Boy Color) in einer Firmware, mit Gerätewahl samt Gerätebildern und Spieleliste. | `pbcemu.uf2` |
| [`sms`](sms/) | Sega Master System und Game Gear (SMS Plus aus pico-smsplus). Game Gear 1,5-fach vergrößert. | `sms.uf2` |
| [`pce`](pce/) | PC Engine / TurboGrafx-16, HuCards (pce-go aus retro-go über pico-pcePlus). | `pce.uf2` |
| [`gbc`](gbc/) | Game Boy Color und klassischer Game Boy (Peanut-GB mit Farberweiterung), 1,5-fach vergrößert, Spielstände als `.sav`. | `gbc.uf2` |
| [`c64`](c64/) | Commodore-64-Emulator (Frodo4 aus frank-c64). Disk-Images per USB-Stick-Modus, startet mit `LOAD"*",8,1`, Joystick-Port pro Spiel. | `c64.uf2` |
| [`nes`](nes/) | NES-Emulator (QuickNES-Kern aus frank-nes), 240×240 Pixel 1:1, mit Ton. ROMs per USB-Stick-Modus. | `nes.uf2` |
| [`megadrive`](megadrive/) | Sega-Genesis-/Mega-Drive-Emulator (Kern aus frank-genesis), ROM läuft direkt aus dem Flash, bis 5 MB inkl. Super Street Fighter II. ROMs per USB-Stick-Modus, mit Ton. | `megadrive.uf2` |
| [`doom`](doom/) | Doom mit Mehrspieler über ein USB-C-Kabel. | `doom-usb-mp.uf2` |
| [`jumpnbump`](jumpnbump/) | Jump 'n Bump für zwei gekoppelte Geräte. | `jumpnbump.uf2` |
| [`i2collection`](i2collection/) | Neun Zweispieler-Spiele über USB-C: Tron, Pong, Duell, Artillerie, Bomber, Schach, Vier gewinnt, Käsekästchen. | `i2collection.uf2` |
| [`micropython`](micropython/) | MicroPython mit eigenem Board `PBC_PLUS` und Display-Modul. | `micropython-PBC_PLUS.uf2` |
| [`picoboygb-arduino`](picoboygb-arduino/) | Derselbe Emulator als ursprünglicher Arduino-Sketch, zum Nachschlagen. | — |

Jeder Ordner hat eine eigene `README.md` mit Bedienung, Flash-Aufteilung und
Portierungsnotizen. Bei `spout`, `noiz2sa` und `stransball2` steht in
`doc/PORT.md` zusätzlich ausführlich, was beim Portieren anzupassen war und
warum.

## Zwei Geräte koppeln

Vier Projekte spielen zu zweit über ein **USB-C-Kabel** zwischen zwei
PicoBoys: `doom`, `jumpnbump`, `i2collection` und die Zweispieler-Betriebsart
von `tyrian`.

Verkabelung überall gleich: **D+, D− und GND verbinden, VBUS nicht** — beide
Geräte versorgen sich selbst. Die Rolle (Host oder Gast) wird im Menü gewählt,
nicht durch das Kabel.

## Was nicht beiliegt

**Doom braucht eine WAD.** Die Firmware allein spielt nichts — die Level kommen
aus einer WAD-Datei, die als zweite UF2 an eine eigene Flash-Adresse geht.
**Freedoom liegt bei**, damit Doom sofort spielbar ist: freie Ersatzdaten unter
BSD-Lizenz, fertig als `dist/doom/freedoom1.uf2`. Zwei Dateien aufs Gerät
ziehen und los. Für das echte Doom brauchst du weiterhin eine eigene WAD —
[`dist/doom/`](dist/doom/) enthält alles dafür an einer Stelle: die Firmware,
`whd_gen` für Linux und Windows und eine Schritt-für-Schritt-Anleitung
([`ANLEITUNG.md`](dist/doom/ANLEITUNG.md)) vom Umwandeln bis zum Aufspielen.
Eigene WAD hineingeben, flashfertige UF2 kommt heraus; unter Windows reicht ein
Doppelklick. Die Werkzeuge liegen zusätzlich bei ihrem Projekt in
[`doom/wad2uf2/`](doom/wad2uf2/). Die WADs selbst sind kommerzielle
Daten von id Software und dürfen nicht weitergegeben werden.

**PicoBoyGB braucht ROMs.** Nach dem Flashen **Mitte** halten und **RESET**
drücken: das Gerät meldet sich als USB-Stick `PICOBOYGB`. Dort `.gb`-Dateien
ablegen, auswerfen, RESET ohne Mitte — das Menü listet sie dann auf.

**Mega Drive braucht ROMs.** Beim Einschalten **A** oder **B** gedrückt
halten: das Gerät meldet sich als USB-Stick `PBC-ROMS`. Dort `.md`-, `.bin`-,
`.gen`- oder `.smd`-Dateien ablegen und auswerfen — der PicoBoy startet von
selbst neu und zeigt die Spieleliste.

**NES braucht ROMs.** Genauso: beim Einschalten **A** oder **B** halten,
`.nes`-Dateien auf das Laufwerk kopieren, auswerfen. NES und Mega Drive teilen
sich das Laufwerk; jede Firmware listet nur ihre eigenen ROMs.

**PBCemu (`pbcemu.uf2`)** vereint diese Emulatoren; ROMs kommen genauso auf
das Laufwerk `PBC-ROMS`, zusätzlich über die Seite „Add Games“ im Starter.

**Master System/Game Gear, PC Engine und Game Boy Color** bekommen ihre ROMs
(`.sms` `.gg`, `.pce`, `.gb` `.gbc`) genauso über den USB-Stick-Modus. Alle
neuen Emulatoren teilen sich das Laufwerk.

**C64 braucht Disk-Images.** Ebenso über den USB-Stick-Modus, als `.d64`.
Ohne Tastatur tippt der Emulator `LOAD"*",8,1` (und bei Bedarf `RUN`) selbst.

**Tyrian-Daten liegen bei.** Die sind Freeware und stecken schon in
`tyrian-komplett.uf2`.

## Lizenzen

**Es gibt keine einheitliche Lizenz für dieses Repository.** Jede Portierung
erbt die Lizenz des Projekts, auf dem sie aufsetzt — gemischt BSD, MIT,
GPL-2.0, GPL-3.0 und AGPL-3.0. Maßgeblich ist immer die Lizenzdatei im jeweiligen
Ordner.

Die Übersicht, welche wo gilt, steht in [`LICENSE`](LICENSE); die
ausführlichere Fassung mit der Herkunft jeder Portierung in
[`docs/LIZENZEN.md`](docs/LIZENZEN.md).

## Hardware-Referenz

[`docs/HARDWARE.md`](docs/HARDWARE.md) sammelt, was beim Portieren auf dieses
Gerät wirklich zählt: Pinbelegung, Display-Init samt MADCTL und Y-Versatz,
Tonausgabe über PWM, Flash-Aufteilung, das XIP-Kopierfrei-Muster — und die
vier verschiedenen Verbindungsarten, die man leicht verwechselt. Die Zahlen
stammen aus laufenden Projekten, nicht aus Datenblättern; wo etwas nur
gerechnet und nie auf Hardware geprüft wurde, steht es dabei.

## Aufbau des Repos

```
.
├── build_all.sh        Schleife über alle Projekt-build.sh
├── tools/              gemeinsame Bausteine der Build-Skripte
├── common/             Übergabe zwischen Starter (pbcemu/) und Emulatoren
├── docs/               Bauanleitung, Hardware-Referenz, Lizenzübersicht
├── dist/               fertige .uf2 — mitversioniert, damit man ohne
│                       Werkzeugkette flashen kann; dazu wad2uf2/ mit den
│                       WAD-Werkzeugen und deren Anleitung
└── <projekt>/          je Projekt: Quellen, README, LICENSE, build.sh
```
