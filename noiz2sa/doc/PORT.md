# Was gegenüber noiz2sa 0.52 geändert wurde — und warum

Grundsatz wie beim Spout-Port: die Vorlage so weit wie möglich in Ruhe lassen.
Bewegung, Trefferprüfung, Punktevergabe, Bonuslogik, Szenenaufbau und die
Verlangsamung bei dichtem Sperrfeuer sind Zug um Zug übernommen.

## 1. BulletML ohne XML und ohne C++ — der größte Eingriff

Die Vorlage lädt beim Start 73 XML-Dateien und wertet sie mit **libBulletML**
aus: C++, eigener XML-Parser, dynamischer Speicher, `double` überall. Nichts
davon ist auf dem Gerät sinnvoll — und nötig ist es auch nicht, weil sich die
Muster nach dem Bauen nie mehr ändern.

Stattdessen:

* `tools/mkbml.py` übersetzt `data_bml_src/**.xml` in flache Tabellen
  (`src/bml_data.c`): Aktionen als Knotenlisten, Ausdrücke (`$rank`, `$rand`,
  `$1..$9`, Grundrechenarten, Klammern) in umgekehrter polnischer Notation,
  alles in 16.16-Festkomma. Ergebnis: **27 KB Flash** für alle 73 Muster.
* `src/bml.c` führt das aus und bildet das Verhalten von `BulletMLRunner`
  nach: je Tick zuerst die laufenden Änderungen (`changeDirection`,
  `changeSpeed`, `accel`) fortschreiben, dann den Aktionsstapel bis zum
  nächsten `<wait>`.

Ein Läufer entspricht genau **einer** Einsprungaktion. Ein Gegner mit mehreren
`top*`-Aktionen bekommt mehrere Läufer — libBulletML macht das mit mehreren
Runner-Instanzen genauso.

Feinheiten, die stimmen müssen und geprüft sind:

* Vorgaben nach DTD: `direction` ist ohne `type` **aim**, `speed` ist
  **absolute**. Fehlt beides, erbt das Geschoss Richtung des Schützen und
  Geschwindigkeit 1.
* Geltungsbereiche: `<fireRef>`-Parameter gelten für den Feuerbefehl,
  `<bulletRef>`-Parameter für die Geschossvorlage **und** deren Aktion, ein
  `<actionRef>` darin bringt wieder eigene mit. Eingebettete `<action>` erben
  den Bereich des Aufrufers.
* Rekursive und vorwärts verweisende Labels: der Index wird **vor** dem
  Übersetzen vergeben, sonst hängt sich eine Aktion auf, die sich selbst
  aufruft.
* `type="sequence"` addiert je Aufruf auf den vorigen Wert, bei
  `changeDirection` pro Bild; `absolute`/`aim` nehmen den kürzesten Winkelweg.

**Grenze:** 256 gleichzeitige Läufer (46 KB RAM). Bekommt ein Geschoss mit
eigener Aktion keinen Läufer mehr, fliegt es als einfaches Geschoss weiter,
statt zu verschwinden. Über alle 73 Muster stößt genau eines an diese Grenze.

## 2. Bildformat

Die Vorlage zeigt ein Spielfeld von 320×480 mit je 160 px Blende links und
rechts. Das Panel ist hochkant 240×280.

Die **Breite bleibt bei 320** und wird beim Zeichnen mit 3/4 verkleinert. Damit
stimmen sämtliche Konstanten der Vorlage weiter, vor allem die
Geschossgeschwindigkeiten, und der seitliche Ausweichraum ist identisch. Die
Höhe folgt daraus: 280/0,75 = **373** statt 480. Nach oben gibt es also rund
22 % weniger Vorwarnung — das ist die spürbarste Abweichung.

Für die seitlichen Blenden ist kein Platz. Die Anzeige liegt deshalb **über**
dem Feld und ist durchscheinend (`plate()` in `render.c`), sonst verschwinden
Geschosse und Schiff darunter.

