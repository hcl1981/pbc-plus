#!/usr/bin/env python3
"""devices.py -- draws the device pictures of the PBCemu launcher.

All pictures share one look: seen slightly from above, light from the upper
left, flat colours with a soft top highlight, one soft floor shadow, no
logos or lettering. Drawn at 4x and scaled down, so the edges are smooth.

    devices.py <outdir>      writes <name>.png (RGBA, W x H) per device
"""
import math
import sys
from PIL import Image, ImageDraw, ImageFilter

W, H = 200, 128          # final size of every picture
S = 4                    # supersampling factor


def rgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def shade(c, f):
    """f > 0 lightens towards white, f < 0 darkens towards black."""
    c = rgb(c) if isinstance(c, str) else c
    if f >= 0:
        return tuple(int(v + (255 - v) * f) for v in c)
    return tuple(int(v * (1 + f)) for v in c)


class Pic:
    def __init__(self):
        self.img = Image.new('RGBA', (W * S, H * S), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)

    # all coordinates in final pixels (floats fine)
    def poly(self, pts, fill, outline=None, width=1):
        self.d.polygon([(x * S, y * S) for x, y in pts], fill=fill,
                       outline=outline, width=int(width * S) if outline else 0)

    def rrect(self, x0, y0, x1, y1, r, fill, outline=None, width=1):
        self.d.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=r * S,
                                 fill=fill, outline=outline,
                                 width=int(width * S) if outline else 0)

    def ellipse(self, x0, y0, x1, y1, fill, outline=None, width=1):
        self.d.ellipse([x0 * S, y0 * S, x1 * S, y1 * S], fill=fill, outline=outline,
                       width=int(width * S) if outline else 0)

    def line(self, pts, fill, width=1):
        self.d.line([(x * S, y * S) for x, y in pts], fill=fill, width=max(1, int(width * S)))

    def shadow(self, cx, cy, rx, ry, alpha=110):
        sh = Image.new('L', self.img.size, 0)
        ImageDraw.Draw(sh).ellipse([(cx - rx) * S, (cy - ry) * S, (cx + rx) * S, (cy + ry) * S],
                                   fill=alpha)
        sh = sh.filter(ImageFilter.GaussianBlur(5 * S))
        black = Image.new('RGBA', self.img.size, (0, 0, 0, 255))
        black.putalpha(sh)
        self.img = Image.alpha_composite(black, self.img)
        self.d = ImageDraw.Draw(self.img)

    def done(self):
        return self.img.resize((W, H), Image.LANCZOS)

    # --- building blocks -------------------------------------------------
    def box(self, x, y, w, h, depth, col, r=3, inset=0.0, top=None, front=None):
        """A box seen slightly from above: top face (a trapezoid, narrower at
        the back by `inset` per side) above a front face of height h."""
        top = top or shade(col, 0.18)
        front = front or col
        # top face
        self.poly([(x + inset, y - depth), (x + w - inset, y - depth),
                   (x + w, y), (x, y)], top)
        # front face
        self.rrect(x, y, x + w, y + h, r, front)
        self.rrect(x, y, x + w, y + 2.2, 1, shade(top, 0.15))     # lit front edge

    def dpad(self, cx, cy, s, col):
        t = s / 3
        self.rrect(cx - s / 2, cy - t / 2, cx + s / 2, cy + t / 2, t / 4, col)
        self.rrect(cx - t / 2, cy - s / 2, cx + t / 2, cy + s / 2, t / 4, col)
        self.ellipse(cx - t / 4, cy - t / 4, cx + t / 4, cy + t / 4, shade(col, 0.12))

    def button(self, cx, cy, r, col):
        self.ellipse(cx - r, cy - r + 0.8, cx + r, cy + r + 0.8, shade(col, -0.45))
        self.ellipse(cx - r, cy - r, cx + r, cy + r, col)
        self.ellipse(cx - r * 0.55, cy - r * 0.7, cx + r * 0.25, cy - r * 0.1, shade(col, 0.35))

    def pill(self, cx, cy, w, h, col, angle=0):
        self.rrect(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2, h / 2, col)

    def screen(self, x0, y0, x1, y1, bezel, glass='#1d2b3a'):
        self.rrect(x0, y0, x1, y1, 3, bezel)
        m = (y1 - y0) * 0.13
        gx0, gy0, gx1, gy1 = x0 + m * 1.4, y0 + m, x1 - m * 1.4, y1 - m
        self.rrect(gx0, gy0, gx1, gy1, 1.5, glass)
        # soft reflection
        self.poly([(gx0, gy0), (gx0 + (gx1 - gx0) * 0.45, gy0), (gx0 + (gx1 - gx0) * 0.15, gy1),
                   (gx0, gy1)], shade(glass, 0.10))


# ---------------------------------------------------------------------------

