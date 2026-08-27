# PicoBoy Color Plus — Hardware- und Portierungsreferenz

Gesammeltes Wissen aus den Portierungen in diesem Repo: Pinbelegung,
Display-Init, Tonausgabe, Flash-Aufteilung, die vier verschiedenen
Verbindungsarten und die Fallstricke, die dabei Zeit gekostet haben.

**Alle Zahlen stammen aus laufenden Projekten auf genau dieser Hardware, nicht
aus Datenblättern.** Wo etwas nur gerechnet und nie auf dem Gerät geprüft
wurde, steht es dabei — siehe Abschnitt 10.

Bevorzugte Umgebung ist das **Pico SDK** (C, CMake).

---

## 1. Hardware

| | |
|---|---|
| MCU | RP2350, Cortex-M33, Ziel `rp2350-arm-s`, Board `pico2` |
| RAM | 512 KB SRAM (Stack in 4 KB SCRATCH_Y) |
| Flash | **16 MB**, XIP ab `0x10000000` in den Adressraum eingeblendet |
| Display | ST7789, IPS, **240×280** sichtbar, Controller-RAM 240×320 |
| Eingabe | 5-Weg-Joystick + A + B, alle **aktiv LOW mit Pullup** |
| Ton | **Mini-Lautsprecher** an GP15, PWM über RC-Tiefpass |
| LEDs | drei Status-LEDs rot/gelb/grün, **aktiv HIGH** |
| Sensor | Beschleunigungssensor **STK8BA58** an I2C (GP20/GP21) |
| USB | USB-C, **Full Speed only (12 Mbit/s)** — kein HS-PHY |
| FPU | nur einfache Genauigkeit; `double` läuft in Software |

### Pinbelegung (aus drei unabhängigen Projekten übereinstimmend bestätigt)

```
SPI0   SCK  GP18   MOSI GP19
LCD    CS   GP10   (normaler GPIO, NICHT SPI-CSn)
       DC   GP8    RST  GP9
       BL   GP26   (PWM, ADC-fähiger Pin)
JOY    CENTER GP0  RIGHT GP1  DOWN GP2  LEFT GP3  UP GP4
BTN    A GP27      B GP28
AUDIO  GP15        (PWM-Kanal B des Slices!)
LED    ROT GP14   GELB GP13   GRUEN GP12   (aktiv HIGH)
I2C    SDA GP20   SCL  GP21   (BELEGT: Beschleunigungssensor STK8BA58
                              sitzt auf diesem Bus, siehe Abschnitt 5)
SPI-Takt 62 500 000 Hz, Mode 0 (CPOL=0, CPHA=0), MSB first
```

Frühere Versuche mit SPI1/GP14/GP11 oder CPOL=1/CPHA=1: Schirm bleibt dunkel.
GP14 ist die rote LED — das erklärt, warum dieser Fehlgriff nicht einmal einen
Kurzschluss erzeugt hat, sondern schlicht nichts.

SPI-Takt: 62,5 MHz sind auf diesem Panel unauffällig, 80 MHz laufen meist,
125 MHz sind Glückssache. Für Pixelblöcke lohnt `spi_set_format(..., 16, ...)`
— halb so viele FIFO-Zugriffe; für Kommandos vorher auf 8 Bit zurück.

---

## 2. Pflicht-Buildflags

```cmake
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD    pico2)

PICO_FLASH_SIZE_BYTES=16777216   # pico2-Boardfile meldet 4 MB -> 12 MB weg
PICO_STACK_SIZE=0x1000           # Default 0x800, Bank ist 4 KB -> hälfte verschenkt
PICO_USE_STACK_GUARDS=1          # Überlauf als MPU-Fault statt stiller .bss-Korruption
```

Erwartete Toolchain: `arm-none-eabi-gcc`, `cmake`, `picotool`, dazu
`PICO_SDK_PATH` (übliche Ablage `~/pico/pico-sdk`, `~/pico/pico-extras`).

```bash
cmake -B build -DPICO_SDK_PATH=$HOME/pico/pico-sdk \
      -DCMAKE_BUILD_TYPE=MinSizeRel -Dpicotool_DIR=/usr/local/lib/cmake/picotool
cmake --build build -j
```

---

## 3. Display

Init-Reihenfolge (auf dieser Hardware funktionierend):

```
RST 1 / 10ms / 0 / 10ms / 1 / 120ms
SWRESET  150ms
SLPOUT   120ms
COLMOD 0x55      (RGB565)
MADCTL 0xC8      (MY|MX|BGR — portrait, 180° gedreht, BGR-Bit gesetzt)
INVON            (IPS-Panel sieht ohne Invertierung falsch aus)
NORON, DISPON
danach volle 240x320 RAM löschen, nicht nur 240x280
```

Harte Punkte:

