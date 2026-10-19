/* color565.h -- the ONE place that decides the bit order sent to the panel.
 *
 * MADCTL 0xC8 sets the BGR bit: the ST7789 then reads blue from the upper
 * five bits and red from the lower five -- BGR565, not RGB565. Getting this
 * wrong does not look obviously wrong (grey, black and white stay correct,
 * only yellow turns cyan and red turns blue), so never use raw 0xF800-style
 * constants anywhere else.
 */
#ifndef PBC_COLOR565_H
#define PBC_COLOR565_H

#include <stdint.h>

#define PBC_RGB(r, g, b) ((uint16_t)((((unsigned)(b) & 0xf8u) << 8) | \
                                     (((unsigned)(g) & 0xfcu) << 3) | \
                                     (((unsigned)(r) & 0xf8u) >> 3)))

/* 0xRRGGBB -> panel word */
static inline uint16_t pbc_rgb24(uint32_t rgb)
{
    return PBC_RGB((rgb >> 16) & 0xffu, (rgb >> 8) & 0xffu, rgb & 0xffu);
}

/* 50/50 mix of two panel words. Channel order does not matter, only the
 * 5/6/5 field widths: clear each field's low bit before halving so no bit
 * carries into the neighbouring field. */
static inline uint16_t pbc_avg565(uint16_t a, uint16_t b)
{
    return (uint16_t)((a & b) + (((a ^ b) & 0xF7DEu) >> 1));
}

#define PBC_BLACK   PBC_RGB(0, 0, 0)
#define PBC_WHITE   PBC_RGB(255, 255, 255)
#define PBC_GREY    PBC_RGB(128, 128, 128)
#define PBC_DKGREY  PBC_RGB(48, 48, 56)
#define PBC_RED     PBC_RGB(255, 64, 64)
#define PBC_YELLOW  PBC_RGB(255, 220, 0)
#define PBC_BLUE    PBC_RGB(40, 80, 200)

#endif /* PBC_COLOR565_H */