def nes():
    p = Pic()
    p.shadow(100, 112, 92, 9)
    grey, dark = '#c9c8c4', '#3f3f43'
    x, y, w, h, dep = 14, 50, 152, 52, 26
    # top: the light lid at the front, the dark ribbed part at the back
    p.poly([(x + 6, y - dep), (x + w - 6, y - dep), (x + w, y), (x, y)], shade(grey, 0.25))
    p.poly([(x + 6, y - dep), (x + w - 6, y - dep), (x + w - 3, y - dep * 0.45), (x + 3, y - dep * 0.45)],
           shade(dark, 0.15))
    for i in range(6):
        yy = y - dep + 2.2 + i * 2.2
        p.line([(x + 8 + i * 0.5, yy), (x + w - 8 - i * 0.5, yy)], shade(dark, -0.4), 0.8)
    p.line([(x + 3, y - dep * 0.45), (x + w - 3, y - dep * 0.45)], shade(grey, -0.25), 0.8)
    # front: the cartridge door with fine ridges over a dark band
    p.rrect(x, y, x + w, y + h, 2, grey)
    p.rrect(x, y, x + w, y + 2, 1, shade(grey, 0.35))
    p.rrect(x + 4, y + 5, x + w - 4, y + h * 0.56, 1.5, shade(grey, 0.06))      # door
    for i in range(5):
        yy = y + 9 + i * 3.2
        p.line([(x + 48, yy), (x + w - 8, yy)], shade(grey, -0.12), 0.7)
    p.rrect(x + 8, y + 9, x + 40, y + 13, 1, shade(grey, -0.18))               # logo plate
    p.rrect(x, y + h * 0.62, x + w, y + h, 2, dark)                           # dark band
    # in the band: LED, POWER and RESET on the left, the two ports on the right
    by = y + h * 0.72
    p.ellipse(x + 7, by + 1.5, x + 10, by + 4.5, '#e8402a')
    p.rrect(x + 14, by, x + 26, by + 7, 1, shade(grey, -0.05))
    p.rrect(x + 30, by, x + 42, by + 7, 1, shade(grey, -0.05))
    for bx in (x + w - 50, x + w - 28):
        p.rrect(bx, by - 1, bx + 18, by + 9, 1, shade(grey, -0.1))
        p.rrect(bx + 5, by + 1, bx + 13, by + 7, 0.5, '#1a1a1c')
    # controller: dark frame, light face, black panel, red buttons
    cx, cy = 164, 112
    p.rrect(cx - 30, cy - 12, cx + 30, cy + 12, 2, '#2a2a2d')
    p.rrect(cx - 28, cy - 10, cx + 28, cy + 10, 1.5, shade(grey, 0.12))
    p.rrect(cx - 26, cy - 2, cx + 26, cy + 8, 1, '#1e1e20')
    p.dpad(cx - 18, cy - 1, 13, '#111113')
    p.rrect(cx - 7, cy + 2, cx - 1, cy + 4.5, 1.2, '#7c7c80')
    p.rrect(cx + 1, cy + 2, cx + 7, cy + 4.5, 1.2, '#7c7c80')
    p.button(cx + 13, cy + 3, 3.2, '#d8322a')
    p.button(cx + 21, cy + 3, 3.2, '#d8322a')
    p.line([(cx - 30, cy - 4), (cx - 40, cy - 6), (x + w - 41, by + 8)], '#1d1d1f', 1)
    return p.done()


def master_system():
    p = Pic()
    p.shadow(98, 110, 90, 9)
    blk, red = '#1c1c20', '#d42a2a'
    x, y, w, h, dep = 12, 60, 150, 34, 36
    # top in two levels: a sloped front part, a raised back with the slot
    p.poly([(x + 4, y - dep), (x + w - 4, y - dep), (x + w, y), (x, y)], shade(blk, 0.14))
    p.poly([(x + 4, y - dep), (x + w - 4, y - dep), (x + w - 2, y - dep * 0.58), (x + 2, y - dep * 0.58)],
           shade(blk, 0.24))
    p.rrect(x + 40, y - dep + 4, x + 104, y - dep + 10, 1.5, '#0b0b0d')       # cartridge slot
    p.rrect(x + 44, y - dep + 6, x + 100, y - dep + 8, 0.5, shade(blk, 0.3))
    # the red line along the edge between the two levels
    p.poly([(x + 2, y - dep * 0.58), (x + w - 2, y - dep * 0.58), (x + w - 1.6, y - dep * 0.5),
            (x + 1.6, y - dep * 0.5)], red)
    # front level: diagonal vent grille on the left, three buttons on the right
    for i in range(9):
        xx = x + 12 + i * 4.4
        p.line([(xx + 3, y - dep * 0.44), (xx - 2, y - 4)], '#0c0c0e', 1.4)
    for i, col in enumerate(('#3a3a40', '#3a3a40', red)):
        bx = x + w - 54 + i * 15
        p.poly([(bx + 1, y - dep * 0.4), (bx + 11, y - dep * 0.4), (bx + 12, y - 5), (bx, y - 5)],
               shade(col, -0.35))
        p.poly([(bx + 1.5, y - dep * 0.42), (bx + 10.5, y - dep * 0.42), (bx + 11, y - 7), (bx + 1, y - 7)], col)
    p.rrect(x, y, x + w, y + h, 2.5, blk)
    p.rrect(x, y, x + w, y + 2, 1, shade(blk, 0.3))
    p.rrect(x + 8, y + 10, x + 58, y + 16, 1, '#0d0d10')                     # card slot
    p.rrect(x + w - 30, y + 18, x + w - 8, y + 26, 1.5, '#121215')          # pad ports
    # pad: plain black rectangle, grey D-pad, two buttons
    cx, cy = 150, 104
    p.rrect(cx - 26, cy - 12, cx + 26, cy + 12, 3, '#18181b')
    p.rrect(cx - 26, cy - 12, cx + 26, cy - 10, 2, shade('#18181b', 0.3))
    p.rrect(cx - 23, cy - 8, cx - 3, cy + 10, 3, '#222226')
    p.dpad(cx - 13, cy + 1, 15, '#55555c')
    p.button(cx + 9, cy + 3, 3.6, '#2e2e33')
    p.button(cx + 18, cy + 3, 3.6, '#2e2e33')
    p.line([(cx - 26, cy - 5), (cx - 40, cy - 12), (x + w - 19, y + 22)], '#111113', 1)
    return p.done()