* **Backlight GP26 muss per PWM an.** Ohne das ist der Schirm schwarz, egal wie
  korrekt der SPI-Verkehr ist. Das ist der häufigste Fehlschluss bei „kein Bild".
* **MADCTL 0xC8 setzt das BGR-Bit** → Blau in die oberen 5 Bit, Rot in die
  unteren. Falsch herum sieht *nicht* offensichtlich falsch aus, nur etwas zu
  dunkel. Nur an EINER Stelle im Code entscheiden (ein `PBC_RGB()`-Makro).
* **Y-Versatz +20.** Sichtbar sind die Controller-Zeilen 20..299.
* **Ecken sind rund**, sicherer Rand 20 px. Nichts Bedienrelevantes hinein.
* **Kein Vollbildpuffer.** 240×280×2 = 134 KB. Stattdessen 8-Zeilen-Streifen
  (2×3840 B) umrechnen und per DMA schieben. Nebeneffekt, der wichtiger ist als
  der Speicher: ein Vollbild belegt den SPI-Bus **17 ms am Stück**; zwischen
  zwei Streifen liegt alle ~0,5 ms eine Gelegenheit, andere Dinge zu bedienen.

---

## 4. Ton

PWM auf GP15, über einen RC-Tiefpass auf einen **Mini-Lautsprecher** — kein
Piezo. Das ist ein echter elektrodynamischer Wandler mit brauchbarem
Frequenzgang: Mehrstimmigkeit, Samples und Musik lohnen sich hier, anders als
bei einem Piezo, der nur Rechteck-Pieptöne sinnvoll wiedergibt.

GP15 ist **Kanal B** seines Slices — die DMA-Zieladresse muss die *obere*
Hälfte des CC-Registers treffen. Falsch getroffen = stummer Ausgang ohne
Fehlermeldung.

**Die Grenze ist die Rechenzeit, nicht der Wandler.** 11025 Hz sind in den
Ports hier gewählt worden, weil eine OPL-/Softsynth-Emulation je Ausgabewert
rechnet und damit der größte Rechenzeitposten überhaupt ist — die Rate zu
halbieren halbiert unmittelbar diese Last. Wo die Musik dagegen aus fertigen
Samples kommt, ist eine höhere Rate hörbar und kostet fast nichts. Also nicht
reflexhaft auf 11025 Hz gehen, sondern danach entscheiden, wer die Abtastwerte
erzeugt.

**Ein PWM/DMA-Audiopfad kann beim Start blockieren** — das Gerät hängt dann
ohne Bild und ohne Backlight, sieht also nach einem Display-Fehler aus. Kommt
das vor: Sound- und Musik-Init versuchsweise überspringen. Bleibt das Gerät
dann stehen, liegt es woanders; startet es, ist der Audiopfad die Ursache und
nicht das Display.

---

## 5. Beschleunigungssensor STK8BA58

Angebunden über **I2C: SDA GP20, SCL GP21** — derselbe Bus, den auch eine
Gerät-zu-Gerät-Kopplung benutzen würde (Abschnitt 7d). Der Bus ist also
**nicht frei**.

* Der Sensor ist im Arduino-Umfeld ungewöhnlich, es gibt **keine verbreitete
  Bibliothek**. Register direkt über `hardware_i2c` ansprechen; ein
  Herstellerbeispiel existiert, gehört aber vor Gebrauch gegen ein Datenblatt
  geprüft.
* **Adresse nicht raten — scannen.** Ein I2C-Scan über 0x08…0x77 beim Start
  kostet nichts und beantwortet zugleich, ob Pull-ups und Verdrahtung stimmen.
  Sensortek-Teile dieser Reihe liegen üblicherweise auf 0x18 bzw. 0x28 je nach
  SDO-Pegel; das ist zu bestätigen, nicht zu übernehmen.
* Pull-ups sind für den Sensor auf der Platine bereits vorhanden — bei einer
  Zweitverwendung des Busses also keine weiteren zufügen.
* Sinnvolle Verwendung: Lagesteuerung, Schütteln als Eingabe, Ruhe-Erkennung.
  Als *Ersatz* für das Steuerkreuz taugt er nicht — sieben Knöpfe sind für die
  meisten Portierungen ohnehin knapp, und ein Sensor ohne Rastung macht Menüs
  unbedienbar.

## 6. Diagnose — es gibt keine serielle Konsole

* **`pico_enable_stdio_uart` ist auf diesem Board eine Falle:** UART0 liegt per
  Default auf GP0/GP1 = Joystick CENTER und RIGHT. Anschalten kostet still zwei
  Eingaben und legt TX gegen einen Pullup. Prüfen mit `picotool info`
  ("Fixed Pin Information").
