# OpenTyrian für den PicoBoy Color Plus

Portierung von [OpenTyrian](https://github.com/opentyrian/opentyrian) auf den
PicoBoy Color Plus (RP2350, 512 KB RAM, 16 MB Flash, ST7789 240×280, Piezo).
Einzelspieler und Zweispieler über ein USB-C-Kabel zwischen zwei Geräten.

**Stand: baut vollständig, `tyrian.uf2` liegt vor.** 229 KB Firmware, 468 KB
statisches RAM von 512 KB, rund 41 KB Heap. **Auf Hardware gelaufen ist noch
nichts** — was als Nächstes zu prüfen ist, steht unten.

---

## Bauen

```bash
# Firmware
cmake -B build -DPICO_SDK_PATH=$HOME/pico/pico-sdk
cmake --build build -j

# Spieldaten (einmalig; braucht die Tyrian-2.1-Daten)
python3 tools/mkdata.py ../data/tyrian21 -o ../build-out/tyrian-data.uf2

# beides zu einer Datei
python3 tools/merge_uf2.py build/tyrian.uf2 ../build-out/tyrian-data.uf2 \
                          -o ../build-out/tyrian-komplett.uf2
```

Flashen: Gerät mit gedrückter BOOTSEL anstecken, `tyrian-komplett.uf2` auf das
Laufwerk kopieren. Fertig.

Wer öfter an der Firmware arbeitet, kopiert nur `build/tyrian.uf2` — das Daten-
archiv liegt an einer anderen Flash-Adresse und bleibt dabei unangetastet.

Die Aufteilung im Flash:

```
0x10000000  Firmware        269 KB
0x10100000  Datenarchiv    8,86 MB
0x10FF8000  Spielstände      16 KB
0x10FFFF00  SDK-Zusatzblock 256 B   ← muss frei bleiben, siehe unten
```

**Zur Numerierung, das ist die heikle Stelle:** Jeder UF2-Block trägt außer
seiner Zieladresse die Angabe „Block i von N", und das Bootrom erkennt daran
das Ende der Übertragung — davon hängt ab, ob das Gerät danach von selbst
neu startet. Diese Zählung läuft **je Familienkennung getrennt**, nicht über
die Datei: die vom SDK erzeugte Firmware-UF2 enthält bereits zwei Familien mit
je eigener Zählung.

Solange also keine Familie in zwei Eingabedateien vorkommt, ist schlichtes
Aneinanderhängen bereits richtig. Wer stattdessen global durchnumeriert,
zerstört beide Sequenzen — die Datei wird trotzdem geschrieben, aber das
Bootrom erkennt das Ende nicht mehr und startet nicht neu. `merge_uf2.py`
lässt die Numerierung deshalb in Ruhe und zählt nur dann neu, wenn dieselbe
Familie aus mehreren Dateien kommt.

Prüfen lässt sich das Ergebnis mit `picotool info` — es muss die Datei als
`rp2350-arm-s` mit gültigem Abbild erkennen, genau wie die Firmware allein.

Außerdem prüft das Werkzeug auf überschneidende Adressbereiche. Das ist hier
keine theoretische Sorge: die Firmware-UF2 beschreibt nicht nur den
Flash-Anfang, sondern zusätzlich einen einzelnen Block ganz am Ende
(`0x10FFFF00`). Der Spielstandsbereich lag dort zunächst mitten drin und hätte
ihn bei jedem Speichern gelöscht.

Die Tyrian-2.1-Daten sind Freeware und liegen z. B. unter
`https://camanis.net/tyrian/tyrian21.zip`.

---

## Bedienung

Der PicoBoy hat ein Steuerkreuz und zwei Knöpfe; Tyrian erwartet acht Tasten
plus ESC. Zwei Kombinationen schließen die Lücke — dieselbe Lösung, die die
Doom-Portierung auf demselben Gerät benutzt.

| | Im Spiel | Im Menü |
|---|---|---|
| Steuerkreuz | fliegen | navigieren |
| **B** | Feuer | zurück |
| **A** | Waffenmodus wechseln | bestätigen |
| **Mitte** | linker Begleiter | bestätigen |
| **Mitte + B** | rechter Begleiter | — |
| **Mitte + A** | **Menü (ESC)** | — |

---

## Bildaufteilung

Tyrian rechnet in 320×200. Darin steckt ein Spielfeld von 264×184, rechts eine
56 Pixel breite Statusspalte, unten ein schmaler Streifen. Das Panel ist 240×280
— hochkant, also genau falsch herum für 320×200 und genau richtig für einen
senkrecht scrollenden Shooter.

```
┌────────────────────────┐ 0
│                        │
│  Spielfeld 240 × 184   │   Originalpixel, nicht skaliert.
│  (240 der 264 Spalten) │   Die Kamera verschiebt sich im
│                        │   Rahmen der übrigen 24 Spalten
│                        │   mit dem Schiff.
├────────────────────────┤ 184
│  Anzeigeleiste 240×96  │   neu gebaut, aber mit Tyrians
│  Schild · Panzerung    │   eigenen Zeichenroutinen und
│  Waffen · Geld · Link  │   in derselben Palette
└────────────────────────┘ 280
```

In Menüs, im Laden und in Zwischenbildern wird stattdessen das **ganze**
320×200-Bild auf 240×150 verkleinert (jede vierte Spalte und Zeile fällt weg,
320→240 und 200→150 sind beide genau ¾). Dort trägt jede Ecke Inhalt;
abzuschneiden hieße, Text zu verlieren.

Umschalten: `pbc_set_view_mode()` in `platform/pbc_video.h`.

---

## Multiplayer

Zwei Geräte, ein USB-C-Kabel (D+/D−/GND durchverbunden, **VBUS nicht**), jedes
mit eigenem Bild.

Der entscheidende Befund: **OpenTyrians Mehrspielermodus ist deterministischer
Gleichschritt.** Beide Geräte rechnen dieselbe Simulation und tauschen je Bild
nur ein 28-Byte-Paket mit den Tasteneingaben aus. Es werden keine Objektlisten
übertragen — das wäre über diese Leitung aussichtslos, 28 Byte sind mühelos.

Damit bleibt `network.c` **unverändert**. Getauscht wurde nur der Transport:

```
usblink.c          Bits über D+/D− (aus der Doom-Portierung übernommen)
pbc_link.c         Austausche anstoßen, zwei Paketwarteschlangen
SDL_net-Attrappe   SDLNet_UDP_Send/Recv legen hinein bzw. holen heraus
network.c          OpenTyrians Netzcode, unangetastet
```

Dass OpenTyrians eigene Absicherung erhalten bleibt (Quittungen,
Wiederholungen, das XOR-Ersatzpaket gegen Verluste), ist dabei kein Nebeneffekt
— über diese Leitung braucht man sie mehr als in einem LAN.

Rollen werden im Menü gewählt: *Host Game* → Master (taktet), *Join Game* →
Slave. Beide Geräte tragen dieselbe Firmware.

Der Zustand des Links steht dauerhaft in der Anzeigeleiste. Das ist Absicht:
während eines Netzspiels gehört der USB-Port dem Link, die serielle
Schnittstelle fällt als Diagnoseweg also aus.

---

## Aufbau

```
src/          OpenTyrian-Upstream, bis auf drei Stellen unverändert → PATCHES.md
platform/
  fakesdl/    SDL- und SDL_net-Attrappen (stehen im Include-Pfad vor echtem SDL)
  pbc_config.h    Pins, SPI-Takt, Bildaufteilung
  pbc_display.c   ST7789: Init aus der Doom-Portierung, Ausgabe streifenweise
  pbc_video.c     Ersatz für video.c: Zeichenflächen + Compositor
  pbc_hud.c       Anzeigeleiste unter dem Spielfeld
  pbc_input.c     sieben GPIOs als Tastatur (ersetzt nur SDL_PollEvent)
  pbc_audio.c     PWM + DMA am Piezo (ersetzt nur das SDL-Audiogerät)
  pbc_link.c      Multiplayer-Transport + SDL_net-Attrappe
  usblink.c       Bit-Banging auf D+/D− (unverändert aus picoboy-doom)
  xipfs.c         Flash-Archiv + stdio-Ersatz
  pbc_main.c      Startpunkt, Hardware-Aufbau
tools/mkdata.py   Tyrian-Daten → Flash-Archiv → UF2
```

Zwei Entwurfsentscheidungen prägen alles andere:

**Kein Bildpuffer im RAM.** Das Bild entsteht in Streifen zu acht Zeilen und
wird sofort per DMA weitergeschoben. Ein 240×280-RGB565-Bild wären 134 KB, die
neben Tyrians drei 320×200-Puffern nicht mehr hineinpassen; streifenweise
kostet dasselbe Ergebnis 7,7 KB. Nebeneffekt, der sich als wichtiger erweist:
ein Vollbild belegt den SPI-Bus 17 ms am Stück, und so lange darf der
Multiplayer-Link nicht unbedient bleiben. Zwischen zwei Streifen liegt alle
~0,5 ms eine Gelegenheit dafür.

**Kein Dateisystem.** Die Spieldaten liegen als ein Archiv im Flash, und Flash
ist ab `0x10000000` in den Adressraum eingeblendet. Eine Datei zu „öffnen" ist
deshalb ein Zeiger, kein Lesevorgang — es wird nichts ins RAM kopiert.

---

## Was noch offen ist

Alles baut. Was fehlt, ist der erste Lauf auf echter Hardware — und der wird
Dinge zeigen, die sich ohne Gerät nicht klären lassen.

### Zuerst prüfen

1. **Bild überhaupt da?** Wenn der Schirm schwarz bleibt, zuerst das
   Hintergrundlicht verdächtigen (GP26, muss per PWM an) und dann die
   SPI-Pins — beides sind die Stellen, an denen die Doom-Portierung auf
   derselben Hardware hängengeblieben ist.
2. **Farben richtig herum?** MADCTL steht auf `0xC8` (BGR, 180° gedreht) und
   der Panelversatz auf 20 Zeilen. Beides aus der laufenden Doom-Portierung
   übernommen, aber ungeprüft für diesen Bildausschnitt.
3. **Bildrate.** Das Spielfeld sind 240×184 in RGB565, also 88 KB über SPI je
   Bild — rechnerisch 11 ms bei 62,5 MHz, dazu die Umrechnung. Die
   Anzeigeleiste wird nur geschoben, wenn sie sich geändert hat. Reicht es
   nicht: die Musik ist bereits aus (siehe unten), der nächste Hebel wäre die
   Bildrate selbst.
4. **Ton.** PWM-Träger 146 kHz an GP15, Mischer mit 11025 Hz. Wenn nichts
   kommt: die DMA-Zieladresse trifft die obere Hälfte des
   PWM-Vergleichsregisters (GP15 ist Kanal B) — das falsch zu treffen ergibt
   einen stummen Ausgang.
5. **USB-Link.** Bleibt der Multiplayer stumm, liegt es oft am Kabel: viele
   USB-C-Kabel sind reine Ladekabel und führen D+/D− gar nicht. Vor der
   Fehlersuche im Spiel also erst das Kabel gegen ein bekannt gutes Datenkabel
   tauschen.

### Bekannte Grenzen

* **Musik ist aus.** Die OPL-Emulation bildet den Klangchip in Software nach
  und rechnet je Ausgabewert — sie ist der größte Rechenzeitposten der
  Tonausgabe und läuft in einer Unterbrechung. Klänge (Schüsse, Explosionen)
  bleiben erhalten. Einschalten mit `cmake -B build -DPBC_MUSIC=ON`.

* **Die Schlussanimation wird ausgelassen.** Sie braucht 128 KB Puffer, die es
  nicht gibt; `animlib.c` bricht sauber ab, statt abzustürzen. Mit
  `tools/mkdata.py --no-ending` lassen sich die 3,3 MB im Flash zurückgewinnen.
* **Keine Namenseingabe im Netzspiel.** Feste Namen P1/P2 — mit sieben Knöpfen
  ist eine Eingabe nicht zu bedienen, und kurze Namen halten das
  Verbindungspaket unter den 36 Byte, die über den Link passen.
* **Kein Joystick, keine Maus.** Die Menüpunkte dafür bleiben wirkungslos.
* **Heap ist knapp (41 KB).** Wenn beim Laden etwas fehlschlägt, ist das der
  erste Verdacht. Weiterer Spielraum wäre bei `cube` (13 KB), `helpTxt` (9 KB)
  und `shipInfo` (7 KB) zu holen — alle drei werden entschlüsselt bzw. geparst
  geladen und bräuchten deshalb einen Vorberechnungsschritt in `mkdata.py`.

### Wie der Speicher untergebracht wurde

Ausgangslage waren 785 KB gegen 512 KB. Was geholfen hat, in der Reihenfolge
des Ertrags:

| Maßnahme | Ersparnis |
|---|---:|
| Kachelgrafiken der Kartenebenen direkt aus dem Flash | −144 KB |
| Klänge als 8 Bit im Flash statt auf 16 Bit hochgerechnet | −530 KB **Heap** |
| Sprites kopierfrei aus dem Flash | −400 KB **Heap** |
| `enemyDat` (851 × 77 Byte) als Zeiger ins Flash | −68 KB |
| `weapons` (781 × 80 Byte) als Zeiger ins Flash | −62 KB |
| Spielstandsbereich 121 KB → 12 KB (dabei ein Fehler behoben) | −109 KB |
| Anzeigeleiste zeilenweise in RGB565 statt als 8-Bit-Fläche | −23 KB |
| Paletten vorberechnet ins Flash | −23 KB |

Der gemeinsame Nenner: die Originaldaten liegen bereits im richtigen Format im
Flash, und Flash ist beim RP2350 in den Adressraum eingeblendet. Wo Dateiinhalt
und Speicherabbild Byte für Byte übereinstimmen, ist „laden" nur noch ein
Zeiger. Zwei `_Static_assert` sichern genau das ab — ändert jemand eine der
beiden Strukturen, bricht der Übersetzer ab, statt dass sich verschobene
Waffen- oder Gegnerwerte im Spiel zeigen.

Nicht angefasst wurde der Umbau der Kartenzeiger auf Indizes (−86 KB, 49
Stellen mit Zeigerarithmetik mitten im Hintergrund-Renderer). Er wird nicht
gebraucht und bleibt als Reserve.

## Lizenz

GPLv2, wie OpenTyrian. `usblink.c` stammt aus der rp2040-doom-Portierung für
denselben PicoBoy und ist ebenfalls GPLv2. Die Tyrian-Daten sind Freeware von
Epic MegaGames / World Tree Games und liegen dem Port nicht bei.