def mega_drive():
    p = Pic()
    p.shadow(96, 110, 90, 9)
    blk = '#1b1b1f'
    x, y, w, h, dep = 14, 64, 140, 30, 40
    p.poly([(x + 6, y - dep), (x + w - 6, y - dep), (x + w, y), (x, y)], shade(blk, 0.14))
    # cartridge slot block at the back
    p.rrect(x + 70, y - dep + 2, x + w - 14, y - dep + 14, 2, shade(blk, 0.24))
    p.rrect(x + 78, y - dep + 5, x + w - 22, y - dep + 9, 1, '#09090b')
    # the round platter, seen from above (ellipse) with a metallic ring
    cx, cy = x + 46, y - dep * 0.45
    p.ellipse(cx - 34, cy - 13, cx + 34, cy + 15, shade(blk, -0.4))
    p.ellipse(cx - 34, cy - 15, cx + 34, cy + 13, '#8e9096')
    p.ellipse(cx - 30, cy - 13, cx + 30, cy + 11, '#c9cbd0')
    p.ellipse(cx - 25, cy - 11, cx + 25, cy + 9, shade(blk, 0.2))
    p.ellipse(cx - 20, cy - 9, cx + 20, cy + 6, shade(blk, 0.1))
    p.rrect(x, y, x + w, y + h, 3, blk)
    p.rrect(x, y, x + w, y + 2, 1, shade(blk, 0.3))
    p.rrect(x + 10, y + 8, x + 36, y + 13, 1.5, '#0c0c0e')               # volume slider
    p.rrect(x + 22, y + 7, x + 27, y + 14, 1, '#6b6d72')
    p.ellipse(x + 44, y + 9, x + 47, y + 12, '#e33b2b')                  # power LED
    for bx in (x + 70, x + 98):                                           # pad ports
        p.rrect(bx, y + 10, bx + 18, y + 22, 2, '#111114')
    # 3-button pad: kidney shape (two lobes and a bridge), A B C slanting up
    cx, cy = 154, 104
    pad = '#1d1d22'
    for dx in (-15, 15):
        p.ellipse(cx + dx - 17, cy - 12, cx + dx + 17, cy + 15, shade(pad, -0.4))
        p.ellipse(cx + dx - 17, cy - 13, cx + dx + 17, cy + 13, pad)
    p.rrect(cx - 16, cy - 13, cx + 16, cy + 4, 6, pad)
    p.ellipse(cx - 28, cy - 11, cx + 2, cy - 2, shade(pad, 0.1))
    p.ellipse(cx - 26, cy - 8, cx - 4, cy + 10, '#27272d')
    p.dpad(cx - 15, cy + 1, 14, '#3c3c43')
    for bx, by in ((cx + 6, cy + 6), (cx + 14, cy + 3), (cx + 22, cy)):
        p.button(bx, by, 3.2, '#34343b')
    p.pill(cx + 4, cy - 6, 6, 2.6, '#45454d')                                 # START
    p.line([(cx, cy - 15), (cx - 18, cy - 24), (x + 79, y + 16)], '#101012', 1)
    return p.done()


def game_gear():
    p = Pic()
    p.shadow(100, 114, 82, 7)
    blk = '#1e1f23'
    x0, y0, x1, y1 = 22, 22, 178, 104
    # body: wide rounded grips
    p.rrect(x0, y0 + 2, x1, y1 + 2, 26, shade(blk, -0.45))
    p.rrect(x0, y0, x1, y1, 26, blk)
    p.rrect(x0 + 6, y0 + 2, x1 - 6, y0 + 16, 14, shade(blk, 0.10))       # top highlight
    p.screen(64, 30, 136, 92, '#2b2c31', glass='#24364a')
    p.rrect(70, 94, 130, 97, 1, '#2a2b30')                                # speaker slot area
    for i in range(5):
        p.ellipse(141 + i * 3, 92, 142.5 + i * 3, 93.5, '#0c0c0e')
    p.dpad(41, 66, 22, '#2e2f35')
    p.ellipse(31, 56, 51, 76, shade(blk, 0.06))
    p.dpad(41, 66, 20, '#3a3b42')
    p.button(152, 70, 5.4, '#3a3b42')
    p.button(166, 61, 5.4, '#3a3b42')
    p.pill(160, 42, 9, 4.5, '#2f74d6')                                   # start (blue)
    p.ellipse(54, 36, 57, 39, '#46d36a')                                  # power LED
    return p.done()


def pc_engine():
    p = Pic()
    p.shadow(92, 112, 76, 9)
    wht, dark = '#e8e6e1', '#2b2c30'
    x, y, w, h, dep = 26, 52, 96, 40, 44
    p.poly([(x + 5, y - dep), (x + w - 5, y - dep), (x + w, y), (x, y)], shade(wht, 0.35))
    # dark top panel with ribs
    p.poly([(x + 9, y - dep + 4), (x + w - 9, y - dep + 4), (x + w - 6, y - 10), (x + 6, y - 10)],
           shade(dark, 0.08))
    for i in range(7):
        yy = y - dep + 8 + i * 3.6
        f = i / 7
        p.line([(x + 10 - f * 3, yy), (x + w - 10 + f * 3, yy)], shade(dark, -0.4), 0.9)
    p.rrect(x, y, x + w, y + h, 2.5, wht)
    p.rrect(x, y, x + w, y + 2, 1, '#ffffff')
    p.rrect(x + 8, y + 7, x + w - 8, y + 15, 1.5, dark)                  # HuCard slot
    p.rrect(x + 22, y + 10, x + w - 22, y + 12, 0.5, '#0b0b0d')
    p.rrect(x + 10, y + 22, x + 24, y + 28, 1, '#c9c6bf')                # power switch
    p.rrect(x + 12, y + 23, x + 17, y + 27, 1, dark)
    p.rrect(x + w - 26, y + 23, x + w - 10, y + 30, 1, '#bdbab3')        # pad port
    # pad
    cx, cy = 150, 102
    p.rrect(cx - 30, cy - 13, cx + 30, cy + 14, 12, '#bdbab3')
    p.rrect(cx - 30, cy - 14, cx + 30, cy + 13, 12, '#e4e1da')
    p.rrect(cx - 26, cy - 13, cx + 26, cy - 8, 6, '#f6f4f0')
    p.rrect(cx - 25, cy - 8, cx + 25, cy + 9, 8, dark)
    p.rrect(cx + 8, cy - 12, cx + 13, cy - 9.5, 1, '#8d8a84')            # turbo switches
    p.rrect(cx + 17, cy - 12, cx + 22, cy - 9.5, 1, '#8d8a84')
    p.dpad(cx - 15, cy + 0.5, 12, '#5b5c63')
    p.pill(cx - 3, cy + 4, 6, 2.5, '#77787f')
    p.pill(cx + 4, cy + 4, 6, 2.5, '#77787f')
    p.button(cx + 12, cy + 1, 3.3, '#cf3a33')
    p.button(cx + 20, cy - 2, 3.3, '#cf3a33')
    p.line([(cx - 30, cy - 4), (cx - 44, cy - 12), (x + w - 18, y + 27)], '#2a2a2d', 1)
    return p.done()


