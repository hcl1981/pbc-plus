# Änderungen an den OpenTyrian-Quellen

Der Ordner `src/` ist der OpenTyrian-Upstream
(github.com/opentyrian/opentyrian, Stand `1c34d1b`). Ziel war, ihn so weit wie
möglich unverändert zu lassen, damit ein späterer Abgleich mit dem Upstream
möglich bleibt.

Betroffen sind **15 von 100 Dateien**, zusammen rund 640 Zeilen — davon der
größte Teil Kommentar. Vergleichen lässt sich das jederzeit gegen
`../upstream-opentyrian/src/`.

| Datei | Zeilen | Grund |
|---|---:|---|
| `tyrian2.c` | 221 | Kachellader, `enemyDat`-Vorlage, Ansichtsmodus |
| `episodes.c/.h` | 136 | `weapons` und `enemyDat` ins Flash |
| `nortsong.c/.h` | 78 | Klänge als 8 Bit im Flash |
| `loudness.c/.h` | 45 | Mischer für 8-Bit-Klänge |
| `palette.c/.h` | 48 | Paletten ins Flash |
| `varz.h` | 33 | Kachelfelder entfallen |
| `sprite.c` | 30 | Sprites kopierfrei |
| `animlib.c` | 20 | Schlussanimation sauber auslassen |
| `picload.c` | 17 | Bilder kopierfrei |
| `game_menu.c`, `opentyr.c` | 9 | Ansichtsmodus |

## Gelöschte Dateien

| Datei | Ersetzt durch | Warum |
|---|---|---|
| `video.c` | `platform/pbc_video.c` | Fenster, Renderer und Texturen gibt es nicht; das Panel ist fest verbaut. |
| `video_scale.c`, `video_scale_hqNx.c` | — | Skalierer für ein Desktop-Fenster. Die Bildaufteilung steht hier fest. `video_scale.h` bleibt als Attrappe, weil `config.c` den Skalierernamen speichert. |

---

## Das durchgehende Muster: kopierfrei aus dem Flash

Fast alle Änderungen sind dieselbe. Der RP2350 blendet sein Flash ab
`0x10000000` in den Adressraum ein — die Spieldaten sind also bereits lesbar da,
wo sie liegen. Wo Dateiinhalt und Speicherabbild übereinstimmen, ist „laden"
deshalb nur noch ein Zeiger, und `xipfs_inplace()` liefert ihn.

Das ist keine Feinheit: OpenTyrians Arbeitsspeicherbedarf lag bei 785 KB, das
Gerät hat 512 KB.

### Wo das ohne Weiteres ging

* **`sprite.c`** — Sprite-Blöcke (`malloc` + `fread` → Zeiger). Wichtig dabei:
  `free_sprites()` und `free_sprite2s()` dürfen Flash-Zeiger nicht an `free()`
  geben; `xipfs_owns()` unterscheidet das. Ohne diese Ergänzung wäre der
  Absturz erst beim Episodenwechsel gekommen.
* **`picload.c`** — die Bilder aus `tyrian.pic` werden ohnehin nur einmal
  durchlaufen und dabei entpackt.
* **`tyrian2.c`, Kachellader** — die Kacheln liegen in `shapes*.dat`
  unkomprimiert (je Kachel ein Flag und 672 Byte). Die drei Kartenebenen
  hielten davon eigene Kopien; jetzt zeigt die Karte direkt ins Flash. Das
  entsprechende Feld in `varz.h` ist entfallen. **−144 KB.**
  * Dabei ist aufgefallen, dass das Feld `fill` zwar berechnet und gespeichert,
    aber an keiner Stelle je gelesen wird — ein Überbleibsel der DOS-Fassung.
    Mit entfernt.

### Wo erst etwas zu prüfen war

* **`episodes.c/.h`, `weapons`** — 781 Datensätze zu 80 Byte. `JE_WeaponType`
  ist mit 80 Byte exakt die Summe seiner Felder, hat also keine Polsterung, und
  die Deklarationsreihenfolge entspricht der Lesereihenfolge. Der Dateiinhalt
  *ist* damit das Speicherabbild. **−62 KB.**
* **`episodes.c/.h`, `enemyDat`** — dasselbe, aber die Struktur brauchte
  `__attribute__((packed))`: ohne sie legt der Übersetzer vor `startx` ein
  Füllbyte ein (80 statt 77 Byte) und die Datei passt nicht mehr. **−68 KB.**
  * **Ausnahme Eintrag 0:** die Ereignisse 49–52 benutzen ihn als *Vorlage* und
    schreiben Panzerung und Grafik hinein, bevor sie den Gegner erzeugen. Er
    liegt deshalb als Kopie im RAM (`enemyDat_scratch`), und `JE_makeEnemy`
    greift über einen lokalen Zeiger darauf zu. Nur dort ist das nötig — die
    übrigen Lesestellen holen `startyc`, `esize` und `value`, und die verändert
    die Vorlage nicht.

Beide Umstellungen sind mit `_Static_assert` auf die Strukturgröße abgesichert.
Ändert jemand später ein Feld, bricht der Übersetzer ab — statt dass sich
verschobene Waffen- oder Gegnerwerte im Spiel zeigen, was man lange suchen
würde.

