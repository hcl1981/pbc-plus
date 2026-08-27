# OpenTyrian für den PicoBoy Color Plus

Portierung von [OpenTyrian](https://github.com/opentyrian/opentyrian) — dem
quelloffenen Nachbau des Weltraum-Schützen *Tyrian* (1995) — auf den PicoBoy
Color Plus. Einzelspieler und Zweispieler über ein USB-C-Kabel.

```bash
./build.sh          # -> ../dist/tyrian-komplett.uf2
```

> **Stand:** Läuft auf dem Gerät. 229 KB Firmware, 468 KB von 512 KB
> statischem RAM, rund 41 KB Heap. Was beim Portieren anzupassen war und wo
> noch Luft ist, steht in [`pbc-tyrian/README.md`](pbc-tyrian/README.md).

## Aufbau

| Ordner | Inhalt |
|---|---|
| `pbc-tyrian/` | die Portierung: Quelltext, CMake, Werkzeuge |
| `data/tyrian21/` | die Spieldaten von Tyrian 2.1 |
| `shots/` | Bildschirmfotos, unter anderem zum Vergleich der Skalierungsverfahren |

## Firmware und Daten

Anders als bei Doom liegen die Spieldaten bei: die **Tyrian-2.1-Daten sind
Freeware**. `build.sh` packt sie in ein Flash-Archiv und verschmilzt Firmware
und Archiv zu einer einzigen Datei — es ist also nur ein Flash-Vorgang nötig.

```
0x10000000  Firmware        269 KB
0x10100000  Datenarchiv    8,86 MB
0x10FF8000  Spielstände      16 KB
0x10FFFF00  SDK-Zusatzblock 256 B   <- muss frei bleiben
```

Wer an der Firmware arbeitet, flasht `pbc-tyrian/build/tyrian.uf2` — die
unverschmolzene Firmware aus dem Bauordner.
Das Datenarchiv liegt an einer anderen Adresse und bleibt dabei liegen — das
spart bei jedem Durchlauf das Übertragen von fast 9 MB.

## Lizenz

GPL-2.0, wie OpenTyrian. Die Datei `usblink.c` stammt aus der
rp2040-doom-Portierung für dasselbe Gerät und ist ebenfalls GPL-2.0. Die
Tyrian-Daten sind Freeware von Jason Emery. Siehe `LICENSE`.
