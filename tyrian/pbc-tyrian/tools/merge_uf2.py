#!/usr/bin/env python3
"""
merge_uf2.py -- Firmware und Spieldaten zu einer einzigen UF2-Datei vereinen.

Aufruf:
    ./merge_uf2.py build/tyrian.uf2 ../build-out/tyrian-data.uf2 \\
                   -o ../build-out/tyrian-komplett.uf2

Zur Numerierung -- das ist die Stelle, an der es leicht schiefgeht:

Ein UF2-Block traegt neben seiner Zieladresse die Angabe "Block i von N", und
das Bootrom erkennt daran das Ende der Uebertragung. Diese Zaehlung laeuft je
FAMILIENKENNUNG getrennt, nicht ueber die Datei. Die vom SDK erzeugte
Firmware-UF2 enthaelt zum Beispiel zwei Familien mit je eigener Zaehlung.

Daraus folgt: solange keine Familie in zwei Eingabedateien vorkommt, ist
schlichtes Aneinanderhaengen bereits richtig -- jede Zaehlung bleibt in sich
stimmig. Wird trotzdem global durchnumeriert, sind beide Sequenzen kaputt, das
Bootrom erkennt das Ende nicht und startet am Schluss nicht neu.

Dieses Werkzeug laesst die Numerierung deshalb in Ruhe, solange sie stimmig
ist, und zaehlt nur dann neu, wenn dieselbe Familie aus mehreren Dateien kommt.
Zusaetzlich prueft es, ob sich Adressbereiche ueberschneiden -- taeten sie das,
wuerde die zweite Datei die erste stillschweigend teilweise ueberschreiben.

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
    ap.add_argument("--one-family", action="store_true",
                    help="alle Bloecke auf die Familienkennung der ersten Datei "
                         "setzen (normalerweise nicht noetig, siehe unten)")
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

    # Kommt eine Familie aus mehr als einer Datei? Nur dann muss neu gezaehlt
    # werden -- sonst ist die vorhandene Zaehlung bereits stimmig.
    files_per_family = defaultdict(set)
    for path, blocks in parts:
        for b in blocks:
            files_per_family[b[3]].add(path)

    renumber = {fam for fam, files in files_per_family.items() if len(files) > 1}
    for fam in sorted(renumber):
        print(f"   Familie 0x{fam:08X} kommt aus mehreren Dateien -- neu gezaehlt")

    # Familienkennungen bleiben, wie sie sind.
    #
    # Zuerst hatte ich hier alles auf eine Familie vereinheitlicht, aus Sorge,
    # das Bootrom koenne mit mehreren in einer Datei nichts anfangen. Die Sorge
    # war unbegruendet: die vom SDK erzeugte Firmware-UF2 enthaelt SELBST schon
    # zwei (das Abbild und den Zusatzblock am Flash-Ende) und wird anstandslos
    # geladen. --one-family bleibt fuer den Fall, dass sich das als falsch
    # erweist.
    family = all_blocks[0][3]

    counts = {fam: sum(1 for b in all_blocks if b[3] == fam) for fam in renumber}
    seen = defaultdict(int)

    with open(args.out, "wb") as f:
        for flags, addr, payload, fam, data, no, num in all_blocks:
            if fam in renumber:
                out_no, out_num = seen[fam], counts[fam]
                seen[fam] += 1
            else:
                out_no, out_num = no, num   # unveraendert uebernehmen

            f.write(struct.pack(HDR, MAGIC0, MAGIC1, flags, addr, payload,
                                out_no, out_num,
                                family if args.one_family else fam))
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
