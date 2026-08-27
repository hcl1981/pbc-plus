// MicroPython binding for the PBC+ display.
//
// Provides:
//   display.Display()            -- singleton-ish driver instance
//   display.rgb(r, g, b) -> int  -- pack RGB888 into RGB565
//   display.WIDTH / HEIGHT
//   display.BLACK, WHITE, RED, GREEN, BLUE, YELLOW, CYAN,
//                  MAGENTA, GRAY, ORANGE, PURPLE
//
// Display methods:
//   fill(color)
//   fill_rect(x, y, w, h, color)
//   pixel(x, y, color)
//   hline(x, y, w, color)
//   vline(x, y, h, color)
//   line(x0, y0, x1, y1, color)
//   rect(x, y, w, h, color, fill=False)
//   ellipse(cx, cy, xr, yr, color, fill=False, mask=0x0F)
//   poly(ox, oy, coords, color, fill=False)        # coords: bytes-like
//   text(s, x, y, color, bg=None)                  # 8x8 petme
//   text12(s, x, y, color, bg=None)                # 12-px proportional
//   text12_width(s) -> int
//   blit(buf, x=0, y=0, w=WIDTH, h=HEIGHT)         # bytes-like RGB565
//   set_backlight(level)                           # 0..255

#include "py/runtime.h"
#include "py/objstr.h"

#include "st7789.h"
#include "drawing.h"
#include "png_decode.h"
#include "idat_stream.h"
#include "colors.h"

// --------------------------------------------------------------------
// Display object: stateless wrapper around the C driver.
// --------------------------------------------------------------------

typedef struct _display_obj_t {
    mp_obj_base_t base;
} display_obj_t;

const mp_obj_type_t display_type;
static display_obj_t display_singleton = { { &display_type } };

// Boot splash: white "+" on black, ~70% of screen width, balken
// 1/3 of cross size. Replaces uninitialised LCD RAM noise as soon
// as the panel comes up. The first user graphics call (fill, show,
// text, ...) overwrites it -- no flag tracking needed.
static void draw_boot_splash(void) {
    int cross_size    = ST7789_WIDTH * 7 / 10;        // 168 on 240
    int bar_thickness = cross_size / 4;               // 42
    int cross_x       = (ST7789_WIDTH  - cross_size) / 2;
    int cross_y       = (ST7789_HEIGHT - cross_size) / 2;
    int hbar_y        = cross_y + (cross_size - bar_thickness) / 2;
    int vbar_x        = cross_x + (cross_size - bar_thickness) / 2;

    // Fill black
    st7789_set_window(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);
    st7789_fill_color(COLOR_BLACK,
                      (uint32_t)ST7789_WIDTH * ST7789_HEIGHT);

    // Horizontal bar
    st7789_set_window(cross_x, hbar_y,
                      cross_x + cross_size - 1,
                      hbar_y + bar_thickness - 1);
    st7789_fill_color(COLOR_WHITE,
                      (uint32_t)cross_size * bar_thickness);

    // Vertical bar (overlaps the horizontal one in the middle --
    // writing white over white is harmless).
    st7789_set_window(vbar_x, cross_y,
                      vbar_x + bar_thickness - 1,
                      cross_y + cross_size - 1);
    st7789_fill_color(COLOR_WHITE,
                      (uint32_t)bar_thickness * cross_size);
}

static mp_obj_t display_make_new(const mp_obj_type_t *type,
                                 size_t n_args, size_t n_kw,
                                 const mp_obj_t *args) {
    (void)type; (void)n_args; (void)n_kw; (void)args;
    st7789_init(62500000);   // 62.5 MHz SPI -- max safe for ST7789
    draw_boot_splash();
    return MP_OBJ_FROM_PTR(&display_singleton);
}

// --- fill -----------------------------------------------------------

static mp_obj_t display_fill(mp_obj_t self_in, mp_obj_t color_in) {
    (void)self_in;
    uint16_t color = (uint16_t)mp_obj_get_int(color_in);
    st7789_set_window(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);
    st7789_fill_color(color, (uint32_t)ST7789_WIDTH * ST7789_HEIGHT);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_fill_obj, display_fill);

// --- fill_rect ------------------------------------------------------

