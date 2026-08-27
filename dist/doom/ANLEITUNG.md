# Doom auf den PicoBoy Color Plus bringen

Zwei Dateien müssen aufs Gerät: die **Firmware** liegt hier bei, die **Level**
nicht. Die kommen aus einer WAD-Datei, die du selbst mitbringst und mit dem
Werkzeug in diesem Ordner in eine flashbare `.uf2` umwandelst.

| Datei | Inhalt | Flash-Adresse |
|---|---|---|
| `doom-usb-mp.uf2` | das Spiel — liegt hier bei | `0x10000000` |
| deine `DOOM.uf2` | die Level — erzeugst du selbst | `0x10048000` |

Die beiden überschneiden sich nicht, du kannst sie also unabhängig voneinander
aufspielen und später einzeln austauschen.

---

## Sofort losspielen: Freedoom liegt bei

**`freedoom1.uf2` ist fertig zum Aufspielen** — du brauchst nichts umzuwandeln
und nichts zu besorgen. Zwei Dateien draufziehen, fertig:

1. `doom-usb-mp.uf2`  (das Spiel)
2. `freedoom1.uf2`    (die Level)

Wie das geht, steht unten unter „Aufs Gerät schieben".

[Freedoom](https://freedoom.github.io/) ist ein vollständig freier Ersatz für
die Doom-Spieldaten: eigene Level, eigene Grafiken, eigene Gegner, aber
dieselbe Engine. Es steht unter der **3-Klausel-BSD-Lizenz** und darf deshalb
frei weitergegeben werden — anders als die WADs von id Software. Die
Lizenzbedingungen stehen in `freedoom-LIZENZ.txt`, die Mitwirkenden in
`freedoom-CREDITS.txt`; beide gehören zur Weitergabe dazu.

Beigelegt ist **Freedoom Phase 1** in der Fassung 0.13.0, vier Episoden. Die
Rohdaten liegen als `freedoom1.wad` daneben, falls du sie anderweitig
verwenden oder selbst umwandeln willst.

> Es füllt den Flash fast aus: 14,7 MB von den rund 15,7 MB, die für Spieldaten
> zur Verfügung stehen. Es passt, aber viel Luft bleibt nicht.

## Lieber das echte Doom?

Dann brauchst du eine eigene Kopie von `DOOM.WAD` oder `DOOM2.WAD`, etwa aus
einer gekauften Fassung oder von Steam. **Die liegen hier nicht bei und dürfen
nicht weitergegeben werden** — das Werkzeug darfst du weitergeben, die
Spieldaten nicht. Wie du daraus eine flashbare Datei machst, steht im nächsten
Abschnitt.

---

## Schritt 1: Die eigene WAD umwandeln

### Windows

**`bin\whd_gen-windows-x86_64.exe` doppelklicken.** Es öffnet sich ein
Auswahlfenster; dort die WAD anklicken. Die fertige `.uf2` liegt danach neben
der WAD.

Alternativ die WAD-Datei mit der Maus **auf die .exe ziehen**.

Es muss nichts nachinstalliert werden — der Dialog benutzt nur eingebaute
Windows-Funktionen.

### Linux

```bash
bin/whd_gen-linux-x86_64  DOOM.WAD
```

Daraus wird `DOOM.uf2` im selben Ordner. Das Programm ist statisch gebunden
und braucht keine weiteren Pakete. Falls es sich nicht starten lässt:

```bash
chmod +x bin/whd_gen-linux-x86_64
```

### macOS

Für macOS liegt **kein fertiges Programm bei** — es muss auf einem Mac gebaut
werden, das geht auf einem anderen System nicht. Das Bauskript dafür liegt im
Repo unter `doom/wad2uf2/build-whd_gen.sh`.

### Eigener Ausgabename

```bash
bin/whd_gen-linux-x86_64  DOOM.WAD  MEIN-NAME.uf2  -no-super-tiny
```

* Endet der Zielname auf **`.uf2`**, entsteht eine flashfertige Datei für die
  Adresse `0x10048000`.
* Endet er auf **`.whd`**, wird nur das rohe Zwischenformat geschrieben.
* Gibt es die `.uf2` schon, wird sie **nicht überschrieben** — es wird `_1`,
  `_2` … angehängt.
* **`-no-super-tiny` ist wichtig** und wird bei der Kurzform oben automatisch
  gesetzt. Es passt zu der hier beiliegenden Firmware, die die Vollversion mit
  drei Episoden unterstützt. Die sparsamere „super tiny"-Variante kann deren
  größere Level nicht packen.

---

## Schritt 2: Aufs Gerät schieben

Der bequeme Weg, ganz ohne Zusatzprogramme:

1. **BOOTSEL gedrückt halten** und dabei das USB-Kabel einstecken.
2. Taste loslassen. Der PicoBoy meldet sich am Rechner als **USB-Laufwerk**
   mit dem Namen `RP2350`.
3. Die `.uf2` einfach **auf dieses Laufwerk kopieren** — hinüberziehen genügt.
4. Das Gerät startet von selbst neu, sobald die Datei vollständig übertragen
   ist. Das Laufwerk verschwindet dabei; das ist normal und kein Fehler.

Für Doom sind es zwei Dateien, also zweimal derselbe Ablauf: erst
`doom-usb-mp.uf2`, dann deine `DOOM.uf2`. Nach der ersten verschwindet das
Laufwerk, also für die zweite wieder BOOTSEL halten und neu einstecken.

Später reicht es, nur die WAD-Datei zu tauschen — die Firmware bleibt liegen.

### Beim allerersten Mal: Flash komplett löschen

**Wenn vorher MicroPython oder ein anderes Programm auf dem Gerät war, genügt
das Hinüberziehen nicht.** Es überschreibt nur die eigenen Blöcke, alte Reste
bleiben weiter oben im Flash stehen. Doom startet dann zwar, aber bei einem
Neustart zeigt das Display noch das alte Bild — es sieht aus, als liefe das
alte Programm weiter.

Einmalig hilft `picotool`, das Gerät dafür im BOOTSEL-Modus anschließen:

```bash
picotool erase -a                 # ganzen Flash löschen
picotool load doom-usb-mp.uf2     # das Spiel
picotool load DOOM.uf2            # die Level
picotool reboot
```

Danach einmal **wirklich stromlos machen**: USB abziehen, kurz warten, wieder
einstecken. Ab dann startet das Gerät normal ins Spiel, und für alle weiteren
Male genügt wieder das Hinüberziehen.

> `picotool erase -a` kann bei 16 MB über zwei Minuten dauern. Bricht es ab,
> zusätzlich den oberen Bereich löschen:
> `picotool erase -r 0x10600000 0x11000000`

---

## Zwischen Doom 1 und Doom 2 wechseln

Dieselbe Firmware spielt beides — welcher Modus gilt, erkennt sie beim Start
an der WAD, samt Super-Shotgun bei Doom 2. Beide WAD-Dateien liegen aber an
derselben Adresse, es passt also immer nur **eine** aufs Gerät.

Zum Umschalten genügt es, die andere hinüberzuziehen; die Firmware bleibt
unangetastet. Kein erneutes Löschen nötig.

---

## Wenn es klemmt

**Das Display bleibt schwarz.** Meist wurde der Flash beim ersten Mal nicht
vollständig gelöscht — siehe oben. Sonst prüft `picotool info -a`, ob das
Gerät überhaupt als `rp2350-arm-s` gemeldet wird.

**Das Laufwerk erscheint nicht.** Dann wurde BOOTSEL zu früh losgelassen. Kabel
abziehen, Taste drücken und *gedrückt halten*, einstecken, erst dann loslassen.
Und es muss ein Datenkabel sein — viele USB-C-Kabel führen nur Strom.

**Die WAD ist zu groß.** Für Spieldaten stehen rund 15,7 MB zur Verfügung —
der Rest des 16-MB-Flash gehört der Firmware. Getestet ist das Werkzeug vor
allem mit den Original-WADs von Doom 1 und Doom 2; Freedoom Phase 1 passt mit
14,7 MB gerade noch. Bei größeren Sammlungen kann es eng werden.

**Die Umwandlung bricht mit „Expected MUS track" ab.** Dann liegt die Musik
der WAD im MIDI-Format vor, `whd_gen` erwartet aber MUS — so wie es die WADs
von id Software mitbringen. Freedoom etwa benutzt MIDI. Da die Firmware Musik
ohnehin nicht abspielt, genügt es, die Musikstücke vorher durch leere
MUS-Platzhalter zu ersetzen; genau so ist die beiliegende `freedoom1.uf2`
entstanden.

**Kein Ton bei der Musik.** Das ist Absicht: Die Klangeffekte laufen, nur die
OPL-Musik ist abgeklemmt. Der Piezo könnte sie ohnehin nicht sinnvoll
wiedergeben.

---

## Mehrspieler über USB-C

Zwei Geräte lassen sich mit einem USB-C-Kabel koppeln und spielen Coop oder
Deathmatch. **D+, D− und GND verbinden, VBUS nicht** — beide Geräte versorgen
sich selbst.

Die Rolle wird im Menü gewählt: *Options → Network Game → Host Game* macht das
Gerät zum Taktgeber, *Join Game* lässt es warten. Am einfachsten erst Host,
dann Join. Beide Geräte brauchen **dieselbe Firmware und dieselbe WAD**.

Das Menü öffnet sich mit **A + Mitte**.

---

## Cheats beim Einschalten

Taste gedrückt halten, während das Gerät startet — gilt dann für die ganze
Sitzung:

| Gehalten | Wirkung |
|---|---|
| **A** | Unverwundbarkeit (`iddqd`) |
| **B** | alle Waffen, volle Munition, alle Schlüssel, Rüstung 200 (`idkfa`) |
| **A + B** | beides |

Im Spiel schaltet **A** zur nächsten Waffe weiter.

---

## Was in diesem Ordner liegt

| | |
|---|---|
| `doom-usb-mp.uf2` | die Firmware, fertig zum Aufspielen |
| `freedoom1.uf2` | freie Spieldaten, fertig zum Aufspielen |
| `freedoom1.wad` | dieselben Daten als WAD, falls du sie anderweitig brauchst |
| `freedoom-LIZENZ.txt` | die BSD-Lizenz von Freedoom — gehört zur Weitergabe dazu |
| `freedoom-CREDITS.txt` | die Mitwirkenden an Freedoom |
| `bin/` | `whd_gen` für Linux und Windows |

Hier liegt nur, was zum Spielen gebraucht wird. Wer `whd_gen` selbst bauen
will — etwa für macOS —, findet Quelltext und Bauskript im Repo unter
`doom/wad2uf2/`, die Portierung selbst unter `doom/`.
