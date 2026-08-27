# Was gegenüber spout-1.4 geändert wurde — und warum

Grundsatz: die Vorlage so weit wie möglich in Ruhe lassen, damit ein späterer
Abgleich möglich bleibt. Die Physik ist Zug um Zug übernommen — Schwerkraft
(+8/Bild), Schub (`sintable[]/128`), Temposchranke ±1024, `pos += v/16`,
Kornbewegung in 1/256-Zellen, Abprall mit `v = -v/2`, Trefferzähler in Bit 6/7,
Höhlenkästen `20 - (Höhe+40)/64` (min. 4), Punkte, Zeitbonus alle 128 Höhe.

## 1. Bildformat: 120 × 120 Zellen statt 128 × 78

Die Vorlage zeigt 128×78 Zellen (Querformat einer Handheld-Anzeige). Das Panel
des PBC+ ist **hochkant** 240×280. Bei Zoom 2 passen 120 Zellen exakt auf
240 px Breite — das sind genau die Spielspalten 4..123 der Vorlage, die
Spielfeldbreite ist also **unverändert**. Die Höhe wächst von 78 auf 120
Zeilen (240 px), darunter 40 px Anzeigeleiste.

Folgen, die mitgezogen werden mussten:

| Vorlage | hier | Grund |
|---|---|---|
| Ring 128×128 | Ring 128×256 | 120 sichtbar + Vorlauf zum Höhlengraben |
| Scrollschwelle Zeile 40 | Zeile 60 | gleiche relative Lage im Bild |
| Startwert `height = -58` | `-(SCROLL_Y+18) = -78` | das Einpendeln des Schiffs zählt nicht |
| Sperrmauer bei `upperLine == 111` | `(height + SCROLL_Y + 1) & 127 == 0` | dieselbe Absicht, unabhängig von Ringgröße: die Mauer erreicht das Schiff genau beim Zeitbonus |
| Tod bei x ≤ 2 / x ≥ 125 | x ≤ 4 / x ≥ 122 | die Randspalten sind hier nicht mehr im Bild; sonst stirbt das Schiff außerhalb des Sichtbereichs |

Die Höhle selbst ist unverändert: pro Höhenmeter ein Zufallskasten, gleiche
Größen, gleiche Verengung. Nur die Sichtweite nach oben wächst.

## 2. Farbe statt vier Graustufen

Die Vorlage bestimmt die Farbe aus `Zellbyte & 3`. Der Trefferzähler steckt in
Bit 6/7 und ist damit **unsichtbar** — man sieht beim Sprengen nicht, wie weit
man ist. Hier bildet `src/palette.h` das *ganze* Zellbyte ab: angeschlagener
Fels wird heller, die Sperrmauer hat einen eigenen (roten) Ton, Körner leuchten
bernstein. Kostet nichts (eine 256-Einträge-Tabelle), macht das Kernmechanik
sichtbar. Ein dunkles Schema liegt daneben (beim Booten **Hoch** halten).

## 3. Ton — in der Vorlage gibt es keinen

Der PBC+ hat einen echten Mini-Lautsprecher, kein Piezo. Neu:
Schubzischen (gefiltertes Rauschen), Kornaufschläge als Knistern, dessen
Lautstärke aus der Zahl der Treffer je Bild kommt, Zeitbonus-, Sperrmauer-,
Start- und Game-Over-Motive, Sekundenpiep unter 5 s Restzeit.
22050 Hz, PWM auf GP15 (**Kanal B**, obere Hälfte des CC-Registers), DMA im
ENDLESS-Modus mit Lesering, gefüllt aus der Hauptschleife — kein Interrupt.

Weil ein PWM/DMA-Tonpfad beim Start blockieren kann und das dann wie ein
Display-Fehler aussieht: Display wird **vor** dem Ton hochgefahren, die gelbe
LED leuchtet während der Ton-Init und geht danach aus. Beim Booten **B**
halten überspringt den Ton ganz.

## 4. Bedienung

