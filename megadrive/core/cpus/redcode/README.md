# redcode/Z80 im Mega-Drive-Emulator

Z80-Kern von Manuel Sainz de Baranda y Goñi, https://github.com/redcode/Z80
(Stand `dcc3f28`, 25.10.2026), **LGPL-3.0-or-later** (`COPYING.LESSER`, `COPYING`).
Dazu die nötigen Header seiner Bibliothek Zeta, https://github.com/redcode/Zeta
(Stand `42830b0`), ebenfalls LGPL-3.0 (`Z/`, Dateiköpfe).

Er ersetzt den Z80-Kern von Marat Fayzullin, der nur für nicht-kommerzielle
Zwecke frei ist. Gwenesis spricht ihn über `../../sound/z80inst.c` an; dort
liegt auch die Speicherkarte des Z80 (RAM, YM2612, Bankregister, PSG,
Bankfenster), die vorher `z80_mem_opt.S` war.

`Z80.c` ist das Original mit Änderungen von `../../../tools/redcode_patch.py`,
alle hinter Schaltern; ohne sie ist es der unveränderte Kern:

| Schalter | Wirkung |
|---|---|
| `Z80_PBC_FETCH8K=Z80_RAM` | Opcodes und Operanden direkt aus dem 8-KB-Z80-RAM (Code läuft nur dort, wie beim alten Kern) |
| `Z80_PBC_READ=z80_read` | Datenzugriffe über die Speicherkarte in `z80inst.c` |
| `Z80_PBC_WRITE=z80_write` | Schreiben über die Speicherkarte in `z80inst.c` |
| `Z80_PBC_SWITCH` | die 256 Opcodes über `switch` statt Funktionstabelle; der Compiler baut die Befehle mit konstantem Opcode in die Schleife ein |
| `Z80_PBC_RAM`, `Z80_PBC_RAM_TABLES` | Code und Tabellen ins RAM |
| `Z80_PBC_SWITCH_XY` | nur in PBCemu (genug RAM): auch die IX/IY-Befehle über `switch`, die Soundtreiber viel nutzen |

Gemessen am PC und als Cortex-M33-Code (Unicorn, ZEXDOC): alle 67 Tests von
ZEXDOC bestanden. Auf dem Gerät (Benchmark, 30 s Spiel ohne Eingabe) brauchte
die erste Fassung mit Tabellen im Flash 15,9 % statt 12,0 % der Rechenzeit für
den Z80; daraufhin die Tabellen ins RAM und der IX/IY-`switch`. Die Anbindung ist am PC mit einem kleinen Z80-Programm geprüft
(Interrupt, YM2612, PSG, Bankfenster, Zeitstempel).

Neu erzeugen: `python3 ../../../tools/redcode_patch.py <redcode>/sources/Z80.c Z80.c`
