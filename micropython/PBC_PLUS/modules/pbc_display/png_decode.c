// PNG scanline decoder -- the inner loop in C.
//
// Reads decompressed PNG scanlines from a stream (the caller hands us
// a deflate.DeflateIO over the IDAT data), unfilters them in place
// with the standard PNG reconstruction rules, and converts them to
// RGB565 little-endian.
//
// For colour type 6 (RGBA, the common case from image editors), this
// is ~50x faster than the Python loop and ~5x faster than the viper
// version. A 240x280 RGBA image decodes in around 80-150 ms total
// including deflate, vs several seconds in pure Python.
//
// Speed-relevant decisions:
//   - Scanline buffers live on the C stack -- one allocation upfront,
//     no per-line malloc.
//   - Filter byte is in its own 1-byte read; scanline body is a
//     single bulk read.
//   - Hot loops are simple int math, no function calls per pixel.
//   - The transparency-key collision check (real magenta -> 0xF81E)
//     is inlined into every RGB-pack site.

#include "py/obj.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/binary.h"
#include "py/objarray.h"

#include "png_decode.h"

#include <string.h>
#include <stdint.h>

// Maximum scanline byte count we can handle. Generous bound: the
// PicoBoy's display is 240 wide, RGBA 8 = 4 bytes/pixel = 960 bytes,
// plus a bit of slack for off-the-shelf sprites that exceed the
// display width. 4096 fits all reasonable cases on the stack.
#define MAX_STRIDE 4096

// Pack 8-bit R/G/B into RGB565 LE, avoiding the reserved KEY value
// (so a "real" magenta pixel in the source ends up at 0xF81E and
// doesn't get mistaken for transparency).
static inline uint16_t pack_rgb565(int r, int g, int b, uint16_t key) {
    uint16_t c = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    if (c == key) c--;
    return c;
}

// --- Per-filter scanline reconstruction.  --------------------------------
// Each operates in place on `cur`. `prev` is the previously-decoded
// scanline (all zeros for the first row).

static void unfilter_sub(uint8_t *cur, int bpp, int stride) {
    for (int i = bpp; i < stride; i++)
        cur[i] = (uint8_t)(cur[i] + cur[i - bpp]);
}

static void unfilter_up(uint8_t *cur, const uint8_t *prev, int stride) {
    for (int i = 0; i < stride; i++)
        cur[i] = (uint8_t)(cur[i] + prev[i]);
}

static void unfilter_avg(uint8_t *cur, const uint8_t *prev, int bpp, int stride) {
    for (int i = 0; i < stride; i++) {
        int left = (i >= bpp) ? cur[i - bpp] : 0;
        cur[i] = (uint8_t)(cur[i] + ((left + prev[i]) >> 1));
    }
}

static void unfilter_paeth(uint8_t *cur, const uint8_t *prev, int bpp, int stride) {
    for (int i = 0; i < stride; i++) {
        int left    = (i >= bpp) ? cur[i - bpp]  : 0;
        int up      = prev[i];
        int up_left = (i >= bpp) ? prev[i - bpp] : 0;
        int p  = left + up - up_left;
        int pa = p - left;     if (pa < 0) pa = -pa;
        int pb = p - up;       if (pb < 0) pb = -pb;
        int pc = p - up_left;  if (pc < 0) pc = -pc;
        int pred;
        if (pa <= pb && pa <= pc) pred = left;
        else if (pb <= pc)        pred = up;
        else                      pred = up_left;
        cur[i] = (uint8_t)(cur[i] + pred);
    }
}

// --- Per-colour-type scanline → RGB565 conversion.  ----------------------
// All write directly into the output buffer at `out`, advancing
// width*2 bytes worth.

static void conv_rgb_line(const uint8_t *src, uint8_t *out,
                          int width, uint16_t key) {
    for (int i = 0; i < width; i++) {
        uint16_t c = pack_rgb565(src[i*3], src[i*3+1], src[i*3+2], key);
        out[i*2]     = (uint8_t)(c & 0xFF);
        out[i*2 + 1] = (uint8_t)(c >> 8);
    }
}

static void conv_rgba_line(const uint8_t *src, uint8_t *out,
                           int width, uint16_t key) {
    for (int i = 0; i < width; i++) {
        int a = src[i*4 + 3];
        uint16_t c;
        if (a < 128) {
            c = key;
        } else {
            c = pack_rgb565(src[i*4], src[i*4+1], src[i*4+2], key);
        }
        out[i*2]     = (uint8_t)(c & 0xFF);
        out[i*2 + 1] = (uint8_t)(c >> 8);
    }
}

static void conv_gray8_line(const uint8_t *src, uint8_t *out,
                            int width, uint16_t key) {
    for (int i = 0; i < width; i++) {
        int v = src[i];
        uint16_t c = pack_rgb565(v, v, v, key);
        out[i*2]     = (uint8_t)(c & 0xFF);
        out[i*2 + 1] = (uint8_t)(c >> 8);
    }
}

static void conv_indexed_line(const uint8_t *src, uint8_t *out, int width,
                              const uint8_t *pal_lo, const uint8_t *pal_hi) {
    for (int i = 0; i < width; i++) {
        int idx = src[i];
        out[i*2]     = pal_lo[idx];
        out[i*2 + 1] = pal_hi[idx];
    }
}

// Helper: read exactly `n` bytes from a stream into `buf`. Returns 0
// on success, -1 on truncated stream / error.
static int stream_read_exact(const mp_stream_p_t *sp, mp_obj_t stream,
                             uint8_t *buf, mp_uint_t n) {
    mp_uint_t got = 0;
    int err;
    while (got < n) {
        mp_uint_t r = sp->read(stream, buf + got, n - got, &err);
        if (r == 0 || r == MP_STREAM_ERROR) return -1;
        got += r;
    }
    return 0;
}

