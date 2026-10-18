/* HDMI.h -- stand-in for frank-genesis' HDMI driver header.
 *
 * The VDP core (gwenesis_vdp_mem.c) includes "HDMI.h" and pushes every CRAM
 * change through graphics_set_palette(). On the PicoBoy Color Plus there is
 * no HDMI: the palette lands in a 256-entry table of panel words that the
 * ST7789 scaler (video.c) reads. Keeping the original names means the core
 * stays unmodified.
 */
#ifndef PBC_HDMI_SHIM_H
#define PBC_HDMI_SHIM_H

#include <stdint.h>
#include <stdbool.h>

/* Longest source frame (PAL) a per-line palette table must cover. */
#define HDMI_MAX_LINES 240

/* Per-line palette mapping: maps a raw framebuffer byte to a palette slot.
 * Mid-frame CRAM writes are redirected to spare slots (128..239) and the
 * lines below the split point at a table that knows about them. */
extern const uint8_t *hdmi_line_lut[HDMI_MAX_LINES];
extern uint8_t hdmi_identity_lut[256];

/* Panel words (BGR565, see color565.h), indexed by palette slot. */
extern uint16_t pbc_palette[256];

void hdmi_line_lut_reset(void);
void graphics_set_palette(uint8_t i, uint32_t color888);

/* No CRT scanline effect on this device. */
static inline bool graphics_get_crt_enabled(void) { return false; }

#endif /* PBC_HDMI_SHIM_H */