* **USB-stdio geht nicht**, wenn USB der Datenlink ist.
* Deshalb: **das Panel ist die Konsole.** `-Wl,--wrap=panic` und
  `-Wl,--wrap=hard_assertion_failure` auf eine `fatal_screen()`, die den
  fehlerhaften PC anzeigt (für `addr2line`). Ersatz-`panic` darf nicht
  zurückkehren.
* Zähler dauerhaft aufs Bild legen: Frames, Bytes, Fehler je Ursache, letzte
  Ursache, Byteposition des letzten Abbruchs.
* Lautsprecher als zweiter Kanal: N kurze Töne als Fortschrittsmarke.
* **Drei LEDs (GP12/13/14, aktiv HIGH) als dritter Kanal** — sie funktionieren
  auch, wenn Display-Init oder SPI die Ursache sind, und brauchen weder Puffer
  noch Interrupt. Erste Instrumentierung eines toten Boards gehört hierhin.

---

## 7. Verbindungen — vier verschiedene Dinge, nicht verwechseln

### 7a. PBC+ als USB-Gerät am PC (TinyUSB device)

* **Full Speed, harte Decke ~1,1 MB/s Nutzlast.** Ein volles 240×280-Bild sind
  134 KB → 8,5 fps bei Vollbildänderung. Für Text/Menüs/Terminal (~7 KB/Frame)
  dagegen panel-limitiert, ~47 fps. Jede Architekturentscheidung folgt daraus:
  Kacheln + Änderungserkennung + RLE, nicht Vollbilder.
* Bulk-Endpunkte, `EP_SIZE 64`.
* Windows braucht **keinen Treiber**, wenn die Firmware MS-OS-2.0-Deskriptoren
  mit WinUSB-Compatible-ID meldet — in-box-Bindung, kein INF, keine Signatur.
* VID/PID: `0x1209` (pid.codes-Community-VID) plus eigene PID reicht für den
  Eigengebrauch.
* Pixel schon auf dem Host in Panel-Byteorder (BE, BGR565) legen, dann kann die
  Firmware empfangene Bytes unverändert per DMA nach RAMWR schieben.

### 7b. PBC+ ↔ PBC+ über D+/D− — **der bevorzugte Weg für zwei Geräte**

Kein USB-Protokoll, keine Enumeration, kein Host-Modus, kein VBUS. Der RP2350
kann die beiden PHY-Leitungen per `USBPHY_DIRECT`-Override direkt als IO
treiben:

```
D- (DM) = CLK    nur der Master treibt
D+ (DP) = DATA   Halbduplex
GND              gemeinsam
```

* Paketformat: Sync-Byte, Länge, CRC16, **max. 32 Byte** — die Zustellrate
  fällt exponentiell mit der Länge.
* Halbbitzeit 600–1500 ns; der Master regelt sich zwischen 400 und 4000 ns
  selbst nach. Der Slave hat **keine** eigene Zeitbasis, er sampelt auf den
  Flanken des Masters.
* **Der Richtungswechsel ist die kritische Stelle.** Nach dem Senden erst
  abwarten, bis die Leitung wirklich unten ist — Freigeben ist nicht dasselbe
  wie Low, die Leitung entlädt sich nur über den Pulldown. Sonst liest der
  Master den Rest seines EIGENEN Signals als „Gegenstelle bereit". Das war eine
  Fehlerrate von 65–80 %, unabhängig von der Taktrate, und hat Tage gekostet.
  Allgemeine Form der Falle: *sehe ich gerade die Gegenstelle oder mich selbst?*
* **Aller flankenkritische Code gehört ins RAM** (`__not_in_flash_func`). Ein
  XIP-Cache-Miss mitten in der Bitschleife verschiebt eine Flanke. Im
  Testprogramm (Cache warm) tritt das nie auf, im Spiel mit Core1 auf dem
  SPI-Bus ständig.
* Kosten im Spiel: alle 10 ms ~70 Byte, dabei 0,5–1 ms Interrupts aus.
* **Verkabelung:** D+ ↔ D+, D− ↔ D−, GND ↔ GND. **VBUS NICHT verbinden** —
  beide Boards sind eigenversorgt; ein Kabel/Adapter mit aufgetrennter
  VBUS-Ader verwenden. CC-Erkennung wird ignoriert, das ist bewusst nicht
  USB-C-konform.
* **Kabel zuerst verdächtigen.** Viele USB-C-Kabel sind reine Ladekabel ohne
  D+/D−. Alle vier Steckerorientierungen durchprobieren. Immer eine eigene
  Testfirmware ohne Spiel bauen — Aufbau in **Abschnitt 11c**.
* Während eines Links gehört der Port dem Link — kein Flashen, kein stdio.
  BOOTSEL bleibt davon unberührt, der Bootrom setzt den PHY zurück.

### 7c. TinyUSB Host/Device zwischen zwei Geräten (echtes USB, dual role)

