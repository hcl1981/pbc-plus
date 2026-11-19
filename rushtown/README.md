# Rushtown

Eine Stadt von oben für den **PicoBoy Color Plus** (RP2350A, 240×280).
Du läufst durch eine Inselstadt, steigst in jedes Auto, fährst Aufträge und
hängst die Polizei ab. Ein eigenes Spiel im Stil der frühen
Draufsicht-Stadtspiele, nichts ist portiert oder nachgebaut: Stadt, Figuren,
Bilder und Ton entstehen im Programm.

```bash
./build.sh              # -> ../dist/rushtown.uf2 (im eigenen Repo: dist/rushtown.uf2)
```

## Zwei Spiele, zwei Sprachen

Im Titelbild wählst du mit hoch/runter **Story** oder **Freies Spiel** und mit
links/rechts **Englisch** oder **Deutsch**. Beide Spiele haben einen eigenen
Spielstand.

- **Story:** zwanzig Missionen mit vier Auftraggebern (Rosa am Platz, Dave am
  Hafen, Lola auf Palm Key, Mr. Gray auf Iron Isle). Wer als Nächstes Arbeit
  hat, steht mit einem türkisen Ring und Fragezeichen an seinem Ort, der
  türkise Pfeil zeigt hin. Zu Fuß hingehen, lesen, mit A annehmen. Die Fähre
  nach Palm Key öffnet mit Mission 10, die Brücke nach Iron Isle mit Mission 14.
  Die Telefonaufträge gibt es nebenher weiter.
- **Freies Spiel:** keine Story, alle Inseln sind von Anfang an offen.

Die Namen der Stadt (Viertel, Schilder, Ränge, Radiosender) bleiben in beiden
Sprachen englisch.

## Bedienung

| | zu Fuß | im Auto |
|---|---|---|
| Joystick | laufen | links/rechts lenken, hoch Gas, runter Bremse |
| A | schießen, wenn Munition da ist; sonst einsteigen | Gas |
| B halten | rennen | Bremse, im Stand rückwärts |
| B antippen | nach einem Taxi pfeifen | hupen |
| A + B | | Handbremse (zum Driften) |
| Mitte kurz | einsteigen | aussteigen |
| Mitte halten | Karte und Menü | Karte und Menü |

Beim Einschalten **B** gehalten: Start ohne Ton.

## Was es zu tun gibt

- **Aufträge** gibt es an den zwölf blauen Telefonen (gelber Ring, der Pfeil am
  Bildrand zeigt zum nächsten). Draufstellen genügt. Neun Arten, die mit dem
  Rang dazukommen:

  | Rang (verdientes Geld) | neue Aufträge |
  |---|---|
  | Rookie | Paket holen und abliefern; Fahrgast abholen und hinbringen |
  | Driver (400 $) | sechs Kontrollpunkte gegen die Uhr; vier Abgaben in beliebiger Reihenfolge |
  | Wheelman (1500 $) | den rosa Renner stehlen und zur Lackiererei bringen; einen Lieferwagen heil abliefern |
  | Outlaw (4000 $) | die Polizei abhängen; Komplizen abholen und mit drei Sternen ins Versteck; einer schwarzen Limousine folgen, ohne aufzufallen |
  | Kingpin (10 000 $) | |

- **Zu kaufen** (zu Fuß auf das grüne Dollarzeichen stellen):
  die **Garage** für 1500 $ – Autos, die in ihrem Hof stehen, bleiben dort,
  auch über das Ausschalten hinweg (bis zu vier); **Munition** im Waffenladen,
  24 Schuss für 100 $; der **Tower** für 15 000 $ – dann gehört dir die Stadt.
- **Polizei:** Wer Leute umfährt, Autos rammt, schießt, jemanden aus dem
  Wagen wirft oder vor den Augen einer Streife bei Rot fährt, sammelt Sterne
  (bis vier). Streifenwagen jagen dich; stehst du oder bist du zu Fuß, steigt
  ein Polizist aus und kommt gelaufen – erreicht er dich, bist du gefasst. Ab
  drei Sternen schießen sie und stellen Wagen quer auf die Kreuzung, auf die
  du zufährst. Außer Sicht verblassen die Sterne nach einigen Sekunden.
  Gefasst heißt: Start an der Wache, ein Viertel des Geldes als Kaution
  (höchstens 1000 $), die Waffe und der Wagen sind weg, und der laufende
  Auftrag ist verloren. Eine Story-Mission bleibt, zahlt aber mit jeder
  Festnahme ein Viertel weniger. Ein Leben kostet das nicht.
