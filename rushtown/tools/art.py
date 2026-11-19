#!/usr/bin/env python3
"""Makes src/pics.bin from art/sheet.jpeg: the story's pictures.

    ~/.cache/pbcplus-venv/bin/python tools/art.py        (needs Pillow)

The sheet is a 4 x 4 grid with white gaps: four portraits, then twelve
scenes. Each panel is cut out, brought to its size in the game (portraits
72 x 90, scenes the whole screen, 240 x 280) and to 256 colours of its own.

pics.bin: uint32 count, then per picture uint16 w, h and uint32 offset;
at the offset 256 colours as panel words (rt.h, RGB()), then w * h bytes.
The result is checked in, so building the game needs neither Pillow nor
this script. The order is that of PIC_* in src/rt.h.
"""
import os, struct, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SHEET = os.path.join(HERE, "..", "art", "sheet.jpeg")
OUT = os.path.join(HERE, "..", "src", "pics.bin")
PREVIEW = os.path.join(HERE, "..", "art", "preview.png")

def runs(white, need):
    """the stretches between the white gaps along one axis"""
    out, start = [], None
    for i, w in enumerate(white + [True]):
        if not w and start is None:
            start = i
        elif w and start is not None:
            if i - start >= need:
                out.append((start, i))
            start = None
    return out

def main():
    im = Image.open(SHEET).convert("RGB")
    w, h = im.size
    px = im.load()
    bright = lambda p: p[0] > 235 and p[1] > 235 and p[2] > 235
    cols = runs([sum(bright(px[x, y]) for y in range(0, h, 4)) > h // 4 * 0.9 for x in range(w)], w // 8)
    rows = runs([sum(bright(px[x, y]) for x in range(0, w, 4)) > w // 4 * 0.9 for y in range(h)], h // 8)
    assert len(cols) == 4 and len(rows) == 4, "not a 4 x 4 grid: %d columns, %d rows" % (len(cols), len(rows))
    pics = []
    for j, (y0, y1) in enumerate(rows):
        for i, (x0, x1) in enumerate(cols):
            panel = im.crop((x0 + 1, y0 + 1, x1 - 1, y1 - 1))
            size = (72, 90) if j == 0 else (240, 280)
            panel = panel.resize(size, Image.LANCZOS).quantize(256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
            pal = panel.getpalette()[:768]
            pal += [0] * (768 - len(pal))
            words = [((pal[k * 3 + 2] & 0xf8) << 8) | ((pal[k * 3 + 1] & 0xfc) << 3) | (pal[k * 3] >> 3) for k in range(256)]
            pics.append((size, struct.pack("<256H", *words) + panel.tobytes(), panel))
    head = 4 + 8 * len(pics)
    blob, body = struct.pack("<I", len(pics)), b""
    for size, data, _ in pics:
        blob += struct.pack("<HHI", size[0], size[1], head + len(body))
        body += data + b"\0" * (-len(data) % 4)
    open(OUT, "wb").write(blob + body)
    # what the game will show, for a look
    sheet = Image.new("RGB", (4 * 244, 4 * 284), (40, 40, 40))
    for k, (size, _, panel) in enumerate(pics):
        sheet.paste(panel.convert("RGB"), (k % 4 * 244 + 2, k // 4 * 284 + 2))
    sheet.save(PREVIEW)
    print("%d pictures, %d KB -> %s" % (len(pics), len(blob + body) // 1024, os.path.relpath(OUT)))

if __name__ == "__main__":
    sys.exit(main())