## 3. Weißer Grund — und wie ich ihn zuerst verfehlt habe

Noiz2sa spielt auf **weißem** Grund: `clearScreen()` füllt mit Index 0, und
Index 0 der Palette der Vorlage ist `{255,255,255}`. Innerhalb jeder
Sechzehnergruppe wird die Farbe mit steigendem Index *dunkler*. Alles Bewegte
ist also dunkle Tinte auf Weiß, die Hintergrundbretter sind sehr helle
Grautöne (Index 1 und 3).

Der erste Anlauf hatte das genau umgekehrt: schwarzer Grund, leuchtende
Objekte. Jetzt ist Stufe 0 reines Weiß und die Farbe wird mit der Stufe
kräftiger — was zugleich zum Abklingschritt passt: Spuren verblassen nach
Weiß, nicht nach Schwarz.

Die Mischkurve ist **linear**. Eine quadratische Kurve sieht bei
Leuchteffekten gut aus, drückt hier aber genau die Stufen, mit denen die
Vorlage ihre Hintergrundbretter zeichnet (12 bis 25 Prozent Deckung),
unsichtbar ins Weiß.

## 4. Zwei Ebenen statt Mischtabelle

Die Vorlage führt zwei 8-Bit-Ebenen, mischt sie über eine 64-KB-Tabelle und
lässt sie über eine Diffusionstabelle ausklingen — pro Bild mehrere
100 000 Tabellenzugriffe. Hier:

* `fb_glow` glimmt nach: je Bild fällt die Helligkeit um eine Stufe (vier
  Bildpunkte je Wort, ohne Verzweigung). Darin liegen Hintergrund, Gegner,
  Geschossspuren, Splitter und Bonus.
* `fb_top` wird jedes Bild geleert und trägt scharf Schüsse, Schiff und
  Geschosse.

Das entspricht der Vorlage, die genau diese Objekte in den fertigen Puffer
zeichnet statt in die nachglimmenden Ebenen — ohne das ziehen die Schüsse eine
durchgehende Leuchtspur.

Der Farbindex ist wie dort `Gruppe*16 + Helligkeit`, nur steigt die Helligkeit
mit dem Index: das braucht der Abklingschritt.

## 5. Gekürzt

* **Hintergrund:** bis zu 128 Bretter, jedes bis zu 16-fach gekachelt, sind
  über 2000 Kästen je Bild. Gedeckelt auf 64 Bretter und 2×2 (Stufe 3: 3×3)
  Kachelungen; das Prinzip der Parallaxe über einen z-Teiler bleibt.
* **Musik:** die Vorlage spielt Ogg-Vorbis über SDL_mixer. Dafür gibt es hier
  weder Platz noch Decoder. Ersatz ist ein Grundrauschen, dessen Lautstärke der
  Zahl der Geschosse im Bild folgt — die Dichte des Vorhangs wird hörbar —
  dazu Motive für Treffer, Bonus, Bossabschuss, Extraleben und Tod.
* **Grenzen:** 640 Gegner/Geschosse statt 1024, 160 Splitter, 64 Bretter.
  Im härtesten gemessenen Lauf waren 282 Geschosse gleichzeitig im Bild.

## 6. Bedienung: kein Pausenknopf

Im Spiel sind Joystick (Bewegung), **B** (Feuer) und **A** (langsam) dauernd in
Gebrauch. Die Joystickmitte war zunächst die Pause, ist aber die einzige Taste,
die beim Ausweichen versehentlich auslösen kann — eine Pause mitten im
Kugelvorhang ist schlimmer als keine. Auch jede Kombination *mit* der Mitte
erbt dieses Problem, und A oder B sind ohnehin gehalten.

Deshalb gibt es keine Pause. Wer sie zurück will: ein `if (pad & TRG_x)` im
Zweig `ST_GAME` von `noiz_tick()` genügt.

