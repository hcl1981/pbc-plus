// Native stream type used by png.py to feed compressed PNG IDAT data
// into deflate.DeflateIO without first concatenating every chunk's
// body into RAM.
//
// The Python side does this:
//
//     idat = display._IdatStream(open_file_handle, chunks_list)
//     inflater = deflate.DeflateIO(idat, deflate.ZLIB)
//     display._png_decode_scanlines(inflater, ...)
//
// `chunks_list` is a Python list of (offset, length) tuples giving the
// position and size of every IDAT chunk inside the PNG file. The
// adapter reads them in order, jumping over the 4-byte CRCs and PNG
// chunk headers that sit between them. To the inflater it looks like
// one contiguous compressed stream.
//
// We implement MicroPython's C-level stream protocol (read / ioctl) so
// deflate sees a "native" stream and doesn't go through any Python
// indirection -- that's what burned us with the recursion-depth bug
// when _IdatReader was a Python IOBase subclass.

#include "py/obj.h"
#include "py/runtime.h"
#include "py/stream.h"

#include "idat_stream.h"

#include <string.h>

// Maximum IDAT chunks we'll honour per image. PNGs from common tools
// have one or a few; this is a safety net against malformed files
// claiming an absurd number of chunks. The array of (off, len) pairs
// lives inline in the object so we don't malloc here.
#define MAX_IDAT_CHUNKS 64

typedef struct _idat_stream_obj_t {
    mp_obj_base_t base;
    mp_obj_t file;               // open file object we read from
    const mp_stream_p_t *file_p;  // its stream protocol pointer
    uint32_t off[MAX_IDAT_CHUNKS];
    uint32_t len[MAX_IDAT_CHUNKS];
    int n_chunks;
    int cur_chunk;
    uint32_t cur_pos;             // bytes already read from cur_chunk
} idat_stream_obj_t;

extern const mp_obj_type_t display_idat_stream_type;

// Seek `file` to absolute byte `pos`. MicroPython's stream protocol
// uses an mp_obj_stream_seek that goes through the high-level API,
// but for a plain file object we can call ioctl(SEEK) directly.
static int seek_file(idat_stream_obj_t *self, uint32_t pos) {
    struct mp_stream_seek_t seek = { .offset = pos, .whence = MP_SEEK_SET };
    int err;
    mp_uint_t r = self->file_p->ioctl(self->file,
                                      MP_STREAM_SEEK, (uintptr_t)&seek, &err);
    if (r == MP_STREAM_ERROR) return -1;
    return 0;
}

// Stream-protocol read entry point. Pulls up to `size` bytes from the
// current IDAT chunk (or whatever follows). Returns 0 at EOF, the
// number of bytes filled otherwise.
static mp_uint_t idat_stream_read(mp_obj_t self_in, void *buf_, mp_uint_t size,
                                  int *errcode) {
    idat_stream_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t *buf = (uint8_t *)buf_;
    mp_uint_t got = 0;

    while (got < size && self->cur_chunk < self->n_chunks) {
        uint32_t remain = self->len[self->cur_chunk] - self->cur_pos;
        if (remain == 0) {
            // Done with this chunk -- move on.
            self->cur_chunk++;
            self->cur_pos = 0;
            if (self->cur_chunk < self->n_chunks) {
                if (seek_file(self, self->off[self->cur_chunk]) != 0) {
                    *errcode = MP_EIO;
                    return MP_STREAM_ERROR;
                }
            }
            continue;
        }
        mp_uint_t want = size - got;
        if (want > remain) want = remain;

        mp_uint_t r = self->file_p->read(self->file, buf + got, want, errcode);
        if (r == MP_STREAM_ERROR) return MP_STREAM_ERROR;
        if (r == 0) break;        // unexpected EOF in the middle of an IDAT
        got += r;
        self->cur_pos += r;
    }
    return got;
}

static mp_uint_t idat_stream_ioctl(mp_obj_t self_in, mp_uint_t request,
                                   uintptr_t arg, int *errcode) {
    (void)self_in; (void)arg;
    // We're read-only and unseekable from the inflater's perspective.
    // deflate.DeflateIO doesn't actually call any ioctl on us; just
    // refuse politely for everything else.
    *errcode = MP_EINVAL;
    return MP_STREAM_ERROR;
    (void)request;
}

static const mp_stream_p_t idat_stream_p = {
    .read  = idat_stream_read,
    .ioctl = idat_stream_ioctl,
    .is_text = false,
};

// Python-facing constructor: display._IdatStream(file, [(off, len), ...])
static mp_obj_t idat_stream_make_new(const mp_obj_type_t *type, size_t n_args,
                                     size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 2, 2, false);

    mp_obj_t file = args[0];
    mp_obj_t list = args[1];

    idat_stream_obj_t *self = mp_obj_malloc(idat_stream_obj_t, type);
    self->file   = file;
    self->file_p = mp_get_stream_raise(file, MP_STREAM_OP_READ | MP_STREAM_OP_IOCTL);
    self->n_chunks = 0;
    self->cur_chunk = 0;
    self->cur_pos = 0;

    mp_obj_iter_buf_t it_buf;
    mp_obj_t iter = mp_getiter(list, &it_buf);
    mp_obj_t item;
    while ((item = mp_iternext(iter)) != MP_OBJ_STOP_ITERATION) {
        if (self->n_chunks >= MAX_IDAT_CHUNKS) {
            mp_raise_ValueError(MP_ERROR_TEXT("too many IDAT chunks"));
        }
        size_t pair_len;
        mp_obj_t *pair_items;
        mp_obj_get_array(item, &pair_len, &pair_items);
        if (pair_len != 2) {
            mp_raise_ValueError(MP_ERROR_TEXT("IDAT entry needs (off, len)"));
        }
        self->off[self->n_chunks] = (uint32_t)mp_obj_get_int(pair_items[0]);
        self->len[self->n_chunks] = (uint32_t)mp_obj_get_int(pair_items[1]);
        self->n_chunks++;
    }

    // Seek to the first chunk so the first read() finds bytes ready.
    if (self->n_chunks > 0) {
        if (seek_file(self, self->off[0]) != 0) {
            mp_raise_OSError(MP_EIO);
        }
    }
    return MP_OBJ_FROM_PTR(self);
}

MP_DEFINE_CONST_OBJ_TYPE(
    display_idat_stream_type,
    MP_QSTR__IdatStream,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    make_new, idat_stream_make_new,
    protocol, &idat_stream_p
    );
