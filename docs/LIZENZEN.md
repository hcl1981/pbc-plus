# Lizenzen

Jedes Projekt hat eine eigene Lizenzdatei in seinem Ordner. Diese Übersicht
sagt, welche das ist und woher sie kommt. Die Kurzfassung steht als
[`LICENSE`](../LICENSE) im Wurzelverzeichnis — dort sucht GitHub sie.

| Projekt | Lizenz | Datei | Herkunft |
|---|---|---|---|
| `noiz2sa` | BSD | `noiz2sa/COPYING` | noiz2sa 0.52, Kenta Cho (ABA Games) |
| `spout` | MIT | `spout/COPYING` | Spout, Kuni 2002–2006; Unix-Fassung Nick White 2010 |
| `stransball2` | GPL-2.0 | `stransball2/COPYING` | Super Transball 2, Santiago Ontañón |
| `micropolis` | GPL-3.0 | `micropolis/LICENSE` | Micropolis (quelloffenes SimCity 1) |
| `tyrian` | GPL-2.0 | `tyrian/LICENSE` | OpenTyrian |
| `doom` | GPL-2.0 | `doom/LICENSE` | Chocolate Doom → rp2040-doom |
| `jumpnbump` | GPL-2.0 | `jumpnbump/LICENSE` | Jump 'n Bump, Brainchild Design 1998 |
| `picoboygb` | MIT | `picoboygb/LICENSE` | Peanut-GB → RP2040-GB → Pico-GB, siehe `picoboygb/HERKUNFT.md` |
| `picoboygb-arduino` | MIT | `picoboygb-arduino/LICENSE` | dieselbe Kette, Arduino-Fassung |
| `c64` | GPL-2.0+ | `c64/LICENSE` | Frodo4 (Christian Bauer) → frank-c64 (rh1tech); C64-ROMs: MEGA65 OpenROMs (LGPL-3.0+, `c64/tools/openroms/`); Flash-Laufwerk und USB-Stick aus `picoboygb` (MIT, `c64/LICENSE.picoboygb`) |
| `sms` | GPL-3.0 | `sms/LICENSE` | SMS Plus (Charles MacDonald, GPL-2.0+) → pico-smsplus (PicoPlus-devel, GPL-3.0); Flash/USB aus `picoboygb` (MIT) |
| `pce` | GPL-3.0 | `pce/LICENSE` | pce-go aus retro-go (ducalex, GPL-2.0) → pico-pcePlus (PicoPlus-devel, GPL-3.0); Flash/USB aus `picoboygb` (MIT) |
| `pbcemu` | GPL-3.0+ | `pbcemu/LICENSE` | eigener Starter; Plattformteile aus `picoboygb` (MIT), Port/Programm-Einstellungen aus `c64` (GPL-2.0+), Schrift Lato (OFL-1.1, `pbcemu/LICENSE.lato`). `dist/pbcemu.uf2` bündelt alle Emulatoren, jeder unter seiner eigenen Lizenz |
| `gbc` | GPL-3.0 | `gbc/LICENSE` | Peanut-GB (MIT) mit GBC-Erweiterung von tvecera → pico-peanutGB (PicoPlus-devel, GPL-3.0); minigb_apu, Flash/USB aus `picoboygb` (MIT) |
| `nes` | GPL-3.0 | `nes/LICENSE` | QuickNES (Shay Green, LGPL-2.1+) → frank-nes (rh1tech); Flash-Laufwerk und USB-Stick aus `picoboygb` (MIT, `nes/LICENSE.picoboygb`) |
| `megadrive` | AGPL-3.0 | `megadrive/LICENSE` | Gwenesis → frank-genesis (rh1tech); Flash-Laufwerk und USB-Stick aus `picoboygb` (MIT, `megadrive/LICENSE.picoboygb`) |
| `i2collection` | MIT | `i2collection/LICENSE` | Eigenentwicklung |
| `micropython` | MIT | Upstream | MicroPython, hier nur die Board-Portierung |

## Worauf zu achten ist

