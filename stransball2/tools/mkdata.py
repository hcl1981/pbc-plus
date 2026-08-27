#!/usr/bin/env python3
"""Wandelt Kacheln, Kollisionsmasken und Karten nach src/data_*.c.

Die Vorlage laedt zur Laufzeit PCX-Bilder und Textdateien.  Auf dem Geraet
gibt es weder Dateisystem noch Bilddecoder, und die Daten aendern sich nach
dem Bauen nie mehr -- also wird vorab gewandelt:

  tiles.pcx       500 Kacheln zu 16x16 -> RGB565 im Panelformat
  tiles-mask.pcx  dieselben Kacheln    -> 1 Bit je Punkt (fest/frei)
  *.map           Text                 -> uint16-Felder

Schwarz (0,0,0) ist in der Vorlage der Transparenzschluessel.  Hier wird
daraus der Wert 0x0000; alle uebrigen Farben, die zufaellig auf 0x0000
abbilden wuerden, werden auf 0x0001 gehoben.
"""
import os, sys, glob
from PIL import Image

TW = TH = 16
COLS = 20
NTILES = 500

def rgb565_be(r, g, b):
    """Panelwort: MADCTL 0xC8 setzt das BGR-Bit -> Blau in die oberen 5 Bit."""
    return ((b & 0xf8) << 8) | ((g & 0xfc) << 3) | ((r & 0xf8) >> 3)