### Wo die Daten erst umgeformt werden mussten

* **`palette.c/.h`** — die Palettendatei enthält VGA-Werte mit 6 Bit je
  Farbanteil, die beim Laden auf 8 Bit gestreckt werden (und zwar nicht durch
  simples Schieben — die oberen zwei Bit werden unten wieder angehängt, sonst
  wäre der Höchstwert 252 statt 255). `tools/mkdata.py` nimmt diese Rechnung
  jetzt vorweg und legt fertige `SDL_Color`-Werte ab. **−23 KB.**
  * `palettes` ist bewusst **nicht** `const`: `set_palette` und die
    Überblendungen nehmen `Palette` ohne `const` entgegen, und diese Signaturen
    zu ändern hätte sich durch ein Dutzend Dateien gezogen. Geschrieben wird
    nichts — die Arbeitskopie ist `colors` bzw. `palette`.

### Klänge: der größte Posten überhaupt

* **`nortsong.c/.h`, `loudness.c/.h`** — im Original werden Tyrians Klänge beim
  Laden von 8 auf 16 Bit und von 11025 Hz auf die Gerätrate hochgerechnet. Aus
  264 KB Ausgangsmaterial würde dabei **über ein Megabyte** — mehr als das
  Gerät überhaupt an RAM besitzt.

  Sie bleiben deshalb, wie sie sind, und der Mischer rechnet im Vorbeigehen um:
  die Amplitude mit einer Schiebeoperation, die Tonhöhe mit einem Phasenzähler
  je Kanal. Da das Gerät ohnehin mit 11025 Hz läuft — der Originalrate — ist
  der Schritt genau 1 und kostet nichts. Der Zähler bleibt trotzdem, damit eine
  spätere Änderung der Rate nicht stillschweigend die Tonhöhe verschiebt.

  Der ganze Umrechnungsblock in `loadSndFile` ist entfallen. **−530 KB Heap.**

---

## Änderungen, die nichts mit Speicher zu tun haben

* **`tyrian2.c`, `game_menu.c`, `opentyr.c`** — je ein Aufruf von
  `pbc_set_view_mode()`. Das Spielfeld wird in Originalpixeln gezeigt, Menüs
  und Laden als verkleinertes Gesamtbild; siehe `platform/pbc_video.h`.
  Die Kamera braucht **keinen** Eingriff: sie wird in `JE_showVGA()`
  nachgeführt, und das läuft ohnehin genau einmal je Bild.

* **`animlib.c`** — die Schlussanimation fordert zwei Puffer von zusammen
  128 KB an. Die gibt es nicht. Statt auf einem Nullzeiger abzustürzen, bricht
  die Funktion sauber ab und der Abspann entfällt. Ein übersprungener Abspann
  ist allemal besser als ein eingefrorenes Gerät nach dem gewonnenen Spiel.

---

## Nicht geändert, aber umgebogen

Diese Eingriffe finden ohne Quelltextänderung statt:

| Was | Wie | Wo |
|---|---|---|
| `fopen`/`fread`/`fseek`/… | Makros auf `pbc_*`, die aus dem Flash-Archiv lesen | `platform/pbc_prelude.h` (per `-include`) |
| `fileno` | Makro auf `pbc_fileno` — newlibs Fassung würde in einen Zeiger greifen, der kein newlib-`FILE` ist. Ohne das kein Übersetzungsfehler, sondern ein Absturz beim Speichern. | ebenda |
| `exit`/`abort` | `-Wl,--wrap=` auf eine lesbare Meldung, die stehenbleibt | `CMakeLists.txt`, `platform/pbc_main.c` |
| `main` | `-Dmain=opentyrian_main`; der echte Startpunkt richtet vorher die Hardware ein und fragt Einzel- oder Netzspiel ab | ebenda |
| `SDL.h`, `SDL_net.h` | Attrappen, die im Include-Pfad vor einem echten SDL stehen | `platform/fakesdl/` |
| `mkdir`, `fsync` | Leerimplementierungen — es gibt keine Verzeichnisse, und `pbc_fclose` brennt den Sektor in einem Zug | `platform/xipfs.c` |
| Audio-Abtastrate | Das Gerät meldet 11025 Hz statt der gewünschten 44100. OpenTyrian erlaubt das ausdrücklich und rechnet mit dem, was es bekommt. | `platform/pbc_audio.c` |

---

## Bewusst nicht gemacht

**Keine pauschale Umleitung von `double` auf `float`.** Der RP2350 hat eine FPU
nur für einfache Genauigkeit, `double` wird in Software gerechnet. Ein Makro
`#define sin sinf` hätte aber auch die Deklarationen in `math.h` getroffen und
die Genauigkeit von Vergleichen stillschweigend verändert. Die betroffenen
Stellen laufen höchstens einmal je Bild. Falls eine Messung später `double` als
Kostenpunkt zeigt, gehört die Änderung an die betroffene Stelle — nicht in eine
globale Kopfdatei.

**Kein Umbau der Kartenzeiger auf Indizes.** Das wären weitere 86 KB, verteilt
auf 49 Stellen mit Zeigerarithmetik mitten im Hintergrund-Renderer und im
Ereignissystem. Es wird nicht gebraucht und bleibt als Reserve.
