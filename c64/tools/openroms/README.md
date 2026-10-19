# OpenROMs für PBC C64

Der C64 braucht BASIC-, KERNAL- und Zeichensatz-ROM. Die Original-ROMs von
Commodore sind nicht frei. PBC C64 nutzt deshalb
[MEGA65 OpenROMs](https://github.com/MEGA65/open-roms), einen freien Nachbau
(**LGPL-3.0-or-later**, einige BASIC-Teile MIT von Microsoft, siehe
`LICENSE.openroms`, `COPYING.LESSER`, `COPYING`).

Im Emulator stecken sie als `../../core/rp2350/ROM_data.cpp` (erzeugt), dazu
`../../core/rp2350/openroms_syms.h` mit den Adressen, die der Emulator braucht.

## Was gegenüber OpenROMs geändert ist

1. **Konfiguration** `config_pbc.s` (auf Basis von `src/,,config_custom.s`):
   Markenname „PBC C64“, **JiffyDOS und DolphinDOS aus**. Die Schnelllader
   würden am emulierten seriellen Bus vorbeiarbeiten.
2. **Fallen für die Laufwerksemulation** im fertigen KERNAL-Abbild. Frodo
   emuliert Laufwerk 8 auf DOS-Ebene und antwortet direkt auf Busbefehle.
   `make_rom_data.py` ersetzt dazu den Anfang von sechs IEC-Grundroutinen
   durch Frodos Erweiterungsbefehl `$F2 xx` + `RTS`. Die Adressen kommen aus
   der Symboltabelle des Builds:

   | Routine | Falle | Frodo |
   |---|---|---|
   | `iec_tx_command` | `$F2 $20` | `OutATN` / `OutSec` |
   | `iec_tx_byte` | `$F2 $21` | `Out` (Carry = EOI) |
   | `iec_rx_byte` | `$F2 $22` | `In` |
   | `iec_turnaround_to_listen`, `iec_turnaround_to_talk`, `iec_tx_command_finalize` | `$F2 $23` | nichts nötig |

   Die Gegenstücke stehen in `../../core/CPUC64.cpp` (Fälle `0x20`–`0x23`).

3. **`$CB` (SFDX) wird gepflegt**, `scnkey_sfdx.patch`. Das Original-KERNAL
   legt dort bei jedem Tastaturscan die gedrückte Taste ab (`$40` = keine).
   OpenROMs lässt die Stelle leer (0, also „DEL gedrückt“). Spiele, die `$CB`
   abfragen, warten dann ewig aufs Loslassen; VVVVVV etwa reagierte im
   Hauptmenü nicht mehr. Der Patch schreibt `$CB` an denselben Stellen wie
   `LSTX`.

BASIC und Zeichensatz sind unverändert.

## Neu erzeugen

```bash
git clone https://github.com/MEGA65/open-roms
cd open-roms
git checkout ad178db                 # die verwendete Fassung
git submodule update --init
cp <pbc-plus>/c64/tools/openroms/config_pbc.s "src/,,config_custom.s"
git apply <pbc-plus>/c64/tools/openroms/scnkey_sfdx.patch
make build/kernal_custom.rom build/basic_custom.rom      # nicht mit -j
cd <pbc-plus>/c64
tools/openroms/make_rom_data.py <open-roms> ad178db core/rp2350/openroms_syms.h \
    > core/rp2350/ROM_data.cpp
```

Gebraucht werden `make`, `gcc`/`g++` und `python3`; den Assembler (ACME)
baut OpenROMs selbst aus seinem Submodul.

## Bekannte Unterschiede zum Original

* Der Startbildschirm zeigt „OPEN ROMS PBC C64“ und 51199 freie BASIC-Bytes.
* Das OpenROMs-BASIC ist nicht vollständig, z. B. fehlen `OPEN`, `CLOSE` und
  `INPUT#` als BASIC-Befehle („NOT IMPLEMENTED ERROR“). Für den Autostart
  (`LOAD`, `RUN`) spielt das keine Rolle, und Maschinenprogramme nutzen die
  entsprechenden KERNAL-Routinen, die vorhanden sind.
* Spiele, die direkt in Routinen des Original-KERNAL springen statt über die
  offizielle Sprungtabelle, können abstürzen.
