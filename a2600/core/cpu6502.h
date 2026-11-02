/* cpu6502.h -- MOS 6502 / 6507 for the Atari 2600 core
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). All documented
 * opcodes with their cycle counts (page crossing included) and the stable
 * undocumented ones that 2600 programs use (LAX, SAX, DCP, ISB, SLO, RLA,
 * SRE, RRA, ANC, ALR, ARR, SBX, the NOPs). Decimal mode as on the NMOS
 * 6502. The 6507 has no interrupt lines; BRK works.
 *
 * Memory goes through cpu_read() and cpu_write(), supplied by the machine
 * (a2600.c), which also counts the cycles of each access.
 */
#ifndef CPU6502_H
#define CPU6502_H

#include <stdint.h>

typedef struct {
    uint16_t pc;
    uint8_t  a, x, y, s, p;
    uint8_t  jammed;        /* a KIL opcode stopped the CPU */
} cpu6502_t;

extern cpu6502_t cpu;

/* supplied by the machine: one bus cycle each */
uint8_t cpu_read(uint16_t addr);
void cpu_write(uint16_t addr, uint8_t v);

void cpu_reset(void);
/* executes one instruction; the machine counts the cycles in its bus
 * functions (each cpu_read/cpu_write is one cycle, dummy accesses
 * included) */
void cpu_step(void);

#endif