- **Leben:** Fünf (oben in der Mitte). K.O. gehen oder ins Wasser fahren
  kostet eines und ein Zehntel des Geldes. Sind alle weg, ist das Spiel zu
  Ende: Geld und Waffe sind verloren, und die Story geht an den Anfang ihres
  Kapitels zurück (Rosa, Dave, Lola, Mr. Gray). Was dir gehört, die Garage
  und der Rang bleiben. Fünf neue Leben gibt es mit jedem neuen Kapitel, im
  freien Spiel mit jedem neuen Rang.
- **Lackiererei** (zwei, auf der Karte rosa): mit dem Auto langsam auf das
  gelb-schwarze Feld. Für 50 $ gibt es eine neue Farbe, die Beulen sind raus
  und die Sterne weg – aber nur, wenn kein Verfolger in der Nähe ist.
- **Taxi:** Antippen von B zu Fuß ruft das nächste Taxi. Es kommt, hält, und
  mit Mitte fährst du für ein paar Dollar zum Ziel des Auftrags oder, ohne
  Auftrag, zum nächsten Telefon.
- **Fahrzeuge:** Kleinwagen, Limousine, Sportwagen, Lieferwagen, Taxi,
  Streifenwagen, Bus, Lastwagen und Motorrad – alle fahrbar, mit eigenem
  Gewicht und Fahrverhalten (der Bus schiebt alles weg, das Motorrad ist
  schnell und zerbrechlich). Rettungswagen kommen zu Umgefahrenen,
  Abschleppwagen holen Wracks.
- **32 Chips** liegen abseits der Straßen, auf allen drei Inseln. Jeder bringt 50 $, der letzte 1050 $.
- Zu Fuß hast du drei Herzen; ein schnelles Auto oder eine Kugel kostet
  eines, in Ruhe kommen sie zurück. Wasser ist das Ende der Fahrt.

Das Spiel ist nicht für die Schule gedacht: Es wird geschossen, und wer
getroffen wird, bleibt liegen. Wer langsam angefahren wird, fällt hin und
steht wieder auf; wer schnell erwischt oder am Boden überrollt wird, nicht. Geld, Rang, Chips, Besitz, Munition, die
Garage und der Stand der Story werden gespeichert, je Spiel getrennt.

## Aufbau

Das Spiel selbst kennt die Hardware nicht. Es zeichnet in ein Bild aus
240 × 280 Panel-Wörtern und bekommt 50-mal je Sekunde die Tasten.

```
src/rt.h        was alle Teile teilen
src/world.c     die Stadt: Insel, Straßennetz, Fluss, Viertel, Gebäude
src/art.c       alle Bilder: Bodenkacheln, Autos, Figuren, Baumkronen
src/game.c      Regeln: Fahren, Verkehr, Fußgänger, Polizei, Aufträge
src/draw.c      ein Bild: Boden, Schatten, Figuren, Dächer, Anzeige, Karte
src/gfx.c       Zeichnen ins Bild, gedrehte Sprites, Schrift
src/snd.c       Ton: Motor, Reifen, Sirene, zwei Kanäle für Effekte, Sprachrufe
src/story.c     die Story: zwanzig Missionen als Listen von Schritten, zweisprachig
src/music.c     das Autoradio
src/speech_data.c  die Sprachrufe (erzeugt)
src/pbc/        PicoBoy Color Plus: Display, Tasten, Ton, Speichern, main
tools/host.c    dasselbe Spiel auf dem PC, ohne Fenster: Tests und Bilder
```

**Die Welt** entsteht beim Start aus einer festen Zahl, ist also immer
dieselbe: 160 × 150 Kacheln zu 16 Pixeln mit drei Inseln. Die Hauptinsel hat
einen Ring und ein Gitter zweispuriger Straßen in ungleichen Abständen
(einzelne Stücke fehlen und sind Fußgängerzone), einen Fluss mit vier Brücken
und sechs Viertel; zur Orientierung die **Hauptstraße** mit Parkstreifen, den
großen **Platz** mit Brunnen im Norden und das **Hafenbecken** im Südosten.
**Iron Isle** im Osten (Werk, Hallen, Containerstapel) hängt an einer langen
Brücke, **Palm Key** im Süden (Häuser, Gärten, Lolas Bar) erreicht die Fähre:
auf das Ende des Stegs gehen oder fahren, 10 $, das Auto kommt mit. Derselbe
Code baut alle drei Inseln. Die Kreuzungen bilden einen Graphen; der Verkehr
fährt auf ihm rechts und hält an **Ampeln** (an jeder Kreuzung mit drei oder
vier Straßen, 14 Sekunden je Umlauf). Wer ein Ziel hat – Polizei,
Rettungswagen, Abschleppwagen, gerufenes Taxi – nimmt an jeder Kreuzung die
Straße, die näher heranführt, und lenkt direkt darauf zu, sobald es in Sicht
ist. Telefonaufträge bleiben auf der Insel, auf der sie beginnen.