## 7. Schrift: Segmentanzeige statt Bitmap

Die Vorlage zeichnet ihren Text als Strichsegmente (`letterrender.c`), nicht
als Bitmap — daher der Taschenrechner-Look. `src/segfont.c` macht dasselbe:
je Zeichen eine Maske aus 16 Segmenten plus zwei Punkten, in beliebiger Größe
gezeichnet. Waagerechte und senkrechte Balken bekommen harte Kanten, so sieht
eine Anzeige aus; die vier Schrägen werden kantengeglättet, sonst franst das X
aus.

V und Y sehen gleich aus — beide laufen in der Mittelsäule zusammen. Auf einer
echten Sechzehn-Segment-Anzeige ist das nicht anders, und in den Texten des
Spiels kommt kein V vor.

Nebeneffekt: die Schrift kostet statt 74 KB Bitmapdaten nur noch eine Tabelle
mit 59 Masken. Der Flashbedarf fällt von 139 KB auf 65 KB.

## 8. Sonstige Umbauten

* **Kein `malloc`** im ganzen Programm; `arm-none-eabi-nm` bestätigt, dass
  keine `malloc`-Symbole gelinkt sind. Die Vorlage legt Runner mit `new` an.
* **Kein `double`.** Ränge, Winkel und Geschwindigkeiten laufen in
  16.16-Festkomma, Positionen wie in der Vorlage in 8.8.
* **Eigener Zufall** (xorshift32) statt `rand()`, damit Host und Gerät
  denselben Ablauf erzeugen.
* **Undefiniertes Verhalten behoben:** die Vorlage bildet die Geschossspur mit
  `mx << wl`; bei negativer Bewegung ist Linksschieben in C undefiniert. Hier
  wird multipliziert. UBSan hat das im Host-Test gemeldet.
* **Fehlerbildschirm** wie beim Spout-Port: `panic` und
  `hard_assertion_failure` per `-Wl,--wrap=` auf ein rotes Bild mit der Adresse
  für `addr2line`, dazu ein HardFault-Handler. Serielle Ausgabe gibt es nicht,
  UART0 läge auf GP0/GP1 = Joystick CENTER und RIGHT.

## Prüfstand

| geprüft | wie |
|---|---|
| Übersetzer und Ausführung von BulletML | `tools/bml_test` spielt **alle 73 Muster** je 2400 Ticks durch, mit Geschossen, die selbst Läufer bekommen; ASan + UBSan |
| benötigte Verschachtelungstiefe | aus den übersetzten Tabellen ausgerechnet: 4, vorgehalten 6 |
| Spiellogik, Renderer, Streifengrenzen | `tools/host_test`, ASan + UBSan, > 100 000 Bilder über mehrere Saatwerte und Zufallseingaben |
| Buchhaltung (Gegnerzähler, Läufer, Grenzen) | `noiz_selfcheck()` nach **jedem** Bild |
| Layout, Farben, Schrift | PPM-Ausgabe des Host-Tests; alle 59 Glyphen einzeln gegengelesen |
| Firmware baut, Ziel stimmt | warnungsfrei, `picotool info`: `rp2350-arm-s`, ARM Secure, Fixed Pins `none` |
| Stack 4 KB, kein Heap | `arm-none-eabi-nm` |

**Nicht geprüft, weil dafür das Gerät nötig ist:** ST7789-Init, Y-Versatz,
BGR-Bit, Backlight, tatsächliche Bildzeit über SPI, Tastenentprellung,
Tonpfad, Flash-Schreibvorgang — und ob das Spiel bei diesem Feldformat noch
gut spielbar ist. Dafür gab es während der Portierung eine eigene
Testfirmware ohne Spiel, deren fünfte Seite die Bildzeit bei laufendem
Kugelvorhang maß; sie wird nicht mehr mitgeliefert.
