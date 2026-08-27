# Jump 'n Bump für zwei PicoBoy Color Plus (USB-Kopplung)

Portierung des Originalspiels *Jump 'n Bump* (Brainchild Design, 1998) auf zwei
PicoBoy Color Plus (RP2350), die über USB miteinander verbunden werden. Jedes
Gerät zeigt einen eigenen Bildausschnitt und steuert einen Hasen.

Übernommen wurden **Originalgrafik, Originalklänge, Originalmusik und die
Originalmechanik** aus dem GPL-Quelltext von jumpnbump (`main.c`, `network.c`).

## Verkabelung

USB-C ↔ USB-C, **D+/D−/GND verbinden, VBUS NICHT** (beide Geräte sind
eigenversorgt). Gleiches Kabel wie bei der Spielesammlung `i2collection`.

## Bedienung

Beim Start wählt jedes Gerät seine Rolle:

| | |
|---|---|
| **HOST** | rechnet die Spielmechanik, spielt Hase 1 (weiß) |
| **JOIN** | spielt Hase 2 (braun) |

Eines der Geräte auf HOST stellen, das andere auf JOIN — die Reihenfolge des
Einschaltens ist egal.

| Taste | Wirkung |
|---|---|
| Links / Rechts | laufen |
| Hoch, A oder B | springen |
| A/B nach Spielende | neues Spiel (von beiden Seiten auslösbar) |
| A+B lang bei Verbindungsverlust | Neustart ins Menü |

Ziel sind wie im Original **100 Treffer**. Getroffen wird, wer von oben
zerquetscht wird; seitliche Zusammenstöße lassen die Hasen nur abprallen.

## Bild

Das Display ist 240×280, das Original 400×256 (davon 352 Spielfeld).

Gezeichnet wird **immer 1:1 in Originalpixeln**, ohne Skalierung: oben 240×256
Spielfeld. Die volle Levelhöhe passt genau, horizontal wird gescrollt — und
zwar auf jedem Gerät auf den **eigenen** Hasen.

Unten liegen 24 Pixel Punkteleiste in `#606060` mit den beiden Hasenköpfen, den
Original-Steinziffern aus `numbers.pcx` und mittig dem Schriftzug. Weil das
Display abgerundete Ecken hat, bleiben links und rechts je 20 Pixel frei.

Das Startmenü zeigt das Titelbild (`data-src/title.png`, exakt 4:1 auf 240×280
verkleinert) mit einer halbdurchsichtigen weißen Box darauf. HOST und JOIN
stehen in der **Originalschrift des Spiels** (`font.pcx`, 81 Zeichen), zweifach
vergrößert, weiß mit schwarzem Rand; daneben steht jeweils der Hase, den man in
dieser Rolle spielt. Lage und Größe stecken als `MENU_BOX_Y` und `MENU_SCALE`
oben in `src/render.cpp`.

Hinweis zu `data-src/logo.png`: die Datei hat keinen Alphakanal — das Karomuster
des Bildbetrachters ist als Pixel eingebrannt. `tools/convert_assets.py` stellt
die Transparenz wieder her, indem es vom Bildrand über die beiden Karotöne
flutet (die schwarze Kontur der Buchstaben hält die Füllung auf), abgeschnittene
Karo-Inseln über eine Zwei-Pixel-Brücke nachholt und den weichgezeichneten Saum
um zwei Pixel abträgt.

Sprites werden — wie `put_pob()` im Original — nur dort gezeichnet, wo die
Maske 0 ist. Hasen und Fleischfetzen verschwinden dadurch korrekt hinter
Baumstämmen, Grasbüscheln und der Wasseroberfläche.

## Was von der Originalmechanik übernommen ist

Festkommaphysik 16.16 mit den Originalkonstanten, 60 Hz Spieltakt:

* Laufen (Beschleunigung 16384/12288, Höchstgeschwindigkeit 98304), Bremsen mit
  Staubwölkchen
* **Rutschen auf Eis** — nur 768/1024 Beschleunigung, kein Bremsen; der Hase
  gleitet weiter (im Test: 90 Ticks Nachlauf gegenüber 5 auf festem Boden)
* **Schwimmen im Wasser** — Eintauchfontäne ab Fallgeschwindigkeit 32768,
  Auftrieb 1536/Tick, eigene Schwimmanimation, Absprung aus dem Wasser
* Sprungfeder (−400000) samt ihrer Animation — im Originallevel gibt es genau
  eine, in Zeile 14/Spalte 9
