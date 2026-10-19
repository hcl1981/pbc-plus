#!/usr/bin/env python3
"""import_icons.py -- device pictures from a generated sprite sheet.

    import_icons.py <sheet.jpg|png> <outdir>

The sheet is a 4 x 2 grid on a magenta background (made with the prompt in
README.md), cells in the order of NAMES. Each cell is keyed out, cut to the
object, scaled to fit W x H, placed on the common baseline and given the
same soft floor shadow as the drawn pictures (tools/devices.py), so all of
them sit alike in the launcher. Writes <outdir>/<name>.png (RGBA).
"""
import sys
from PIL import Image, ImageDraw, ImageFilter

NAMES = ['nes', 'sms', 'md', 'gg', 'pce', 'gbc', 'c64', 'usb']
W, H = 200, 128
BG = (227, 21, 208)                 # the magenta of the cells


def key(cell):
    """Alpha from how magenta a pixel is: (min(R, B) - G) / max(R, B).
    Background and its darker shadows are ~0.8-0.9, the purple Game Boy
    Color ~0.15, greys ~0."""
    cell = cell.convert('RGB')
    out = Image.new('RGBA', cell.size)
    src, dst = cell.load(), out.load()
    for y in range(cell.size[1]):
        for x in range(cell.size[0]):
            r, g, b = src[x, y]
            m = (min(r, b) - g) / max(r, b, 1)
            a = min(1.0, max(0.0, (0.48 - m) / 0.24))
            if 0 < a < 1:               # remove the magenta mixed into the edge
                r = int(min(255, max(0, (r - (1 - a) * BG[0]) / a)))
                g = int(min(255, max(0, (g - (1 - a) * BG[1]) / a)))
                b = int(min(255, max(0, (b - (1 - a) * BG[2]) / a)))
            if a < 1 and r > g + 24 and b > g + 24 and m > 0.2:
                a *= 0.5                # what is still pink is fringe
            dst[x, y] = (r, g, b, int(a * 255))
    return out


def shave(img):
    """Takes the alpha one pixel in: the outermost ring is mostly fringe."""
    img.putalpha(img.getchannel('A').filter(ImageFilter.MinFilter(3)))
    return img


def crop_object(img):
    alpha = img.getchannel('A').point(lambda v: 255 if v > 128 else 0)
    # drop specks: shrink then grow the mask before taking its bounding box
    solid = alpha.filter(ImageFilter.MinFilter(5)).filter(ImageFilter.MaxFilter(5))
    box = solid.getbbox() or alpha.getbbox()
    return img.crop(box)


def place(obj):
    s = min((W - 6) / obj.width, (H - 10) / obj.height)
    obj = obj.resize((max(1, round(obj.width * s)), max(1, round(obj.height * s))), Image.LANCZOS)
    pic = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    x, y = (W - obj.width) // 2, H - 6 - obj.height
    # floor shadow, as in devices.py
    sh = Image.new('L', (W * 4, H * 4), 0)
    cx, cy, rx, ry = W / 2, H - 7, obj.width * 0.46, 7
    ImageDraw.Draw(sh).ellipse([(cx - rx) * 4, (cy - ry) * 4, (cx + rx) * 4, (cy + ry) * 4], fill=110)
    sh = sh.filter(ImageFilter.GaussianBlur(20)).resize((W, H), Image.LANCZOS)
    black = Image.new('RGBA', (W, H), (0, 0, 0, 255))
    black.putalpha(sh)
    pic = Image.alpha_composite(pic, black)
    layer = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    layer.paste(obj, (x, y))
    return Image.alpha_composite(pic, layer)


def main():
    sheet = Image.open(sys.argv[1]).convert('RGB')
    out = sys.argv[2]
    cw, ch = sheet.width / 4, sheet.height / 2
    for i, name in enumerate(NAMES):
        c, r = i % 4, i // 4
        cell = sheet.crop((round(c * cw), round(r * ch), round((c + 1) * cw), round((r + 1) * ch)))
        place(crop_object(shave(key(cell)))).save(f'{out}/{name}.png')


if __name__ == '__main__':
    main()