**Die GPL färbt ab.** Bei `stransball2`, `tyrian`, `doom` und `jumpnbump` steht
die Vorlage unter GPL, damit auch die Portierung. Wer diese Firmware
weitergibt, muss den Quelltext mitliefern oder anbieten. Bei `micropolis` ist
es GPL-3.0, bei den anderen GPL-2.0. Das ist ein echter Unterschied zu den
MIT-/BSD-Projekten daneben.

**Bei PicoBoyGB war die Lizenz nicht frei wählbar.** Die Kette
Peanut-GB → RP2040-GB → Pico-GB steht durchgehend unter MIT; MIT ist damit die
richtige Wahl. Die vollständige Herleitung samt der mitgelieferten
Fremdbestandteile (ChaN FatFs, Hedley, TinyUSB) steht in
`picoboygb/HERKUNFT.md`.

**Mega Drive steht unter AGPL-3.0.** Der Emulatorkern stammt aus
frank-genesis, und das steht unter der GNU Affero GPL 3.0. Für die Weitergabe
der Firmware gilt dasselbe wie bei der GPL: Quelltext mitliefern oder
anbieten. Die aus PicoBoyGB übernommenen Dateien (Flash-Laufwerk,
USB-Massenspeicher) bleiben MIT; ihr Lizenztext liegt als
`megadrive/LICENSE.picoboygb` bei.

**C64: freie ROMs.** Statt der Original-ROMs von Commodore (Rechte heute bei
Cloanto, nicht frei) enthält `c64` die freien **MEGA65 OpenROMs**
(LGPL-3.0-or-later, einige BASIC-Teile MIT). Das KERNAL ist für die
Laufwerksemulation gepatcht; Lizenztexte, verwendete Fassung, Änderungen und
der Weg zum Neubauen liegen in `c64/tools/openroms/`, wie es die LGPL
verlangt. Ältere Stände dieses Repositorys (bis Commit `a095a08`) enthielten
noch die Commodore-ROMs.

**Urheberrechtsvermerke müssen erhalten bleiben.** Sowohl MIT als auch die
FatFs-Lizenz verlangen das. Die Vermerke stehen in den Quelldateien — beim
Aufräumen nicht entfernen.

## Spieldaten

| Daten | Lage |
|---|---|
| Freedoom Phase 1 (`dist/doom/`) | **3-Klausel-BSD** — liegen bei, dürfen weitergegeben und verändert werden |
| Tyrian 2.1 (`tyrian/data/`) | **Freeware**, liegen bei und dürfen weitergegeben werden |
| Jump'n'Bump (`jumpnbump/data-src/`) | GPL-2.0, liegen bei |
| Doom-WADs | **kommerziell (id Software)** — liegen **nicht** bei, dürfen nicht weitergegeben werden |
| Game-Boy-ROMs | fremdes Urheberrecht — liegen **nicht** bei |
| Genesis-/Mega-Drive-ROMs | fremdes Urheberrecht — liegen **nicht** bei |
| NES-ROMs | fremdes Urheberrecht — liegen **nicht** bei |
| Master-System-, Game-Gear-, PC-Engine- und Game-Boy-Color-ROMs | fremdes Urheberrecht — liegen **nicht** bei |
| C64-Disk-Images | fremdes Urheberrecht — liegen **nicht** bei |

`.gitignore` schließt WADs und ROMs aus, damit eigene Kopien nicht
versehentlich im Repo landen. Die beiliegende `freedoom1.wad` ist davon
ausgenommen — sie steht unter BSD und darf weitergegeben werden.

## Freedoom

Damit Doom ohne eigene WAD spielbar ist, liegt **Freedoom Phase 1 0.13.0** bei:
`dist/doom/freedoom1.uf2` als flashfertiges Abbild und `freedoom1.wad` als
Rohdaten. Freie Ersatzdaten für die Doom-Engine — eigene Level, Grafiken und
Gegner, dieselbe Engine.

Die 3-Klausel-BSD-Lizenz verlangt, dass Urheberrechtsvermerk, Lizenztext und
Haftungsausschluss bei der Weitergabe erhalten bleiben. Beides liegt deshalb
mit dabei: `freedoom-LIZENZ.txt` und `freedoom-CREDITS.txt`. **Beim Aufräumen
nicht entfernen** — ohne sie wäre die Weitergabe nicht mehr lizenzkonform.
