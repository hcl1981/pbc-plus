/* ym2612.h -- YM2612 (OPN2) FM sound chip, see ym2612.c
 *
 * Written for the PicoBoy Color Plus port (GPL-3.0-or-later); same
 * interface as the YM2612 it replaces. */
#ifndef _H_YM2612_
#define _H_YM2612_

#include <stdint.h>
#include <stdbool.h>

/* chip types (YM2612Config) */
enum {
  YM2612_DISCRETE = 0,   /* discrete YM2612: 9-bit DAC with ladder effect */
  YM2612_INTEGRATED,     /* YM3438 in the ASIC: no ladder effect */
  YM2612_ENHANCED        /* the same here */
};

/* samples (one per AUDIO_FREQ_DIVISOR master clocks) and their position */
extern int16_t *gwenesis_ym2612_buffer;
extern volatile int ym2612_index;
extern volatile int ym2612_clock;

void YM2612Init(void);
void YM2612Config(unsigned char type);
void YM2612ResetChip(void);
void YM2612Write(unsigned int a, unsigned int v, int target);  /* a: port 0-3 */
void ym2612_run(int target);                                   /* up to master clock target */
unsigned int YM2612Read(int target);                           /* status */

void gwenesis_ym2612_save_state();
void gwenesis_ym2612_load_state();

#endif
