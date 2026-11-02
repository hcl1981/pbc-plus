/* scc.h -- Konami SCC (sound chip in some Konami MegaROMs)
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). Five channels
 * playing 32-sample waveforms (channels 4 and 5 share one), 12-bit
 * frequency, 4-bit volume. Registers as seen at 9800h-98FFh of the
 * cartridge (mirrored up to 9FFFh).
 */
#ifndef SCC_H
#define SCC_H

#include <stdint.h>

typedef struct {
    int8_t   wave[4][32];
    uint16_t period[5];
    uint8_t  volume[5];
    uint8_t  enable;
    uint32_t phase[5];      /* 5.27: wave index in the top 5 bits */
    uint32_t inc[5];
    uint32_t rate_k;        /* clock * 2^27 / rate / 32, scaled for the division */
    uint8_t  used;          /* written at least once: mix it */
} scc_t;

void scc_init(scc_t *s, uint32_t clock, uint32_t rate);
void scc_write(scc_t *s, uint16_t addr, uint8_t v);
uint8_t scc_read(scc_t *s, uint16_t addr);

/* n samples added to out[] (about -1920*5 .. 1905*5) */
void scc_render(scc_t *s, int32_t *out, int n);

#endif
