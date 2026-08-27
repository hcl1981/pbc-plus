#!/usr/bin/env python3
"""Erzeugt src/font_data.c aus DejaVu-TTFs.

Nicht einen kleinen Font hochskalieren, sondern jede Groesse einzeln rastern:
das ist der Unterschied zwischen "verpixelt" und "scharf".

  - Anzeigeschriften (ui_*) mit 8 Bit Deckung je Pixel; gemischt wird zur
    Laufzeit gegen den tatsaechlichen Hintergrund.
  - Alle Tabellen tragen 8 Bit Deckung je Pixel; gemischt wird zur Laufzeit
    gegen den tatsaechlichen Hintergrund.  Auch der Titeltext laeuft als
    Bildschirm-Overlay, nicht mehr durch den Zellring -- eine Zelle sind zwei
    Bildpunkte, feiner wird es dort nicht.

Proportionale Vorschuebe statt fester Zellbreite.
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

DIR = "/usr/share/fonts/truetype/dejavu"
REG = os.path.join(DIR, "DejaVuSans.ttf")
BLD = os.path.join(DIR, "DejaVuSans-Bold.ttf")
CND = os.path.join(DIR, "DejaVuSansCondensed-Bold.ttf")

# Space .. Z; Kleinbuchstaben werden beim Zeichnen hochgesetzt.
FULL = (0x20, 0x5A)
# Der Schriftzug SPOUT braucht nur O..U -- ein voller Satz in dieser Groesse
# waere 80 KB Flash fuer ein einziges Wort.
TITLE = (0x4F, 0x55)

FONTS = [
    dict(name="ui_s",  ttf=REG, px=12, rng=FULL),   # Beschriftungen
    dict(name="ui_m",  ttf=BLD, px=20, rng=FULL),   # Zahlenwerte, Titelzeilen
    dict(name="ui_l",  ttf=BLD, px=27, rng=FULL),   # PAUSE / GAME OVER
    dict(name="ui_xl", ttf=CND, px=48, rng=TITLE),  # der Schriftzug SPOUT
]


def render(path, px, rng):
    f = ImageFont.truetype(path, px)
    ascent, descent = f.getmetrics()
    pad = px * 2
    glyphs = []
    blob = bytearray()

    first, last = rng
    for code in range(first, last + 1):
        ch = chr(code)
        img = Image.new("L", (px * 4, px * 4), 0)
        d = ImageDraw.Draw(img)
        d.text((pad, pad), ch, fill=255, font=f, anchor="ls")
        bbox = img.getbbox()
        adv = int(round(f.getlength(ch)))

        if bbox is None:                    # Space
            glyphs.append(dict(w=0, h=0, ox=0, oy=0, adv=adv, off=0))
            continue

        x0, y0, x1, y1 = bbox
        crop = img.crop(bbox)
        w, h = crop.size
        off = len(blob)
        px_data = crop.load()

        for y in range(h):
            for x in range(w):
                blob.append(px_data[x, y])

        glyphs.append(dict(w=w, h=h, ox=x0 - pad, oy=y0 - pad, adv=adv, off=off))

    return dict(glyphs=glyphs, blob=bytes(blob), ascent=ascent,
                line_h=ascent + descent)


def emit(out, cfg, data):
    n = cfg["name"]
    first, last = cfg["rng"]
    out.append("static const uint8_t %s_bits[%d] = {" % (n, len(data["blob"])))
    b = data["blob"]
    for i in range(0, len(b), 16):
        out.append("    " + ",".join("%3d" % v for v in b[i:i + 16]) + ",")
    out.append("};")
    out.append("static const glyph_t %s_glyphs[%d] = {" % (n, len(data["glyphs"])))
    for code, g in zip(range(first, last + 1), data["glyphs"]):
        out.append("    {%3d,%3d,%3d,%4d,%4d,%6d}, /* %s */" %
                   (g["w"], g["h"], g["adv"], g["ox"], g["oy"], g["off"],
                    "space" if code == 32 else chr(code)))
    out.append("};")
    out.append("const font_t font_%s = { 0x%02X, 0x%02X, %d, %d, %s_glyphs, %s_bits };"
               % (n, first, last, data["line_h"], data["ascent"], n, n))
    out.append("")


def main():
    for p in (REG, BLD, CND):
        if not os.path.exists(p):
            sys.exit("Schrift fehlt: %s" % p)

    out = ["/* font_data.c -- erzeugt von tools/mkfont.py, nicht von Hand aendern.",
           " *",
           " * Quelle: DejaVu Sans / Sans Bold / Sans Condensed Bold.",
           " * Bitstream-Vera-Lizenz, siehe doc/LICENSE.DejaVu -- sie erlaubt",
           " * Vervielfaeltigung, Aenderung und Weitergabe ausdruecklich, verlangt",
           " * aber, dass der Lizenztext beiliegt.",
           " */",
           '#include "font.h"', ""]

    report = []
    for cfg in FONTS:
        data = render(cfg["ttf"], cfg["px"], cfg["rng"])
        emit(out, cfg, data)
        widest = max((g["w"] for g in data["glyphs"]), default=0)
        report.append("  %-6s %2dpx  0x%02X..0x%02X  Zeile %2d  Grundlinie %2d  breitestes %2d  %6d B"
                      % (cfg["name"], cfg["px"], cfg["rng"][0], cfg["rng"][1],
                         data["line_h"], data["ascent"], widest, len(data["blob"])))
        cfg["_data"] = data

    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, "..", "src", "font_data.c"), "w") as f:
        f.write("\n".join(out) + "\n")

    print("geschrieben: src/font_data.c")
    print("\n".join(report))

    # Breite der wichtigsten Zeichenketten pruefen
    def wof(cfg, s):
        gl, first = cfg["_data"]["glyphs"], cfg["rng"][0]
        return sum(gl[ord(c) - first]["adv"] for c in s)
    checks = [("ui_xl", "SPOUT", 240), ("ui_m", "CAVEFLYER", 220),
              ("ui_m", "B - THRUST", 220), ("ui_m", "A - PAUSE", 220),
              ("ui_m", "BEST 123456", 220), ("ui_s", "N.WHITE 2010 MIT", 220),
              ("ui_l", "GAME OVER", 240), ("ui_s", "A RESUME   B TITLE", 240),
              ("ui_s", "BEST 123456   HEIGHT 12345", 200)]
    print("\nBreitenpruefung:")
    for name, s, limit in checks:
        cfg = next(c for c in FONTS if c["name"] == name)
        w = wof(cfg, s)
        print("  %-7s %-28s %3d von %3d  %s" % (name, '"' + s + '"', w, limit,
                                                "ok" if w <= limit else "ZU BREIT"))


def measure(argv):
    """--measure <fontname> <text> ...  gibt die Breite in Pixeln/Zellen aus."""
    name = argv[0]
    cfg = next(c for c in FONTS if c["name"] == name)
    data = render(cfg["ttf"], cfg["px"], cfg["rng"])
    gl, first = data["glyphs"], cfg["rng"][0]
    for s in argv[1:]:
        t = s.upper()
        w = sum(gl[ord(c) - first]["adv"] for c in t)
        print("  %-7s %-32s %3d" % (name, '"' + t + '"', w))


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--measure":
        measure(sys.argv[2:])
    else:
        main()
