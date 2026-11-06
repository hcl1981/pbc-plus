# Link-Spiel: zwei PicoBoy Color Plus, ein Spiel

Zwei Geräte mit USB-C-Kabel verbunden spielen **ein** Atari-2600-, NES-, SG-1000-,
MSX-, ZX-Spectrum-, Amstrad-CPC-, Master-System-, PC-Engine-, Mega-Drive- oder C64-Spiel zu zweit: Das eine Gerät ist
Controller 1, das andere Controller 2, beide zeigen dasselbe Bild.

Das kann PBCemu (`dist/pbcemu.uf2`) seit November 2026; bis dahin war es eine
eigene Firmware (PBClink). Game Gear und Game Boy (Color) spielen nur allein. Im Karussell tragen die Konsolen mit Link-Spiel eine runde goldene
Plakette **LINK**. Die Einzel-Firmwares (`nes.uf2`, `sms.uf2`, …) haben kein
Link-Spiel.

```bash
pbcemu/build.sh        # -> dist/pbcemu.uf2
common/test/run.sh     # Protokolle am PC, über eine fehlerhafte Leitung
```

**Stand:** Finden und Spielen zu zweit laufen mit NES auf zwei echten Geräten.
Master System, PC Engine, C64, MSX, ZX Spectrum, Amstrad CPC, SG-1000 und
Atari 2600 sind im Link-Spiel auf Geräten noch nicht getestet. MSX und Atari 2600 laufen allein auf dem Gerät
in Echtzeit, SG-1000 bisher nur am PC.

## Benutzung

1. `pbcemu.uf2` auf beide Geräte flashen. Laufwerk und Spiele bleiben.
2. Kabel: USB-C ↔ USB-C mit **D+, D− und GND verbunden, VBUS nicht**, wie bei
   i2collection und Jump 'n Bump.
3. Beide Geräte im Starter lassen. Innerhalb von etwa einer Sekunde steht oben
   grün *Link connected*.
4. Auf einem Gerät ein Spiel wählen und mit **B** starten, dann *Player 1* oder
   *Player 2* wählen. Das andere Gerät startet dasselbe Spiel von selbst, egal
   wo es im Starter gerade steht, und bekommt den freien Controller.
5. RESET beendet das Spiel. Das andere Gerät meldet nach 3 Sekunden
   *Connection lost* und geht zurück zum Starter.

Ohne Partner im Starter startet ein Spiel ganz normal allein. Hat das andere
Gerät die Datei nicht, steht das auf dem startenden Gerät. Wählen beide fast
gleichzeitig, gewinnt eines; das andere startet dessen Spiel.

Beide Geräte brauchen dieselbe ROM-Datei. Beim C64 müssen auch Diskette (auch
nach einem Spielstand darauf), ROMs, Joystick-Port, Programm und True drive
gleich sein; sonst steht dort *Game or settings differ*.

Das Gerät, das das Spiel gestartet hat, gibt den Takt vor. Das ist unabhängig
vom Controller.

| System | Spieler 1 | Spieler 2 |
|---|---|---|
| NES | Controller 1 | Controller 2 |
| Master System | Controller 1 | Controller 2; PAUSE geht von beiden |
| PC Engine | Controller 1 | Controller 2 am eingebauten Multitap |
| C64 | der Port aus den Spieloptionen | der andere Port |
| SG-1000 | Controller 1 | Controller 2 |
| MSX | Joystick 1 und Tastatur | Joystick 2 |
| ZX Spectrum | Joystick-Art aus dem Menü (Kempston, Sinclair 1 …) | Sinclair-Joystick 2 (Tasten 1–5); SPACE, ENTER und Menütasten gehen von beiden |
| Amstrad CPC | Joystick 0 (Art aus dem Menü) | Joystick 1 (Tastaturzeile 6); RETURN und Menütasten gehen von beiden |
| Atari 2600 | linker Joystick | rechter Joystick; RESET und SELECT gehen von beiden |
| Mega Drive | Controller 1 | Controller 2 |

Beim C64 hat jeder Spieler seine eigene Taste auf Center + B und RETURN auf A.
**Diskettenwechsel:** Wer Center + A drückt, wählt die Diskette; beide Geräte
halten nach demselben Bild an, beim anderen steht *Your partner is choosing a
disk*. Danach legen beide dieselbe Diskette ein. Dazu muss das andere Gerät sie
unter demselben Namen und mit demselben Inhalt haben, sonst bleibt auf beiden
die alte drin und unter dem Bild steht, was fehlt. Drücken beide gleichzeitig,
wählt Spieler 1. Beim MSX geht das Tastenmenü (Center + A) genauso; eine MSX2-Diskette wird
auf beiden Geräten in die eigene Datei geschrieben, beide brauchen also
dieselbe. Der Game Gear bietet kein Link-Spiel an, er hat keinen zweiten
Controller; der Game Boy verbindet sich stattdessen wie mit einem Linkkabel
(unten).

