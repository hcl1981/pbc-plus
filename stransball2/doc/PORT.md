# Was gegenüber Super Transball 2 geändert wurde — und warum

Grundsatz wie bei den anderen beiden Ports: die Vorlage so weit wie möglich in
Ruhe lassen. Schiffsphysik, Ballphysik am Traktorstrahl, Kollisionsprinzip,
Gegnerverhalten, Türen und Schalter sind Zug um Zug übernommen, samt der
Festkommaeinheit `FACTOR 512` und den acht leeren Vorlaufzeilen über jeder
Karte.

## 1. Lizenz: GPL-2.0 statt MIT/BSD

Das ist der auffälligste Unterschied zu den Ports von Spout und noiz2sa. GPL-2.0
erlaubt die Portierung ausdrücklich, verlangt aber, dass das Ergebnis ebenfalls
unter GPL-2.0 steht und bei Weitergabe der Quelltext mitgeliefert oder angeboten
wird. `COPYING` enthält den vollständigen Lizenztext.

## 2. Daten: alles vorab gewandelt

Die Vorlage lädt zur Laufzeit PCX-Bilder und Textdateien. Auf dem Gerät gibt es
weder Dateisystem noch Bilddecoder, und die Daten ändern sich nach dem Bauen nie
mehr. `tools/mkdata.py` wandelt deshalb vorab:

| | |
|---|---|
| 500 Kacheln 16×16 | 250 KB, direkt im Panelformat (BGR565) |
| Kollisionsmasken | 15 KB, 1 Bit je Punkt |
| 14 Level | 30 KB, größte Karte 96×32 Kacheln |
| 23 Schiffsbilder 32×32 | 46 KB, mit Masken |

Schwarz ist in der Vorlage der Transparenzschlüssel; daraus wird hier der Wert
`0x0000`, und Farben, die zufällig darauf abbilden würden, werden auf `0x0001`
gehoben.

### Das Kartenformat hat einen Anhang

Nach den Zellen folgen in der Datei noch die **Panzer** (Anzahl, dann je
`x y Typ`) und erst danach der Hintergrundtyp. Wer nur die Zellen liest und die
nächste Zahl für den Hintergrund hält, bekommt die Panzeranzahl — und keine
Panzer. Genau das war im ersten Anlauf der Fall.

### Kacheln sind nicht Markierungen

Die Kugel steht in den Kartendaten als Kachel **110**, gezeichnet wird sie aber
mit **320** (liegend) bzw. **321** (am Traktorstrahl). Wer die Markierung
zeichnet, bekommt ein Stück Plattform, das durch die Gegend fliegt. Ebenso:
Explosionen laufen über die sechs Bilder 240/241/260/261/280/281, Panzer über
`282 + 4*Typ` in zwei 16×16-Hälften.

## 3. Kollision ohne SDL und SGE

Die Vorlage prüft pixelgenau, indem sie das gedrehte Schiffssprite und den
Kartenausschnitt in SDL-Flächen rendert und beides mit der SGE-Bibliothek
vergleicht — pro Prüfung mehrere Flächen anlegen, blitten und wieder freigeben.

Hier wird die Schiffsmaske **einmal je Bild** von Hand gedreht
(Rückwärtsabbildung über eine Winkeltabelle) und direkt Bit für Bit gegen die
Kachelmasken geprüft. Kein SDL, kein SGE, keine Speicheranforderung im
Spielverlauf. Das Ergebnis ist dieselbe Pixelgenauigkeit.

Die vier Richtungssonden des Balls (Kacheln 340/342/360/362) sind unverändert
übernommen.

## 4. Bildformat

Die Vorlage zeigt 320×240 im Querformat. Das Panel ist hochkant 240×280. Da die
Ansicht ohnehin dem Schiff folgt, wird daraus schlicht ein anderer Ausschnitt:
240×256 Spielfeld, darunter 24 px Anzeige mit Treibstoffbalken und Schiffszahl.
Ein Kachelspiel verträgt das problemlos — anders als bei den beiden
Vektorspielen musste hier nichts skaliert werden, die Kacheln bleiben 1:1.

Gezeichnet wird zeilenweise direkt ins Panelformat. Ein Indexpuffer wie bei
noiz2sa lohnt hier nicht, weil die Kacheln schon als RGB565 vorliegen — deshalb
braucht dieser Port nur **34 KB RAM** statt 242 KB.

## 5. Ton

Die Vorlage spielt WAV-Dateien über SDL_mixer. Ersatz ist der schon vorhandene
Synthesepfad: Rauschen für die Schubdüse, Motive für Kugelaufnahme,
Levelabschluss, Explosion, Schalter und Tod.

## 6. Gekürzt oder anders gelöst

* **Zustandsschirme:** die Vorlage hat eigene Dateien für Logo, Menü, Anleitung,
  Zwischenbilder, Abspann, Wiederholungen und Tastenbelegung. Hier gibt es
  Titel mit Levelwahl, Schiffswahl und die Meldungen im Spiel — der Rest ist auf
  einem Handgerät ohne Tastatur ohnehin gegenstandslos.
* **Wiederholungen (Replays):** entfallen. Die Vorlage zeichnet Eingaben auf und
  spielt sie ab; das braucht Dateien.
* **Türen:** die Vorlage animiert sie über eigene Kachelfolgen. Hier wird
  `113 + Zustand` gezeichnet (Zustand 0..14), was der Kachelanordnung
  entspricht.
* **Drehkanonen:** die Vorlage zeichnet einen Sockel und dreht den Turm zur
  Laufzeit. Der Kanonenkörper steht ohnehin als Kachel in der Karte; der
  gedrehte Turm entfällt, weil Kacheldrehung je Bild und Gegner zu teuer wäre.
  Das Zielverhalten selbst ist übernommen — die Kanone feuert weiterhin nur,
  wenn sie ausgerichtet ist.
* **Grenzen:** 192 Gegner und Geschosse gleichzeitig, 32 Türen, 16 Schalter,
  16 Tankstellen. Im härtesten gemessenen Level waren 52 Gegner aktiv.

## Prüfstand

| geprüft | wie |
|---|---|
| Datenwandlung | 500 Kacheln, 14 Level, 23 Schiffsbilder ohne Fehler gewandelt |
| `atan2` in Festkomma | alle acht Oktanten gegen Sollwerte nachgerechnet |
| Spiellogik und Renderer | `tools/host_test`, ASan + UBSan, über 40 000 Bilder in fünf Leveln und mit Zufallseingaben |
| Buchhaltung, Grenzen, Kartenmaße | `stb_selfcheck()` nach **jedem** Bild |
| Bildaufbau | PPM-Ausgabe des Host-Tests |
| Firmware baut, Ziel stimmt | warnungsfrei, `picotool info`: `rp2350-arm-s`, ARM Secure, Fixed Pins `none` |

**Nicht geprüft, weil dafür das Gerät nötig ist:** ST7789-Init, Y-Versatz,
BGR-Bit, Backlight, tatsächliche Bildzeit über SPI, Tastenentprellung, Tonpfad,
Flash-Schreibvorgang.

**Ebenfalls nicht geprüft: ob die Level durchspielbar sind.** Der Autopilot im
Host-Test hält das Schiff in der Luft und schießt, aber er sucht die Kugel nicht
und bringt sie nicht ans Ziel. Ob Traktorstrahl, Türen, Schalter und die
Zielbedingung im Zusammenspiel wirklich funktionieren, zeigt erst ein Mensch am
Gerät. Das ist die größte offene Frage dieses Ports.