Rahmenformat, Zustandsautomat und API stehen vollständig in **Abschnitt 11b**
— danach ist die Bibliothek in einer Datei nachgebaut. CMake braucht das
Verzeichnis mit `tusb_config.h` im Include-Pfad und
`tinyusb_device tinyusb_host tinyusb_board`.

**Ohne die Korrekturen aus Abschnitt 11a ist das unbenutzbar** — nicht erst
optimieren, sondern zuerst patchen.

```c
usb_link_begin_host();   /* bzw. usb_link_begin_device() auf der Gegenseite */
usb_link_task();                          /* in der Hauptschleife OFT */
usb_link_connected();                     /* CDC gemountet? */
usb_link_peer_alive(2500);                /* Paket in den letzten 2,5 s? */
usb_link_send(data, len);                 /* len <= 250 */
usb_link_available() / usb_link_receive(buf, sizeof buf) / usb_link_on_receive(cb)
```

Transport ist CDC-ACM, beide Stacks sind einkompiliert, die Rolle wird zur
Laufzeit gewählt. Nur RP2350 (nativer USB-Host des Chips).

Deutlich fragiler als 7b. Bekannte Punkte:

* Ohne Patches hält **ein einziger** Übertragungsfehler den ganzen MCU an
  (Bild eingefroren, Hauptschleife nie wieder erreicht). Zu entschärfen:
  `panic("Data Seq Error")`, `"Unhandled IRQ"`, `"hcd_clear_stall"`,
  `"ep already available"`, `"Can't continue xfer"`, `"Unhandled buffer"`,
  `"Invalid speed"`, fehlende RESUME-Quittung (sonst Interrupt-Dauersturm),
  Control-Transfer-Timeout in `usbh.c`. Fast alles betrifft die HOST-Rolle.
  Patch auf **genau die SDK-Kopie**, mit der gebaut wird, danach Build-Ordner
  löschen — sonst übersetzt CMake TinyUSB nicht neu. **Die vollständige
  Ersetzungsliste steht in Abschnitt 11a**; sie ist textuell und lässt sich als
  Skript ausführen. Die pico-sdk-Fassung von TinyUSB
  hat **fünf Panic-Stellen mehr** als die Arduino-Fassung; ohne die Korrekturen
  friert die Verbindung nach Minuten bis Stunden dauerhaft ein.
* Notlösung ohne SDK-Eingriff: `-Wl,--wrap=panic` → `watchdog_reboot()`.
* Selbstheilung selbst bauen: 3 s ohne Empfang → USB-Neustart, zweimal sanft,
  dann `reset_unreset_block_num_wait_blocking(RESET_USBCTRL)`.
* Schon 60–240 ms ohne `tud_task()` fahren die Device-Rolle fest → Neu-
  Enumeration. Kein `sleep_ms()` im Betrieb; eigenes `linkDelay()`, das
  weiterpollt, auch in Ladevorgängen, Levelwechseln, Siegesbildschirmen.
* Rolle zur Laufzeit wählbar: vor der Rollenwahl `tud_disconnect()`.

### 7d. PBC+ ↔ PBC+ über I2C

**SDA GP20, SCL GP21, gemeinsame GND.** Auf einem Gerät HOST, auf dem anderen
JOIN. Über eine ganze Spielesammlung hinweg erprobt und damit der
unaufwendigste Weg, wenn ohnehin Draht gelegt werden kann und der USB-Port frei
bleiben soll.

**Auf diesem Bus sitzt bereits der Beschleunigungssensor STK8BA58**
(Abschnitt 5). Daraus folgt:

* Die eigene Slave-Adresse muss sich von der des Sensors unterscheiden — vorher
  scannen, nicht annehmen.
* Werden beide Geräte gekoppelt, hängen **zwei** Sensoren am selben Bus, jeder
  mit derselben Adresse. Solange nur der jeweils lokale Master sie anspricht,
  ist das unkritisch; ein Zugriff über die Kopplung hinweg trifft dagegen
  beide. Wer den Sensor im Zweispielerbetrieb braucht, liest ihn lokal und
  überträgt den Messwert als Nutzdatum.
* Kein Buszugriff auf den Sensor mitten in einem laufenden Austausch.

**Achtung bei Fremdcode:** die I2C-Defaults vieler Portierungen liegen auf
GP18/GP19 — das sind hier die Display-Pins. Das ist regelmäßig der Grund,
warum ein übernommener I2C-Transport ersetzt werden muss.

### Regeln für jeden Link (gilt für 7b, 7c und 7d)

* Eigene Blindheit nicht der Gegenstelle anlasten: war der eigene Poll-Abstand
  > 100 ms, diese Spanne gutschreiben statt anrechnen. Uhr bei Spielstart neu
  stellen.
