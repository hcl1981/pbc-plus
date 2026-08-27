#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Wandelt die Original-Daten von Jump 'n Bump (Brainchild Design, GPL2+) in
C-Header fuer die PicoBoy-Color-Plus-Portierung um.

  level.pcx    -> Palette (RGB565) + Hintergrundbild (Palettenindizes)
  mask.pcx     -> 1-Bit-Vordergrundmaske (Sprites werden dort nicht gezeichnet)
  *.pcx/*.txt  -> Sprite-Atlanten (rabbit / objects / numbers) mit Hotspots
  *.smp        -> Effekte, 8 Bit vorzeichenbehaftet (Originalformat)
  bump.mod     -> ProTracker-Modul als Byteblob

Aufruf:  python3 tools/convert_assets.py
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)
DATA = os.path.join(PROJ, "data-src")
OUT = os.path.join(PROJ, "src")

JNB_WIDTH = 400
JNB_HEIGHT = 256


# --------------------------------------------------------------------------
# PCX
# --------------------------------------------------------------------------
def read_pcx(path):
    """Liefert (breite, hoehe, indizes, palette) eines 8-Bit-PCX."""
    raw = open(path, "rb").read()
    if raw[0] != 0x0A:
        raise ValueError("%s ist keine PCX-Datei" % path)
    xmin = raw[4] | (raw[5] << 8)
    ymin = raw[6] | (raw[7] << 8)
    xmax = raw[8] | (raw[9] << 8)
    ymax = raw[10] | (raw[11] << 8)
    planes = raw[65]
    bpl = raw[66] | (raw[67] << 8)
    w = xmax - xmin + 1
    h = ymax - ymin + 1
    if raw[3] != 8 or planes != 1:
        raise ValueError("%s: nur 8 Bit / 1 Ebene wird unterstuetzt" % path)

    # RLE-Dekodierung, zeilenweise (bytes_per_line kann > Breite sein)
    pix = bytearray(w * h)
    pos = 128
    for y in range(h):
        x = 0
        line = bytearray()
        while len(line) < bpl:
            a = raw[pos]
            pos += 1
            if (a & 0xC0) == 0xC0:
                cnt = a & 0x3F
                b = raw[pos]
                pos += 1
                line.extend([b] * cnt)
            else:
                line.append(a)
        pix[y * w:(y + 1) * w] = line[:w]
        x = w

    # Palette: letzte 769 Bytes, eingeleitet von 0x0C
    pal = None
    if len(raw) >= 769 and raw[-769] == 0x0C:
        pal = raw[-768:]
    return w, h, pix, pal


def pal_to_rgb565(pal):
    """VGA-Palette (0..255 aus der PCX-Datei) -> RGB565."""
    out = []
    for i in range(256):
        r, g, b = pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]
        out.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
    return out


# --------------------------------------------------------------------------
# GOB-Beschreibungen (rabbit.txt / objects.txt / numbers.txt)
# --------------------------------------------------------------------------
def read_gob_txt(path):
    """Liefert Liste von (x, y, w, h, hs_x, hs_y) in Bildreihenfolge."""
    cur = {}
    imgs = []
    num = None
    for line in open(path, "r"):
        line = line.strip()
        if not line or ":" not in line:
            continue
        key, val = line.split(":", 1)
        key = key.strip()
        val = val.strip()
        if key == "num_images":
            num = int(val)
        elif key == "image":
            if cur:
                imgs.append(cur)
            cur = {}
        else:
            cur[key] = int(val)
    if cur:
        imgs.append(cur)
    if num is not None and len(imgs) != num:
        raise ValueError("%s: %d Bilder erwartet, %d gefunden" % (path, num, len(imgs)))
    return [(i["x"], i["y"], i["width"], i["height"], i["hotspot_x"], i["hotspot_y"])
            for i in imgs]


def cut_gob(pcx_path, txt_path):
    """Schneidet die Einzelbilder aus dem Atlas-PCX heraus."""
    w, h, pix, _ = read_pcx(pcx_path)
    rects = read_gob_txt(txt_path)
    out = []
    for (x, y, iw, ih, hx, hy) in rects:
        data = bytearray(iw * ih)
        for row in range(ih):
            sy = y + row
            if sy >= h:
                continue
            src = sy * w + x
            data[row * iw:(row + 1) * iw] = pix[src:src + iw]
        out.append((iw, ih, hx, hy, bytes(data)))
    return out


# --------------------------------------------------------------------------
# Ausgabe-Helfer
# --------------------------------------------------------------------------
def emit_bytes(f, data, per_line=24):
    for i in range(0, len(data), per_line):
        f.write("  " + ",".join("0x%02X" % b for b in data[i:i + per_line]) + ",\n")


def emit_words(f, data, per_line=12):
    for i in range(0, len(data), per_line):
        f.write("  " + ",".join("0x%04X" % v for v in data[i:i + per_line]) + ",\n")


def emit_i8(f, data, per_line=24):
    for i in range(0, len(data), per_line):
        f.write("  " + ",".join("%d" % v for v in data[i:i + per_line]) + ",\n")


HEADER_NOTE = """/* Automatisch erzeugt von tools/convert_assets.py - nicht von Hand aendern.
 * Inhalt: Originaldaten von Jump 'n Bump (c) 1998 Brainchild Design, GPL2+.
 */
