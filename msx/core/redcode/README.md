# redcode/Z80 im Master-System-Emulator

Z80-Kern von Manuel Sainz de Baranda y Goñi, https://github.com/redcode/Z80
(Stand `dcc3f28`, 25.10.2026), **LGPL-3.0-or-later** (`COPYING.LESSER`, `COPYING`).
Dazu die nötigen Header seiner Bibliothek Zeta, https://github.com/redcode/Zeta
(Stand `42830b0`), ebenfalls LGPL-3.0 (`Z/`, Dateiköpfe).

Er ersetzt den Z80-Kern aus MAME (Juergen Buchmueller), der nur für
nicht-kommerzielle Zwecke frei ist. SMS Plus spricht ihn über
`../smsplus/z80_redcode.c` an, mit der alten Schnittstelle aus `z80.h`.

`Z80.c` ist das Original mit Änderungen von `../../tools/redcode_patch.py`,
alle hinter Schaltern; ohne sie ist es der unveränderte Kern:

| Schalter | Wirkung |
|---|---|
| `Z80_PBC_MAP`, `Z80_PBC_READMAP=cpu_readmap` | Lesen direkt aus SMS Plus' 8-KB-Seitentabelle statt über Rückruf |
| `Z80_PBC_WRITE=cpu_writemem16` | Schreiben direkt über SMS Plus' Handler (Mapper-Register) |
| `Z80_PBC_SWITCH` | die 256 Opcodes über `switch` statt Funktionstabelle; der Compiler baut die Befehle mit konstantem Opcode in die Schleife ein |
| `Z80_PBC_RAM` | Code und Tabellen ins RAM (nicht neben dem ROM im XIP-Cache) |

Gemessen am PC und als Cortex-M33-Code (Unicorn, ZEXDOC): alle 67 Tests von
ZEXDOC bestanden; ARM-Befehle pro Z80-Takt etwa 10 % mehr als beim MAME-Kern
(mit `z80_run`, das die Interrupts zwischen jedem Befehl prüft), Code und
Tabellen 38 KB statt 74 KB.

Neu erzeugen: `python3 ../../tools/redcode_patch.py <redcode>/sources/Z80.c Z80.c`
