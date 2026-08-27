// PNG scanline decoder: native-code hot path for png.py.
//
// The Python decoder still parses the PNG header chunks, locates the
// IDAT slices, and wraps them in a deflate.DeflateIO. All of that is
// cheap (runs once). The work that hurts performance -- looping over
// every pixel of every scanline -- happens here in C.
//
// Exposed as display._png_decode_scanlines(...). Python should treat
// it as an implementation detail; pbc.load_image() / png.load() is
// the public surface.

#ifndef PBC_DISPLAY_PNG_DECODE_H
#define PBC_DISPLAY_PNG_DECODE_H

#include "py/obj.h"

// Reads `height` scanlines from `inflater` (a deflate.DeflateIO or any
// other readable stream), runs PNG filter reconstruction, converts to
// RGB565 little-endian, and writes the result into `out`.
//
// Python signature:
//   _png_decode_scanlines(inflater, out, width, height,
//                         bpp, stride, ctype, depth, key,
//                         pal_lo, pal_hi)
//
//   inflater : a readable stream (e.g. deflate.DeflateIO)
//   out      : a writable bytearray of size width*height*2
//   width    : pixel width
//   height   : pixel height
//   bpp      : bytes per pixel for filter purposes (1, 2, 3 or 4)
//   stride   : bytes per scanline (no filter byte)
//   ctype    : PNG colour type (0, 2, 3 or 6)
//   depth    : bit depth (currently only 8 is handled fully; sub-byte
//              expansion still happens in Python before calling us)
//   key      : RGB565 transparency sentinel (typically 0xF81F)
//   pal_lo   : bytearray of low palette bytes (ctype 3 only, else None)
//   pal_hi   : bytearray of high palette bytes (ctype 3 only, else None)
//
// Returns None on success, raises on errors (truncated stream, bad
// filter type, etc).
extern const mp_obj_fun_builtin_var_t display_png_decode_scanlines_obj;

#endif