Tastatur → sieben Knöpfe. `Esc` (Pause) liegt auf der Joystick-Mitte,
Schub auf A **und** B. Die Namenseingabe gibt es in der Vorlage nicht, also
war nichts zu streichen. Neu ist eine Sperre von einer Sekunde nach
„game over“ — sonst wischt ein gehaltener Schubknopf das Bild sofort weg.

## 5. Titelbild

Die Vorlage schreibt Text in die einlaufenden Ringzeilen und lässt dabei die
ersten knapp vier Sekunden ein leeres Bild stehen. Hier wird der erste Block
beim Start direkt sichtbar gesetzt. Die vier Blöcke (Titel, Steuerung,
Bestwert, Herkunft) laufen im Abstand von 64 Zeilen durch — bei 120 sichtbaren
Zeilen wären die 32 der Vorlage zu eng. Die Blöcke sind feste Zellen, die Sand
also anhäufen kann, genau wie in der Vorlage.

## 6. Technische Umbauten ohne Spielwirkung

* **Zielhilfe mit Subpixelgenauigkeit.** Die neun Punkte des Düsenstrahls
  laufen in der Vorlage über `x/256` ins Zellraster — bei Zoom 2 werden daraus
  2×2-Klötzchen, und die Bahn springt beim Drehen von Zelle zu Zelle. Die
  Rechnung liegt aber ohnehin in 1/256 Zellen vor; das Runden hat die
  Auflösung nur weggeworfen. Jetzt wandern die Punkte als 1/64-Bildpunkt in
  `spout.dot[]`, und `dots_row()` bestimmt je Bildpunkt die Überlappungsfläche
  eines zwei Bildpunkte breiten Quadrats — ein Kastenfilter von Hand, bei
  höchstens zwölf Punkten je Bild billiger als jede Allgemeinlösung.
  Nebeneffekt: auch die Pulsanimation (`flame_phase`) läuft weich statt in
  Zellsprüngen. Das Schiff bleibt bewusst hart im Zellraster — es markiert die
  Zelle, in der es stirbt, und darf nicht weichgezeichnet sein.
* **Kein Bildpuffer.** Die Vorlage kopiert Ring → `vbuff` (8 Bit) → Fenster.
  Hier erzeugt `render.c` 8-Zeilen-Streifen direkt aus dem Ring; Schiff und
  Düsenstrahl liegen als Overlayliste darüber, statt in einen Puffer gemalt zu
  werden. Spart 134 KB und gibt dem Rest alle ~0,5 ms den Bus frei.
* **`v2g` als 16-Bit-Index** statt Zeigerfeld — 64 KB statt 128 KB, und ein
  fehlender Rücklink (`0xffff`) wird als Wandtreffer behandelt statt als
  Nullzeiger dereferenziert.
* **Eigener Zufall** (xorshift32) statt `rand()`, damit Host und Gerät
  denselben Ablauf erzeugen und ein Fehler reproduzierbar ist.
* **Kein `malloc`** im ganzen Programm; alles statisch. `arm-none-eabi-nm`
  bestätigt, dass keine `malloc`-Symbole gelinkt sind.
* **Schrift: jede Größe einzeln gerastert statt eine kleine hochskaliert.**
  Erster Anlauf hatte einen selbstgezeichneten 5×7-Font, der für Anzeige und
  Titel 2- bzw. 3-fach vergrößert wurde — auf dem Gerät sichtbar klobig.
  `tools/mkfont.py` rastert jetzt fünf Größen einzeln aus DejaVu Sans:

  | Tabelle | Größe | Tiefe | wofür |
  |---|---|---|---|
  | `ui_s` | 12 px | 8 bit Deckung | Beschriftungen, Debugzeile |
  | `ui_m` | 20 px fett | 8 bit | Zahlenwerte, „A = START" |
  | `ui_l` | 27 px fett | 8 bit | PAUSE / GAME OVER |
  | `ui_xl` | 48 px schmal fett | 8 bit | der Schriftzug SPOUT (nur O..U, sonst 80 KB für ein Wort) |

  Die Anzeigeschriften werden zur Laufzeit gegen den *tatsächlichen*
  Hintergrund gemischt (`blend565()` in `src/font.c`), funktionieren also auch
  über dem Spielfeld. Vorschübe sind proportional statt fester Zellbreite.

  **Der Titeltext liegt nicht mehr im Zellring.** In der Vorlage ist er echte
  Spielmaterie, auf der sich Sand häuft. Eine Zelle sind hier aber zwei
  Bildpunkte — feiner wird Text dort nicht, auch nicht mit Farbstufen als
  Ersatz-Kantenglättung. Deshalb wandern die Titelblöcke jetzt als Overlay
  durchs Bild (`spout.banner[]`, gezeichnet in `banners_row()`), mit voller
  8-Bit-Deckung. Preis: der Sand fällt durch die Buchstaben statt sich darauf
  zu legen. Das ist eine bewusste Abweichung zugunsten der Lesbarkeit — auf
  einem 240 px breiten Panel wiegt scharfe Schrift schwerer als ein Detail,
  das die Sandfontäne im Titelbild ohnehin selten trifft.

  Kosten insgesamt: 39 KB Flash (27 → 66 KB), RAM praktisch unverändert.