* „Verbindung tot" frühestens nach 3–10 s. Kurze Schwellen nur für Anzeigen.
* Ein Abbruch darf nie einfrieren: Wachhund → Verbindung auflösen, allein
  weiterspielen, Zustand sichtbar machen.
* Wiederkehrender Zustand (Positionen je Tick) ist unkritisch. **Einmalige
  Ereignisse** (Rundenstart, Tod, Levelwechsel) mehrfach senden (5× über
  200 ms) oder quittieren lassen.
* Deterministischer Gleichschritt ist die richtige Netzarchitektur: nur
  Eingaben tauschen (in einem gemessenen Fall 28 Byte je Bild), niemals
  Objektlisten.

---

## 8. Flashen

Das Gerät muss dafür im **BOOTSEL-Modus** hängen: BOOTSEL gedrückt halten,
währenddessen USB einstecken, dann loslassen. Es meldet sich als Laufwerk
`RPI-RP2` und ist damit für `picotool` sichtbar.

Am einfachsten geht es ganz ohne Werkzeug: die `.uf2` auf das Laufwerk ziehen,
fertig. `picotool` ist die Alternative, wenn man mehrere Dateien nacheinander
schreiben oder Adressen kontrollieren will:

```bash
picotool info -a                  # prueft Verbindung und meldet rp2350-arm-s
picotool load firmware.uf2
picotool load daten.uf2
picotool reboot
# danach einmal wirklich stromlos machen
```

* `picotool load -x firmware.uf2` lädt und startet in einem Schritt — spart das
  `reboot`, wenn nur eine Datei zu schreiben ist.
* **`picotool -f` (Gerät aus dem laufenden Programm in BOOTSEL zwingen) geht
  hier nicht.** Das setzt ein USB-Reset-Interface in der Firmware voraus, und
  USB-stdio ist auf diesem Board abgeschaltet (Abschnitt 6). Also immer von
  Hand über den Knopf.
* Unter Linux braucht `picotool` ohne passende udev-Regel Root-Rechte; sonst
  meldet es „no accessible RP2350 devices". Erst daran denken, bevor man
  Kabel oder Firmware verdächtigt.
* Läuft gerade ein D+/D−- oder CDC-Link, gehört der Port dem Link — dann ist
  kein Flashen möglich, BOOTSEL bleibt davon aber unberührt.
* **Den Flash zu löschen ist nicht nötig.** Drag&Drop überschreibt nur die
  eigenen Blöcke, und weiter oben bleiben Reste des Vorgängers liegen — die
  richten aber nichts an, weil der Bootrom nur den Anfang des Flash startet.
  Wer sie trotzdem loswerden will: `picotool erase -a`, dauert bei 16 MB über
  zwei Minuten. Bricht es ab, zusätzlich
  `picotool erase -r 0x10600000 0x11000000`.
* Zeigt das Display nach dem Aufspielen noch das alte Bild, war es nur ein
  Soft-Reset. **Einmal stromlos machen** — USB abziehen, kurz warten, wieder
  einstecken — dann startet auch der Displaycontroller sauber neu.
* Bewährte Flash-Aufteilung: Firmware ab `0x10000000`, Daten ab `0x10100000`,
  Spielstände kurz vor Ende, **`0x10FFFF00` muss frei bleiben** — die SDK-UF2
  schreibt dort einen Zusatzblock.
