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
[`doom/wad2uf2/`](doom/wad2uf2/) enthält `whd_gen` für Linux und Windows: eigene
WAD hineingeben, flashfertige UF2 kommt heraus; unter Windows reicht ein
Doppelklick. Die WADs selbst sind kommerzielle Daten von id Software und dürfen
nicht weitergegeben werden. Ablauf und Adressen: [`doom/FLASHEN.md`](doom/FLASHEN.md).

**PicoBoyGB braucht ROMs.** Nach dem Flashen **Mitte** halten und **RESET**
drücken: das Gerät meldet sich als USB-Stick `PICOBOYGB`. Dort `.gb`-Dateien
ablegen, auswerfen, RESET ohne Mitte — das Menü listet sie dann auf.

**Tyrian-Daten liegen bei.** Die sind Freeware und stecken schon in
`tyrian-komplett.uf2`.

## Lizenzen

Gemischt: BSD, MIT, GPL-2.0 und GPL-3.0 — je nachdem, worauf die Portierung
aufsetzt. Jedes Projekt hat seine eigene Lizenzdatei, die Übersicht steht in
[`docs/LIZENZEN.md`](docs/LIZENZEN.md).

Bei den GPL-Projekten (`stransball2`, `tyrian`, `doom`, `jumpnbump`,
`micropolis`) muss beim Weitergeben der Firmware der Quelltext mitgeliefert
oder angeboten werden. Das ist der praktische Unterschied zu den MIT-Projekten
daneben.

## Bekannte Baustellen

Ehrlich gesagt, damit niemand danach sucht:

* **`tyrian`** baut vollständig, ist aber **noch nie auf Hardware gelaufen**.
* **`doom`: der Ton ist aus.** Der PWM/DMA-Audiopfad blockiert beim Start,
  deshalb wird die Sound-Init auf dem PicoBoy übersprungen. Details in
  `doom/FLASHEN.md`.
* **`doom`: die vier Änderungen an der Bildausgabe** sollen die vollen 35 Tics
  je Sekunde halten. Die erwartete Wirkung ist beschrieben, aber **auf Hardware
  noch nicht gemessen** — siehe `doom/picoboy-doom/TEMPO.md`.
* **`micropolis`**: die Display-Pins SCK/MOSI wurden aus dem Vorgängerprojekt
  MicroCityPBC nie notiert. `src/config.h` nimmt spi0, SCK=GP18, MOSI=GP19 an.
  Bleibt der Schirm dunkel, ist das die erste Stelle zum Nachsehen.

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
├── docs/               Bauanleitung, Hardware-Referenz, Lizenzübersicht
├── dist/               fertige .uf2 — mitversioniert, damit man
│                       ohne Werkzeugkette flashen kann
└── <projekt>/          je Projekt: Quellen, README, LICENSE, build.sh
```