def game_boy_color():
    p = Pic()
    p.shadow(100, 118, 42, 6)
    body = '#6a3db0'                                                       # "grape"
    x0, y0, x1, y1 = 70, 4, 130, 114
    # body: square top corners, evenly rounded bottom
    p.rrect(x0, y0 + 2, x1, y1 + 2, 6, shade(body, -0.45))
    p.rrect(x0, y0, x1, y1 - 14, 5, body)
    p.rrect(x0, y0 + 40, x1, y1, 14, body)
    p.rrect(x0 + 2, y0 + 1, x1 - 2, y0 + 5, 2.5, shade(body, 0.22))
    p.rrect(x0 + 24, y0 - 1.5, x1 - 24, y0 + 1, 0.5, '#2a1e3d')             # IR port
    # screen lens: dark, wider than tall, with the screen inside
    p.rrect(x0 + 4, y0 + 8, x1 - 4, y0 + 48, 4, '#2f3038')
    p.rrect(x0 + 11, y0 + 13, x1 - 11, y0 + 43, 1.5, '#22303f')
    p.poly([(x0 + 11, y0 + 13), (x0 + 28, y0 + 13), (x0 + 17, y0 + 43), (x0 + 11, y0 + 43)],
           shade('#22303f', 0.1))
    p.ellipse(x0 + 6, y0 + 25, x0 + 8.5, y0 + 27.5, '#ff4c3a')             # power LED
    p.dpad(x0 + 14, y0 + 68, 16, '#2a2433')
    p.button(x1 - 18, y0 + 71, 4.4, '#2a2433')                              # B
    p.button(x1 - 9, y0 + 65, 4.4, '#2a2433')                               # A
    p.pill(x0 + 24, y0 + 89, 8, 2.8, '#2a2433')                             # select
    p.pill(x0 + 35, y0 + 89, 8, 2.8, '#2a2433')                             # start
    for r in range(3):                                                      # speaker holes
        for c in range(3):
            if r + c >= 1:
                cx, cy = x1 - 15 + c * 3.6, y0 + 96 + r * 3.6
                p.ellipse(cx - 0.9, cy - 0.9, cx + 0.9, cy + 0.9, shade(body, -0.45))
    return p.done()


def c64():
    p = Pic()
    p.shadow(92, 112, 86, 9)
    beige, brown = '#cfc3a5', '#5b4637'
    x, y, w, h, dep = 8, 72, 150, 22, 46
    # breadbox: rounded top face sloping towards the front
    p.poly([(x + 8, y - dep), (x + w - 8, y - dep), (x + w, y), (x, y)], shade(beige, 0.2))
    p.rrect(x + 8, y - dep - 3, x + w - 8, y - dep + 4, 3, shade(beige, 0.32))
    # keyboard: rows narrowing to the back
    rows = 5
    for r in range(rows):
        f0, f1 = r / rows, (r + 1) / rows
        ya, yb = y - dep + 7 + f0 * (dep - 12), y - dep + 7 + f1 * (dep - 12)
        ins_a, ins_b = 16 - f0 * 6, 16 - f1 * 6
        keys = 15 if r < 4 else 1
        xa0, xa1 = x + ins_a, x + w - 34 - ins_a * 0.4
        xb0, xb1 = x + ins_b, x + w - 34 - ins_b * 0.4
        if r == 4:                                                         # space bar
            m = 0.3
            p.poly([(xa0 + (xa1 - xa0) * m, ya + 0.6), (xa1 - (xa1 - xa0) * m, ya + 0.6),
                    (xb1 - (xb1 - xb0) * m, yb - 0.6), (xb0 + (xb1 - xb0) * m, yb - 0.6)], brown)
            continue
        for k in range(keys):
            a0 = xa0 + (xa1 - xa0) * k / keys
            a1 = xa0 + (xa1 - xa0) * (k + 1) / keys
            b0 = xb0 + (xb1 - xb0) * k / keys
            b1 = xb0 + (xb1 - xb0) * (k + 1) / keys
            p.poly([(a0 + 0.5, ya + 0.6), (a1 - 0.5, ya + 0.6), (b1 - 0.5, yb - 0.6), (b0 + 0.5, yb - 0.6)],
                   brown)
            p.line([(a0 + 0.8, ya + 0.9), (a1 - 0.8, ya + 0.9)], shade(brown, 0.25), 0.6)
    for r in range(4):                                                     # function keys
        f0, f1 = r / 4, (r + 0.75) / 4
        ya, yb = y - dep + 7 + f0 * (dep - 12), y - dep + 7 + f1 * (dep - 12)
        p.poly([(x + w - 26 + f0 * 3, ya), (x + w - 14 + f0 * 4, ya),
                (x + w - 13 + f1 * 4, yb), (x + w - 27 + f1 * 3, yb)], '#b9ad92')
    p.rrect(x, y, x + w, y + h, 3, beige)
    p.rrect(x, y, x + w, y + 2, 1, shade(beige, 0.35))
    # badge and power LED sit on the top, to the right above the keys
    p.poly([(x + w - 58, y - dep + 4), (x + w - 30, y - dep + 4), (x + w - 30, y - dep + 6.5),
            (x + w - 58.5, y - dep + 6.5)], shade(beige, -0.15))
    p.ellipse(x + w - 26, y - dep + 3.6, x + w - 22.5, y - dep + 7, '#e0412e')
    # joystick
    cx, cy = 168, 104
    p.rrect(cx - 18, cy - 8, cx + 18, cy + 10, 4, '#1c1c20')
    p.rrect(cx - 18, cy - 10, cx + 18, cy + 6, 4, '#2a2a30')
    p.button(cx - 11, cy - 4, 3.0, '#d83a2e')
    p.button(cx + 11, cy - 4, 3.0, '#d83a2e')
    p.ellipse(cx - 5, cy - 3, cx + 5, cy + 1, '#141417')
    p.line([(cx, cy - 1), (cx, cy - 26)], '#141417', 3)
    p.ellipse(cx - 4.5, cy - 31, cx + 4.5, cy - 22, '#1b1b1e')
    p.ellipse(cx - 2.5, cy - 29.5, cx + 0.5, cy - 26.5, '#55555c')
    p.line([(cx - 18, cy), (cx - 26, cy - 8), (x + w, y + 12)], '#151518', 1)
    return p.done()