Beim Mega Drive liefert das Sprite-Overflow-Flag des Videochips im
Link-Spiel immer 0: Es entsteht beim Zeichnen auf Kern 1 und hinge sonst
davon ab, wann und ob ein Bild gezeichnet wird. Kaum ein Spiel liest es.

## Game Boy: Linkkabel

Der Game Boy spielt nicht im Gleichschritt, sondern wie mit einem echten
Linkkabel: In den Einstellungen des Spiels (Mitte in der Spieleliste) den
Schalter **Link cable** einschalten, auf beiden Geräten, dann startet jeder
sein eigenes Spiel. Die Spiele müssen nicht gleich sein: Pokémon Rot tauscht
mit Blau, wie im Original. Beide Geräte laufen unabhängig; nur ein Byte, das
eines der Spiele über seinen seriellen Anschluss schickt, geht über das
USB-C-Kabel, und das Byte des anderen kommt im selben Austausch zurück
(`common/pbc_cable.c`). Wer ein Byte hat, klopft an, das andere Gerät schaut
bei jeder Bildzeile nach (etwa alle 0,1 ms); ein Austausch dauert weniger als
eine Millisekunde, so lange wie ein Byte am echten Kabel. Ohne Partner
antwortet niemand, und das Spiel sieht ein Kabel ohne Gegenüber.

## Wie es funktioniert

Es wird kein Bild übertragen. Beide Geräte rechnen dasselbe Spiel, und vor
jedem Bild tauschen sie ihre Tasten (`common/pbc_lockstep.c`, 16 Byte je
Richtung, rechnerisch gut 1 ms). Keines rechnet ein Bild, bevor es die Tasten des anderen
hat. Dazu kommt keine Eingabeverzögerung, denn beide lesen ihre Tasten direkt
vor dem Tausch.

* **Transport:** `common/usblink.c` aus der Doom-Portierung (auch in Tyrian):
  D+/D− als getaktete Leitung, ohne USB-Protokoll. Das startende Gerät ist Master.
* **Finden:** `common/pbc_lobby.c`. Im Starter schaut ein 1-ms-Timer auf die
  Leitung und klopft etwa einmal pro Sekunde für 5 ms an. Dabei gibt es keinen
  festen Master; klopfen beide zugleich, trennt sie die zufällige Pause.
* **Takt:** Der Master hält die Bildrate, der andere wartet nur auf ihn. So laufen
  die Quarze nicht auseinander. Schafft eines ein Bild nicht, warten beide.
* **Fehler:** Ein Austausch kann in jeder Phase scheitern. Der Master wiederholt,
  und jede Antwort trägt auch die Eingabe des Bildes davor, falls der Slave
  schon weiter ist (Kommentar am Anfang von `pbc_lockstep.c`).
* **Start:** `pbc_link_start()` vergleicht eine Prüfsumme über ROM bzw. Diskette,
  ROMs und Optionen, bevor das erste Bild läuft.
* **Gleichlauf-Kontrolle:** Jedes Bild geht eine Prüfsumme über 256 Byte RAM
  mit, reihum über den ganzen Speicher. Unterscheiden sie sich, steht
  `LINK OUT OF SYNC` unter dem Bild; das Spiel läuft weiter.

Damit das aufgeht, muss die Emulation deterministisch sein. Im Code geprüft:
NES und Master System setzen ihren Speicher beim Start fest (`nes_core.cpp`,
`sms.c`), überspringen beim Rechnen keine Bilder und hängen nicht von der Zeit
ab; Ton an/aus ändert nur die Ausgabe. Der C64 nutzt `rand()` (Farb-RAM,
offener Bus) und startet deshalb mit `srand(1)`; der Autostart hängt nur vom
Zustand der Emulation ab. Die Kerne von MSX und Atari 2600 sind dafür
geschrieben: fester Start, keine Zeit, kein Zufall.

## Wo der Code liegt

| Datei | Aufgabe |
|---|---|
| `common/pbc_lockstep.*`, `pbc_link.*`, `pbc_lobby.*`, `usblink.*` | Tastentausch, Verbindung, Partnersuche, Leitung |
| `common/test/` | die Protokolle am PC zweimal gelinkt, über eine Leitung mit Fehlern |
| `pbcemu/src/ui.c`, `main.c`, `devices.*` | Partner finden, Controller wählen, Spiel anbieten, nur bei Link-Geräten (`device_link`) |
| `nes/`, `sms/`, `pce/`, `c64/`, `msx/`, `a2600/`, `zx/`, `cpc/`, `megadrive/` (`src/main.c`) | je ein Aufruf von `pbc_link_frame()` pro Bild, Controller 2, Prüfsumme beim Start |

In den Emulatoren sind es nur wenige Zeilen, alle unter `#if PBC_MULTI`.

`usblink.c` und `usblink.h` stehen wie die Doom-Portierung unter GPL-2.0-or-later.