**Autos** haben eine Geschwindigkeit in der Ebene. Sie wird je Schritt in
einen Anteil längs und einen quer zum Wagen zerlegt; der Motor wirkt längs,
die Reifen nehmen den Queranteil weg – mit Handbremse viel langsamer, und
schon rutscht das Heck. Gegen Häuser gleiten sie an der Wand entlang, weil
beide Achsen einzeln geprüft werden.

**Schaden** merkt sich jedes Auto für Front, Heck und beide Seiten getrennt,
je nachdem, womit es angestoßen ist. Man sieht ihn in drei Stufen: Beulen,
ein eingedrücktes Ende mit kaputten Lichtern, ein Wrack dieser Seite mit
gesprungenen Scheiben. Aufs Fahren wirkt er absichtlich wenig: Erst ein
zerstörtes Ende kostet ein Zehntel Tempo (Front) oder Anzug (Heck), eine
verbogene Seite lässt den Wagen leicht dorthin ziehen. Die Lackiererei
richtet alles.

**Das Bild:** Zwei ganze Bilder liegen im RAM (268 KB). Eines wird
gezeichnet, während DMA das andere in 17 ms zum Panel schickt. Der Takt
ist 125 MHz, weil die 62,5 MHz des Panels dann genau die Hälfte sind. Im
Emulator PBConPC läuft das mit 50 Bildern je Sekunde bei rund 83 % Last auf
einem Kern; der zweite Kern ist frei. Dauert ein Bild einmal zu lange, holt
das Spiel die Schritte nach: Es wird dann ruckeliger, nicht langsamer.

## Was sich wieder weglassen lässt

Fünf Teile haben je einen Schalter; ohne sie ist das Spiel das, was es vorher
war. Beim Bauen:

```bash
cmake -S . -B build -DRT_STORY=0      # oder RT_ISLANDS, RT_MUSIC, RT_SPEECH, RT_ART
```

| Schalter | Teil | Dateien |
|---|---|---|
| `RT_STORY` | die Story; ohne sie gibt es nur das freie Spiel | `src/story.c` (wird von `game.c` eingebunden) |
| `RT_ISLANDS` | Iron Isle, Palm Key, Brücke und Fähre; ohne sie ist die Welt 96 × 96 Kacheln | in `src/world.c` |
| `RT_MUSIC` | das Autoradio: sechs Sender als Chip-Musik und der Polizeifunk. Jede Fahrzeugart ist auf einen Sender eingestellt (Motorrad: keiner, Streifen- und Rettungswagen: Funk); im Menü umschaltbar, bis man umsteigt | `src/music.c` |
| `RT_SPEECH` | gesprochene Rufe in beiden Sprachen (Polizei, Passanten, Taxi), 97 KB | `src/speech_data.c`, erzeugt von `tools/speech.py` mit `espeak` |
| `RT_ART` | die Bilder der Story: vier Porträts im Briefing, neun Szenen an den Wendepunkten, „gefasst“ und „K.O.“, 820 KB | `src/pics.bin` und `src/pics.S`, erzeugt von `tools/art.py` aus `art/sheet.jpeg` |

`tools/test.sh` baut jede dieser Fassungen einmal durch.

## Testen ohne Gerät

```bash
tools/host.sh                              # baut build-host/host
build-host/host bigmap stadt.ppm           # die ganze Stadt als Bild
build-host/host w10 +A w2 -A w100 sbild.ppm p    # spielen, Bild, Zustand
tools/test.sh                              # die Abläufe unten
```

`tools/test.sh` spielt die neun Aufträge und alle zwanzig Story-Missionen
durch (die Missionen mit Zauberei statt mit Fahren), lässt sich fassen, kauft
Garage, Tower und Munition, schießt, nimmt die Fähre, baut jede Fassung ohne
einen ihrer Teile und lässt den Verkehr drei Minuten laufen.

## Stand

Version 0.1, eine frühe Fassung. Geprüft mit `tools/test.sh` und als Firmware
im Emulator PBConPC; auf einem echten PBC+ läuft sie und ist angespielt, aber
nicht durchgespielt: Ob alle Missionen fahrbar und fair sind, ist offen.

## Die Bilder

`art/sheet.jpeg` ist ein Bogen aus sechzehn Bildern, erzeugt mit Gemini nach
einer Beschreibung aus diesem Projekt (Figuren, Szenen, Stil). `tools/art.py`
schneidet ihn auseinander und bringt jedes Bild auf seine Größe im Spiel und
auf 256 Farben; `art/preview.png` zeigt das Ergebnis. Das ganze Storybuch mit
allen Texten und Bildschirmen steht in `doc/Rushtown-Story.pdf`
(`tools/storybook.py`).

## Lizenz

MIT, siehe `LICENSE`. Die 8×8-Schrift (`src/font8x8.h`) ist gemeinfrei.