* Springen (−280000) mit Abbruch beim Loslassen der Taste, Schwerkraft 12288
* **Blutspur beim Tod**: 6 Fellfetzen (die sich in Flugrichtung drehen, 8
  Stufen über `atan2`) + 30 Fleischstücke, die während des Flugs zu 30 % pro
  Tick eine verblassende Blutspur hinterlassen und beim Liegenbleiben zu 10 %
  einen **dauerhaften Blutfleck** in den Hintergrund brennen
* **Fliegenschwarm**: 20 einzelne schwarze Pixel, die zusammenhalten, den
  Hasen ausweichen, nicht in Wände geraten — mit Dauergeräusch, dessen
  Lautstärke vom Abstand zum nächsten Hasen abhängt (`32 - dist/3`)
* Vier Schmetterlinge (gelb/rosa) mit Zufallsbeschleunigung und Abprallen
* Wiedereinstieg an einer zufälligen begehbaren Stelle mit Mindestabstand

Die Kachelkarte stammt aus `levelmap.txt` des Originals, nicht aus der fest
einkompilierten Tabelle in `main.c` — `read_level()` überschreibt diese zur
Laufzeit, und beide unterscheiden sich (Zeile 15 ist in Wahrheit ganz fest, und
die Sprungfeder existiert nur in `levelmap.txt`).

## Ton

PWM auf GP15 mit 10 Bit Auflösung als Träger (≈146 kHz), gefüttert von einem
DMA-Kanalpaar im Ping-Pong-Betrieb mit 22050 Hz. Nachgefüllt wird im
DMA-Interrupt — dadurch bleibt der Ton auch während der ~16 ms sauber, in denen
das Bild über SPI zum Display geschoben wird.

* **Effekte**: die Original-`.smp`-Dateien (8 Bit vorzeichenbehaftet), gespielt
  mit den Originalfrequenzen und der zufälligen Tonhöhenstreuung `±1000 Hz` wie
  im Original. Acht Mischstimmen, davon eine fest für die Fliegen reserviert.
* **Aussteuerung**: Die Originaldateien sind sehr ungleich ausgesteuert —
  `jump.smp` ist ein Rechteck mit nur ±30, `death.smp` geht bis ±127. Beim Start
  wird je Datei der Spitzenwert gesucht und ein Ausgleichsfaktor gebildet
  (höchstens vierfach), damit alle Effekte gleich laut kommen.
* **Begrenzung**: Statt hart abzuschneiden läuft die Summe oberhalb von 400
  (von 511) in eine Kennlinie, die sich dem Anschlag nur annähert. Dadurch darf
  der Mischpegel hoch liegen, ohne dass bei Musik plus mehreren gleichzeitigen
  Effekten Klirren entsteht — gemessen 0,000 % Übersteuerung.
* **Musik**: eigener 4-Kanal-ProTracker-Player für `bump.mod`. Unterstützt
  Arpeggio, Portamento auf/ab, Tonhöhengleiten, Vibrato, Klangversatz,
  Lautstärkegleiten, Positionssprung, Lautstärke setzen, Musterabbruch,
  Tempo/Geschwindigkeit und die E-Unterbefehle — alles, was `bump.mod`
  tatsächlich benutzt. Blendet wie im Original auf Lautstärke 30 von 64 hoch.

## Aufgabenteilung über USB

Der Host rechnet die **vollständige** Mechanik und schickt je Bild **18 Byte**:
Position, Bildnummer und Zustandsbits beider Hasen, Punktestand, Spielstatus
und einen Todeszähler je Hase. Der Gast schickt 2 Byte (Tasten + Zähler).

Alles Schmückende — Rauch, Fellfetzen, Fleisch, Blutspuren, Wasserfontänen,
Schmetterlinge, Fliegenschwarm — läuft auf beiden Geräten **lokal** und wird
nur durch Ereignisbits im Zustandspaket angestoßen. Das hält das Paket winzig
und macht das Protokoll selbstheilend: ein verlorenes Bild wird durch das
nächste ersetzt, und ein verpasster Tod fällt über den Todeszähler auch später
noch auf.

Rahmenformat, Dual-Role-Umschaltung und die Wiederverbindung bei gestörter
Leitung sind aus `i2collection.cpp` übernommen (SYNC/Typ/Länge/CRC8, RHELLO/RACK,
gestaffelte Neuanmeldung, Watchdog).

### Maßnahmen gegen sporadische Abbrüche

* **Der Mischinterrupt steht unter dem USB-Interrupt** (`irq_set_priority` auf
  `PICO_LOWEST_IRQ_PRIORITY`). Er läuft rund 100 µs am Stück und würde die
  USB-Bedienung sonst genau so lange aufhalten. Der Tonpuffer hat 11,6 ms
  Vorlauf und verträgt jede Unterbrechung — umgekehrt gilt das nicht.