static mp_obj_t display_fill_rect(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    int x = mp_obj_get_int(args[1]);
    int y = mp_obj_get_int(args[2]);
    int w = mp_obj_get_int(args[3]);
    int h = mp_obj_get_int(args[4]);
    uint16_t color = (uint16_t)mp_obj_get_int(args[5]);

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ST7789_WIDTH)  w = ST7789_WIDTH  - x;
    if (y + h > ST7789_HEIGHT) h = ST7789_HEIGHT - y;
    if (w <= 0 || h <= 0) return mp_const_none;

    st7789_set_window(x, y, x + w - 1, y + h - 1);
    st7789_fill_color(color, (uint32_t)w * h);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(display_fill_rect_obj, 6, 6,
                                           display_fill_rect);

// --- pixel ----------------------------------------------------------

static mp_obj_t display_pixel(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    int x = mp_obj_get_int(args[1]);
    int y = mp_obj_get_int(args[2]);
    uint16_t color = (uint16_t)mp_obj_get_int(args[3]);
    if (x < 0 || x >= ST7789_WIDTH || y < 0 || y >= ST7789_HEIGHT) {
        return mp_const_none;
    }
    st7789_pixel(x, y, color);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(display_pixel_obj, 4, 4,
                                           display_pixel);

// --- hline / vline --------------------------------------------------

static mp_obj_t display_hline(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    int x = mp_obj_get_int(args[1]);
    int y = mp_obj_get_int(args[2]);
    int w = mp_obj_get_int(args[3]);
    uint16_t color = (uint16_t)mp_obj_get_int(args[4]);
    if (y < 0 || y >= ST7789_HEIGHT || w <= 0) return mp_const_none;
    if (x < 0) { w += x; x = 0; }
    if (x + w > ST7789_WIDTH) w = ST7789_WIDTH - x;
    if (w <= 0) return mp_const_none;
    st7789_set_window(x, y, x + w - 1, y);
    st7789_fill_color(color, (uint32_t)w);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(display_hline_obj, 5, 5,
                                           display_hline);

static mp_obj_t display_vline(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    int x = mp_obj_get_int(args[1]);
    int y = mp_obj_get_int(args[2]);
    int h = mp_obj_get_int(args[3]);
    uint16_t color = (uint16_t)mp_obj_get_int(args[4]);
    if (x < 0 || x >= ST7789_WIDTH || h <= 0) return mp_const_none;
    if (y < 0) { h += y; y = 0; }
    if (y + h > ST7789_HEIGHT) h = ST7789_HEIGHT - y;
    if (h <= 0) return mp_const_none;
    st7789_set_window(x, y, x, y + h - 1);
    st7789_fill_color(color, (uint32_t)h);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(display_vline_obj, 5, 5,
                                           display_vline);

// --- line -----------------------------------------------------------

static mp_obj_t display_line(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    int x0 = mp_obj_get_int(args[1]);
    int y0 = mp_obj_get_int(args[2]);
    int x1 = mp_obj_get_int(args[3]);
    int y1 = mp_obj_get_int(args[4]);
    uint16_t color = (uint16_t)mp_obj_get_int(args[5]);
    draw_line(x0, y0, x1, y1, color);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(display_line_obj, 6, 6,
                                           display_line);

// --- rect (outline / filled) ---------------------------------------

static mp_obj_t display_rect(size_t n_args, const mp_obj_t *pos_args,
                             mp_map_t *kw_args) {
    enum { ARG_x, ARG_y, ARG_w, ARG_h, ARG_color, ARG_fill };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_x, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_y, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_w, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_h, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_color, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_fill, MP_ARG_BOOL, {.u_bool = false} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    int x = a[ARG_x].u_int, y = a[ARG_y].u_int;
    int w = a[ARG_w].u_int, h = a[ARG_h].u_int;
    uint16_t color = (uint16_t)a[ARG_color].u_int;
    if (w <= 0 || h <= 0) return mp_const_none;

    if (a[ARG_fill].u_bool) {
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > ST7789_WIDTH)  w = ST7789_WIDTH  - x;
        if (y + h > ST7789_HEIGHT) h = ST7789_HEIGHT - y;
        if (w > 0 && h > 0) {
            st7789_set_window(x, y, x + w - 1, y + h - 1);
            st7789_fill_color(color, (uint32_t)w * h);
        }
    } else {
        draw_rect_outline(x, y, w, h, color);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_rect_obj, 6, display_rect);

// --- ellipse --------------------------------------------------------

static mp_obj_t display_ellipse(size_t n_args, const mp_obj_t *pos_args,
                                mp_map_t *kw_args) {
    enum { ARG_cx, ARG_cy, ARG_xr, ARG_yr, ARG_color,
           ARG_fill, ARG_mask };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_cx, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_cy, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_xr, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_yr, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_color, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_fill, MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_mask, MP_ARG_INT, {.u_int = 0x0F} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    draw_ellipse(a[ARG_cx].u_int, a[ARG_cy].u_int,
                 a[ARG_xr].u_int, a[ARG_yr].u_int,
                 (uint16_t)a[ARG_color].u_int,
                 a[ARG_fill].u_bool,
                 (uint8_t)(a[ARG_mask].u_int & 0x0F));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_ellipse_obj, 6, display_ellipse);

// --- poly -----------------------------------------------------------

static mp_obj_t display_poly(size_t n_args, const mp_obj_t *pos_args,
                             mp_map_t *kw_args) {
    enum { ARG_ox, ARG_oy, ARG_coords, ARG_color, ARG_fill };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_ox, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_oy, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_coords, MP_ARG_OBJ | MP_ARG_REQUIRED,
          {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_color, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_fill, MP_ARG_BOOL, {.u_bool = false} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    mp_buffer_info_t bi;
    mp_get_buffer_raise(a[ARG_coords].u_obj, &bi, MP_BUFFER_READ);
    if (bi.len % (2 * sizeof(int16_t)) != 0) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("coords must be (x,y) int16 pairs"));
    }
    int n_points = bi.len / (2 * sizeof(int16_t));
    if (n_points < 2) return mp_const_none;

    int ox = a[ARG_ox].u_int, oy = a[ARG_oy].u_int;
    uint16_t color = (uint16_t)a[ARG_color].u_int;
    if (a[ARG_fill].u_bool) {
        draw_poly_fill(ox, oy, (const int16_t *)bi.buf, n_points, color);
    } else {
        draw_poly_outline(ox, oy, (const int16_t *)bi.buf, n_points, color);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_poly_obj, 5, display_poly);

// --- text (8x8) -----------------------------------------------------

static mp_obj_t display_text(size_t n_args, const mp_obj_t *pos_args,
                             mp_map_t *kw_args) {
    enum { ARG_s, ARG_x, ARG_y, ARG_color, ARG_bg };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_s, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_x, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_y, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_color, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_bg, MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    const char *s = mp_obj_str_get_str(a[ARG_s].u_obj);
    bool transparent = (a[ARG_bg].u_obj == mp_const_none);
    uint16_t bg = transparent ? 0 : (uint16_t)mp_obj_get_int(a[ARG_bg].u_obj);

    draw_text(s, a[ARG_x].u_int, a[ARG_y].u_int,
              (uint16_t)a[ARG_color].u_int, bg, transparent);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_text_obj, 5, display_text);

// --- text12 (12-px proportional) -----------------------------------

static mp_obj_t display_text12(size_t n_args, const mp_obj_t *pos_args,
                               mp_map_t *kw_args) {
    enum { ARG_s, ARG_x, ARG_y, ARG_color, ARG_bg };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_s, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_x, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_y, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_color, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_bg, MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    const char *s = mp_obj_str_get_str(a[ARG_s].u_obj);
    bool transparent = (a[ARG_bg].u_obj == mp_const_none);
    uint16_t bg = transparent ? 0 : (uint16_t)mp_obj_get_int(a[ARG_bg].u_obj);

    int end_x = draw_text12(s, a[ARG_x].u_int, a[ARG_y].u_int,
                            (uint16_t)a[ARG_color].u_int, bg, transparent);
    return mp_obj_new_int(end_x);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_text12_obj, 5, display_text12);

static mp_obj_t display_text12_width(mp_obj_t self_in, mp_obj_t s_in) {
    (void)self_in;
    const char *s = mp_obj_str_get_str(s_in);
    return mp_obj_new_int(text12_width(s));
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_text12_width_obj,
                                 display_text12_width);

// --- text12 into framebuffer (RGB565 u16 native LE) -----------------
//
// text12_fb(buf, w, h, s, x, y, color, bg=None)
//
// Renders into a writeable buffer instead of the panel. Used by
// pbc.text12() so it composes with other framebuffer drawing rather
// than being clobbered by the next show().

static mp_obj_t display_text12_fb(size_t n_args, const mp_obj_t *pos_args,
                                  mp_map_t *kw_args) {
    enum { ARG_buf, ARG_w, ARG_h, ARG_s, ARG_x, ARG_y, ARG_color, ARG_bg };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_buf,   MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_w,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_h,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_s,     MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_x,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_y,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_color, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_bg,    MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    mp_buffer_info_t bi;
    mp_get_buffer_raise(a[ARG_buf].u_obj, &bi, MP_BUFFER_RW);

    int w = a[ARG_w].u_int;
    int h = a[ARG_h].u_int;
    if (w <= 0 || h <= 0 ||
        (size_t)w * (size_t)h * 2u > bi.len) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer too small for w*h"));
    }

    const char *s = mp_obj_str_get_str(a[ARG_s].u_obj);
    bool has_bg = (a[ARG_bg].u_obj != mp_const_none);
    uint16_t bg = has_bg ? (uint16_t)mp_obj_get_int(a[ARG_bg].u_obj) : 0;

    int end_x = draw_text12_fb((uint16_t *)bi.buf, w, h,
                               s, a[ARG_x].u_int, a[ARG_y].u_int,
                               (uint16_t)a[ARG_color].u_int, has_bg, bg);
    return mp_obj_new_int(end_x);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_text12_fb_obj, 8, display_text12_fb);

// --- text18 ---------------------------------------------------------
//
// Same proportional font as text12, 1.5x larger (18-px cell). The
// drawing routines live in drawing.c (draw_text18 / draw_text18_fb /
// text18_width); these are just the MicroPython bindings, parallel to
// the text12 ones above.

static mp_obj_t display_text18(size_t n_args, const mp_obj_t *pos_args,
                               mp_map_t *kw_args) {
    enum { ARG_s, ARG_x, ARG_y, ARG_color, ARG_bg };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_s, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_x, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_y, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_color, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_bg, MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    const char *s = mp_obj_str_get_str(a[ARG_s].u_obj);
    bool transparent = (a[ARG_bg].u_obj == mp_const_none);
    uint16_t bg = transparent ? 0 : (uint16_t)mp_obj_get_int(a[ARG_bg].u_obj);

    int end_x = draw_text18(s, a[ARG_x].u_int, a[ARG_y].u_int,
                            (uint16_t)a[ARG_color].u_int, bg, transparent);
    return mp_obj_new_int(end_x);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_text18_obj, 5, display_text18);

static mp_obj_t display_text18_width(mp_obj_t self_in, mp_obj_t s_in) {
    (void)self_in;
    const char *s = mp_obj_str_get_str(s_in);
    return mp_obj_new_int(text18_width(s));
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_text18_width_obj,
                                 display_text18_width);

// --- text18 into framebuffer (RGB565 u16 native LE) -----------------
//
// text18_fb(buf, w, h, s, x, y, color, bg=None)

static mp_obj_t display_text18_fb(size_t n_args, const mp_obj_t *pos_args,
                                  mp_map_t *kw_args) {
    enum { ARG_buf, ARG_w, ARG_h, ARG_s, ARG_x, ARG_y, ARG_color, ARG_bg };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_buf,   MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_w,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_h,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_s,     MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_x,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_y,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_color, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_bg,    MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    mp_buffer_info_t bi;
    mp_get_buffer_raise(a[ARG_buf].u_obj, &bi, MP_BUFFER_RW);

    int w = a[ARG_w].u_int;
    int h = a[ARG_h].u_int;
    if (w <= 0 || h <= 0 ||
        (size_t)w * (size_t)h * 2u > bi.len) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer too small for w*h"));
    }

    const char *s = mp_obj_str_get_str(a[ARG_s].u_obj);
    bool has_bg = (a[ARG_bg].u_obj != mp_const_none);
    uint16_t bg = has_bg ? (uint16_t)mp_obj_get_int(a[ARG_bg].u_obj) : 0;

    int end_x = draw_text18_fb((uint16_t *)bi.buf, w, h,
                               s, a[ARG_x].u_int, a[ARG_y].u_int,
                               (uint16_t)a[ARG_color].u_int, has_bg, bg);
    return mp_obj_new_int(end_x);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_text18_fb_obj, 8, display_text18_fb);

// drawing routines live in drawing.c (draw_text24 / draw_text24_fb /
// text24_width); these are just the MicroPython bindings, parallel to
// the text12/text18 ones above.

static mp_obj_t display_text24(size_t n_args, const mp_obj_t *pos_args,
                               mp_map_t *kw_args) {
    enum { ARG_s, ARG_x, ARG_y, ARG_color, ARG_bg };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_s, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_x, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_y, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_color, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_bg, MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    const char *s = mp_obj_str_get_str(a[ARG_s].u_obj);
    bool transparent = (a[ARG_bg].u_obj == mp_const_none);
    uint16_t bg = transparent ? 0 : (uint16_t)mp_obj_get_int(a[ARG_bg].u_obj);

    int end_x = draw_text24(s, a[ARG_x].u_int, a[ARG_y].u_int,
                            (uint16_t)a[ARG_color].u_int, bg, transparent);
    return mp_obj_new_int(end_x);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_text24_obj, 5, display_text24);

static mp_obj_t display_text24_width(mp_obj_t self_in, mp_obj_t s_in) {
    (void)self_in;
    const char *s = mp_obj_str_get_str(s_in);
    return mp_obj_new_int(text24_width(s));
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_text24_width_obj,
                                 display_text24_width);

// --- text24 into framebuffer (RGB565 u16 native LE) -----------------
//
// text24_fb(buf, w, h, s, x, y, color, bg=None)

static mp_obj_t display_text24_fb(size_t n_args, const mp_obj_t *pos_args,
                                  mp_map_t *kw_args) {
    enum { ARG_buf, ARG_w, ARG_h, ARG_s, ARG_x, ARG_y, ARG_color, ARG_bg };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_buf,   MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_w,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_h,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_s,     MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_x,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_y,     MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_color, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_bg,    MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    mp_buffer_info_t bi;
    mp_get_buffer_raise(a[ARG_buf].u_obj, &bi, MP_BUFFER_RW);

    int w = a[ARG_w].u_int;
    int h = a[ARG_h].u_int;
    if (w <= 0 || h <= 0 ||
        (size_t)w * (size_t)h * 2u > bi.len) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer too small for w*h"));
    }

    const char *s = mp_obj_str_get_str(a[ARG_s].u_obj);
    bool has_bg = (a[ARG_bg].u_obj != mp_const_none);
    uint16_t bg = has_bg ? (uint16_t)mp_obj_get_int(a[ARG_bg].u_obj) : 0;

    int end_x = draw_text24_fb((uint16_t *)bi.buf, w, h,
                               s, a[ARG_x].u_int, a[ARG_y].u_int,
                               (uint16_t)a[ARG_color].u_int, has_bg, bg);
    return mp_obj_new_int(end_x);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_text24_fb_obj, 8, display_text24_fb);


// --- blit -----------------------------------------------------------
//
// Two modes:
//   - opaque: stream the whole region as a single rectangular blit.
//     Default, fastest, matches the historical behaviour.
//   - keyed: skip pixels matching `key` (used as a transparency
//     sentinel). Walks each scanline looking for runs of
//     non-transparent pixels; each run becomes its own
//     set_window+blit_pixels burst. For typical sprite-like images
//     with large transparent borders + a few opaque runs per line,
//     this is fast enough -- a 32×32 sprite is ~32 runs.

static mp_obj_t display_blit(size_t n_args, const mp_obj_t *pos_args,
                             mp_map_t *kw_args) {
    enum { ARG_buf, ARG_x, ARG_y, ARG_w, ARG_h, ARG_key };
    static const mp_arg_t allowed[] = {
        { MP_QSTR_buf, MP_ARG_OBJ | MP_ARG_REQUIRED,
          {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_x, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_y, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_w, MP_ARG_INT, {.u_int = ST7789_WIDTH} },
        { MP_QSTR_h, MP_ARG_INT, {.u_int = ST7789_HEIGHT} },
        // Use MP_OBJ_NULL as the "not passed" marker so we can tell
        // an explicit key=None apart from no-arg-given. None is then
        // treated the same as not-given (no transparency).
        { MP_QSTR_key, MP_ARG_OBJ | MP_ARG_KW_ONLY,
          {.u_obj = MP_OBJ_NULL} },
    };
    mp_arg_val_t a[MP_ARRAY_SIZE(allowed)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
                     MP_ARRAY_SIZE(allowed), allowed, a);

    int x = a[ARG_x].u_int, y = a[ARG_y].u_int;
    int w = a[ARG_w].u_int, h = a[ARG_h].u_int;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
        x + w > ST7789_WIDTH || y + h > ST7789_HEIGHT) {
        mp_raise_ValueError(MP_ERROR_TEXT("blit region out of bounds"));
    }

    mp_buffer_info_t bi;
    mp_get_buffer_raise(a[ARG_buf].u_obj, &bi, MP_BUFFER_READ);
    size_t needed = (size_t)w * h * 2;
    if (bi.len < needed) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer too small"));
    }

    // Decide between opaque and keyed paths. key=None / key not
    // passed → opaque. Anything else gets cast to uint16_t.
    bool has_key = (a[ARG_key].u_obj != MP_OBJ_NULL &&
                    a[ARG_key].u_obj != mp_const_none);
    if (!has_key) {
        st7789_set_window(x, y, x + w - 1, y + h - 1);
        st7789_blit_pixels((const uint16_t *)bi.buf, (uint32_t)w * h);
        return mp_const_none;
    }

    uint16_t key = (uint16_t)mp_obj_get_int(a[ARG_key].u_obj);
    const uint16_t *pixels = (const uint16_t *)bi.buf;
    // Walk each scanline. For every run of consecutive non-key
    // pixels, set the window to that run's rectangle (1 px tall) and
    // blit the slice. Transparent runs are simply skipped -- the
    // pixels currently on the display stay untouched, which is
    // exactly what "transparent" means for direct-to-screen blits.
    for (int row = 0; row < h; row++) {
        const uint16_t *line = pixels + (size_t)row * w;
        int col = 0;
        while (col < w) {
            // Skip key pixels.
            while (col < w && line[col] == key) col++;
            if (col >= w) break;
            // Start of an opaque run.
            int run_start = col;
            while (col < w && line[col] != key) col++;
            int run_len = col - run_start;
            st7789_set_window(x + run_start, y + row,
                              x + run_start + run_len - 1, y + row);
            st7789_blit_pixels(line + run_start, (uint32_t)run_len);
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(display_blit_obj, 2, display_blit);

// --- set_backlight --------------------------------------------------

static mp_obj_t display_set_backlight(mp_obj_t self_in, mp_obj_t level_in) {
    (void)self_in;
    int level = mp_obj_get_int(level_in);
    if (level < 0) level = 0;
    if (level > 255) level = 255;
    st7789_set_backlight((uint8_t)level);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_set_backlight_obj,
                                 display_set_backlight);

// --- type table -----------------------------------------------------

static const mp_rom_map_elem_t display_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_fill),         MP_ROM_PTR(&display_fill_obj) },
    { MP_ROM_QSTR(MP_QSTR_fill_rect),    MP_ROM_PTR(&display_fill_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_pixel),        MP_ROM_PTR(&display_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_hline),        MP_ROM_PTR(&display_hline_obj) },
    { MP_ROM_QSTR(MP_QSTR_vline),        MP_ROM_PTR(&display_vline_obj) },
    { MP_ROM_QSTR(MP_QSTR_line),         MP_ROM_PTR(&display_line_obj) },
    { MP_ROM_QSTR(MP_QSTR_rect),         MP_ROM_PTR(&display_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_ellipse),      MP_ROM_PTR(&display_ellipse_obj) },
    { MP_ROM_QSTR(MP_QSTR_poly),         MP_ROM_PTR(&display_poly_obj) },
    { MP_ROM_QSTR(MP_QSTR_text),         MP_ROM_PTR(&display_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_text12),       MP_ROM_PTR(&display_text12_obj) },
    { MP_ROM_QSTR(MP_QSTR_text12_width),
      MP_ROM_PTR(&display_text12_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_text12_fb),    MP_ROM_PTR(&display_text12_fb_obj) },
    { MP_ROM_QSTR(MP_QSTR_text18),       MP_ROM_PTR(&display_text18_obj) },
    { MP_ROM_QSTR(MP_QSTR_text18_width),
      MP_ROM_PTR(&display_text18_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_text18_fb),    MP_ROM_PTR(&display_text18_fb_obj) },
    { MP_ROM_QSTR(MP_QSTR_text24),       MP_ROM_PTR(&display_text24_obj) },
    { MP_ROM_QSTR(MP_QSTR_text24_width),
      MP_ROM_PTR(&display_text24_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_text24_fb),    MP_ROM_PTR(&display_text24_fb_obj) },
    { MP_ROM_QSTR(MP_QSTR_blit),         MP_ROM_PTR(&display_blit_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_backlight),
      MP_ROM_PTR(&display_set_backlight_obj) },
};
static MP_DEFINE_CONST_DICT(display_locals_dict, display_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    display_type,
    MP_QSTR_Display,
    MP_TYPE_FLAG_NONE,
    make_new, display_make_new,
    locals_dict, &display_locals_dict
    );