def msx2():
    """An MSX2 computer of the later kind (Sony HB-F1XD, Philips NMS 8245):
    black keyboard unit, cartridge slot at the back, disk drive on the right,
    function keys in a row above the keys; a joystick beside it."""
    p = Pic()
    p.shadow(92, 112, 88, 9)
    blk, key, fkey = '#232327', '#3a3a40', '#8e8e96'
    x, y, w, h, dep = 6, 74, 156, 20, 48
    # top face sloping towards the front
    p.poly([(x + 6, y - dep), (x + w - 6, y - dep), (x + w, y), (x, y)], shade(blk, 0.16))
    # raised back strip with the cartridge slot
    p.poly([(x + 6, y - dep), (x + w - 6, y - dep), (x + w - 5, y - dep + 9), (x + 5, y - dep + 9)],
           shade(blk, 0.26))
    p.rrect(x + 34, y - dep + 2, x + 92, y - dep + 6, 1, '#0b0b0d')
    p.rrect(x + 38, y - dep + 3.2, x + 88, y - dep + 4.8, 0.5, shade(blk, 0.35))
    p.ellipse(x + w - 22, y - dep + 2.6, x + w - 18.5, y - dep + 6, '#e0412e')      # power LED
    # function keys: one row of five wide keys
    fy0, fy1 = y - dep + 12, y - dep + 16.5
    for k in range(5):
        a0 = x + 16 + k * 15
        p.poly([(a0, fy0), (a0 + 12, fy0), (a0 + 12.4, fy1), (a0 - 0.4, fy1)], fkey)
    # keyboard: rows narrowing to the back, the right part left free for the drive
    rows = 5
    for r in range(rows):
        f0, f1 = r / rows, (r + 1) / rows
        span = dep - 22
        ya, yb = fy1 + 2.5 + f0 * span, fy1 + 2.5 + f1 * span
        ins_a, ins_b = 14 - f0 * 6, 14 - f1 * 6
        xa0, xa1 = x + ins_a, x + w - 46 - ins_a * 0.4
        xb0, xb1 = x + ins_b, x + w - 46 - ins_b * 0.4
        if r == 4:                                                         # space bar
            m = 0.28
            p.poly([(xa0 + (xa1 - xa0) * m, ya + 0.6), (xa1 - (xa1 - xa0) * m, ya + 0.6),
                    (xb1 - (xb1 - xb0) * m, yb - 0.6), (xb0 + (xb1 - xb0) * m, yb - 0.6)], key)
            continue
        keys = 14
        for k in range(keys):
            a0 = xa0 + (xa1 - xa0) * k / keys
            a1 = xa0 + (xa1 - xa0) * (k + 1) / keys
            b0 = xb0 + (xb1 - xb0) * k / keys
            b1 = xb0 + (xb1 - xb0) * (k + 1) / keys
            p.poly([(a0 + 0.5, ya + 0.6), (a1 - 0.5, ya + 0.6), (b1 - 0.5, yb - 0.6), (b0 + 0.5, yb - 0.6)],
                   key)
            p.line([(a0 + 0.8, ya + 0.9), (a1 - 0.8, ya + 0.9)], shade(key, 0.3), 0.6)
    # cursor keys on the right of the keyboard
    cxk, cyk = x + w - 30, y - 12
    for dx, dy in ((0, -4.5), (-5, 0), (5, 0), (0, 4.5)):
        p.rrect(cxk + dx - 2.2, cyk + dy - 2, cxk + dx + 2.2, cyk + dy + 2, 0.6, '#c94040')
    # front: the disk drive on the right
    p.rrect(x, y, x + w, y + h, 3, blk)
    p.rrect(x, y, x + w, y + 2, 1, shade(blk, 0.3))
    p.rrect(x + w - 58, y + 5, x + w - 8, y + 15, 1.5, shade(blk, 0.12))
    p.rrect(x + w - 54, y + 9, x + w - 18, y + 11, 0.5, '#0a0a0c')                     # slot
    p.rrect(x + w - 15, y + 8, x + w - 11, y + 12, 1, '#9a9aa2')                       # eject
    p.ellipse(x + w - 57, y + 7, x + w - 55, y + 9, '#3ccf7a')                          # drive LED
    # joystick
    cx, cy = 172, 106
    p.rrect(cx - 16, cy - 7, cx + 16, cy + 9, 4, '#1c1c20')
    p.rrect(cx - 16, cy - 9, cx + 16, cy + 6, 4, '#2c2c32')
    p.button(cx - 9, cy - 3, 2.8, '#d83a2e')
    p.button(cx + 9, cy - 3, 2.8, '#2f74d6')
    p.ellipse(cx - 4.5, cy - 3, cx + 4.5, cy + 1, '#141417')
    p.line([(cx, cy - 1), (cx, cy - 24)], '#141417', 3)
    p.ellipse(cx - 4.5, cy - 29, cx + 4.5, cy - 20, '#1b1b1e')
    p.ellipse(cx - 2.5, cy - 27.5, cx + 0.5, cy - 24.5, '#55555c')
    p.line([(cx - 16, cy), (cx - 22, cy - 6), (x + w, y + 12)], '#151518', 1)
    return p.done()


