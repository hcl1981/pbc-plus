/* ymfuzz.c: core 1's YM2612 (ym2612_synth.c, included) under random register writes; hash of the samples */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
bool audio_enabled = true;
#include "ym2612_synth.c"
unsigned char *ROM_DATA;
unsigned int m68k_read_memory_8(unsigned int a) { return 0; }
unsigned int m68k_read_memory_16(unsigned int a) { return 0; }
static int16_t buf[GWENESIS_AUDIO_BUFFER_MAX];
int16_t *synth_ym2612_buffer = buf;
volatile int synth_ym2612_index, synth_ym2612_clock;
static uint32_t rs = 12345;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static inline uint64_t mix64(uint64_t h, uint64_t v) { h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); h *= 0xff51afd7ed558ccdull; return h ^ (h >> 31); }
static void wr(int port, int reg, int val, int clk)
{
    synth_YM2612Write(port * 2, reg, clk);
    synth_YM2612Write(port * 2 + 1, val, clk);
}
int main(int argc, char **argv)
{
    long frames = argc > 1 ? atol(argv[1]) : 2000;
    rs = argc > 2 ? strtoul(argv[2], 0, 0) : 12345;
    synth_YM2612Init();
    synth_YM2612ResetChip();
    synth_YM2612Config(YM2612_DISCRETE);
    uint64_t h = 0;
    for (long f = 0; f < frames; f++) {
        int clk = 0;
        const int frame_end = 888 * AUDIO_FREQ_DIVISOR;
        while (1) {
            uint32_t r = rnd();
            clk += (r & 7) == 0 ? (int)(rnd() % (60 * AUDIO_FREQ_DIVISOR)) : (int)(rnd() % (4 * AUDIO_FREQ_DIVISOR));
            if (clk >= frame_end) break;
            int port = (r >> 3) & 1;
            r = rnd();
            switch (r % 16) {
            case 0: wr(0, 0x22, rnd() & 0x0F, clk); break;                          /* LFO */
            case 1: { if (rnd() % 2) { wr(0, 0x24, 0xFF, clk); wr(0, 0x25, 3 - rnd() % 3, clk); }
                      int v = rnd() & 0xFF; if (rnd() % 4) v &= 0x3F | (rnd() % 8 == 0 ? 0xC0 : 0x40); wr(0, 0x27, v, clk); break; }
            case 2: wr(0, 0x24 + (rnd() % 3), rnd() & 0xFF, clk); break;            /* timers */
            case 3: case 4: { int c = rnd() % 8; int ops = rnd() % 3 ? 0xF0 : (rnd() & 0xF0); wr(0, 0x28, ops | c, clk); break; }
            case 5: wr(0, 0x2A, rnd() & 0xFF, clk); if (rnd() % 64 == 0) wr(0, 0x2B, rnd() & 0x80, clk); break;
            case 6: case 7: case 8: case 9: {                                         /* operator */
                int base = 0x30 + 0x10 * (rnd() % 7);
                int reg = base + (rnd() % 16);
                int v = rnd() & 0xFF;
                if (base == 0x40 && rnd() % 2) v &= 0x1F;                         /* loud */
                if (base == 0x90 && rnd() % 2) v |= 0x08;                         /* SSG-EG on */
                if (base == 0x50 && rnd() % 2) v |= 0x1C;
                wr(port, reg, v, clk);
                break;
            }
            case 10: case 11: { int c = rnd() % 3; wr(port, 0xA4 + c, rnd() & 0x3F, clk); wr(port, 0xA0 + c, rnd() & 0xFF, clk); break; }
            case 12: { int c = rnd() % 3; wr(0, 0xAC + c, rnd() & 0x3F, clk); wr(0, 0xA8 + c, rnd() & 0xFF, clk); break; }
            case 13: wr(port, 0xB0 + rnd() % 3, rnd() & 0x3F, clk); break;
            case 14: wr(port, 0xB4 + rnd() % 3, (rnd() & 0x37) | (rnd() % 4 ? 0xC0 : (rnd() & 0xC0)), clk); break;
            default: wr(port, rnd() & 0xFF, rnd() & 0xFF, clk); break;            /* anything */
            }
        }
        synth_ym2612_run(frame_end);
        for (int i = 0; i < synth_ym2612_index; i++) h = mix64(h, (uint16_t)buf[i]);
        if (f % 100 == 99) printf("%ld %016llx\n", f, (unsigned long long)h);
        synth_ym2612_index = synth_ym2612_clock = 0;
    }
    return 0;
}