// --------------------------------------------------------------------
// Module-level helpers
// --------------------------------------------------------------------

static mp_obj_t display_rgb(mp_obj_t r_in, mp_obj_t g_in, mp_obj_t b_in) {
    int r = mp_obj_get_int(r_in);
    int g = mp_obj_get_int(g_in);
    int b = mp_obj_get_int(b_in);
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return mp_obj_new_int(MAKE_RGB565(r, g, b));
}
static MP_DEFINE_CONST_FUN_OBJ_3(display_rgb_obj, display_rgb);

// --------------------------------------------------------------------
// Module table.
// --------------------------------------------------------------------

static const mp_rom_map_elem_t display_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_display) },
    { MP_ROM_QSTR(MP_QSTR_Display),  MP_ROM_PTR(&display_type) },
    { MP_ROM_QSTR(MP_QSTR_rgb),      MP_ROM_PTR(&display_rgb_obj) },

    // Native PNG-decode hot path used by png.py. Private (single
    // underscore) so the Python module is the only public surface --
    // this is an implementation detail.
    { MP_ROM_QSTR(MP_QSTR__png_decode_scanlines),
      MP_ROM_PTR(&display_png_decode_scanlines_obj) },
    { MP_ROM_QSTR(MP_QSTR__IdatStream),
      MP_ROM_PTR(&display_idat_stream_type) },

    { MP_ROM_QSTR(MP_QSTR_WIDTH),    MP_ROM_INT(ST7789_WIDTH) },
    { MP_ROM_QSTR(MP_QSTR_HEIGHT),   MP_ROM_INT(ST7789_HEIGHT) },

    { MP_ROM_QSTR(MP_QSTR_BLACK),    MP_ROM_INT(COLOR_BLACK) },
    { MP_ROM_QSTR(MP_QSTR_WHITE),    MP_ROM_INT(COLOR_WHITE) },
    { MP_ROM_QSTR(MP_QSTR_RED),      MP_ROM_INT(COLOR_RED) },
    { MP_ROM_QSTR(MP_QSTR_GREEN),    MP_ROM_INT(COLOR_GREEN) },
    { MP_ROM_QSTR(MP_QSTR_BLUE),     MP_ROM_INT(COLOR_BLUE) },
    { MP_ROM_QSTR(MP_QSTR_YELLOW),   MP_ROM_INT(COLOR_YELLOW) },
    { MP_ROM_QSTR(MP_QSTR_CYAN),     MP_ROM_INT(COLOR_CYAN) },
    { MP_ROM_QSTR(MP_QSTR_MAGENTA),  MP_ROM_INT(COLOR_MAGENTA) },
    { MP_ROM_QSTR(MP_QSTR_GRAY),     MP_ROM_INT(COLOR_GRAY) },
    { MP_ROM_QSTR(MP_QSTR_ORANGE),   MP_ROM_INT(COLOR_ORANGE) },
    { MP_ROM_QSTR(MP_QSTR_PURPLE),   MP_ROM_INT(COLOR_PURPLE) },
};
static MP_DEFINE_CONST_DICT(display_module_globals,
                            display_module_globals_table);

const mp_obj_module_t display_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&display_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_display, display_user_cmodule);
