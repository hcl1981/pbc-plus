# Doom für den PicoBoy Color Plus

Portierung von [rp2040-doom](https://github.com/kilograham/rp2040-doom)
(Graham Sanderson, auf Basis von Chocolate Doom) auf den PicoBoy Color Plus.
Mit **Mehrspieler über ein USB-C-Kabel** zwischen zwei Geräten.

```bash
./build.sh
```

## Die Tempo-Änderungen

Doom läuft mit festen 35 Tics je Sekunde, die Bildrate ist frei. Der geradeaus
portierte Stand kippte im Netzspiel auf 24 Tics, weil `pd_end_frame()` auf
Core0 rund 11 ms lang auf den Bildtransfer wartete — bei 30 Bildern je Sekunde
ein Drittel der Rechenzeit, nur fürs Warten.

Vier Eingriffe beheben das: der Bildtransfer wird angestoßen statt abgewartet,
Bilder entfallen wenn die Tics zurückfallen, die Tic-Pakete tragen keinen
Ballast mehr und der Takt ist schneller. **20 Bilder mit vollen 35 Tics spielen
sich deutlich besser als 30 Bilder mit 24** — sonst laufen Steuerung, Gegner
und Waffen in Zeitlupe.

Begründung, die einzelnen CMake-Schalter, die Messanleitung und die dabei
verworfenen Sackgassen stehen in [`picoboy-doom/TEMPO.md`](picoboy-doom/TEMPO.md).
**Auf Hardware gemessen wurde das noch nicht.**

## Die WAD fehlt — und das muss so sein

Die Firmware allein spielt nichts. Die Level kommen aus einer WAD-Datei, die
als **zweite** UF2 an eine eigene Flash-Adresse geht:

| Datei | Inhalt | Flash-Adresse |
|---|---|---|
| `doom-usb-mp.uf2` | die Firmware | 0x10000000 |
| deine `DOOM.uf2` | Level und Daten aus der WAD | 0x10048000 |

Dieselbe Firmware spielt Doom 1 und Doom 2 — der Modus wird zur Laufzeit aus
der WAD erkannt, samt Super-Shotgun bei Doom 2. Zum Umschalten reicht es, die
andere WAD-UF2 zu flashen; die Firmware bleibt liegen.

Aus der eigenen WAD eine flashfertige UF2 machen:

```bash
wad2uf2/bin/whd_gen-linux-x86_64  DOOM.WAD      # -> DOOM.uf2
```

Unter Windows die `.exe` doppelklicken oder die WAD daraufziehen. Einzelheiten
in [`wad2uf2/ANLEITUNG.md`](wad2uf2/ANLEITUNG.md), der genaue Flash-Ablauf ebenfalls —
dort steht auch, warum reines Drag&Drop beim ersten Mal nicht reicht, wenn
vorher MicroPython auf dem Gerät war.

Die WADs selbst sind kommerzielle Daten von id Software. Sie liegen diesem
Repo nicht bei und dürfen nicht weitergegeben werden — nur das Werkzeug darf
weitergegeben werden, die WAD bringt jede und jeder selbst mit.

## Mehrspieler über USB-C

Zwei Geräte, ein USB-C-Kabel: **D+, D− und GND verbinden, VBUS nicht** (beide
sind eigenversorgt). Coop und Deathmatch.

Die Rolle wird im Menü gewählt, nicht durch das Kabel:

* **Options → Network Game → Host Game / Deathmatch** → das Gerät taktet die
  Verbindung, Spielername `PLAYER 1`.
* **Options → Network Game → Join Game** → das Gerät wartet, `PLAYER 2`.

Am einfachsten erst Host, dann Join. Ein enges Zeitfenster gibt es nicht — der
Host wiederholt seine Anfrage alle 20 ms. Menü-Taste ist **A + CENTER**.

Beide Geräte brauchen **dieselbe Firmware und dieselbe WAD**, sonst lehnt der
Host mit „network game is not compatible" ab; die Firmware vergleicht dafür
einen Hash aus Version und WHD.

Alles Weitere in [`USB-MULTIPLAYER.md`](USB-MULTIPLAYER.md), Messwerte und
Feinheiten des Links in [`USB-LINK-HINWEISE.txt`](USB-LINK-HINWEISE.txt).

## Cheats beim Einschalten

Taste gedrückt halten, während das Gerät startet — gilt für die ganze Sitzung:

| Gehalten | Wirkung |
|---|---|
| **A** | `iddqd` — Unverwundbarkeit |
| **B** | `idkfa` — alle Waffen, volle Munition, alle Schlüssel, Rüstung 200 |
| **A + B** | beides |

## Waffenwechsel

Knopf **A** schaltet im Spiel zur nächsten Waffe weiter. Die Logik
(`G_NextWeapon`) läuft alle Waffen durch, die man *besitzt*. In der Shareware
blieben Plasmagewehr (6) und BFG9000 (7) außen vor, weil die Engine dort
`gamemode == shareware` erkennt. Mit einer Vollversions-WAD ist der Modus
„registered" — Plasma und BFG sind dann automatisch im Wechsel dabei, ganz
ohne Änderung am Code. Einen Super-Shotgun gibt es nur in Doom 2, deshalb
bleibt es bei sieben Waffen.

## Was anders ist als im Ursprungsprojekt

**Gebaut wird `doom_tiny_nost`**, das Target für die Vollversion mit drei
Episoden. `doom_tiny` wäre die Shareware-Variante („super tiny", nur Episode 1,
WAD bei 0x10040000). Passend dazu wandelt `whd_gen` die WAD mit
`-no-super-tiny` ins WHD-Format um — die super-tiny-Variante kann die
größeren Level der Vollversion nicht packen.

**Die Musik ist aus, die Klangeffekte laufen.** Schüsse, Türen und Gegner sind
zu hören; nur die OPL-Musik ist abgeklemmt. `I_OPL_InitMusic()` meldet auf dem
PicoBoy schlicht „keine Musik" und lässt `music_module` auf `NULL`, womit jeder
spätere Musikaufruf ins Leere läuft. Zwei Gründe: Der Piezo kann die
OPL-Klänge ohnehin nicht sinnvoll wiedergeben, und der echte Init-Pfad würde
GENMIDI aus der WAD laden und I2S-Audiotreiber einrichten, die es hier nicht
gibt.

**Die Targets `doom_tiny_usb` und `doom_tiny_nost_usb` werden nicht gebaut.**
Ihr `USB_SUPPORT` ist die USB-**Tastatur**unterstützung aus rp2040-doom und
braucht einen USB-Host. Den hat diese Portierung entfernt, weil der PicoBoy
keinen USB-A-Anschluss hat (`picoboy-doom/src/pico/CMakeLists.txt`). Sie
ließen sich hier gar nicht übersetzen, `tusb.h` fehlt. Der USB-Mehrspieler
läuft stattdessen über `usblink.c` und steckt in der gebauten Firmware.

## Was im Quellordner sonst noch liegt

`picoboy-doom/` ist ein Abkömmling von rp2040-doom, das wiederum von Chocolate
Doom abstammt. Deren Dateien sind absichtlich stehen geblieben, damit sich
gegen den Ursprung vergleichen lässt:

| Datei | Herkunft |
|---|---|
| `README-rp2040-doom.md` | die README von rp2040-doom (Graham Sanderson) |
| `README-chocolate.md` | die README von Chocolate Doom |
| `AUTHORS`, `ChangeLog`, `COPYING.md` | Urheber und Lizenz der Vorfahren |
| `Makefile.am`, `configure.ac`, `autogen.sh`, `rpm.spec.in`, `pkg/`, `man/`, `win32/` | der Autotools-Bau für Desktop-Systeme |

**Der Autotools-Bau wird hier nicht benutzt.** Für den PicoBoy läuft alles über
CMake, angestoßen von `build.sh`. Wer `./autogen.sh` aufruft, baut am Ziel
vorbei.

`TEMPO.md` im selben Ordner beschreibt die vier Änderungen an der Bildausgabe.

## Lizenz

GPL-2.0, wie Chocolate Doom und rp2040-doom. Siehe `LICENSE`. Wer die Firmware
weitergibt, muss den Quelltext mitliefern oder anbieten.