* **Eigene Sinustabelle**, erzeugt von `tools/mksin.py`.
* **Textausgabe in den Ring wickelt sauber um.** In der Vorlage kann der
  16-Zeilen-Titeltext über das Ende von `vbuff2` hinausschreiben; dass das
  nicht passiert, hängt dort nur an den vier tatsächlich vorkommenden
  Zeilenwerten.
* **Bitfolge zum Panel steht in `src/color565.h`, und nur dort.** MADCTL 0xC8
  setzt das BGR-Bit, der ST7789 liest also Blau aus den oberen 5 Bit und Rot
  aus den unteren — BGR565, nicht RGB565. Erster Anlauf hatte hier normales
  RGB565 und zusätzlich rohe Farbkonstanten in der Testfirmware; auf dem Gerät
  sah man das nicht als „falsch", sondern als vertauschte Farbpaare (Gelb
  wurde Cyan, Rot wurde Blau) — Grau, Weiß und Schwarz stimmen dabei weiter.
  Deshalb: keine `0xF800`-Literale außerhalb dieser Datei. `pbc_rgb24_of()`
  ist die Rückrichtung, damit die PPM-Ausgabe des Host-Tests echte Farben
  zeigt und der Fehler dort auffällt.
* **Bestwert im Flash** bei `0x10FFE000` (vorletzter Sektor, mit Prüfsumme).
  `0x10FFFF00` bleibt frei — dort legt die SDK-UF2 einen Zusatzblock ab.
* **Fehlerbildschirm.** `panic` und `hard_assertion_failure` sind per
  `-Wl,--wrap=` auf ein rotes Bild mit der Adresse für `addr2line` umgebogen,
  dazu ein eigener HardFault-Handler, der den gestapelten PC zeigt. Serielle
  Ausgabe gibt es nicht: UART0 läge auf GP0/GP1 = Joystick CENTER und RIGHT.
  `picotool info` meldet deshalb korrekt „Fixed Pin Information: none“.

## Prüfstand

| geprüft | wie |
|---|---|
| Spiellogik, Renderer, Streifengrenzen | Host-Test, ASan + UBSan, > 300 000 Bilder über sieben Saatwerte, Zufallseingaben und einen Lauf bis Höhe 19 973 |
| Kornbuchhaltung (Zellbit ↔ `v2g` ↔ Liste) | `spout_selfcheck()` nach **jedem** Bild |
| Layout, Farben, Schrift | PPM-Ausgabe des Host-Tests (decodiert BGR565, zeigt also, was das Panel zeigen wird) |
| Umrechnung 0xRRGGBB -> Panelwort | Hin- und Rückrichtung für neun Farben nachgerechnet |
| Firmware baut, Ziel stimmt | warnungsfrei, `picotool info` meldet `rp2350-arm-s`, ARM Secure |
| Stack 4 KB, kein Heap | `arm-none-eabi-nm` |

**Nicht geprüft, weil dafür das Gerät nötig ist:** ST7789-Init, Y-Versatz,
BGR-Bit, Backlight, tatsächliche Bildzeit über SPI, Tastenentprellung,
Tonpfad, Flash-Schreibvorgang. Genau dafür ist `spout_selftest.uf2` da.
