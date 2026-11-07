# PBC C64

Commodore-64-Emulator für den **PicoBoy Color Plus** (RP2350A, 512 KB RAM,
16 MB Flash, ST7789 240×280, Joystick + A/B).

Grundlage ist Frodo4 (Christian Bauer) in der Portierung von
[frank-c64](https://github.com/rh1tech/frank-c64), im zeilenbasierten Modus wie
dort. Das Drumherum ist dasselbe wie bei [`../nes`](../nes/) und
[`../megadrive`](../megadrive/):

| frank-c64 | PBC C64 |
|---|---|
| Disk-Images von SD-Karte | **USB-Stick-Modus** in eine FatFs-Partition im Flash (aus [PicoBoyGB](../picoboygb/)); Frodo benutzt das Image direkt von dort, **auch schreibend** (Spielstände) |
| PS/2- oder USB-Tastatur | keine Tastatur: **`LOAD"*",8,1`** wird nach dem Start automatisch getippt, danach bei Bedarf `RUN` |
| HDMI/VGA 320×240 | **ST7789**: 320×240 auf 240×180 verkleinert (gemittelt), auf Kern 1 per DMA |
| SID über I2S/PWM | SID über den **PWM-Lautsprecher** (GP15), 45 454 Hz, **per Vorgabe aus** |
| F9 tauscht die Joystick-Ports | **Joystick-Port pro Spiel** in den Optionen, gespeichert |
| Diskettenwahl | **Mitte + A** im Spiel (alle `.d64` des Laufwerks) |
| Cartridges | **`.crt`** (8K/16K, Ocean, Fun Play, Super Games, Simons' Basic, C64GS, Dinamic, Zaxxon, Magic Desk, Comal 80, EasyFlash), aus dem Flash gelesen |
| Bandlaufwerk | entfällt |

**Stand:** läuft auf dem Gerät, auf dem Gerät geprüft: Ton, Spielstände auf
Diskette (speichern, RESET, wieder laden), Dateiauswahl, USB-Eintrag. Kern 0 lag
bei 300 MHz unter 30 % Last, daher läuft der C64 mit 150 MHz (Grundtakt, keine Übertaktung).

Die Oberfläche auf dem Gerät ist englisch (Menüs, Optionen, Meldungen, Hilfedatei auf dem Laufwerk).

## Benutzung

1. [`../dist/c64.uf2`](../dist/c64.uf2) flashen: BOOTSEL halten, USB anstecken,
   Datei auf das Laufwerk `RPI-RP2` kopieren.
2. **Spiele aufspielen:** beim Einschalten **A oder B** gedrückt halten. Das
   Gerät meldet sich als USB-Stick `PBC-ROMS`. `.d64`-Disk-Images (oder
   `.prg`-Programme) draufkopieren und das Laufwerk **auswerfen** – der PicoBoy
   startet von selbst neu.
3. Ohne gedrückte Taste einschalten: Spieleliste. Hoch/Runter wählt,
   Links/Rechts blättert, **A** oder **Mitte** startet.
   Ganz oben steht immer **„Add games via USB“**: führt direkt in den
   USB-Stick-Modus, ohne Neustart mit gedrückter Taste. Beim Öffnen ist das
   erste Spiel markiert.
4. **B** öffnet die Optionen für das markierte Spiel:
   * **Joystick-Port 1 oder 2** (pro Spiel gespeichert, Vorgabe 2),
   * **Datei**: welches Programm von der Diskette geladen wird – `*` (die
     erste, Vorgabe) oder eines aus dem Inhaltsverzeichnis (pro Spiel
     gespeichert),
   * Ton an/aus (Vorgabe aus) und Lautstärke (für alle Spiele).

   Geändert werden kann das nur hier, nicht im Spiel.

Das Laufwerk teilt sich der C64 mit `megadrive` und `nes`; jede Firmware listet
nur ihre eigenen Dateien.

### Was beim Start passiert

Es gibt keine Tastatur, also tippt der Emulator selbst:

1. Sobald BASIC nach dem Einschalten bereit ist, wird das Image als Laufwerk 8
   eingehängt und `LOAD"*",8,1` eingegeben – oder `LOAD"NAME",8,1` für die in
   den Optionen gewählte Datei. Der Tastaturpuffer des C64 fasst nur 10
   Zeichen; längere Befehle werden stückweise getippt, sobald der Editor den
   Puffer geleert hat.
2. Kehrt BASIC danach zu `READY.` zurück, hat sich das Programm nicht selbst
   gestartet – dann folgt `RUN`. Startet es sich selbst, wird nichts weiter
   getippt.

Bei `.prg`-Dateien wird das Programm direkt in den Speicher geladen und mit
`RUN` gestartet.

Erkannt wird „BASIC ist bereit“ daran, dass der C64 in der Warteschleife des
KERNAL auf eine Taste wartet (`$E5CD`–`$E5D5`).

### Steuerung im Spiel

Vor jedem Spielstart zeigt ein Kasten zwei Sekunden lang diese Belegung, wie
bei PicoBoyGB.

| C64 | PicoBoy |
|---|---|
| Joystick (Port aus den Optionen) | Joystick |
| Feuer | B |
| RETURN | A |
| Leertaste (andere Taste je Spiel in den Optionen: RUN/STOP, F1–F7, Y, N, 1–4; im Spiel über Mitte + A umstellbar) | Mitte + B |
| Menü: Diskette, Settings, Taste drücken | Mitte + A |

**Menü** (Mitte + A, wie bei CPC, ZX und MSX aufgebaut) hält das Spiel an:
„Swap disk“ (bei Diskettenspielen), „Settings“ (Joystick-Port, außer im
Link-Spiel, und die Taste auf Mitte + B; gelten sofort und werden beim Spiel
gespeichert, außer im Link-Spiel), „Press a key“ (eine Taste einmal drücken:
RETURN, SPACE, RUN/STOP, F1–F7, 0–9, A–Z; nicht im Link-Spiel) und ganz unten
„Back to game“ (oder A). „Press a key“ hilft bei Trainer-Fragen und
Titelbildern: Boulder Dash (Nova) etwa fragt „Endless Rockfords? (Y/N)“ und
will dann F1 und Feuer an Port 1.

**Diskettenwechsel:** „Swap disk“ zeigt alle `.d64` auf dem Laufwerk, nach
Namen sortiert, wie die Spieleliste. Die im Laufwerk ist
markiert („in drive“) und vorausgewählt. Hoch/runter wählt, links/rechts
blättert, B legt sie in Laufwerk 8, A geht ohne Wechsel zurück. Ein zu langer
Name läuft in der markierten Zeile durch. Nach einem Wechsel steht der Name
zwei Sekunden unter dem Bild.

Unter **„Settings“** öffnet „Center + B: …“ die Tastenliste (SPACE,
RUN/STOP, F1–F7, Y, N, 1–4), „Joystick: port …“ schaltet den Port um. Beides
gilt sofort und steht beim nächsten Start auch in den Spieloptionen; im
Link-Spiel bleibt es bei dieser Sitzung (die Taste hat dort jeder für sich).

Zurück zur Spieleliste geht es mit RESET. Mit `-DSHOW_FPS=1` gebaut (in
PBCemu: Option im Starter), stehen unter dem Bild gezeichnete Bilder pro
Sekunde, Emulationstempo (100 % = 50 Bilder/s, PAL) und die Auslastung von
Kern 0.

## Bild

Frodo zeichnet 384×272 Pixel samt Rahmen. Gezeigt wird derselbe Ausschnitt wie
bei frank-c64 auf HDMI: 320×240, ab Spalte 32 und Zeile 16. Das wird auf
240×180 verkleinert, genau wie beim Mega Drive: Von je vier Pixeln bleiben die
äußeren, die mittleren werden gemittelt, waagerecht und senkrecht. So bleibt
die 8-Pixel-Schrift des C64 lesbar.

In PBCemu wählt man im Starter einen Anzeigemodus (eigenständig gebaut gilt
immer „Standard“):

| Modus | Ausschnitt | Auf dem Display |
|---|---|---|
| Standard | 320×240 wie oben | 240×180 |
| Full border | ganzer Puffer 384×272 | 240×170 (5/8) |
| No border | nur der Bildschirm 320×200 | 240×150 (3/4) |

Alle Modi behalten das Seitenverhältnis.

Kern 1 rechnet das Bild nach jedem Frame um (in Standard und No border
gruppenweise mit einer Tabelle der 256 Farbmischungen: dieselben Pixel, am PC
gut dreimal so schnell) und schickt es per DMA, während
Kern 0 schon den nächsten emuliert. Weil Kern 1 schneller von oben liest, als
der VIC neu zeichnet, reicht ein einziger Bildpuffer.

## T64-Archive

Eine `.t64` ist trotz des Namens kein Bandabbild, sondern ein Behälter des
alten Emulators C64S: Programme, wie sie im Speicher liegen, mit einem
Inhaltsverzeichnis davor. Der PicoBoy hängt sie wie eine Diskette als
Laufwerk 8 ein (`core/rp2350/T64Drive.cpp`): `LOAD"*",8,1` holt das erste
Programm, und ein Spiel, das weitere Teile nachlädt, findet sie im Archiv
unter ihrem Namen; `LOAD"$",8` zeigt den Inhalt. Nur lesen; die bei vielen
Archiven falsche Endadresse ersetzt der Abstand zum nächsten Programm. „True
drive“ gilt für T64 nicht (die echte 1541 braucht eine Diskette). Auch ein LOAD
vom **Band** (Gerät 1) kommt aus dem Archiv: Mit eingelegter T64 ist der
KERNAL-Eintrag LOAD (`$FFD5`) eine Falle (`$F2 $30`, `core/CPUC64.cpp`); für
Gerät 1 liefert sie ohne Namen die Datei nach der zuletzt geladenen, wie das
Band, mit Namen die erste, deren Name so beginnt; alle anderen Geräte gehen
weiter wie sonst. Spiele, die den Vektor `$0330` direkt anspringen, sehen das
nicht; das BASIC der OpenROMs lehnt `LOAD"",1` selbst ab („NOT IMPLEMENTED“),
Programme, die den KERNAL aufrufen, gehen auch dort. Am PC mit
einem T64 aus den vier Dateien von Great Giana Sisters geprüft: Start und
Nachladen der Highscores; das Band mit einem BASIC-Lader, der `LOAD"",1,1`
den zweiten Teil holt und anspringt.

## ROMs: OpenROMs statt Commodore

Die Original-ROMs von Commodore sind nicht frei. PBC C64 nutzt deshalb
[MEGA65 OpenROMs](https://github.com/MEGA65/open-roms), gebaut ohne die
Schnelllader JiffyDOS und DolphinDOS. Frodos Laufwerksemulation hängt sich auf
dem Original-KERNAL in dessen interne Routinen ein; bei OpenROMs übernehmen
das Fallen an den Einsprüngen der OpenROMs-eigenen IEC-Routinen (Einzelheiten
in [`tools/openroms/README.md`](tools/openroms/README.md)). Auf dem PC
geprüft: Booten, `LOAD"*",8,1` mit VVVVVV, Inhaltsverzeichnis, Löschen über
den Befehlskanal und Speichern. VVVVVV ließ sich anfangs im Menü nicht
bedienen, weil OpenROMs die Speicherstelle `$CB` (gerade gedrückte Taste) nicht
pflegt. Das ergänzt ein kleiner Patch am KERNAL. Seitdem verhält sich das Spiel
auf dem PC bis ins erste Level genauso wie mit den Original-ROMs.

Unterschiede: Der Startbildschirm zeigt „OPEN ROMS PBC C64“, das BASIC kennt
einige Befehle nicht (`OPEN`, `CLOSE`, `INPUT#`), und Spiele, die direkt in
Routinen des Original-KERNAL springen, können abstürzen. Wie viele Spiele
das betrifft, lässt sich nicht vorhersagen; geprüft ist nur VVVVVV.

### Eigene Commodore-ROMs

Wer die Original-ROMs besitzt, kopiert sie auf das USB-Laufwerk, ins
Hauptverzeichnis:

| Datei | Größe | ROM |
|---|---|---|
| `kernal.rom` | 8192 | KERNAL 901227-03 (oder -02) |
| `basic.rom` | 8192 | BASIC V2 901226-01 |
| `chargen.rom` | 4096 | Zeichensatz 901225-01, freiwillig |

Statt `.rom` geht auch `.bin` oder gar keine Endung. Sind KERNAL und BASIC da
und ist der KERNAL ein unveränderter, nimmt der Emulator diese statt OpenROMs.
Die Fallen für die Laufwerksemulation setzt er dabei im RAM (`src/c64_roms.c`).
Ein veränderter KERNAL (JiffyDOS o. Ä.) wird nicht genommen. Welche ROMs
laufen, zeigt die Steuerungsübersicht vor dem Spielstart: „ROMs Commodore“
oder „ROMs OpenROMs“. In der Firmware und im Repo steckt nichts davon.

### True drive: die echte 1541

Normalerweise emuliert Frodo Laufwerk 8 auf DOS-Ebene: Es beantwortet die
Befehle auf dem seriellen Bus direkt, ohne Laufwerksprozessor. Das ist schnell,
aber Spiele mit eigenem Schnelllader (der Code in die 1541 lädt, z. B. Sam's
Journey) hängen dann beim Laden. Für sie gibt es in den Optionen eines Spiels
**True drive**: Dann läuft ein zweiter 6502 mit dem ROM der 1541, die Diskette
dreht sich als GCR-Daten unter dem Lesekopf, und der KERNAL spricht wie auf
echter Hardware über den seriellen Bus.

Dafür braucht es das ROM der 1541, das nicht frei ist und deshalb nicht
mitkommt: `1541.rom` (16384 Bytes, 325302-01 + 901229-05; auch `1541.bin`,
`dos1541.rom`) ins Hauptverzeichnis des Laufwerks. Fehlt es, läuft das Spiel
mit der schnellen Emulation, und unter dem Bild steht kurz „No 1541.rom“.
Das ROM bekommt Frodos Patches im RAM (keine Prüfsumme, Schlaf in der
DOS-Leerlaufschleife); aus dem KERNAL nimmt der Emulator die Fallen der
DOS-Emulation wieder heraus (bei OpenROMs mit den Originalbytes aus
`BuiltinKernalTraps`, erzeugt von `tools/openroms/make_rom_data.py`).

Die LED des Laufwerks ist die RGB-LED im „+“ (WS2812 an GPIO 11, per PIO,
`common/pbc_rgb.c`): Sie leuchtet gedämpft grün, solange die 1541 sie
einschaltet, und blinkt bei einem Laufwerksfehler wie das Original. Der
Launcher schaltet sie beim Start aus, denn sie behält ihre Farbe auch über
RESET hinweg.

Kosten: Solange das Laufwerk arbeitet, wechseln sich die beiden Prozessoren
Befehl für Befehl ab. Mit True drive läuft der C64 deshalb mit 200 MHz statt
150 MHz. Frodo hielt die ganze Diskette als GCR im RAM (gut 250 KB); hier
liegt nur die Spur unter dem Lesekopf im Speicher (höchstens knapp 8 KB) und
wird beim Spurwechsel aus der Datei erzeugt (`core/1541gcr.cpp`). Schreiben
auf die Diskette geht mit True drive nicht über Frodos Fallen; ob ein Spiel
dabei speichern kann, ist ungeprüft. Den Wechsel Befehl für Befehl durch
ein Nachziehen der 1541 nur bei Zugriffen auf `$DD00` zu ersetzen, brächte
wenig: Beim Laden liest der C64 `$DD00` etwa bei jedem dritten Befehl, die
1541 käme selten mehr als drei Befehle am Stück voran.

Beim Umbau auf OpenROMs sind zwei Fehler aus frank-c64 aufgefallen und
behoben: Die Klasse `Prefs` war zweimal mit verschiedenem Aufbau definiert
(`Prefs_rp2350.h` ohne zwei Felder von `Prefs.h`), sodass `ThePrefs = prefs`
beim Einhängen einer Diskette über das Objekt hinaus schrieb; und die CPU rief
die Kassettensteuerung über einen Nullzeiger auf.

Ein dritter Fehler aus frank-c64 betraf Interrupts: Nach `CLI`, `PLP` und
`RTI` nahm die CPU einen anstehenden IRQ erst in der nächsten Rasterzeile an,
der echte 6510 nach dem nächsten Befehl (nach `RTI` sofort). Folgte innerhalb
der Zeile ein `SEI`, kam der IRQ nie, und ein unbeantworteter CIA-Interrupt
blieb für immer anstehend. Bei Great Giana Sisters liefen dann die
Raster-Interrupts des Titelbilds pausenlos hintereinander: Der Scroller
zitterte, und die Hauptschleife, die die Taste 1 abfragt, kam nie dran.
Seit der Korrektur in `core/CPU_emulline.h` kommen die Interrupts an
denselben Zeilen wie in VICE.

## Ton

SID über den Lautsprecher an GP15: Ein DMA-Kanal, getaktet von einem
DMA-Timer, holt alle 22 µs einen Sample aus einem Ringpuffer und schreibt ihn
in den PWM-Wert (`src/audio_pwm.c`, wie in `../sms`). Es gibt keinen
Interrupt pro Sample mehr. Früher tat das ein Timer-Interrupt 22 000-mal pro
Sekunde, und zwar auf Kern 1: Beim C64 reicht die Aufrufkette auf Kern 0 tief
(CPU → Laufwerk → FatFs), und der Interrupt konnte dort den 4-KB-Stack
sprengen (die erste Fassung mit dem Timer auf Kern 0 stürzte mit Ton ab).
Mit DMA stellt sich die Frage nicht mehr. Der Ton ist per Vorgabe aus.

Ist er aus (oder die Lautstärke 0), rechnet der Emulator die Samples gar
nicht erst aus (`c64_sid_enable()`, `core/rp2350/SID_rp2350.cpp`); das
spart auf Kern 0 ein Fünftel bis ein Viertel der Rechenzeit (am PC
gemessen). Das Spiel
merkt davon nichts: Register, OSC3 und ENV3 kommen aus Frodos eigener
Nachbildung der dritten Stimme, nicht aus der Klangerzeugung. Umschalten
lässt sich der Ton ohnehin nur vor dem Spielstart.

## Was im RAM läuft

Spiel-ROMs gibt es beim C64 nicht, aus dem Flash kamen aber Emulatorcode und
die eingebauten ROMs, alles durch denselben 16-KB-Cache. Allein die
6510-Schleife (`MOS6510::EmulateLine`) ist 32 KB groß. Deshalb liegen im RAM
(Makro `C64_HOT` in `core/sysdeps.h`):

- die 6510-Schleife samt Speicherzugriffen, VIC-Zeile mit allen
  Zeichenroutinen (`el_*`), die CIA-Zeile und -Register sowie die SID-Ausgabe
  mit der Hüllkurventabelle, zusammen rund 52 KB;
- mit True drive die Speicherzugriffe der 1541 (ROM und RAM inline, VIAs,
  `read_word`, ADC/SBC), VIA-Timer und -Interrupts, der Lesekopf in
  `GCRDisk` (`rotate_disk`, `ByteReady` …; nur das Füllen des Spurpuffers
  bleibt im Flash) und `c64_run_frame` selbst;
- kleine Helfer, die sonst aus dem RAM über Veneers in den Flash sprangen:
  OSC3/ENV3 des SID, `ChangedVA` des VIC, ROML/ROMH der Cartridges und
  `rand()` für die offenen Bits beim Lesen des Farb-RAMs (`CPUC64.cpp`:
  dieselbe Rechnung auf demselben Zustand wie newlibs `rand()`, also auch
  dieselbe Folge, wichtig fürs Link-Spiel);
- BASIC, KERNAL und Zeichensatz: Die eingebauten OpenROMs werden beim Start
  in die RAM-Puffer kopiert, die sonst eigene ROMs aufnehmen
  (`c64_roms_fill_builtin`).

Für den Heap bleiben rund 197 KB (vorher 249 KB). Den Flash-Takt setzt die
Firmware auch bei 150 MHz selbst (Teiler 2 = 75 MHz); nach jedem Schreiben
aufs Laufwerk stellt `flash_fs.c` ihn wieder her, weil die Flash-Funktionen
des SDK die Einstellung des Bootroms zurückschreiben.

Achtung beim RAM-Code: Direkt hinter `.data` liegt der Ringpuffer des Tons,
der auf 8 KB ausgerichtet sein muss. Wächst `.data` über die nächste
8-KB-Grenze, kostet das auf einen Schlag 8 KB Heap (im November 2026 endete
`.data` in der Gesamtfirmware gut 1,6 KB davor, bei `0x20016000`; nachsehen
mit `arm-none-eabi-nm -n … | grep __data_end__`).

Außerdem überspringt der VIC Sprite-Zeilen ohne gesetzten Pixel und löscht
den Kollisionspuffer nur unter den Sprites, die zeichnen (statt 384 Bytes je
Zeile, `el_sprites` in `core/VIC.cpp`). Diese Änderungen und die oben bei
Ton und Bild sind am PC bitgenau gegen den Stand davor geprüft: Bild,
SID-Samples, C64- und Laufwerks-RAM nach jedem Frame, Boulder Dash und Great
Giana Sisters über Tausende Frames mit Eingaben, mit und ohne True drive, mit
Commodore-ROMs und OpenROMs.

## Absturzanzeige

Es gibt keine serielle Konsole. Stürzt die Firmware ab, zeigt das Display
„CRASH“ mit Kern (Core), PC, LR und dem Fehlerregister CFSR. PC und LR lassen sich
mit `arm-none-eabi-addr2line -e build/pbc_c64.elf <Adresse>` einer
Quellzeile zuordnen.

## Disketten und Grenzen

* **Spielstände auf Diskette gehen.** Das USB-Laufwerk liegt im selben Flash,
  aus dem beide Kerne ihren Code ausführen, und während ein Sektor gelöscht
  wird, ist der Flash nicht lesbar. Deshalb wird für jeden Sektor Kern 1 kurz
  in einer RAM-Schleife geparkt (multicore lockout) und Kern 0 sperrt seine
  Interrupts – das Spiel steht dabei für etwa 50 ms pro 4-KB-Sektor still.
  FatFs puffert Schreibzugriffe; damit ein Spielstand ein RESET übersteht,
  werden gepufferte Daten nach einer halben Sekunde ohne weitere
  Schreibzugriffe durchgeschrieben. **Nach dem Speichern also kurz warten**,
  bevor man RESET drückt oder ausschaltet. Fällt der Strom mitten im Schreiben
  aus, kann das Image beschädigt werden.
* **Echte 1541 nur mit True drive.** Wie bei frank-c64 läuft das Laufwerk
  sonst auf DOS-Ebene (Frodo-IEC-Emulation), nicht als eigene 6502-CPU – das
  spart Rechenzeit und lädt schnell. Spiele mit eigenem Schnelllader oder
  Kopierschutz brauchen „True drive“ und `1541.rom`, siehe oben.
* Nur Laufwerk 8. Kein Bandlaufwerk.
* **Cartridges (`.crt`):** Beim ersten Start legt `src/cart_store.c` die Bänke
  geordnet in den ROM-Bereich, in dem die anderen Emulatoren ihr Spiel
  ablegen (mit Ladebildschirm; eine EasyFlash mit 1 MB braucht ein paar
  Sekunden), mit demselben Infosatz wie deren `rom_store.c`. Danach liest der
  Kern die Bänke direkt aus dem Flash und beim Einstecken nur noch die
  CHIP-Köpfe der Datei. Die Vorlage schrieb die Abbilder an Flash-Adresse
  1 MB, mitten in die Gesamtfirmware. Nicht emuliert: Schreiben in den Flash
  einer EasyFlash (Spielstände solcher Cartridges gehen beim Ausschalten
  verloren); andere Typen (GMod2, Action Replay …) werden mit einer Meldung
  abgelehnt. Ein Spiel, das ständig Code aus der Cartridge ausführt, kann
  langsamer laufen als von Diskette (16 KB XIP-Cache).

## Bauen

```bash
./build.sh                       # -> ../dist/c64.uf2
./build.sh clean
./build.sh -DCPU_MHZ=336 -DCPU_VREG=VREG_VOLTAGE_1_35
```

Werkzeuge und SDK wie bei allen Projekten dieses Repos, siehe
[`../docs/BAUEN.md`](../docs/BAUEN.md). Frodo4 braucht C++20.

| Option | Vorgabe | Bedeutung |
|---|---|---|
| `CPU_MHZ` | 150 | Systemtakt (bei 300 MHz lag die Last unter 30 %) |
| `CPU_VREG` | `VREG_VOLTAGE_1_10` | Kernspannung (bei 150 MHz ohnehin die Grundeinstellung) |
| `FLASH_MAX_FREQ_MHZ` | 88 | Obergrenze für den Flash-Takt (bei 150 MHz: 75 MHz) |
| `PBC_SPI_HZ` | 75 000 000 | Display-SPI: 150 MHz / 2 = 75 MHz (über dem Datenblattwert, läuft auf dem Gerät sauber) |
| `SHOW_FPS` | 0 | Anzeige unter dem Bild (fps, Tempo, Last) |

## Flash-Aufteilung (16 MB)

| Adresse | Größe | Inhalt |
|---|---|---|
| `0x000000` | 1 MB | Firmware (ca. 280 KB) |
| `0x600000` | ~9,9 MB | FatFs-Partition = USB-Stick (`src/flash_fs.h`) |
| `0xFF2000` | 4 KB | Optionen: Ton, Lautstärke, Joystick-Port und Datei je Spiel (bis 150 Spiele, `src/settings.c`) |

Der ROM-Bereich ab `0x100000`, den `megadrive` und `nes` benutzen, bleibt
unberührt: Der C64 liest die Images direkt aus dem Dateisystem.

## Aufbau

```
src/main.c          Startablauf, USB-Stick-Modus, Takt, Autostart, Emulationsschleife
src/c64_video.c     Anzeigemodi/Skalierung, Ausgabe auf Kern 1
src/c64_input.c     Tasten -> Joystick-Port, RETURN, Leertaste
src/c64_audio.c     SID-Samples -> Lautsprecher
src/flash_fs.c      Flash-Laufwerk; parkt Kern 1 beim Schreiben
src/settings.c      Optionen im Flash, Joystick-Port und Datei je Spiel
src/d64_dir.c       Inhaltsverzeichnis einer .d64 für die Dateiauswahl
src/menu.c          Spieleliste und Optionen
src/c64_roms.c      eigene Commodore-ROMs; die eingebauten im RAM
src/audio_pwm.c     Lautsprecher per DMA, wie in ../sms
src/display.c, input.c   aus ../nes
src/flash_fs.*, usb_*.c, tusb_config.h, fatfs/   aus ../picoboygb
core/               Frodo4 aus frank-c64 (zeilenbasierte Fassung)
core/rp2350/        frank-c64-Anbindung, angepasst:
                    Display ohne HDMI, SID mit 45 454 Hz (bis November 2026 22 222 Hz, die Filter daher mit halber Grenzfrequenz), verzögertes f_sync
                    beim Schreiben, c64_get_pc() für den Autostart
```

## Lizenzen

* Gesamtwerk: **GPL-2.0-or-later** wie frank-c64 und Frodo4, siehe `LICENSE`.
* `src/flash_fs.*`, `src/usb_msc.c`, `src/usb_descriptors.c`,
  `src/tusb_config.h`, `src/font8x8.h`, `fatfs/diskio.c`: aus PicoBoyGB,
  **MIT**, siehe `LICENSE.picoboygb`.
* `fatfs/`: ChaN FatFs, eigene BSD-artige Lizenz (im Dateikopf).
* C64-ROMs: **MEGA65 OpenROMs**, ein freier Nachbau von BASIC, KERNAL und
  Zeichensatz, **LGPL-3.0-or-later** (einige BASIC-Teile MIT). Herkunft,
  Änderungen, Lizenztexte und der Weg zum Neubauen stehen in
  [`tools/openroms/`](tools/openroms/). Die Original-ROMs von Commodore sind
  nicht enthalten; ein 1541-ROM wird nicht gebraucht.
