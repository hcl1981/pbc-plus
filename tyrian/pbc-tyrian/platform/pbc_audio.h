/*
 * pbc_audio -- Tonausgabe ueber den Piezo an GP15 (PWM + DMA).
 * Siehe pbc_audio.c fuer den Signalweg und die Sache mit der
 * Unterbrechungsprioritaet.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_AUDIO_H
#define PBC_AUDIO_H

void pbc_audio_init(void);
void pbc_audio_stop(void);

#endif /* PBC_AUDIO_H */
