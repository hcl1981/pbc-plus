#!/usr/bin/env python3
"""make_assets.py -- fonts and pictures of the launcher as C data.

    make_assets.py <launcher dir>

writes src/assets.c and src/assets.h. Needs Pillow and the Lato fonts
(SIL Open Font License, see LICENSE.lato); the generated files are part of
the repository, so building the firmware does not need either.

Fonts: anti-aliased glyphs, 4 bit coverage, code page 437 (what FatFs hands
out for file names) from 32 to 255.
Pictures: RGBA reduced to <= 255 colours (panel BGR565 + 4 bit alpha per palette
entry), pixel indices run-length coded.
"""
import os
import sys
from PIL import Image, ImageFont, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
import devices                                     # noqa: E402

LATO_DIRS = ['/usr/share/fonts/truetype/lato', os.path.expanduser('~/.local/share/fonts')]

FONTS = [                 # C name, file, pixel size
    ('font_title', 'Lato-Black.ttf', 24),
    ('font_head', 'Lato-Bold.ttf', 17),
    ('font_body', 'Lato-Semibold.ttf', 15),
    ('font_small', 'Lato-Bold.ttf', 11),
]

PICTURES = list(devices.DEVICES)   # big: W x H
ICON_W, ICON_H = 70, 45            # small: header of the game list


def find_font(name):
    for d in LATO_DIRS:
        p = os.path.join(d, name)
        if os.path.exists(p):
            return p
    sys.exit(f'{name} not found (package fonts-lato)')


def bgr565(r, g, b):
    return ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3)       # panel order: BGR565


def c_bytes(data, per_line=24):
    out = []
    for i in range(0, len(data), per_line):
        out.append('    ' + ', '.join(f'0x{b:02x}' for b in data[i:i + per_line]) + ',')
    return '\n'.join(out)


def font_c(cname, path, size):
    font = ImageFont.truetype(path, size)
    ascent, descent = font.getmetrics()
    glyphs, bitmap = [], bytearray()
    for code in range(32, 256):
        ch = bytes([code]).decode('cp437')
        bbox = font.getbbox(ch)
        adv = round(font.getlength(ch))
        if bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
            glyphs.append((len(bitmap), 0, 0, 0, 0, adv))
            continue
        w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
        img = Image.new('L', (w, h), 0)
        ImageDraw.Draw(img).text((-bbox[0], -bbox[1]), ch, font=font, fill=255)
        off = len(bitmap)
        px = list(img.getdata())
        for y in range(h):
            row = px[y * w:(y + 1) * w] + [0]
            for x in range(0, w, 2):
                a, b = (row[x] + 8) // 17, (row[x + 1] + 8) // 17
                bitmap.append((min(a, 15) << 4) | min(b, 15))
        glyphs.append((off, w, h, bbox[0], bbox[1], adv))
    lines = [f'static const uint8_t {cname}_bits[] = {{', c_bytes(bitmap), '};',
             f'static const pbc_glyph_t {cname}_glyphs[224] = {{']
    for g in glyphs:
        lines.append('    {%d, %d, %d, %d, %d, %d},' % g)
    lines += ['};', f'const pbc_font_t {cname} = {{ {ascent + descent}, {ascent}, '
              f'{cname}_glyphs, {cname}_bits }};', '']
    return '\n'.join(lines), len(bitmap)


