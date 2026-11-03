/* ay8910.h -- AY-3-8910 / YM2149 sound chip (MSX PSG)
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). Three square
 * wave channels, a noise generator and the envelope generator, after
 * General Instrument's data sheet. Output: 0 .. 3 * AY_MAX (unipolar, as
 * the chip's DACs; the caller removes the DC).
 */
#ifndef AY8910_H
#define AY8910_H

#include <stdint.h>

#define AY_MAX 7000

typedef struct {
    uint8_t  reg[16];
    uint8_t  addr;
    uint16_t tone_cnt[3];
    uint8_t  tone_out[3];
    uint8_t  noise_cnt;
    uint8_t  noise_out;
    uint32_t lfsr;
    uint32_t env_cnt;
    uint8_t  env_step;      /* 0..15 */
    uint8_t  env_up;        /* counting up */
    uint8_t  env_hold;      /* stopped */
    uint8_t  env_vol;       /* 0..15 */
    uint32_t frac;          /* sample timing, 16.16 */
    uint32_t step;          /* chip ticks (clock / 8) per sample, 16.16 */
} ay8910_t;

/* Port A (register 14) as the chip reads it; set by the machine */
typedef uint8_t (*ay_port_read_t)(void);

void ay_init(ay8910_t *ay, uint32_t clock, uint32_t rate);
void ay_reset(ay8910_t *ay);
void ay_write_addr(ay8910_t *ay, uint8_t v);
void ay_write_data(ay8910_t *ay, uint8_t v);
uint8_t ay_read_data(ay8910_t *ay, ay_port_read_t port_a);

/* n samples added to out[] (0 .. 3 * AY_MAX each) */
void ay_render(ay8910_t *ay, int32_t *out, int n);

#endif
