/* z80.h -- the Z80 as SMS Plus calls it (the interface of the MAME core it
 * used to have), implemented on redcode/Z80 in z80_redcode.c. */
#ifndef Z80_H
#define Z80_H

#include "osd_cpu.h"

#define CLEAR_LINE  0
#define ASSERT_LINE 1

enum { Z80_PC = 1, Z80_SP };                /* for z80_set_reg() */

extern int z80_ICount;                      /* T-states left in this slice */

/* 8 KB pages the Z80 reads and writes; writes go through cpu_writemem16() */
extern unsigned char *cpu_readmap[8];
extern unsigned char *cpu_writemap[8];

void z80_reset(void *param);
int  z80_execute(int cycles);               /* returns the T-states run */
void z80_set_reg(int regnum, unsigned val);
unsigned z80_get_pc(void);
void z80_set_irq_line(int irqline, int state);
void z80_set_nmi_line(int state);
void z80_set_irq_callback(int (*callback)(int));

/* the whole CPU state, for save states */
void    *z80_context(void);
unsigned z80_context_size(void);

#endif