def picture_c(cname, img):
    w, h = img.size
    q = img.quantize(colors=255, method=Image.Quantize.FASTOCTREE)
    pal = q.getpalette()[:256 * 3]
    rgba = img.load()
    idx = list(q.getdata())
    # alpha per palette entry: average of the pixels using it
    sums = {}
    for i, k in enumerate(idx):
        a = rgba[i % w, i // w][3]
        s = sums.setdefault(k, [0, 0])
        s[0] += a
        s[1] += 1
    n = max(idx) + 1
    palette = []
    for k in range(n):
        r, g, b = pal[k * 3:k * 3 + 3]
        a = round(sums[k][0] / sums[k][1] / 17) if k in sums else 0
        palette.append((bgr565(r, g, b), a))
    # PackBits: n < 128 -> n+1 literals follow; n >= 128 -> next byte (n-126) times
    rle = bytearray()
    i = 0
    while i < len(idx):
        run = 1
        while i + run < len(idx) and idx[i + run] == idx[i] and run < 129:
            run += 1
        if run >= 3:
            rle += bytes([run + 126, idx[i]])
            i += run
            continue
        j = i
        lit = []
        while j < len(idx) and len(lit) < 128:
            r2 = 1
            while j + r2 < len(idx) and idx[j + r2] == idx[j] and r2 < 3:
                r2 += 1
            if r2 >= 3:
                break
            lit.append(idx[j])
            j += 1
        rle += bytes([len(lit) - 1] + lit)
        i = j
    lines = [f'static const uint16_t {cname}_pal[] = {{']
    lines.append('    ' + ', '.join(f'0x{c:04x}' for c, a in palette) + ',')
    lines += ['};', f'static const uint8_t {cname}_alpha[] = {{',
              '    ' + ', '.join(str(a) for c, a in palette) + ',', '};',
              f'static const uint8_t {cname}_rle[] = {{', c_bytes(rle), '};',
              f'const pbc_picture_t {cname} = {{ {w}, {h}, {cname}_pal, {cname}_alpha, {cname}_rle }};',
              '']
    return '\n'.join(lines), len(rle) + 3 * n


def main():
    root = sys.argv[1]
    parts, sizes = [], {}
    for cname, fname, size in FONTS:
        c, n = font_c(cname, find_font(fname), size)
        parts.append(c)
        sizes[cname] = n
    names = []
    for name in PICTURES:
        # a picture in art/ (tools/import_icons.py) wins over the drawn one
        art = os.path.join(root, 'art', name + '.png')
        big = Image.open(art).convert('RGBA') if os.path.exists(art) else devices.DEVICES[name]()
        c, n = picture_c(f'pic_{name}', big)
        parts.append(c)
        sizes[f'pic_{name}'] = n
        small = big.resize((ICON_W, ICON_H), Image.LANCZOS)
        c, n = picture_c(f'icon_{name}', small)
        parts.append(c)
        sizes[f'icon_{name}'] = n
        names.append(name)

    hdr = ['/* assets.h -- generated by tools/make_assets.py, do not edit */',
           '#ifndef PBC_ASSETS_H', '#define PBC_ASSETS_H', '', '#include <stdint.h>', '',
           'typedef struct { uint32_t off; uint8_t w, h; int8_t x, y; uint8_t adv; } pbc_glyph_t;',
           '/* glyphs[c - 32] for code page 437 characters 32..255; 4 bit coverage,',
           ' * two pixels per byte (high nibble first), rows padded to whole bytes. */',
           'typedef struct { uint8_t height, ascent; const pbc_glyph_t *glyphs; const uint8_t *bits; } pbc_font_t;',
           '/* palette of RGB565 colours with 4 bit alpha; pixels as PackBits runs */',
           'typedef struct { uint16_t w, h; const uint16_t *pal; const uint8_t *alpha; const uint8_t *rle; } pbc_picture_t;',
           '']
    for cname, _, _ in FONTS:
        hdr.append(f'extern const pbc_font_t {cname};')
    hdr.append('')
    for n in names:
        hdr.append(f'extern const pbc_picture_t pic_{n}, icon_{n};')
    hdr += ['', '#endif', '']
    with open(os.path.join(root, 'src/assets.h'), 'w') as f:
        f.write('\n'.join(hdr))
    with open(os.path.join(root, 'src/assets.c'), 'w') as f:
        f.write('/* assets.c -- generated by tools/make_assets.py, do not edit.\n'
                ' * Fonts: Lato by Lukasz Dziedzic, SIL Open Font License 1.1 (LICENSE.lato).\n'
                ' * Pictures: art/*.png (tools/import_icons.py), else drawn by tools/devices.py. */\n'
                '#include "assets.h"\n\n' + '\n'.join(parts))
    total = sum(sizes.values())
    print(f'assets: {total // 1024} KB', ', '.join(f'{k} {v // 1024}K' for k, v in sizes.items()))


if __name__ == '__main__':
    main()
