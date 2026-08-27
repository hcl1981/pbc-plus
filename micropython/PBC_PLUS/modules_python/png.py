# Tiny PNG decoder for the PBC+.
#
# Thin Python wrapper around display._png_decode_scanlines in C. The
# Python side handles things that run once per image (chunk parsing,
# IDAT stream setup); the C side handles the per-pixel work that
# dominates decode time. A 240x280 RGBA image decodes in well under a
# second on the RP2350.
#
# Supports:
#   color type 0 (grayscale, bit depths 1/2/4/8)
#   color type 2 (RGB 8-bit)
#   color type 3 (palette, bit depths 1/2/4/8) with optional tRNS
#   color type 6 (RGBA 8-bit)
# Not supported: interlaced PNGs, 16-bit channels.
#
# Memory model: STREAMING. We never hold the whole file or the whole
# decompressed pixel data in RAM at the same time. The PNG is parsed
# off disk via the open file handle, IDAT chunks are fed to deflate
# through a custom file-like adapter, and the C code reads exactly
# one scanline at a time from the inflater.
#
# Transparency model:
#   - PNG alpha < 128 (or palette index in tRNS) → maps to the
#     reserved RGB565 value KEY (0xF81F, magenta).
#   - REAL magenta pixels in the source PNG are nudged by one LSB
#     to 0xF81E -- a single bit of blue, visually indistinguishable
#     from magenta -- so they don't collide with the transparency
#     key.

import struct
import deflate
import io
import gc
import display     # for the _png_decode_scanlines native function

_MAGIC = b"\x89PNG\r\n\x1a\n"
KEY    = 0xF81F   # transparent sentinel, RGB565 (= pure magenta)


def _scan_chunks(f):
    """Walk the PNG file once, capturing everything needed before we
    can start streaming: header fields, palette, transparency mask,
    and a list of IDAT chunk locations."""
    if f.read(8) != _MAGIC:
        raise ValueError("not a PNG")

    width = height = depth = ctype = 0
    palette = None
    trns = None
    idat_chunks = []

    while True:
        hdr = f.read(8)
        if len(hdr) < 8:
            break
        length, ctag = struct.unpack(">I4s", hdr)
        if ctag == b"IHDR":
            body = f.read(length)
            (width, height, depth, ctype, comp, filt, interlace) = \
                struct.unpack(">IIBBBBB", body)
            if comp != 0 or filt != 0:
                raise ValueError("unsupported PNG compression/filter")
            if interlace != 0:
                raise ValueError("interlaced PNGs not supported")
            if depth == 16:
                raise ValueError("16-bit PNGs not supported")
            f.seek(4, 1)
        elif ctag == b"PLTE":
            palette = f.read(length)
            f.seek(4, 1)
        elif ctag == b"tRNS":
            trns = f.read(length)
            f.seek(4, 1)
        elif ctag == b"IDAT":
            idat_chunks.append((f.tell(), length))
            f.seek(length + 4, 1)
        elif ctag == b"IEND":
            break
        else:
            f.seek(length + 4, 1)

    return width, height, depth, ctype, palette, trns, idat_chunks


def _build_palette_tables(palette, trns, key):
    """Pre-compute (pal_lo, pal_hi) bytearrays of RGB565 values for an
    indexed PNG, applying tRNS-based transparency to the key colour."""
    n_pal = len(palette) // 3
    pal_lo = bytearray(n_pal)
    pal_hi = bytearray(n_pal)
    for i in range(n_pal):
        r = palette[i * 3]
        g = palette[i * 3 + 1]
        b = palette[i * 3 + 2]
        c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        if c == key:
            c -= 1
        pal_lo[i] = c & 0xFF
        pal_hi[i] = (c >> 8) & 0xFF
    if trns is not None:
        for i in range(min(len(trns), n_pal)):
            if trns[i] < 128:
                pal_lo[i] = key & 0xFF
                pal_hi[i] = (key >> 8) & 0xFF
    return pal_lo, pal_hi


def _decode_stream(f, key):
    """Streaming decode core. `f` is an open file-like at byte 0."""
    (width, height, depth, ctype,
     palette, trns, idat_chunks) = _scan_chunks(f)

    if   ctype == 0: spp = 1
    elif ctype == 2: spp = 3
    elif ctype == 3: spp = 1
    elif ctype == 6: spp = 4
    else:
        raise ValueError("color type %d not supported" % ctype)

    bits_per_pixel = depth * spp
    bpp = max(1, (bits_per_pixel + 7) // 8)
    stride = (width * bits_per_pixel + 7) // 8

    pal_lo = None
    pal_hi = None
    if ctype == 3:
        if palette is None:
            raise ValueError("indexed PNG without PLTE")
        pal_lo, pal_hi = _build_palette_tables(palette, trns, key)

    palette = None
    trns = None
    gc.collect()

    out = bytearray(width * height * 2)
    gc.collect()

    # Wrap the open file in our C-side IDAT adapter -- it reads
    # straight from disk and skips chunk boundaries on the fly. No
    # extra copy of the compressed IDAT bytes in RAM.
    idat = display._IdatStream(f, idat_chunks)
    inflater = deflate.DeflateIO(idat, deflate.ZLIB)

    # Hand off the inner loop to native code. This is where the
    # actual speedup lives -- a 240x280 RGBA image goes from ~10 s in
    # pure Python to <200 ms here.
    display._png_decode_scanlines(
        inflater, out, width, height,
        bpp, stride, ctype, depth, key,
        pal_lo, pal_hi,
    )

    return (width, height, out)


def load(path, key=KEY):
    """Decode the PNG at `path` and return (width, height, rgb565_bytes).

    Streams from disk -- never holds the full file or the full
    decompressed pixel array in RAM. Peak allocation is the output
    buffer (W*H*2 bytes) plus deflate's internal window."""
    f = open(path, "rb")
    try:
        return _decode_stream(f, key)
    finally:
        f.close()


def decode(data, key=KEY):
    """Decode in-memory PNG bytes. Prefer load(path) for larger
    files -- decode() means the caller is already holding the whole
    file in RAM."""
    return _decode_stream(io.BytesIO(data), key)
