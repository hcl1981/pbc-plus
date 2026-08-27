# DOOM Vollversion für PicoBoy Color Plus — Flash-Anleitung

Zwei UF2-Dateien (beide im RP2350-UF2-Format, direkt per Drag&Drop oder
`picotool load` auf den PicoBoy Color Plus):

| Datei                | Inhalt                              | Flash-Adresse |
|----------------------|-------------------------------------|---------------|
| `doom-usb-mp.uf2` | Das Spiel (Firmware) — liegt in `../dist/` | 0x10000000 |
| `DOOM.uf2`  | Level/Daten aus der eigenen `DOOM.WAD` (Doom 1) | 0x10048000 |
| `DOOM2.uf2` | Level/Daten aus der eigenen `DOOM2.WAD` (Doom 2)| 0x10048000 |

Die WAD-UF2 erzeugst du selbst aus deiner eigenen WAD — siehe
[`wad2uf2/LIESMICH.md`](wad2uf2/LIESMICH.md). Die Namen richten sich nach
deiner Eingabedatei; unten stehen sie beispielhaft.

Die **gleiche Firmware** (`doom-usb-mp.uf2`) spielt sowohl Doom 1 als auch
Doom 2 — der Spielmodus (inkl. Super-Shotgun bei Doom 2) wird zur Laufzeit aus
der WAD erkannt. Beide WAD-UF2 liegen aber an **derselben Adresse**
(0x10048000), es ist also immer nur **eine** gleichzeitig auf dem Gerät.
Zum Umschalten einfach die andere WAD-UF2 laden — die Firmware bleibt:

```bash
# Auf Doom 2 wechseln (Gerät im BOOTSEL):
picotool load DOOM2.uf2
picotool reboot
# ... oder zurück auf Doom 1:
picotool load DOOM.uf2
picotool reboot
```
(Kein `erase -a` nötig, solange die Firmware schon läuft — es wird nur der
WAD-Bereich überschrieben.)

Die beiden überschneiden sich nicht (Firmware endet bei ~0x1003e000, die
WAD beginnt bei 0x10048000), du kannst sie also unabhängig flashen.

## Flashen — WICHTIG: erst den Flash komplett löschen!

Reines Drag&Drop reicht **nicht**, wenn vorher MicroPython o. Ä. drauf war:
Es überschreibt nur die UF2-Blöcke, MicroPython-Reste bleiben oben im Flash
stehen. Der Bootrom bootet zwar dann Doom, aber bei einem Soft-Reset behält
das Display sein altes Bild — es sieht dann so aus, als liefe noch das alte
MicroPython-Menü. Deshalb einmalig komplett löschen (PicoBoy im BOOTSEL-Modus,
per USB angeschlossen):

```bash
picotool erase -a                    # ganzen Flash löschen (killt alte Reste)
picotool load doom-usb-mp.uf2        # Spiel  -> 0x10000000
picotool load DOOM.uf2      # WAD    -> 0x10048000
picotool reboot
```

Danach einmal **wirklich stromlos machen** (USB ziehen, kurz warten,
wieder einstecken) — sonst zeigt das Display evtl. noch ein eingefrorenes
altes Bild. Ab dann bootet er normal ins Spiel.

> `picotool erase -a` kann bei 16 MB länger als 2 Minuten dauern. Wird es
> abgebrochen, bleibt der obere Flash evtl. stehen — dann zusätzlich
> `picotool erase -r 0x10600000 0x11000000` ausführen.

## Was anders ist als bei der Shareware-Version

* Gebaut wurde das **`doom_tiny_nost`**-Target (die „non super-tiny"-Variante),
  nötig für die Registered/Vollversion mit 3 Episoden. Die Shareware-Anleitung
  benutzte `doom_tiny` (super-tiny, nur Episode 1, WAD bei 0x10040000).
* Die WAD wurde mit `whd_gen DOOM.WAD doom.whd -no-super-tiny` ins
  WHD-Format (Magic `IWHD`) konvertiert — die super-tiny-WHX-Variante kann
  die größeren Level der Vollversion nicht packen.

## Ton

Ton ist **aus**. Der PWM/DMA-Audiopfad des Ports (`I_InitSound` /
`sound_pico_module`) blockiert beim Start, deshalb werden Sound- und
OPL-Musik-Init auf dem PicoBoy übersprungen (`#ifdef PICOBOY_BUILD` in
`src/doom/d_main.c`). Ohne diesen Schritt bleibt das Gerät beim Booten hängen
(kein Bild, kein Backlight).

## Waffen-Wechsel

Knopf **A** schaltet im Spiel zur nächsten Waffe weiter. Das war schon
korrekt: die Logik (`G_NextWeapon`) läuft alle Waffen durch, die du
*besitzt*. In der Shareware blieben Plasmagewehr (6) und BFG9000 (7) außen
vor, weil die Engine dort `gamemode == shareware` erkennt. Mit der
Vollversions-WAD ist der Modus „registered" → Plasma und BFG sind
automatisch im Wechsel dabei, **ohne Code-Änderung**. Ein Super-Shotgun
gibt es nur in DOOM II, nicht in DOOM 1 — daher bleibt es bei 7 Waffen.

## Cheats per Tastendruck beim Einschalten

Beim Booten (Strom anlegen / Reset) abgefragt und für die ganze Sitzung
aktiv (wird bei jedem Spawn/Levelstart neu gesetzt):

| Gehalten beim Einschalten | Cheat                                            |
|---------------------------|--------------------------------------------------|
| **A**                     | `iddqd` — Gottmodus / Unverwundbarkeit           |
| **B**                     | `idkfa` — alle Waffen, volle Munition, alle Schlüssel, Rüstung 200 |
| **A + B**                 | beides                                            |

Knopf einfach gedrückt halten, während der PicoBoy startet — danach
loslassen. Die Auswahl wird einmalig beim Start gelatcht.
