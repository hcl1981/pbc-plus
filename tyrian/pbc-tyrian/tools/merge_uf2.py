#!/usr/bin/env python3
"""
merge_uf2.py -- Firmware und Spieldaten zu einer einzigen UF2-Datei vereinen.

Aufruf:
    ./merge_uf2.py build/tyrian.uf2 ../build-out/tyrian-data.uf2 \\
                   -o ../build-out/tyrian-komplett.uf2

Zur Familienkennung -- das ist die Stelle, an der es schiefging:

Ein UF2-Block traegt neben seiner Zieladresse eine Familienkennung und die
Angabe "Block i von N". Das Bootrom des RP2350 nimmt aus einer Datei nur EINE
Familie an und startet neu, sobald deren letzter Block da ist. Die Firmware
(rp2350-arm-s) und die Daten (Familie "data") hintereinander in einer Datei
heisst deshalb: die Firmware wird geschrieben, das Geraet startet neu, die
Daten kommen nie an. Aufgefallen ist das lange nicht, weil auf dem
Entwicklungsgeraet die Daten schon von frueher im Flash lagen.

Dieses Werkzeug gibt deshalb allen Bloecken die Kennung der Firmware und
zaehlt sie durch: fuer das Bootrom ist das Ganze dann ein einziges Abbild,
das eben auch ab 0x10100000 etwas hat. Ausgenommen bleibt der einzelne
Zusatzblock, den das SDK der Firmware voranstellt (Familie "absolute",
0x10FFFF00): er behaelt Kennung und Zaehlung.

Geprueft im Emulator PBConPC mit dem echten Boot-ROM (A4), auf leerem Flash:
vorher lagen danach 0 von 2269 Datensektoren im Flash, jetzt alle.

Zusaetzlich prueft das Werkzeug, ob sich Adressbereiche ueberschneiden --
taeten sie das, wuerde die zweite Datei die erste stillschweigend teilweise
ueberschreiben.

GPLv2, wie OpenTyrian.
"""

import argparse
import struct
import sys
from collections import defaultdict

BLOCK = 512
HDR = "<IIIIIIII"          # magic0, magic1, flags, addr, payload, blockNo, numBlocks, familyID
MAGIC0 = 0x0A324655
MAGIC1 = 0x9E5D5157
MAGIC_END = 0x0AB16F30
FAMILY_ABSOLUTE = 0xE48BFF57      # der Zusatzblock des SDK vor der Firmware


def read_blocks(path):
    data = open(path, "rb").read()
    if len(data) % BLOCK != 0:
        sys.exit(f"fehler: {path} ist kein Vielfaches von {BLOCK} Byte")

    out = []
    for i in range(0, len(data), BLOCK):
        b = data[i:i + BLOCK]
        m0, m1, flags, addr, payload, _no, _num, family = struct.unpack_from(HDR, b)
        if m0 != MAGIC0 or m1 != MAGIC1:
            sys.exit(f"fehler: {path}, Block {i // BLOCK} hat keine UF2-Kennung")
        if struct.unpack_from("<I", b, BLOCK - 4)[0] != MAGIC_END:
            sys.exit(f"fehler: {path}, Block {i // BLOCK} hat keine Endkennung")
        out.append((flags, addr, payload, family, b[32:32 + 476], _no, _num))
    return out


def ranges(blocks):
    """Zusammenhaengende Adressbereiche einer Datei.

    Nicht die blosse Spanne von kleinster bis groesster Adresse: die vom SDK
    erzeugte Firmware-UF2 beschreibt den Anfang des Flash UND einen einzelnen
    Zusatzblock ganz am Ende. Ueber die Spanne gerechnet belegte sie damit
    scheinbar das gesamte Flash, und jede Pruefung auf Ueberschneidung waere
    wertlos.
    """
    out = []
    for _f, addr, payload, _fam, _d, _n, _m in sorted(blocks, key=lambda b: b[1]):
        if out and addr == out[-1][1]:
            out[-1][1] = addr + payload
        else:
            out.append([addr, addr + payload])
    return [tuple(r) for r in out]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", help="UF2-Dateien, Firmware zuerst")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--keep-families", action="store_true",
                    help="die Familienkennungen lassen, wie sie sind (dann nimmt "
                         "das Bootrom nur den ersten Teil an, siehe oben)")
    args = ap.parse_args()

    parts = [(p, read_blocks(p)) for p in args.inputs]

    # Ueberschneidungen finden, bevor etwas geschrieben wird.
    per_file = []
    for path, blocks in parts:
        rs = ranges(blocks)
        families = sorted({b[3] for b in blocks})
        print(f"{path}:  {len(blocks):,} Bloecke")
        for lo, hi in rs:
            print(f"      0x{lo:08X}..0x{hi:08X}  ({hi - lo:>9,} Byte)")
        print(f"      Familie: {', '.join(f'0x{f:08X}' for f in families)}")
        per_file.append((path, rs))

    for i in range(len(per_file)):
        for j in range(i + 1, len(per_file)):
            for a_lo, a_hi in per_file[i][1]:
                for b_lo, b_hi in per_file[j][1]:
                    if a_lo < b_hi and b_lo < a_hi:
                        sys.exit(f"fehler: {per_file[i][0]} und {per_file[j][0]} "
                                 f"ueberschneiden sich bei "
                                 f"0x{max(a_lo, b_lo):08X}")

    all_blocks = [blk for _p, blocks in parts for blk in blocks]

    # Die Kennung der Firmware: die haeufigste der ersten Datei, den
    # Zusatzblock des SDK nicht mitgerechnet.
    counts = defaultdict(int)
    for b in parts[0][1]:
        if b[3] != FAMILY_ABSOLUTE:
            counts[b[3]] += 1
    family = max(counts, key=counts.get)
    main = sum(1 for b in all_blocks if b[3] != FAMILY_ABSOLUTE)
    print(f"   alle Bloecke bekommen die Familie 0x{family:08X}, {main:,} am Stueck gezaehlt")

    seen = 0
    with open(args.out, "wb") as f:
        for flags, addr, payload, fam, data, no, num in all_blocks:
            if fam == FAMILY_ABSOLUTE or args.keep_families:
                out_no, out_num, out_fam = no, num, fam     # unveraendert uebernehmen
            else:
                out_no, out_num, out_fam = seen, main, family
                seen += 1
            f.write(struct.pack(HDR, MAGIC0, MAGIC1, flags, addr, payload, out_no, out_num, out_fam))
            f.write(data)
            f.write(struct.pack("<I", MAGIC_END))

    print()
    print(f"-> {args.out}")
    print(f"   {len(all_blocks):,} Bloecke, "
          f"{len(all_blocks) * BLOCK / 1024 / 1024:.1f} MB Datei")
    for lo, hi in ranges(all_blocks):
        print(f"   beschreibt 0x{lo:08X}..0x{hi:08X}")


if __name__ == "__main__":
    main()
