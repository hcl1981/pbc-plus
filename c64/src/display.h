/* display.h -- ST7789 240x280 on SPI0, DMA pixel stream plus blocking text. */
#ifndef PBC_DISPLAY_H
#define PBC_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

void pbc_display_init(void);
void pbc_display_backlight(uint8_t level);   /* 0..255 */

/* Streaming: window -> begin -> send/wait ... -> end.
 * CS stays asserted from RAMWR until end(), so the controller sees one
 * continuous write. send() starts a DMA and returns immediately; the buffer
 * must stay untouched until wait() or busy() says it is done. */
void pbc_display_window(int x, int y, int w, int h);
void pbc_display_begin(void);
void pbc_display_send(const uint16_t *px, int n);
bool pbc_display_busy(void);
void pbc_display_wait(void);
void pbc_display_end(void);

/* Blocking helpers for menus and messages. Text uses the 8x8 font,
 * scale 1 or 2. */
void pbc_display_fill(int x, int y, int w, int h, uint16_t colour);
void pbc_display_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
int  pbc_display_text_w(const char *s, int scale);
void pbc_display_text_centered(int y, const char *s, uint16_t fg, uint16_t bg, int scale);

#endif /* PBC_DISPLAY_H */