def load_tiles(path):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    if w != COLS * TW:
        sys.exit("%s: unerwartete Breite %d" % (path, w))
    px = im.load()
    out = []
    for t in range(NTILES):
        tx, ty = (t % COLS) * TW, (t // COLS) * TH
        if ty + TH > h:
            out.append(None)
            continue
        data = []
        for y in range(TH):
            for x in range(TW):
                r, g, b = px[tx + x, ty + y]
                if (r, g, b) == (0, 0, 0):
                    data.append(0)
                else:
                    v = rgb565_be(r, g, b)
                    data.append(v if v else 1)
        out.append(data)
    return out


def load_masks(path):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()
    out = []
    for t in range(NTILES):
        tx, ty = (t % COLS) * TW, (t // COLS) * TH
        bits = bytearray(TH * TW // 8)
        if ty + TH <= h:
            for y in range(TH):
                for x in range(TW):
                    r, g, b = px[tx + x, ty + y]
                    if (r, g, b) != (0, 0, 0):
                        i = y * TW + x
                        bits[i >> 3] |= 0x80 >> (i & 7)
        out.append(bytes(bits))
    return out


def load_map(path):
    """Format:
         Breite Hoehe
         Breite*Hoehe Kachelnummern (1-basiert)
         je Tuerzelle (Kachel 113, in Lesereihenfolge): Zustand Ereignis
         Zahl der Panzer, dann je Panzer: x y Typ
         Hintergrundtyp

    Die Tuerpaare stehen dazwischen, nicht am Ende -- die Vorlage liest sie
    mitten im Zellendurchlauf.  Wer sie ueberspringt, haelt den Zustand der
    ersten Tuer fuer die Panzeranzahl und verschiebt alles Weitere.
    """
    nums = open(path).read().split()
    sx, sy = int(nums[0]), int(nums[1])
    cells = [int(v) - 1 for v in nums[2:2 + sx * sy]]
    if len(cells) != sx * sy:
        sys.exit("%s: %d Zellen erwartet, %d gefunden" % (path, sx * sy, len(cells)))
    k = 2 + sx * sy
    doors = []
    for c in cells:
        if c == 113:
            if k + 1 >= len(nums):
                break
            doors.append((int(nums[k]), int(nums[k + 1])))
            k += 2
    ntanks = int(nums[k]) if len(nums) > k else 0
    k += 1
    tanks = []
    for t in range(ntanks):
        if k + 2 >= len(nums):
            break
        tanks.append((int(nums[k]), int(nums[k + 1]), int(nums[k + 2])))
        k += 3
    bg = int(nums[k]) if len(nums) > k else 0
    return sx, sy, cells, bg, tanks, doors


# Schiffsbilder: 32x32-Ausschnitte aus tiles.pcx.  Lage laut
# TRANSBALL::ship_map_collision der Vorlage.
#   Typ 0 SHADOW RUNNER  anim 0..5 bei (96+anim*32, 272), 6..10 bei (128+(anim-6)*32, 304)
#   Typ 1 V-PANTHER 2    anim 0..5 bei (32+anim*32, 240)
#   Typ 2 X-TERMINATOR   anim 0..5 bei (96+anim*32, 336)
SHIP_FRAMES = []
for a in range(11):
    SHIP_FRAMES.append((96 + a * 32, 272) if a < 6 else (128 + (a - 6) * 32, 304))
for a in range(6):
    SHIP_FRAMES.append((32 + a * 32, 240))
for a in range(6):
    SHIP_FRAMES.append((96 + a * 32, 336))
SHIP_BASE = (0, 11, 17)          # Index des ersten Bildes je Schiffstyp
SHIP_NANIM = (11, 6, 6)
SW = SH = 32


def load_ships(gfx, mask):
    gi = Image.open(gfx).convert("RGB").load()
    mi = Image.open(mask).convert("RGB").load()
    pix, msk = [], []
    for (sx, sy) in SHIP_FRAMES:
        d = []
        b = bytearray(SW * SH // 8)
        for y in range(SH):
            for x in range(SW):
                r, g, bb = gi[sx + x, sy + y]
                if (r, g, bb) == (0, 0, 0):
                    d.append(0)
                else:
                    v = rgb565_be(r, g, bb)
                    d.append(v if v else 1)
                if mi[sx + x, sy + y] != (0, 0, 0):
                    i = y * SW + x
                    b[i >> 3] |= 0x80 >> (i & 7)
        pix.append(d)
        msk.append(bytes(b))
    return pix, msk


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.join(here, "..")
    src = os.path.join(root, "data_src")

    tiles = load_tiles(os.path.join(src, "tiles.pcx"))
    masks = load_masks(os.path.join(src, "tiles-mask.pcx"))

    # --- Kacheln ---
    L = ['/* data_tiles.c -- erzeugt von tools/mkdata.py, nicht von Hand aendern.',
         ' * Kacheln aus tiles.pcx von Super Transball 2 (GPL-2.0, siehe COPYING).',
         ' */',
         '#include "data.h"', '']
    L.append("const uint16_t stb_tile[%d][%d] = {" % (NTILES, TW * TH))
    solid_count = 0
    for t, d in enumerate(tiles):
        if d is None:
            d = [0] * (TW * TH)
        if any(d):
            solid_count += 1
        L.append("{" + ",".join("0x%04X" % v for v in d) + "},")
    L.append("};")
    open(os.path.join(root, "src", "data_tiles.c"), "w").write("\n".join(L) + "\n")

    # --- Masken ---
    L = ['/* data_masks.c -- erzeugt von tools/mkdata.py.',
         ' * 1 Bit je Punkt aus tiles-mask.pcx: gesetzt = fest.',
         ' */',
         '#include "data.h"', '']
    L.append("const uint8_t stb_mask[%d][%d] = {" % (NTILES, TW * TH // 8))
    for m in masks:
        L.append("{" + ",".join("0x%02X" % b for b in m) + "},")
    L.append("};")
    open(os.path.join(root, "src", "data_masks.c"), "w").write("\n".join(L) + "\n")

    # --- Karten ---
    files = sorted(glob.glob(os.path.join(src, "map*.map")),
                   key=lambda p: int(''.join(c for c in os.path.basename(p) if c.isdigit()) or 0))
    L = ['/* data_maps.c -- erzeugt von tools/mkdata.py.',
         ' * Die Spielkarten von Super Transball 2 (GPL-2.0, siehe COPYING).',
         ' */',
         '#include "data.h"', '']
    cells_all = []
    meta = []
    tanks_all, doors_all = [], []
    for p in files:
        sx, sy, cells, bg, tanks, doors = load_map(p)
        meta.append((os.path.basename(p)[:-4], sx, sy, len(cells_all), bg,
                     len(tanks_all), len(tanks), len(doors_all), len(doors)))
        cells_all.extend(cells)
        tanks_all.extend(tanks)
        doors_all.extend(doors)
    L.append("const int16_t stb_mapdata[%d] = {" % len(cells_all))
    for i in range(0, len(cells_all), 20):
        L.append("    " + ",".join("%4d" % v for v in cells_all[i:i + 20]) + ",")
    L.append("};")
    L.append("const stb_mapinfo_t stb_map[%d] = {" % len(meta))
    for name, sx, sy, off, bg, toff, tn, doff, dn in meta:
        L.append('    {%3d,%3d,%7d,%d,%4d,%2d,%4d,%2d,"%s"},'
                 % (sx, sy, off, bg, toff, tn, doff, dn, name[:11]))
    L.append("};")
    L.append("const stb_door_t stb_doordata[%d] = {" % max(1, len(doors_all)))
    if doors_all:
        for (st, ev) in doors_all:
            L.append("    {%2d,%2d}," % (st, ev))
    else:
        L.append("    {0,0},")
    L.append("};")
    L.append("const uint16_t stb_map_num = %d;" % len(meta))
    L.append("const stb_tank_t stb_tank[%d] = {" % max(1, len(tanks_all)))
    if tanks_all:
        for (tx, ty, tt) in tanks_all:
            L.append("    {%3d,%3d,%d}," % (tx, ty, tt))
    else:
        L.append("    {0,0,0},")
    L.append("};")
    open(os.path.join(root, "src", "data_maps.c"), "w").write("\n".join(L) + "\n")

    # --- Schiffe ---
    spix, smsk = load_ships(os.path.join(src, "tiles.pcx"),
                            os.path.join(src, "tiles-mask.pcx"))
    L = ['/* data_ships.c -- erzeugt von tools/mkdata.py.',
         ' * Schiffsbilder und Kollisionsmasken; gedreht wird zur Laufzeit.',
         ' */',
         '#include "data.h"', '']
    L.append("const uint16_t stb_ship[%d][%d] = {" % (len(spix), SW * SH))
    for d in spix:
        L.append("{" + ",".join("0x%04X" % v for v in d) + "},")
    L.append("};")
    L.append("const uint8_t stb_shipmask[%d][%d] = {" % (len(smsk), SW * SH // 8))
    for b in smsk:
        L.append("{" + ",".join("0x%02X" % v for v in b) + "},")
    L.append("};")
    L.append("const uint8_t stb_ship_base[3] = {%d,%d,%d};" % SHIP_BASE)
    L.append("const uint8_t stb_ship_nanim[3] = {%d,%d,%d};" % SHIP_NANIM)
    open(os.path.join(root, "src", "data_ships.c"), "w").write("\n".join(L) + "\n")

    print("geschrieben: src/data_tiles.c, src/data_masks.c, src/data_maps.c, src/data_ships.c")
    print("  %d Schiffsbilder 32x32, %d KB" % (len(spix), len(spix) * SW * SH * 2 // 1024))
    print("  %d Kacheln (%d nicht leer), %d KB" % (NTILES, solid_count, NTILES * TW * TH * 2 // 1024))
    print("  Masken %d KB" % (NTILES * TW * TH // 8 // 1024))
    print("  %d Karten, %d Zellen, %d KB, %d Panzer, %d Tueren"
          % (len(meta), len(cells_all), len(cells_all) * 2 // 1024,
             len(tanks_all), len(doors_all)))
    big = max(meta, key=lambda m: m[1] * m[2])
    print("  groesste Karte: %s %dx%d" % (big[0], big[1], big[2]))


if __name__ == "__main__":
    main()
