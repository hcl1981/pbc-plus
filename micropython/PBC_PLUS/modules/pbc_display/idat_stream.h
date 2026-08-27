// Streaming-IDAT adapter (see idat_stream.c).
//
// Exposed to Python via display._IdatStream so png.py can wrap it
// in deflate.DeflateIO without first copying every IDAT body into
// a giant bytearray.

#ifndef PBC_DISPLAY_IDAT_STREAM_H
#define PBC_DISPLAY_IDAT_STREAM_H

#include "py/obj.h"

extern const mp_obj_type_t display_idat_stream_type;

#endif
