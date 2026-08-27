# Lizenzen

Jedes Projekt hat eine eigene Lizenzdatei in seinem Ordner. Diese Übersicht
sagt, welche das ist und woher sie kommt.

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