def usb_drive():
    p = Pic()
    p.shadow(100, 112, 60, 7)
    # the PicoBoy's USB drive: a stick lying on the floor, plugged into a cable
    body = '#2f7de0'
    p.rrect(52, 58, 132, 92, 9, shade(body, -0.4))
    p.rrect(52, 54, 132, 88, 9, body)
    p.rrect(56, 56, 128, 64, 6, shade(body, 0.22))
    p.rrect(132, 62, 160, 82, 2, '#c9ccd2')                               # metal plug
    p.rrect(132, 62, 160, 65, 1, '#eceef2')
    p.rrect(140, 67, 146, 71, 0.5, '#5d6068')
    p.rrect(150, 67, 156, 71, 0.5, '#5d6068')
    # arrow down onto the stick: "copy games here"
    p.rrect(84, 14, 100, 36, 2, '#f2f4f8')
    p.poly([(76, 34), (108, 34), (92, 50)], '#f2f4f8')
    p.ellipse(60, 66, 66, 72, '#7fd0ff')                                  # activity LED
    return p.done()


def sg1000():
    """Sega SG-1000: a black wedge with a silver band, the slot on top, and
    its joystick with the red ball."""
    p = Pic()
    p.shadow(96, 110, 90, 9)
    blk, silver, red = '#1d1d22', '#b9bcc4', '#d42a2a'
    x, y, w, h, dep = 14, 62, 144, 32, 34
    p.poly([(x + 6, y - dep), (x + w - 6, y - dep), (x + w, y), (x, y)], shade(blk, 0.16))
    p.rrect(x + 42, y - dep + 6, x + 102, y - dep + 13, 1.5, '#0b0b0d')       # cartridge slot
    p.rrect(x + 46, y - dep + 8.5, x + 98, y - dep + 10.5, 0.5, shade(blk, 0.32))
    for i in range(6):                                                          # vents
        xx = x + 14 + i * 4
        p.line([(xx + 2, y - dep + 8), (xx - 1, y - 6)], '#0c0c0e', 1.2)
    p.rrect(x, y, x + w, y + h, 2.5, blk)
    p.rrect(x, y, x + w, y + 2, 1, shade(blk, 0.3))
    p.rrect(x + 4, y + 6, x + w - 4, y + 15, 1.5, silver)                     # silver band
    p.rrect(x + 4, y + 6, x + w - 4, y + 8, 1, shade(silver, 0.35))
    p.rrect(x + 10, y + 9, x + 34, y + 12, 0.8, shade(silver, -0.25))
    p.ellipse(x + w - 16, y + 9, x + w - 12, y + 13, red)                     # power LED
    p.rrect(x + w - 34, y + 20, x + w - 10, y + 27, 1.5, '#121215')          # joystick port
    # joystick: black base, stick with a red ball, red fire button
    cx, cy = 150, 106
    p.rrect(cx - 18, cy - 8, cx + 18, cy + 10, 3, '#18181b')
    p.rrect(cx - 18, cy - 8, cx + 18, cy - 6, 2, shade('#18181b', 0.3))
    p.rrect(cx - 7.5, cy - 24, cx - 4.5, cy - 2, 1.5, '#2a2a2e')
    p.button(cx - 6, cy - 26, 4.5, red)
    p.button(cx + 9, cy + 1, 3.6, red)
    p.line([(cx - 18, cy + 2), (cx - 30, cy - 4), (x + w - 22, y + 24)], '#111113', 1)
    return p.done()


def atari2600():
    """Atari VCS / 2600: black ribbed top, wood front, the silver switches,
    and the joystick with its red button."""
    p = Pic()
    p.shadow(96, 110, 90, 9)
    blk, wood = '#1b1a1a', '#8a5a2e'
    x, y, w, h, dep = 14, 62, 140, 30, 36
    p.poly([(x + 6, y - dep), (x + w - 6, y - dep), (x + w, y), (x, y)], shade(blk, 0.14))
    for i in range(9):                                                          # ribs
        yy = y - dep + 3 + i * 3.4
        inset = 6 - i * 0.6
        p.line([(x + inset + 2, yy), (x + w - inset - 2, yy)], shade(blk, 0.32), 1.0)
    p.rrect(x + 52, y - dep + 4, x + 88, y - dep + 12, 1.5, '#0a0a0b')        # cartridge slot
    for i in range(4):                                                          # switches
        sx = x + 12 + i * 10 if i < 2 else x + w - 52 + (i - 2) * 10 + 22
        p.rrect(sx, y - 15, sx + 5, y - 5, 1, '#d6d8dc')
        p.rrect(sx + 1, y - 15, sx + 4, y - 12, 0.8, '#f2f3f5')
    p.rrect(x, y, x + w, y + h, 2.5, wood)                                     # wood front
    for i in range(7):                                                          # grain
        yy = y + 4 + i * 3.6
        p.line([(x + 3, yy), (x + w * 0.4, yy + 0.8), (x + w - 3, yy - 0.5)], shade(wood, -0.18), 0.7)
    p.rrect(x, y, x + w, y + 2, 1, shade(wood, 0.3))
    p.rrect(x, y + h - 6, x + w, y + h, 2, shade(blk, 0.05))                  # black bottom band
    # joystick: square black base, stick, red button at the top left
    cx, cy = 150, 106
    p.rrect(cx - 15, cy - 10, cx + 15, cy + 12, 3, '#161617')
    p.rrect(cx - 15, cy - 10, cx + 15, cy - 8, 2, shade('#161617', 0.3))
    p.rrect(cx - 2, cy - 30, cx + 2, cy, 1.8, '#202022')
    p.ellipse(cx - 5, cy - 3, cx + 5, cy + 3, '#2a2a2c')
    p.button(cx - 9, cy - 5, 3.2, '#d8322a')
    p.line([(cx - 15, cy + 3), (cx - 28, cy - 3), (x + w - 10, y + 20)], '#111113', 1)
    return p.done()


