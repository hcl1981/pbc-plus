/* tms9918.h -- TMS9918A video chip (MSX1, ColecoVision, SG-1000)
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). 16 KB of VRAM,
 * graphics I and II, multicolour and text mode, 32 sprites (4 per line),
 * status flags and the frame interrupt. tms_line() draws one of the 192
 * active lines into palette indices 0..15 (0 already replaced by the
 * backdrop colour); the caller maps them to RGB with tms_palette.
 */
#ifndef TMS9918_H
#define TMS9918_H

#include <stdint.h>
#include <stdbool.h>

#define TMS_LINES   192
#define TMS_WIDTH   256

typedef struct {
    uint8_t  vram[0x4000];
    uint8_t  reg[8];
    uint8_t  status;
    uint8_t  latch;         /* first byte of a control write */
    uint8_t  latched;       /* waiting for the second byte */
    uint8_t  readahead;
    uint16_t addr;
} tms9918_t;

extern tms9918_t tms;

/* RGB of the 16 colours (0x00RRGGBB) */
extern const uint32_t tms_palette[16];

void tms_reset(void);
uint8_t tms_read_data(void);
uint8_t tms_read_status(void);          /* clears the flags and the interrupt */
void tms_write_data(uint8_t v);
void tms_write_ctrl(uint8_t v);

/* Draws active line y (0..191) into out[256]; sprites included. */
void tms_line(int y, uint8_t *out);     /* out word-aligned (TMS_WIDTH bytes) */

/* End of the active picture (after line 191): sets the frame flag. */
void tms_vblank(void);

/* The interrupt line: frame flag set and enabled in register 1. */
static inline bool tms_irq(void)
{
    return (tms.status & 0x80) && (tms.reg[1] & 0x20);
}

/* Backdrop colour (border), 0..15 */
static inline int tms_backdrop(void)
{
    return tms.reg[7] & 15;
}

#endif
