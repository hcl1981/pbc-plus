# Musashi im Mega-Drive-Emulator

68000-Kern **Musashi 4.10** von Karl Stenerud, https://github.com/kstenerud/Musashi
(Stand `313ebf1`, 8.3.2026), **MIT-Lizenz** (`readme.txt`, Dateiköpfe).
Er ersetzt seit November 2026 den 68000-Kern aus Genesis Plus GX (Musashi 3.32
in Eke-Ekes Fassung), dessen Lizenz keine kommerzielle Nutzung erlaubt. Von
dort ist nichts übernommen; was das Mega Drive anders braucht als Musashi,
ist hier neu geschrieben oder aus Motorolas Handbuch und veröffentlichten
Messungen (Yacht, Jorge Cwik) belegt.

## Dateien

| Datei | Herkunft |
|---|---|
| `m68k_in.c` | Musashis Befehlsbeschreibung, mit den Änderungen unten (`PBC:`) |
| `m68kops.c` | erzeugt von `../../../tools/m68k_gen.py` aus `m68k_in.c` (nicht von Hand ändern) |
| `m68kcpu.h` | Musashis Header, auf den 68000 und das Mega Drive zugeschnitten |
| `m68kcpu.c` | Musashis Tabellen; Ablauf, Interrupts, Reset, Spielstände neu |
| `m68k.h`, `m68kconf.h` | Schnittstelle zum Emulator und Einstellungen, neu |
| `readme.txt` | Musashis Beschreibung mit der Lizenz |

## Was anders ist als bei Musashi

**Nur 68000.** `m68kmake.c` erzeugt Routinen für 68000 bis 68040 und eine
Tabelle, die das Programm beim Start zu zwei Feldern mit je 64K Einträgen
aufbläht. `tools/m68k_gen.py` liest dieselbe Beschreibung, erzeugt dieselben
Routinen (Namen, Masken, Takte) und füllt die Tabelle wie Musashi für den
68000, aber schon beim Bauen: nur die 1701 Routinen, die ein 68000 erreicht,
und die Tabelle gepackt (Routine und Takte je Opcode, 32er-Abschnitte
zusammengelegt) auf 42 KB im RAM. Opcodes, die es erst ab dem 68010 gibt,
lösen wie beim 68000 die Illegal- bzw. Line-F-Ausnahme aus.

**Zeit in Master-Takten.** Das Mega Drive misst alles in Takten des 53-MHz-
Quarzes, sieben pro 68000-Takt. `m68k_run(t)` rechnet, bis `m68k.cycles`
mindestens `t` ist; die Tabelle enthält die Takte schon mal 7. STOP und ein
Sprung auf sich selbst (Warteschleife) beenden den Zeitabschnitt.

**Speicher.** ROM (unter $800000) und Arbeits-RAM ($FF0000 aufwärts) liest
der Kern direkt, alles andere über `gwenesis_bus.c`. Befehle und
PC-relative Daten kommen aus ROM oder RAM; anders als im alten Kern stimmt
das auch für Code, der im RAM läuft.

**Interrupts.** Der VDP setzt die Stufe (`m68k_set_irq`, `m68k_update_irq`)
und löscht sie in seiner Quittung (`m68k_set_int_ack_callback`).
Angenommen wird zwischen zwei Befehlen, am Anfang von `m68k_run` und wenn
ein Befehl die Maske im SR senkt. Autovektor-Interrupts brauchen 54 statt 44
Takte: Mit VPA wartet die Quittung auf den E-Takt (Prozessortakt / 10).

**Mega-Drive-Verhalten.** TAS schreibt nicht zurück (der Bus des Mega Drive
führt den Lese-Schreib-Zyklus nicht aus; Gargoyles und Ex-Mutants brauchen
das). Bei DIVU/DIVS mit Überlauf N gesetzt und C gelöscht, bei Division
durch null C gelöscht, wie beim 68000 (Blood Shot).

**Takte.** Gegenüber Musashis Tabelle korrigiert:

| Befehl | Musashi | jetzt | Quelle |
|---|---|---|---|
| ADDQ.W #,An | 4 | 8 | Handbuch (wie SUBQ) |
| ADD/SUB/AND/OR/ADDA/SUBA.L mit Dn/An | 6 | 8 | Handbuch |
| dieselben .W mit #imm | +2 | +0 | Handbuch (die +2 gelten nur für .L) |
| ANDI.L #,Dn | 14 | 16 | gemessen (Yacht), Handbuch 14 |
| DIVU, DIVS | 140, 158 fest | 76–140, 120–158 | Jorge Cwik, Ablauf des Mikrocodes |
| Autovektor-Interrupt | 44 | 54 | E-Takt-Synchronisation |

MULU und MULS rechnen wie bei Musashi 38 + 2n, n jetzt per `popcount`.

## Geprüft

Am PC mit `../../../tools/host` (mdhost): der alte und der neue Kern im
selben Emulator, Zustand (PC, SR, alle Register, Takte) vor jedem Befehl
verglichen. Sonic 1 (3000 Bilder), Sonic 2 (Chemical Plant mit Eingaben,
3000 Bilder; Spezialstufe, 2000), Sonic 3 (3000): zusammen 144 Millionen
Befehle ohne jeden Unterschied, nachdem die Takte oben korrigiert waren.

## Neu erzeugen

```bash
python3 tools/m68k_gen.py                         # m68kops.c aus m68k_in.c
./build.sh && python3 tools/m68k_hot.py build/pbc_megadrive.elf && ./build.sh
```

`tools/m68k_hot.py` markiert die meistbenutzten Routinen für den RAM
(`M68K_HOT1`, 48 KB, in beiden Firmwares; `M68K_HOT2`, 30 KB, nur in PBCemu);
`m68k_gen.py` behält die Marken beim Neuerzeugen.