def zx_spectrum():
    p = Pic()
    p.shadow(92, 112, 84, 9)
    black, panel, key = '#1b1b1f', '#26262c', '#c9c9cf'
    x, y, w, h, dep = 14, 86, 140, 10, 50
    # a flat wedge: the top face slopes up to the back
    p.poly([(x + 6, y - dep), (x + w - 6, y - dep), (x + w, y), (x, y)], panel)
    p.poly([(x + 6, y - dep), (x + w - 6, y - dep), (x + w - 5.6, y - dep + 2.2), (x + 6.4, y - dep + 2.2)],
           shade(panel, 0.18))
    # 4 rows of 10 small rubber keys, narrowing to the back
    rows, cols = 4, 10
    for r in range(rows):
        f0, f1 = r / rows, (r + 0.62) / rows
        ya, yb = y - dep + 6 + f0 * (dep - 14), y - dep + 6 + f1 * (dep - 14)
        ins_a, ins_b = 12 - f0 * 5, 12 - f1 * 5
        xa0, xa1 = x + ins_a, x + w - ins_a
        xb0, xb1 = x + ins_b, x + w - ins_b
        for k in range(cols):
            m = 0.18
            a0 = xa0 + (xa1 - xa0) * (k + m) / cols
            a1 = xa0 + (xa1 - xa0) * (k + 1 - m) / cols
            b0 = xb0 + (xb1 - xb0) * (k + m) / cols
            b1 = xb0 + (xb1 - xb0) * (k + 1 - m) / cols
            p.poly([(a0, ya), (a1, ya), (b1, yb), (b0, yb)], key)
            p.line([(a0 + 0.3, ya + 0.4), (a1 - 0.3, ya + 0.4)], shade(key, 0.35), 0.5)
    # the four diagonal stripes in the lower right corner
    for i, c in enumerate(['#e0302b', '#f2c12e', '#3fae3c', '#29b6d8']):
        o = i * 3.2
        p.poly([(x + w - 30 + o, y), (x + w - 26.8 + o, y), (x + w - 14.8 + o, y - 12), (x + w - 18 + o, y - 12)], c)
    # front edge
    p.rrect(x, y, x + w, y + h, 2, black)
    p.rrect(x, y, x + w, y + 1.6, 1, shade(black, 0.25))
    # joystick
    cx, cy = 172, 104
    p.rrect(cx - 13, cy - 8, cx + 13, cy + 8, 3, '#1c1c20')
    p.rrect(cx - 13, cy - 10, cx + 13, cy + 5, 3, '#2a2a30')
    p.button(cx - 7, cy - 4, 2.6, '#d83a2e')
    p.ellipse(cx - 4, cy - 4, cx + 6, cy, '#141417')
    p.line([(cx + 1, cy - 2), (cx + 1, cy - 24)], '#141417', 3)
    p.ellipse(cx - 3.5, cy - 29, cx + 5.5, cy - 20, '#1b1b1e')
    p.ellipse(cx - 1.5, cy - 27.5, cx + 1.5, cy - 24.5, '#55555c')
    return p.done()


def amstrad_cpc():
    p = Pic()
    p.shadow(96, 112, 90, 9)
    body, panel, key = '#2c2c31', '#1d1d21', '#3a3a40'
    x, y, w, h, dep = 6, 88, 168, 12, 46
    p.poly([(x + 5, y - dep), (x + w - 5, y - dep), (x + w, y), (x, y)], panel)
    p.poly([(x + 5, y - dep), (x + w - 5, y - dep), (x + w - 4.6, y - dep + 2), (x + 5.4, y - dep + 2)],
           shade(panel, 0.2))
    # keyboard: 5 rows of dark keys, a red row at the top, green and blue keys to the right
    rows, cols = 5, 14
    kw = 112
    for r in range(rows):
        f0, f1 = r / rows, (r + 0.68) / rows
        ya, yb = y - dep + 6 + f0 * (dep - 12), y - dep + 6 + f1 * (dep - 12)
        ins_a, ins_b = 10 - f0 * 4, 10 - f1 * 4
        xa0, xa1 = x + ins_a, x + ins_a + kw
        xb0, xb1 = x + ins_b, x + ins_b + kw + 2
        for k in range(cols):
            m = 0.14
            a0 = xa0 + (xa1 - xa0) * (k + m) / cols
            a1 = xa0 + (xa1 - xa0) * (k + 1 - m) / cols
            b0 = xb0 + (xb1 - xb0) * (k + m) / cols
            b1 = xb0 + (xb1 - xb0) * (k + 1 - m) / cols
            col = '#c8372d' if r == 0 and k in (0, 13) else key
            p.poly([(a0, ya), (a1, ya), (b1, yb), (b0, yb)], col)
        # the keypad on the right
        for k in range(3):
            c = '#3f9e4a' if r < 4 else '#3b6fc4'
            a0 = x + w - 44 + k * 8.5 + f0 * 2
            p.poly([(a0, ya), (a0 + 6.5, ya), (a0 + 6.8 + 0.6, yb), (a0 + 0.3, yb)], c)
    # the 3-inch drive slot at the right, above the keypad
    p.rrect(x + w - 46, y - dep + 1.5, x + w - 14, y - dep + 5, 1.2, '#111114')
    p.rrect(x + w - 20, y - dep + 2, x + w - 15, y - dep + 4.5, 1, '#d8d8dc')
    p.rrect(x, y, x + w, y + h, 2, body)
    p.rrect(x, y, x + w, y + 1.6, 1, shade(body, 0.25))
    return p.done()


