/* sound.h -- the launcher's few sounds on the speaker (GP15, PWM).
 *
 * A short click when Sound or Volume changes and a bell when the start
 * animation's glint runs, at the volume of the settings, only with Sound
 * on. Each sound is computed into a buffer and a DMA channel paced by a
 * DMA timer plays it once. Between sounds the output rests at 0 (held in
 * the middle, supply noise whistled); each sound glides up to the middle
 * and back down in 10 ms, so the speaker does not pop. */
#ifndef PBC_SOUND_H
#define PBC_SOUND_H

#include <stdbool.h>

void sound_on(void);                /* after the clock is set; with Sound on */
void sound_off(void);               /* before a game, the USB drive, the dark */
void sound_update(void);            /* Sound or Volume changed */

void sound_click(void);
void sound_bell(void);

#endif