"""


def emit_gob(f, name, imgs):
    """Schreibt Atlas als Pixelblob + Indextabelle."""
    blob = bytearray()
    entries = []
    for (w, h, hx, hy, data) in imgs:
        entries.append((w, h, hx, hy, len(blob)))
        blob.extend(data)
    f.write("const uint8_t %s_pixels[%d] = {\n" % (name, len(blob)))
    emit_bytes(f, blob)
    f.write("};\n")
    f.write("const JnbGobEntry %s_gob[%d] = {\n" % (name, len(entries)))
    for (w, h, hx, hy, ofs) in entries:
        f.write("  {%d,%d,%d,%d,%d},\n" % (w, h, hx, hy, ofs))
    f.write("};\n\n")
    return len(blob)


# --------------------------------------------------------------------------
# Titelbild und Logo (PNG mit Transparenz)
# --------------------------------------------------------------------------
TITLE_W, TITLE_H = 240, 280   # genau die Displaygroesse
LOGO_H = 22                   # passt in die 24 Pixel hohe Punkteleiste


def dechecker(w, h, rgba):
    """logo.png ist ein plattgedruecktes Bild: das Karomuster des Bildbetrachters
    steckt als Pixel drin (weiss und hellgrau), einen Alphakanal gibt es nicht.
    Hier wird die Freistellung wiederhergestellt, indem vom Rand her ueber alle
    karofarbenen Pixel geflutet wird. Die Buchstaben sind ringsum schwarz
    umrandet, deshalb kann die Fuellung nicht ins Innere durchsickern."""
    out = bytearray(rgba)

    def is_check(i):
        r, g, b = rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2]
        if abs(r - g) > 6 or abs(g - b) > 6:
            return False
        return r >= 244 or 194 <= r <= 218   # die beiden Karotoene

    seen = bytearray(w * h)

    def flood(seeds):
        stack = list(seeds)
        for i in stack:
            seen[i] = 1
        while stack:
            i = stack.pop()
            out[i * 4 + 3] = 0
            x, y = i % w, i // w
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if 0 <= nx < w and 0 <= ny < h:
                    j = ny * w + nx
                    if not seen[j] and is_check(j):
                        seen[j] = 1
                        stack.append(j)

    # 1. Durchgang: vom Bildrand her fluten.
    border = []
    for x in range(w):
        for y in (0, h - 1):
            if is_check(y * w + x):
                border.append(y * w + x)
    for y in range(h):
        for x in (0, w - 1):
            if is_check(y * w + x):
                border.append(y * w + x)
    flood(border)

    # 2. Durchgang: zwischen den Grastropfen liegen Karofelder, die durch einen
    # schmalen weichen Uebergang vom gefluteten Bereich abgeschnitten sind.
    # Deshalb erneut fluten von jedem Karopixel aus, das hoechstens zwei Pixel
    # neben einem bereits durchsichtigen liegt. Zwei Pixel ueberbruecken den
    # Uebergang, sind aber viel duenner als die schwarze Kontur der Buchstaben -
    # die bleibt dicht.
    for _ in range(8):
        seeds = []
        for i in range(w * h):
            if seen[i] or not is_check(i):
                continue
            x, y = i % w, i // w
            near = False
            for dy in range(-2, 3):
                for dx in range(-2, 3):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h and out[(ny * w + nx) * 4 + 3] == 0:
                        near = True
                        break
                if near:
                    break
            if near:
                seeds.append(i)
        if not seeds:
            break
        flood(seeds)

    # Die Kanten waren gegen das Karomuster weichgezeichnet; diese Mischpixel
    # sind noch deckend und bilden einen hellen Saum. Deshalb die deckende
    # Flaeche um zwei Pixel schrumpfen - bei einer Quellbreite von 878 fuer ein
    # 84 Pixel breites Ziel ist das ein Fuenftel Zielpixel, also unsichtbar.
    ring = []
    for i in range(w * h):
        if not out[i * 4 + 3]:
            continue
        x, y = i % w, i // w
        hit = False
        for dy in range(-2, 3):
            for dx in range(-2, 3):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and out[(ny * w + nx) * 4 + 3] == 0:
                    hit = True
                    break
            if hit:
                break
        if hit:
            ring.append(i)
    for i in ring:
        out[i * 4 + 3] = 0

    freed = sum(1 for i in range(w * h) if out[i * 4 + 3] == 0)
    print("  logo freigestellt: %d von %d Pixeln durchsichtig (%.1f%%)"
          % (freed, w * h, 100.0 * freed / (w * h)))
    return bytes(out)


def emit_title_and_logo(_pal):
    from pngread import load_png, box_scale

    tw, th, tpx = load_png(os.path.join(DATA, "title.png"))
    title = box_scale(tw, th, tpx, TITLE_W, TITLE_H)

    lw, lh, lpx = load_png(os.path.join(DATA, "logo.png"))
    lpx = dechecker(lw, lh, lpx)
    logo_w = int(round(lw * LOGO_H / float(lh)))
    logo = box_scale(lw, lh, lpx, logo_w, LOGO_H)

    def to565(rgba, i):
        r, g, b = rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2]
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

    path = os.path.join(OUT, "assets_title.h")
    with open(path, "w") as f:
        f.write(HEADER_NOTE)
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define JNB_TITLE_W %d\n#define JNB_TITLE_H %d\n" % (TITLE_W, TITLE_H))
        f.write("#define JNB_LOGO_W  %d\n#define JNB_LOGO_H  %d\n\n" % (logo_w, LOGO_H))
        f.write("extern const uint16_t jnb_title[%d];\n" % (TITLE_W * TITLE_H))
        f.write("// Logo mit Deckkraft: Farbe und Alpha getrennt, damit es ueber\n"
                "// jedem Untergrund sauber liegt.\n")
        f.write("extern const uint16_t jnb_logo_rgb[%d];\n" % (logo_w * LOGO_H))
        f.write("extern const uint8_t  jnb_logo_a[%d];\n" % (logo_w * LOGO_H))
    size = os.path.getsize(path)

    path = os.path.join(OUT, "assets_title.cpp")
    with open(path, "w") as f:
        f.write(HEADER_NOTE)
        f.write('#include "assets_title.h"\n\n')
        f.write("const uint16_t jnb_title[%d] = {\n" % (TITLE_W * TITLE_H))
        emit_words(f, [to565(title, i) for i in range(TITLE_W * TITLE_H)], 16)
        f.write("};\n\n")
        f.write("const uint16_t jnb_logo_rgb[%d] = {\n" % (logo_w * LOGO_H))
        emit_words(f, [to565(logo, i) for i in range(logo_w * LOGO_H)], 16)
        f.write("};\n\n")
        f.write("const uint8_t jnb_logo_a[%d] = {\n" % (logo_w * LOGO_H))
        emit_bytes(f, [logo[i * 4 + 3] for i in range(logo_w * LOGO_H)], 24)
        f.write("};\n")
    size += os.path.getsize(path)
    print("titelbild: %dx%d (aus %dx%d), logo: %dx%d (aus %dx%d)"
          % (TITLE_W, TITLE_H, tw, th, logo_w, LOGO_H, lw, lh))
    return size


# --------------------------------------------------------------------------
def main():
    total = 0

    # ---------------- Level, Palette, Maske ----------------
    lw, lh, level, pal = read_pcx(os.path.join(DATA, "level.pcx"))
    if (lw, lh) != (JNB_WIDTH, JNB_HEIGHT):
        raise ValueError("level.pcx hat %dx%d statt %dx%d" % (lw, lh, JNB_WIDTH, JNB_HEIGHT))
    if pal is None:
        raise ValueError("level.pcx enthaelt keine Palette")
    mw, mh, mask, _ = read_pcx(os.path.join(DATA, "mask.pcx"))
    if (mw, mh) != (JNB_WIDTH, JNB_HEIGHT):
        raise ValueError("mask.pcx hat %dx%d" % (mw, mh))

    rgb565 = pal_to_rgb565(pal)

    # Maske: 1 Bit je Pixel, gesetzt = Vordergrund (Sprite wird dort verdeckt)
    mask_stride = (JNB_WIDTH + 7) // 8
    maskbits = bytearray(mask_stride * JNB_HEIGHT)
    for y in range(JNB_HEIGHT):
        for x in range(JNB_WIDTH):
            if mask[y * JNB_WIDTH + x] != 0:
                maskbits[y * mask_stride + (x >> 3)] |= 0x80 >> (x & 7)

    # Kachelkarte aus levelmap.txt - read_level() im Original ueberschreibt
    # damit die fest einkompilierte Tabelle. Unterschiede: Zeile 15 ist
    # vollstaendig fest und in Zeile 14 steht eine Sprungfeder.
    rows = []
    for line in open(os.path.join(DATA, "levelmap.txt")):
        digits = [c for c in line if c in "01234"]
        if len(digits) >= 22:
            rows.append([int(c) for c in digits[:22]])
    if len(rows) < 16:
        raise ValueError("levelmap.txt: %d statt 16 Zeilen" % len(rows))
    rows = rows[:16]
    rows.append([1] * 22)  # Zeile 16: read_level() setzt sie auf BAN_SOLID
    kinds = {0: "leer", 1: "fest", 2: "wasser", 3: "eis", 4: "feder"}
    counts = {k: sum(r.count(k) for r in rows) for k in kinds}
    print("kachelkarte: " + ", ".join("%s %d" % (kinds[k], counts[k]) for k in sorted(kinds)))

    path = os.path.join(OUT, "assets_level.h")
    with open(path, "w") as f:
        f.write(HEADER_NOTE)
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define JNB_WIDTH  %d\n" % JNB_WIDTH)
        f.write("#define JNB_HEIGHT %d\n" % JNB_HEIGHT)
        f.write("#define JNB_MASK_STRIDE %d\n\n" % mask_stride)
        f.write("extern const uint16_t jnb_palette[256];\n")
        f.write("extern const uint8_t  jnb_level[%d];\n" % (JNB_WIDTH * JNB_HEIGHT))
        f.write("extern const uint8_t  jnb_maskbits[%d];\n" % len(maskbits))
        f.write("extern const uint8_t  jnb_ban_map[17][22];\n")
    total += os.path.getsize(path)

    path = os.path.join(OUT, "assets_level.cpp")
    with open(path, "w") as f:
        f.write(HEADER_NOTE)
        f.write('#include "assets_level.h"\n\n')
        f.write("const uint16_t jnb_palette[256] = {\n")
        emit_words(f, rgb565)
        f.write("};\n\n")
        f.write("const uint8_t jnb_level[%d] = {\n" % len(level))
        emit_bytes(f, level, 32)
        f.write("};\n\n")
        f.write("const uint8_t jnb_maskbits[%d] = {\n" % len(maskbits))
        emit_bytes(f, maskbits, 32)
        f.write("};\n\n")
        f.write("const uint8_t jnb_ban_map[17][22] = {\n")
        for r in rows:
            f.write("  {" + ",".join(str(v) for v in r) + "},\n")
        f.write("};\n")
    total += os.path.getsize(path)
    print("level+maske: %dx%d, Maske %d Bytes" % (lw, lh, len(maskbits)))

    # ---------------- Sprites ----------------
    rabbit = cut_gob(os.path.join(DATA, "rabbit.pcx"), os.path.join(DATA, "rabbit.txt"))
    objects = cut_gob(os.path.join(DATA, "objects.pcx"), os.path.join(DATA, "objects.txt"))
    numbers = cut_gob(os.path.join(DATA, "numbers.pcx"), os.path.join(DATA, "numbers.txt"))
    font = cut_gob(os.path.join(DATA, "font.pcx"), os.path.join(DATA, "font.txt"))

    gobs = (("rabbit", rabbit), ("objects", objects), ("numbers", numbers), ("font", font))

    path = os.path.join(OUT, "assets_gfx.h")
    with open(path, "w") as f:
        f.write(HEADER_NOTE)
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("typedef struct {\n"
                "  uint8_t  w, h;\n"
                "  int8_t   hs_x, hs_y;\n"
                "  uint32_t ofs;\n"
                "} JnbGobEntry;\n\n")
        f.write("#define JNB_NUM_RABBIT  %d\n" % len(rabbit))
        f.write("#define JNB_NUM_OBJECTS %d\n" % len(objects))
        f.write("#define JNB_NUM_NUMBERS %d\n" % len(numbers))
        f.write("#define JNB_NUM_FONT    %d\n\n" % len(font))
        for nm, imgs in gobs:
            blob = sum(len(i[4]) for i in imgs)
            f.write("extern const uint8_t %s_pixels[%d];\n" % (nm, blob))
            f.write("extern const JnbGobEntry %s_gob[%d];\n" % (nm, len(imgs)))
    total += os.path.getsize(path)

    path = os.path.join(OUT, "assets_gfx.cpp")
    with open(path, "w") as f:
        f.write(HEADER_NOTE)
        f.write('#include "assets_gfx.h"\n\n')
        sizes = [emit_gob(f, nm, imgs) for nm, imgs in gobs]
    total += os.path.getsize(path)
    print("sprites: " + ", ".join("%s %d Bilder/%d B" % (nm, len(imgs), sz)
                                  for (nm, imgs), sz in zip(gobs, sizes)))

    # ---------------- Titelbild und Logo ----------------
    total += emit_title_and_logo(rgb565)

    for nm, imgs in (("rabbit", rabbit), ("objects", objects)):
        big = [i for i in imgs if i[0] > 255 or i[1] > 255]
        if big:
            raise ValueError("%s: Bild groesser als 255 Pixel" % nm)
        hs = [i for i in imgs if not (-128 <= i[2] <= 127 and -128 <= i[3] <= 127)]
        if hs:
            raise ValueError("%s: Hotspot passt nicht in int8" % nm)

    # ---------------- Klaenge ----------------
    sfx = [("death", "death.smp"), ("jump", "jump.smp"), ("splash", "splash.smp"),
           ("spring", "spring.smp"), ("fly", "fly.smp")]
    path = os.path.join(OUT, "assets_snd.h")
    with open(path, "w") as f:
        f.write(HEADER_NOTE)
        f.write("#pragma once\n#include <stdint.h>\n\n")
        for nm, fn in sfx:
            n = os.path.getsize(os.path.join(DATA, fn))
            f.write("#define SFXLEN_%s %d\n" % (nm.upper(), n))
            f.write("extern const int8_t sfx_%s[%d];\n" % (nm, n))
        modlen = os.path.getsize(os.path.join(DATA, "bump.mod"))
        f.write("\n#define JNB_MOD_LEN %d\n" % modlen)
        f.write("extern const uint8_t jnb_mod[%d];\n" % modlen)
    total += os.path.getsize(path)

    path = os.path.join(OUT, "assets_snd.cpp")
    with open(path, "w") as f:
        f.write(HEADER_NOTE)
        f.write('#include "assets_snd.h"\n\n')
        for nm, fn in sfx:
            d = open(os.path.join(DATA, fn), "rb").read()
            vals = [b - 256 if b > 127 else b for b in d]
            f.write("const int8_t sfx_%s[%d] = {\n" % (nm, len(d)))
            emit_i8(f, vals, 32)
            f.write("};\n\n")
        d = open(os.path.join(DATA, "bump.mod"), "rb").read()
        f.write("const uint8_t jnb_mod[%d] = {\n" % len(d))
        emit_bytes(f, d, 32)
        f.write("};\n")
    total += os.path.getsize(path)
    print("klaenge: %s + mod %d B" % (", ".join("%s %d" % (n, os.path.getsize(os.path.join(DATA, fn))) for n, fn in sfx), modlen))

    print("erzeugte Quelltextgroesse: %.1f MB" % (total / 1048576.0))


if __name__ == "__main__":
    main()