def colecovision():
    p = Pic()
    p.shadow(92, 112, 86, 9)
    body = '#1e1e22'
    x, y, w, h, dep = 10, 84, 120, 16, 34
    p.box(x, y, w, h, dep, body, r=3, inset=4)
    # silver ribbing on the top, the cartridge slot at the back right
    for k in range(7):
        yy = y - dep + 6 + k * 3.6
        p.line([(x + 10 + k * 0.6, yy), (x + 70 - k * 0.6, yy)], '#8a8a92', 1.1)
    p.rrect(x + 80, y - dep + 4, x + w - 12, y - dep + 10, 1.5, '#0e0e10')
    p.rrect(x + 4, y + 4, x + 30, y + 7, 1, '#5a5a62')
    # the controller: a slab with a keypad and a short stick
    cx, cy = 160, 96
    p.rrect(cx - 15, cy - 26, cx + 15, cy + 14, 4, '#26262b')
    p.ellipse(cx - 6, cy - 23, cx + 6, cy - 15, '#141417')
    p.line([(cx, cy - 19), (cx, cy - 32)], '#141417', 3)
    p.ellipse(cx - 3.5, cy - 36, cx + 3.5, cy - 29, '#1b1b1e')
    for r in range(4):
        for c in range(3):
            p.rrect(cx - 10 + c * 7, cy - 10 + r * 5.6, cx - 5 + c * 7, cy - 6 + r * 5.6, 1, '#b8b8c0')
    p.rrect(cx - 18, cy - 6, cx - 15, cy + 2, 1, '#d83a2e')
    p.rrect(cx + 15, cy - 6, cx + 18, cy + 2, 1, '#d83a2e')
    return p.done()


def atari7800():
    p = Pic()
    p.shadow(92, 112, 84, 9)
    body = '#1d1d21'
    x, y, w, h, dep = 16, 86, 116, 14, 40
    # the wedge with its silver band and the cartridge slot
    p.box(x, y, w, h, dep, body, r=3, inset=8)
    p.poly([(x + 10, y - dep + 14), (x + w - 10, y - dep + 14), (x + w - 6, y - dep + 24), (x + 6, y - dep + 24)], '#a9a9b1')
    p.rrect(x + 34, y - dep + 3, x + w - 34, y - dep + 9, 1.5, '#0c0c0e')
    for k in range(4):
        p.rrect(x + 14 + k * 24, y + 4, x + 26 + k * 24, y + 8, 1, '#4a4a52')
    # ProLine joystick: a grip with a button on each side
    cx, cy = 160, 104
    p.rrect(cx - 12, cy - 6, cx + 12, cy + 8, 3, '#26262b')
    p.line([(cx, cy - 2), (cx, cy - 24)], '#141417', 4)
    p.rrect(cx - 5, cy - 34, cx + 5, cy - 20, 3, '#1c1c20')
    p.rrect(cx - 8, cy - 31, cx - 5, cy - 26, 1, '#d83a2e')
    p.rrect(cx + 5, cy - 31, cx + 8, cy - 26, 1, '#d83a2e')
    return p.done()


def favorites():
    """Favourites: a golden star standing on the floor, its arms bevelled
    into a lit and a shaded half each."""
    p = Pic()
    p.shadow(100, 112, 40, 6)
    cx, cy, ro, ri = 100, 70, 40, 17
    def pt(k, r):                       # k: 0 = top point, half steps between
        a = math.radians(-90 + 36 * k)
        return (cx + r * math.cos(a), cy + r * math.sin(a))
    outline = [pt(k, ro if k % 2 == 0 else ri) for k in range(10)]
    gold = '#e2b13c'
    p.poly([(x, y + 5) for x, y in outline], shade(gold, -0.55))     # thickness
    lx, ly = -0.6, -0.8                                             # light: upper left
    for k in range(10):
        a, b = pt(k, ro if k % 2 == 0 else ri), pt(k + 1, ro if (k + 1) % 2 == 0 else ri)
        mx, my = (a[0] + b[0]) / 2 - cx, (a[1] + b[1]) / 2 - cy
        n = math.hypot(mx, my)
        f = (mx * lx + my * ly) / n
        p.poly([(cx, cy), a, b], shade(gold, 0.45 * f if f > 0 else 0.4 * f))
    return p.done()


DEVICES = {
    'nes': nes,
    'sms': master_system,
    'md': mega_drive,
    'gg': game_gear,
    'pce': pc_engine,
    'gbc': game_boy_color,
    'c64': c64,
    'msx': msx2,
    'sg': sg1000,
    'a26': atari2600,
    'zx': zx_spectrum,
    'cpc': amstrad_cpc,
    'cv': colecovision,
    'a78': atari7800,
    'usb': usb_drive,
    'fav': favorites,
}

if __name__ == '__main__':
    out = sys.argv[1]
    for name, fn in DEVICES.items():
        fn().save(f'{out}/{name}.png')