* **Eigene Blindzeit wird nicht der Gegenstelle angelastet.** Lag zwischen zwei
  eigenen Durchläufen mehr als 100 ms, wird diese Spanne der Empfangsuhr
  gutgeschrieben statt als Ausfall gewertet.
* **Kein blindes Warten**: im Verbindungsaufbau wird `linkDelay()` benutzt, das
  den Link weiter bedient; während der ~16 ms Bildübertragung wird viermal
  zwischendurch gepumpt.
* **Kein Umbau aus dem Empfangspfad**: ein Neustart-Paket wird nur vorgemerkt
  und in der Spielschleife ausgeführt, damit der Renderer nie über eine Liste
  läuft, die sich unter ihm ändert.

### Diagnose bei Störung

Im Spiel wird nichts eingeblendet. Nur wenn die Verbindung tatsächlich abreißt,
zeigt das „Reconnecting"-Banner über dem eingefrorenen Bild Messwerte:

```
HOST  why:TIMEOUT  mnt:1     Rolle, Ursache, USB verbunden
F:12043 C:0 txd:0 try:0      Rahmen, Prüfsummenfehler, verworfen, Versuche
st:1 bl:0 max:0              Störungen, eigene Blindzeit gesamt und längste
```

Steigt `C`, sind es Leitungsfehler; steigt `txd`, läuft der Sendepuffer über;
ist `bl` groß, war das Programm selbst zu lange blind.

## Bauen

```
python3 tools/convert_assets.py    # nur nötig, wenn data-src/ geändert wird
pio run                            # -> jumpnbump.uf2 (Projektwurzel)
pio run -t upload                  # Board in BOOTSEL halten
```

`pio run` legt die fertige Firmware nach jedem Build als **`jumpnbump.uf2` in
die Projektwurzel** (siehe `tools/copy_uf2.py`). Dieselbe Datei kommt auf
**beide** Geräte — die Rolle wird erst im Startmenü gewählt.

Verbrauch: 392 kB Flash von 16 MB, 29 kB statisches RAM; zur Laufzeit kommen
134 kB Bildpuffer und 90 kB Hintergrundkopie (für die Blutflecken) dazu.

## Tests auf dem Entwicklungsrechner

```
cd test && make
```

Übersetzt die **echten** Quelldateien aus `src/` gegen Stub-Header und prüft
15 Mechanikpunkte (Landen, Laufgrenze, Sprunghöhe, Wasser, Eis, Sprungfeder,
Tod, Blutspur, Seitenstoß, Fliegen, Falter, Spielfeldrand, Wiedereinstieg),
rendert Kontrollbilder als PNG und die Tonausgabe als WAV.

```
cd test && make shots
```

erzeugt Bildschirmfotos aus dem echten Renderer (`shot1_spiel.png` …
`shot9_maske.png`): normale Sicht, Gastsicht, Tod mit Gedärmen, Blutspur,
dauerhafte Blutflecken, Wasser, Eis, Punkteleiste und die
Vordergrundmaskierung. Mit `python3 crop.py <bild> <ziel> x y b h faktor`
lässt sich ein Ausschnitt vergrößern.

## Dateien

```
platformio.ini            Projekt- und Boardkonfiguration
include/                  Dual-Role-tusb_config (Device + Host gleichzeitig)
patches/                  TinyUSB-Korrekturen, vor jedem Build eingespielt
tools/convert_assets.py   PCX/GOB/SMP/MOD -> C-Header
tools/patch_tinyusb.py    spielt patches/ ins Framework ein
tools/copy_uf2.py         legt jumpnbump.uf2 in die Projektwurzel
jumpnbump.uf2             fertige Firmware fuer BEIDE Geraete
data-src/                 Originaldaten aus jumpnbump (GPL2+)
src/jnb.h                 gemeinsame Definitionen
src/game.cpp              Spielmechanik, Portierung von main.c
src/render.cpp            Scrollkamera, Maskierung, Blutflecken, Punkteleiste
src/audio.cpp             PWM/DMA-Ausgabe, Effektmischer, MOD-Player
src/main.cpp              USB-Link, Menü, Host- und Gastschleife
src/assets_*.{h,cpp}      erzeugt aus data-src/
test/                     Prüfprogramme für den Entwicklungsrechner
```

## Lizenz

Jump 'n Bump ist GPL v2 oder später (siehe `data-src/COPYING`). Grafik, Klänge,
Musik und Spielmechanik stammen daraus, diese Portierung steht damit ebenfalls
unter der GPL v2+.

Copyright (C) 1998 Brainchild Design — <http://brainchilddesign.com/>
Portierungen: Chuck Mason, Florian Schulze, Côme Chilliet u. a.