* **UF2-Dateien zusammenfügen:** die Blocknummerierung („Block i von N") läuft
  **je Familienkennung getrennt**. Solange keine Familie in zwei Eingabedateien
  vorkommt, ist schlichtes Aneinanderhängen richtig; global durchnumerieren
  zerstört die Sequenz — die Datei wird geschrieben, aber das Bootrom erkennt
  das Ende nicht und startet nicht neu. Ergebnis mit `picotool info` prüfen
  (muss `rp2350-arm-s` mit gültigem Abbild melden). Auf Adressüberschneidungen
  prüfen.

---

## 9. Portierungsmuster

**Leitsatz: Flash ist eingeblendet, also ist „laden" oft nur ein Zeiger.**
Wo Dateiinhalt und Speicherabbild Byte für Byte übereinstimmen, `malloc`+`fread`
durch einen Zeiger ins XIP-Flash ersetzen. In einem gemessenen Fall brachte
allein das den Bedarf von 785 KB auf unter 512 KB:

| Maßnahme | Ersparnis |
|---|---:|
| Kacheln direkt aus Flash | −144 KB |
| Klänge als 8 Bit im Flash statt auf 16 Bit hochgerechnet | −530 KB Heap |
| Sprites kopierfrei | −400 KB Heap |
| große Datentabellen als Zeiger ins Flash | −130 KB |
| Paletten vorberechnet ins Flash | −23 KB |

Dazugehörige Fallen:

* Strukturen brauchen ggf. `__attribute__((packed))`, sonst schiebt der
  Übersetzer ein Füllbyte ein und die Datei passt nicht mehr. **Immer mit
  `_Static_assert` auf die Strukturgröße absichern** — sonst zeigen sich
  verschobene Werte erst im Spiel.
* `free()` darf Flash-Zeiger nicht sehen → eine `owns()`-Prüfung. Der Absturz
  käme sonst erst beim Levelwechsel.
* Einträge, die als *Vorlage* beschrieben werden, brauchen eine RAM-Kopie.
* Datenaufbereitung gehört in ein Python-Werkzeug (`mkdata.py`), nicht in die
  Firmware.

**Upstream so weit wie möglich unverändert lassen**, damit ein späterer Abgleich
möglich bleibt. Werkzeuge dafür, ohne Quelltextänderung:

| Was | Wie |
|---|---|
| `fopen`/`fread`/`fseek`/`fileno` | Makros auf eigene `*_pbc`-Fassungen per `-include prelude.h` |
| `exit`/`abort` | `-Wl,--wrap=` auf eine stehenbleibende Meldung auf dem Panel |
| `main` | `-Dmain=spiel_main`; eigener Startpunkt richtet Hardware ein |
| `SDL.h`, `SDL_net.h` | Attrappen, die im Include-Pfad vor echtem SDL stehen |
| `mkdir`, `fsync` | Leerimplementierungen |

Ersetzt wird typischerweise nur: `video.c` (kein Fenster, kein Renderer),
Audio-Gerät, `SDL_PollEvent` → GPIO-Abfrage, Netz-Transport. Skalierer für
Desktop-Fenster fliegen raus.

**Weiteres:**

* Keine pauschale `double`→`float`-Umleitung per Makro — das trifft auch die
  Deklarationen in `math.h`. Punktuell an gemessenen Stellen ändern.
* Endsequenzen/Animationen mit großem Pufferbedarf sauber auslassen statt am
  Nullzeiger abzustürzen. Ein fehlender Abspann schlägt ein eingefrorenes Gerät.
* Knapper Heap ist der erste Verdacht, wenn beim Laden etwas fehlschlägt.

### Bedienung: 7 Knöpfe gegen eine Tastatur

Kombinationen statt fehlender Tasten. Bewährt und in mehreren Portierungen
gleich gelöst:

```
A + CENTER   = ESC / Menü
CENTER + B   = zweite Sonderfunktion
A            = im Menü bestätigen, im Spiel Nebenfunktion
B            = im Menü zurück, im Spiel Feuer
```

Textzeichen sind nicht eingebbar → Namenseingaben ersatzlos streichen und feste
Namen aus der Rolle ableiten (P1/P2). Cheats/Optionen stattdessen beim Booten
per gehaltenem Knopf latchen.

### Bildaufteilung

Das Panel ist 240×280 — **hochkant**. Für 320×200-Spiele:

* Senkrecht scrollende Spiele: Spielfeld in **Originalpixeln** oben, Rest der
  Höhe für eine selbstgebaute Anzeigeleiste. Kamera im Rahmen der überzähligen
  Spalten mit dem Spieler mitführen.
* Menüs/Läden/Zwischenbilder: **nicht** dasselbe Layout. Entweder 1:1-Ausschnitt
  mittig (Menüs sind meist um x=160 zentriert) oder ganzes Bild verkleinert.
* **Verkleinern durch Weglassen macht 5–7 Punkt hohe Schrift unlesbar.** 320→240
  ist ¾, also fällt jede vierte Spalte weg. Für Bilder über 2×2 mitteln (kostet
  ~0,2 ms je Streifen und versteckt sich hinter dem DMA), für Text lieber
  Ausschnitt statt Skalierung.

---

## 10. Wissensstand und bewährtes Vorgehen

Nicht alles hier ist gleich gut abgesichert. Beim Weiterverwenden lohnt der
Blick auf die Herkunft:

* **Auf echter Hardware bestätigt:** Pinbelegung, Display-Init, MADCTL 0xC8 /
  BGR, Y-Versatz +20, Backlight-PWM, SPI-Takt, Tonausgabe, Flash- und
  Löschvorgehen, die Streifen-Ausgabe, das XIP-Kopierfrei-Muster und die
  I2C-Kopplung (Abschnitt 7d).
* **Gerechnet oder nur gebaut, nie auf dem Gerät geprüft:** alles zu
  USB-als-Display am PC (Abschnitt 7a) — WinUSB-Bindung, Kachel-/RLE-Ansatz
  und die dortigen Durchsatzzahlen —, sowie die pico-sdk-Fassung der
  Link-Bibliothek aus 7c, deren Logik allerdings einer auf Hardware erprobten
  Arduino-Fassung entspricht.
* **Dokumentiert, aber noch von keiner Anwendung benutzt:** der
  Beschleunigungssensor aus Abschnitt 5.

### Was sich beim Portieren bewährt hat

* **Prüfen, was ohne Gerät prüfbar ist.** Encoder gegen Decoder auf der
  Buildmaschine laufen lassen, Byteströme an unbequemen Grenzen zerhacken,
  die Gegenstelle Pakete verweigern lassen, `picotool info` auf die erzeugte
  UF2 anwenden.
* **Immer eine geräteseitige Testfirmware mitbauen**, die den fraglichen Pfad
  ohne die Anwendung zeigt: ein Testbild für Kabel, Geometrie und Byteorder,
  ein Link-Test für D+/D−. Dazu auf der Gegenseite ein `--dump`, das ohne
  Gerät zeigt, was gesendet würde. Zwischen beiden ist jeder Fehler schon auf
  eine Seite eingegrenzt. Die Selbsttest-Abbilder in `dist/selbsttest/` sind
  genau das.
* **Eine Änderung pro Testlauf.** Drei gleichzeitig, und ein Rückschritt ist
  nicht mehr zuzuordnen.
* **Messen statt vermuten.** In einem Fall wurden tagelang Taktraten und Kabel
  verdächtigt; die Ursache war eine falsche Handshake-Bedingung.
* **A/B unter Last prüfen.** Was im Testprogramm läuft, kann im Spiel
  scheitern, weil dort Core1 permanent den SPI-Bus bedient und den XIP-Cache
  durchwirbelt.
* **Am Ende Firmwaregröße, statisches RAM und verbleibenden Heap notieren.**
  Auf 512 KB RAM ist das die Zahl, die zuerst knapp wird.

---

## 11. Reproduzierbare Bausteine

Alles Folgende ist vollständig genug, um es ohne Rückfrage neu zu schreiben.

### 11a. TinyUSB entschärfen (Voraussetzung für 7c)

TinyUSB unterstellt einen normgerechten Bus. Zwei direkt verbundene RP2350 ohne
VBUS und mit doppelten Serienwiderständen erzeugen dagegen regelmäßig Fehler,
die am PC praktisch nie auftreten — und TinyUSB reagiert an mehreren Stellen mit
`panic()`, was den **ganzen** Chip anhält. Ohne diese Korrekturen friert die
Verbindung nach Minuten bis Stunden dauerhaft ein.

Pfade relativ zu `$PICO_SDK_PATH/lib/tinyusb/src/`. Vorher `.orig`-Sicherung
anlegen; die Ersetzungen sind textuell und überstehen SDK-Updates, solange die
Zeilen sich nicht ändern.

Begrenzte Warteschleife (unten `BOUNDED(cond)` genannt) — nie endlos auf ein
Hardware-Flag warten, ein hängender Interrupt legt sonst den Chip lahm:

```c
{ uint32_t _g = 100000u; while ((cond) && _g) { _g--; tight_loop_contents(); } }
```

**`portable/raspberrypi/rp2040/dcd_rp2040.c`**

| Original | Ersetzen durch |
|---|---|
| `while ((usb_hw->abort_done & abort_mask) != abort_mask) {}` | `BOUNDED((usb_hw->abort_done & abort_mask) != abort_mask)` |
| `panic("Unhandled IRQ 0x%x\n", (uint) (status ^ handled));` | `usb_hw_clear->inte = (status ^ handled);` |

**`portable/raspberrypi/rp2040/rp2040_usb.c`**

| Original | Ersetzen durch |
|---|---|
| `while (usb_hw->sie_ctrl & USB_SIE_CTRL_STOP_TRANS_BITS) {}` | `BOUNDED(usb_hw->sie_ctrl & USB_SIE_CTRL_STOP_TRANS_BITS)` |
| `while ((usb_hw->abort_done & abort_bit) != abort_bit) {}` | `BOUNDED((usb_hw->abort_done & abort_bit) != abort_bit)` |
| `panic("ep %02X was already available", ep->ep_addr);` | `(void)0;` |
| `panic("Can't continue xfer on inactive ep %02X", ep->ep_addr);` | `hw_endpoint_lock_update(ep, -1); return false;` |

**`portable/raspberrypi/rp2040/hcd_rp2040.c`**

| Original | Ersetzen durch |
|---|---|
| `while (usb_hw->sie_ctrl & USB_SIE_CTRL_STOP_TRANS_BITS) {}` | `BOUNDED(usb_hw->sie_ctrl & USB_SIE_CTRL_STOP_TRANS_BITS)` |
| `panic("Data Seq Error \n");` | `(void)0;` — DATA0/DATA1-Sequenzfehler ist hier normal, der Stack wiederholt selbst |
| `panic("hcd_clear_stall");` | `return true;` (die Funktion besteht im Original **nur** aus `panic()`) |
| `panic("Unhandled buffer %d\n", remaining_buffers);` | `usb_hw_clear->buf_status = remaining_buffers;` |
| `panic("Unhandled IRQ 0x%x\n", (uint) (status ^ handled));` | `usb_hw_clear->inte = (status ^ handled);` |
| `panic("Invalid speed\n");` | `return TUSB_SPEED_FULL;` (hier immer Full Speed) |

Zusätzlich in derselben Datei **vor** `if (status & USB_INTS_STALL_BITS) {`
einfügen (Schreibweise variiert je Fassung, ggf. ohne Leerzeichen in den
Klammern suchen):

```c
/* RESUME wird freigegeben, im Original aber nie behandelt oder quittiert.
   Tritt die Bedingung ein, bleibt das Bit stehen und der Interrupt löst sofort
   wieder aus — endlos, die Hauptschleife kommt nie wieder dran. */
if (status & USB_INTS_HOST_RESUME_BITS) {
  usb_hw_clear->sie_status = USB_SIE_STATUS_RESUME_BITS;
}
```

**`host/usbh.c`** — der blockierende Control-Transfer hat im Original keinen
Timeout (dort als `TODO` vermerkt). `while (result == XFER_RESULT_INVALID) {`
ersetzen durch:

```c
const uint32_t _to_start = tusb_time_millis_api();
while (result == XFER_RESULT_INVALID) {
  if ((uint32_t)(tusb_time_millis_api() - _to_start) > 2000u) {
    result = XFER_RESULT_TIMEOUT; break;
  }
```

Die letzten fünf Panic-Stellen gibt es **nur** in der pico-sdk-Fassung von
TinyUSB, nicht in der Arduino-Fassung. **Nach dem Patchen den Build-Ordner
löschen und neu erzeugen** — sonst übersetzt CMake TinyUSB nicht neu und die
Änderung bleibt wirkungslos.

Notlösung ohne SDK-Eingriff: `-Wl,--wrap=panic` mit eigenem `__wrap_panic()`,
das `watchdog_reboot()` aufruft (`panic` ist `noreturn`, der Ersatz darf **nicht**
zurückkehren). Deckt die RESUME-Quittung und die Warteschleifen nicht ab.

### 11b. Rahmenformat des CDC-Links (7c)

Transport ist CDC-ACM, beide TinyUSB-Rollen einkompiliert, Rolle zur Laufzeit
gewählt. Darüber ein eigener Rahmen, weil ein Bytestrom keine Paketgrenzen hat:

```
[0xA7][0x5C][len][payload 0..len-1][crc8]
```

* `len` ≤ 250. CRC-8, Polynom `0x07`, Startwert `0`, gerechnet über **`len` und
  danach die Nutzdaten** (nicht über die Sync-Bytes).
* Empfangs-Zustandsautomat: 0 = auf `0xA7` warten; 1 = `0x5C` erwartet, bei
  erneutem `0xA7` in Zustand 1 bleiben (sonst geht ein Rahmen verloren, der
  direkt auf ein Störbyte folgt); 2 = Länge, bei `len == 0` gleich zur
  Prüfsumme; 3 = Nutzdaten zählen; 4 = CRC prüfen, nur bei Übereinstimmung
  ausliefern, danach zurück in Zustand 0.
* Lese- und Schreibschleifen brauchen einen Durchlaufzähler als Bremse —
  ohne den hängt die Hauptschleife an einem Dauerstrom fest.
* Senden: passt der Rahmen nicht in den Puffer, **stillschweigend verwerfen**
  statt zu blockieren; höchstens ein Senden je Task-Durchlauf. Auf der
  Device-Seite ohne DTR-Prüfung schreiben, sonst sendet niemand, solange die
  Gegenseite den Port nicht „öffnet".

Sinnvolle API: `begin_host()` / `begin_device()`, `task()` (oft aufrufen),
`connected()`, `peer_alive(timeout_ms)`, `send(data,len)`, `available()` /
`receive(buf,cap)` oder ein Empfangs-Callback.

### 11c. Link-Testfirmware

Für 7b **und** 7c vor der Anwendung zu bauen — sie beantwortet die Frage, die
sich sonst mit der Anwendung vermischt: kommt überhaupt etwas durch?

* Ohne Spiel, ohne Anwendungslogik, eigenes UF2.
* Rolle per Knopf beim Start (A = Master/Host, B = Slave/Device).
* Anzeige: grüner Balken bei laufendem Link, Pakete je Sekunde, getrennte
  Zähler für Timeout, CRC-Fehler und Datenfehler, dazu die aktuelle Bitzeit.
* Bei 7b: Bitzeit mit A verstellbar, Zähler mit B zurücksetzen. Der niedrigste
  Wert, der über eine Minute null Fehler zeigt, ist der einzutragende.
* Beide Geräte tragen dieselbe Testfirmware.

