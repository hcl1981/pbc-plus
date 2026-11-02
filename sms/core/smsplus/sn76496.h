/* sn76496.h -- SN76489 PSG (Master System, Game Gear)
 *
 * Written for the PicoBoy Color Plus port (GPL-2.0-or-later like SMS Plus);
 * it replaces the SN76496 code from the old MAME, whose licence did not
 * allow commercial use, and keeps its interface and output level. See
 * sn76496.c.
 */
#ifndef SN76496_H
#define SN76496_H

#define MAX_76496 4

typedef struct {
    int SampleRate;
    unsigned int UpdateStep;    /* one PSG tick (clock / 16) in 1/65536 sample */
    int VolTable[16];           /* attenuation -> amplitude */
    int Register[8];            /* tone 0, vol 0, tone 1, vol 1, tone 2, vol 2, noise, vol 3 */
    int LastRegister;           /* register the next data byte goes to */
    unsigned int LFSR;          /* noise shift register */
    int Count[4];               /* time left in the half period, 1/65536 sample */
    int Output[4];              /* flip-flops: 0/1 */
} t_SN76496;

extern t_SN76496 sn[MAX_76496];

void SN76496Write(int chip, int data);

/* `length` samples into buffer[0] (left) and buffer[1] (right); `mask`
 * bit 4 + n puts channel n on the left, bit n on the right (Game Gear
 * stereo, 0xFF on the Master System). */
void SN76496Update(int chip, signed short int *buffer[2], int length, unsigned char mask);

void SN76496_set_clock(int chip, int clock);

void SN76496_set_gain(int chip, int gain);

int SN76496_init(int chip, int clock, int volume, int sample_rate);

#endif
