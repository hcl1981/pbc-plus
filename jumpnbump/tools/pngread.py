import zlib, struct

def load_png(fn):
    """Liefert (w, h, rgba-bytes) fuer 8-Bit-PNG der Farbtypen 2 (RGB) und 6 (RGBA)."""
    d = open(fn, 'rb').read()
    pos, idat = 8, b''
    w = h = ct = None
    while pos < len(d):
        ln, = struct.unpack('>I', d[pos:pos+4])
        typ = d[pos+4:pos+8]
        data = d[pos+8:pos+8+ln]
        pos += 12 + ln
        if typ == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', data[:10])
            assert bd == 8 and ct in (2, 6), (bd, ct)
        elif typ == b'IDAT':
            idat += data
        elif typ == b'IEND':
            break
    raw = zlib.decompress(idat)
    bpp = 3 if ct == 2 else 4
    stride = w * bpp
    out = bytearray()
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        f = raw[p]; p += 1
        line = bytearray(raw[p:p+stride]); p += stride
        for i in range(stride):
            a = line[i-bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i-bpp] if i >= bpp else 0
            if f == 1:   line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a+b)//2) & 255
            elif f == 4:
                pa, pb, pc = abs(b-c), abs(a-c), abs(a+b-2*c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        out += line
        prev = line
    if bpp == 3:  # auf RGBA aufblasen
        rgba = bytearray(w*h*4)
        for i in range(w*h):
            rgba[i*4:i*4+3] = out[i*3:i*3+3]
            rgba[i*4+3] = 255
        out = rgba
    return w, h, bytes(out)

def box_scale(w, h, rgba, nw, nh):
    """Flaechenmittelung mit Alpha-Gewichtung (sonst blutet Schwarz in die Raender)."""
    dst = bytearray(nw*nh*4)
    for y in range(nh):
        y0, y1 = y*h//nh, max(y*h//nh+1, (y+1)*h//nh)
        for x in range(nw):
            x0, x1 = x*w//nw, max(x*w//nw+1, (x+1)*w//nw)
            sr = sg = sb = sa = 0; n = 0
            for sy in range(y0, y1):
                for sx in range(x0, x1):
                    o = (sy*w+sx)*4
                    a = rgba[o+3]
                    sr += rgba[o]*a; sg += rgba[o+1]*a; sb += rgba[o+2]*a
                    sa += a; n += 1
            o = (y*nw+x)*4
            if sa:
                dst[o]   = min(255, sr//sa)
                dst[o+1] = min(255, sg//sa)
                dst[o+2] = min(255, sb//sa)
            dst[o+3] = sa//n
    return bytes(dst)

def save_png(fn, w, h, rgba, scale=1):
    raw = bytearray()
    for y in range(h*scale):
        raw.append(0)
        for x in range(w*scale):
            o = ((y//scale)*w + (x//scale))*4
            raw += rgba[o:o+3]
    comp = zlib.compress(bytes(raw), 9)
    def ch(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t+d))
    open(fn, 'wb').write(b'\x89PNG\r\n\x1a\n' +
        ch(b'IHDR', struct.pack('>IIBBBBB', w*scale, h*scale, 8, 2, 0, 0, 0)) +
        ch(b'IDAT', comp) + ch(b'IEND', b''))
