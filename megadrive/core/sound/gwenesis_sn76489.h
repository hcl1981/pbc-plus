/* gwenesis_sn76489.h -- SN76489 PSG (Mega Drive / Master System)
 *
 * Written for the PicoBoy Color Plus port, GPL-3.0-or-later like the rest
 * of the sound code; it replaces the Maxim / Genesis Plus GX based PSG and
 * keeps its interface and output level. See gwenesis_sn76489.c.
 */
#ifndef _GWENESIS_SN76489_H_
#define _GWENESIS_SN76489_H_

/* chip variants */
enum {
    PSG_DISCRETE = 0,   /* TI SN76489A: 15-bit noise register */
    PSG_INTEGRATED      /* Sega's clone in the VDP: 16-bit, different taps */
};

#undef uint8
#undef uint16
#undef uint32
#undef uint64

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned int uint32;

typedef signed char int8;
typedef signed short int int16;
typedef signed int int32;

typedef unsigned char UINT8;
typedef unsigned short int UINT16;
typedef unsigned int UINT32;

typedef signed char INT8;
typedef signed short int INT16;
typedef signed int INT32;
typedef long signed int INT64;

typedef struct
{
    int step;                   /* PSG ticks (clock / 16) per sample, 16.16 */
    int divisor;                /* master clocks per sample */

    int noise_width;            /* top bit of the noise register */
    int noise_taps;             /* bits fed back for white noise */
    int zero_period;            /* what a period of 0 means */

    UINT16 regs[8];             /* tone 0, vol 0, tone 1, vol 1, tone 2, vol 2, noise, vol 3 */
    int latch;                  /* register the next data byte goes to */
    UINT16 lfsr;                /* noise shift register */

    int count[4];               /* ticks left in the half period, 16.16 */
    int level[4];               /* tone flip-flops +1/-1; noise flip-flop */
} SN76489_Context;

extern int16 *gwenesis_sn76489_buffer;
extern volatile int sn76489_index;
extern volatile int sn76489_clock;

void gwenesis_SN76489_Init(int PSGClockValue, int SamplingRate, int freq_divisor, int type);
void gwenesis_SN76489_Reset();
void gwenesis_SN76489_SetContext(uint8 *data);
void gwenesis_SN76489_GetContext(uint8 *data);
uint8 *gwenesis_SN76489_GetContextPtr();
int gwenesis_SN76489_GetContextSize(void);
void gwenesis_SN76489_Write(int data, int target);
void gwenesis_SN76489_Apply(int data, int target);   /* PBC: core 1 */
void gwenesis_SN76489_run(int target);

void gwenesis_sn76489_save_state();
void gwenesis_sn76489_load_state();

#endif /* _GWENESIS_SN76489_H_ */
