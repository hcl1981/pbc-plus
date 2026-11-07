"""Gerätebilder mit Kontaktschatten: Der Schatten folgt der Unterkante jedes Teils
(Konsole und Controller liegen beide auf), Gerät + Schatten mittig im 200x128-Bild.
Quelle: Bilder aus import_icons.py (deren Ellipsenschatten wird exakt herausgerechnet).
   python3 kontaktschatten.py <ausgabeordner> <bild.png> ..."""
import sys, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
W, H = 200, 128
BOX_W, BOX_H = 188, 108
UNTEN = 9                     # Platz für den Schatten unter dem Gerät
GROESSE = {'ard': 0.82}       # kleiner als die anderen (das Arduboy ist scheckkartengroß)

def ellipse(cx, cy, rx, ry=7):
    sh = Image.new('L', (W * 4, H * 4), 0)
    ImageDraw.Draw(sh).ellipse([(cx - rx) * 4, (cy - ry) * 4, (cx + rx) * 4, (cy + ry) * 4], fill=110)
    return np.array(sh.filter(ImageFilter.GaussianBlur(20)).resize((W, H), Image.LANCZOS)).astype(float) / 255

def objekt(pfad):
    im = np.array(Image.open(pfad).convert('RGBA')).astype(float)
    A = im[..., 3] / 255; C = im[..., :3]
    ow = min(range(20, 200), key=lambda w: np.abs(ellipse(W / 2, H - 7, w * 0.46)[123:] - A[123:]).sum())
    S = ellipse(W / 2, H - 7, ow * 0.46)
    ao = np.clip((A - S) / np.maximum(1 - S, 1e-3), 0, 1); ao[ao < 0.03] = 0
    co = np.clip(np.where(ao[..., None] > 0, C * A[..., None] / np.maximum(ao[..., None], 1e-3), 0), 0, 255)
    o = Image.fromarray(np.dstack([co, ao * 255]).round().astype('uint8'), 'RGBA')
    return o.crop(o.getchannel('A').point(lambda v: 255 if v > 8 else 0).getbbox())

def kontakt(alpha):
    """Schattenmaske: an jeder Unterkante (opak, darunter leer) ein flaches weiches Band."""
    m = alpha > 140
    kante = m & ~np.vstack([m[1:], np.zeros((1, m.shape[1]), bool)])
    saat = np.zeros_like(m, float)
    ys, xs = np.nonzero(kante)
    for y, x in zip(ys, xs):
        saat[y:y + 5, x] = 1.0
    sh = Image.fromarray((saat * 255).astype('uint8')).filter(ImageFilter.GaussianBlur(3.2))
    sh = np.array(sh).astype(float) / 255
    return np.clip(sh * 1.6, 0, 1) * 0.5

def setzen(o, f=1.0):
    s = min(BOX_W / o.width, BOX_H / o.height, 1.0) * f
    if s < 1:
        o = o.convert('RGBa').resize((round(o.width * s), round(o.height * s)), Image.LANCZOS).convert('RGBA')
    x, y = (W - o.width) // 2, (H - (o.height + UNTEN)) // 2
    lage = Image.new('RGBA', (W, H), (0, 0, 0, 0)); lage.paste(o, (x, y))
    S = kontakt(np.array(lage)[..., 3])
    sch = Image.new('RGBA', (W, H), (0, 0, 0, 255)); sch.putalpha(Image.fromarray((S * 255).round().astype('uint8')))
    return Image.alpha_composite(sch, lage)

if __name__ == '__main__':
    aus = sys.argv[1]
    for pfad in sys.argv[2:]:
        # fertiges Objekt ohne Schatten (teile_*.png) direkt setzen, sonst Schatten herausrechnen
        o = Image.open(pfad).convert('RGBA') if os.path.basename(pfad).startswith('teile_') else objekt(pfad)
        name = os.path.basename(pfad).replace('teile_', '')
        setzen(o, GROESSE.get(name[:-4], 1.0)).save(os.path.join(aus, name)); print(name, end=' ')
    print()
