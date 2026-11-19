/* pbc.h -- the PicoBoy Color Plus under the game. */
#ifndef PBC_H
#define PBC_H
#include <stdint.h>
#include <stdbool.h>

void pbc_display_init(void);
void pbc_display_backlight(uint8_t level);              /* 0..255 */
/* Sends a whole frame by DMA and returns at once; the buffer must stay as
 * it is until the next call, which waits for this one to finish. */
void pbc_display_frame(const uint16_t *px);
void pbc_display_wait(void);
/* without DMA, for the screen of last resort */
void pbc_display_fill(int x, int y, int w, int h, uint16_t colour);
void pbc_display_text(int x, int y, const char *s, uint16_t fg, uint16_t bg);

void     pbc_input_init(void);
uint32_t pbc_input(void);                               /* BTN_* of rt.h */

bool pbc_audio_init(void);
void pbc_audio_fill(void);                              /* once per frame */

#endif
