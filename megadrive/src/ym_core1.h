/* ym_core1.h -- Mega Drive sound synthesised on core 1.
 *
 * Core 0 emulates the CPUs. Every write to the YM2612 or the PSG goes into
 * a ring with its time in the frame (master clock), and a marker closes
 * each frame. Core 1 replays the ring into its own YM2612 (ym2612_synth.c)
 * and the PSG, in small steps between drawing lines, and sends each
 * finished frame to the speaker. Core 0's YM2612 copy only keeps the
 * timers and the status the game reads back. */
#ifndef PBC_YM_CORE1_H
#define PBC_YM_CORE1_H

#include <stdint.h>
#include <stdbool.h>

/* Core 0. */
void ym_core1_log_ym(unsigned int port, unsigned int value, int clock);
void ym_core1_log_psg(int value, int clock);
void ym_core1_frame_end(int clock, bool pal);

/* Before core 1 starts. */
void ym_core1_init(void);

/* Core 1: one step (a register write or a few samples). */
void ym_core1_service(void);

/* Core 1 time spent synthesising, in us (statistics). */
extern volatile uint32_t ym_core1_busy_us;

#endif