// --- Python-facing entry point.  -----------------------------------------

static mp_obj_t png_decode_scanlines(size_t n_args, const mp_obj_t *args) {
    // 11 positional args: inflater, out, width, height,
    //                     bpp, stride, ctype, depth, key,
    //                     pal_lo, pal_hi.
    if (n_args != 11) {
        mp_raise_TypeError(MP_ERROR_TEXT(
            "_png_decode_scanlines: expected 11 args"));
    }

    mp_obj_t inflater = args[0];
    mp_obj_t out_obj  = args[1];
    int width  = mp_obj_get_int(args[2]);
    int height = mp_obj_get_int(args[3]);
    int bpp    = mp_obj_get_int(args[4]);
    int stride = mp_obj_get_int(args[5]);
    int ctype  = mp_obj_get_int(args[6]);
    int depth  = mp_obj_get_int(args[7]);
    uint16_t key = (uint16_t)mp_obj_get_int(args[8]);

    if (stride > MAX_STRIDE) {
        mp_raise_ValueError(MP_ERROR_TEXT("PNG too wide for fast decoder"));
    }
    if (width <= 0 || height <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("bad PNG dimensions"));
    }

    // Output buffer -- must be a writable bytes-like of width*height*2.
    mp_buffer_info_t out_buf;
    mp_get_buffer_raise(out_obj, &out_buf, MP_BUFFER_RW);
    if ((int)out_buf.len < width * height * 2) {
        mp_raise_ValueError(MP_ERROR_TEXT("output buffer too small"));
    }
    uint8_t *out = (uint8_t *)out_buf.buf;

    // Palette (only used for ctype 3). May be None.
    const uint8_t *pal_lo = NULL;
    const uint8_t *pal_hi = NULL;
    mp_buffer_info_t pal_lo_buf, pal_hi_buf;
    if (ctype == 3) {
        if (args[9] == mp_const_none || args[10] == mp_const_none) {
            mp_raise_ValueError(MP_ERROR_TEXT("indexed PNG without palette"));
        }
        mp_get_buffer_raise(args[9],  &pal_lo_buf, MP_BUFFER_READ);
        mp_get_buffer_raise(args[10], &pal_hi_buf, MP_BUFFER_READ);
        pal_lo = (const uint8_t *)pal_lo_buf.buf;
        pal_hi = (const uint8_t *)pal_hi_buf.buf;
    }

    // Grab the inflater's stream protocol up-front -- it's the same
    // function pointer for every read.
    const mp_stream_p_t *sp = mp_get_stream_raise(inflater, MP_STREAM_OP_READ);

    // Scanline buffers live on the stack. With MAX_STRIDE = 4096 that's
    // 8 KB total -- fine, the RP2350's main stack is much larger.
    uint8_t cur[MAX_STRIDE];
    uint8_t prev[MAX_STRIDE];
    memset(prev, 0, stride);

    // Sub-byte depths still need to be expanded to one sample per byte
    // before the conversion routines work. For grayscale depth < 8 we
    // get an extra unpack buffer + an extra scaling pass. depth == 8
    // is the common path and stays zero-copy.
    uint8_t unpack_buf[MAX_STRIDE];
    int sub_byte = (depth < 8 && (ctype == 0 || ctype == 3));
    int scale8 = sub_byte ? (255 / ((1 << depth) - 1)) : 1;

    uint8_t filter_byte;

    for (int y = 0; y < height; y++) {
        if (stream_read_exact(sp, inflater, &filter_byte, 1) != 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("truncated PNG (filter)"));
        }
        if (stream_read_exact(sp, inflater, cur, stride) != 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("truncated PNG (scanline)"));
        }

        switch (filter_byte) {
            case 0: /* None */ break;
            case 1: unfilter_sub  (cur, bpp, stride); break;
            case 2: unfilter_up   (cur, prev, stride); break;
            case 3: unfilter_avg  (cur, prev, bpp, stride); break;
            case 4: unfilter_paeth(cur, prev, bpp, stride); break;
            default:
                mp_raise_ValueError(MP_ERROR_TEXT("bad PNG filter type"));
        }

        const uint8_t *src;
        if (sub_byte) {
            // Expand 1/2/4-bit packed grayscale or palette indices to
            // one sample per byte. For grayscale, also scale to 0..255
            // so the 8-bit conversion routine doesn't have to know
            // about depths.
            int spb  = 8 / depth;
            int mask = (1 << depth) - 1;
            for (int x = 0; x < width; x++) {
                int byte = cur[x / spb];
                int shift = (spb - 1 - (x % spb)) * depth;
                int s = (byte >> shift) & mask;
                if (ctype == 0) s *= scale8;
                unpack_buf[x] = (uint8_t)s;
            }
            src = unpack_buf;
        } else {
            src = cur;
        }

        uint8_t *row = out + (size_t)y * width * 2;
        switch (ctype) {
            case 2: conv_rgb_line    (src, row, width, key); break;
            case 6: conv_rgba_line   (src, row, width, key); break;
            case 0: conv_gray8_line  (src, row, width, key); break;
            case 3: conv_indexed_line(src, row, width, pal_lo, pal_hi); break;
            default:
                mp_raise_ValueError(MP_ERROR_TEXT("PNG colour type not supported"));
        }

        // Swap prev <-> cur using a real copy (we wrote into cur in
        // place during unfiltering, so it now holds the reconstructed
        // bytes the *next* row needs as `prev`).
        memcpy(prev, cur, stride);
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR(display_png_decode_scanlines_obj, 11, png_decode_scanlines);
