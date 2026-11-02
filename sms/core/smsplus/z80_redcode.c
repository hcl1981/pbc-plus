/* z80_redcode.c -- the Z80 interface SMS Plus expects (z80.h, from MAME),
 * on top of redcode/Z80 (core/redcode, LGPL-3.0-or-later).
 *
 * The MAME core this replaces is free for non-commercial use only. redcode
 * runs from the same page maps (cpu_readmap for reads, cpu_writemem16 for
 * writes: Z80_PBC_MAP / Z80_PBC_WRITE in CMakeLists.txt) and passes ZEXDOC.
 *
 * Differences to the MAME core worth knowing:
 *   - the INT line is level-triggered and sampled between instructions, as
 *     on the real chip; MAME took an IRQ asserted outside z80_execute() at
 *     once and one asserted inside after the instruction (same effect)
 *   - cycles a line overshoots are paid back on the next line, so a frame
 *     runs exactly 262 x 227 cycles; MAME dropped the overshoot
 *   - no busy-loop and delay-loop skipping
 * Of the MAME interface only what SMS Plus calls is here.
 */
#include <stdint.h>
/* redcode's own z80_execute() (not built) has the name of SMS Plus' one */
#define z80_execute redcode_z80_execute
#include "../redcode/Z80.h"     /* redcode */
#undef z80_execute

/* from z80.h, not included: its Z80_PC/Z80_SP would meet redcode's macros
 * of the same names */
#define CLEAR_LINE 0
#define MAME_Z80_PC 1
#define MAME_Z80_SP 2

extern int cpu_readport(int port);
extern void cpu_writeport(int port, int data);

unsigned char *cpu_readmap[8];
unsigned char *cpu_writemap[8];

/* T-states left in the current z80_execute() slice; the VDP's H counter
 * reads it (vdp_hcounter_r). */
int z80_ICount;

static Z80 cpu;
static int (*irq_callback)(int);
static int nmi_state, debt;

/* The cycles left in this slice at the end of the I/O instruction being
 * executed (redcode counts an instruction's cycles once it is done, MAME
 * before; IN/OUT take 11 or 12). */
static void update_icount(void)
{
    z80_ICount = (int)(cpu.cycle_limit - cpu.cycles) - 11;
}

static zuint8 rd(void *context, zuint16 address)
{
    (void)context;
    return cpu_readmap[address >> 13][address & 0x1FFF];
}

extern void cpu_writemem16(int address, int data);

static void wr(void *context, zuint16 address, zuint8 value)
{
    (void)context;
    cpu_writemem16(address, value);
}

static zuint8 in(void *context, zuint16 port)
{
    (void)context;
    update_icount();
    return (zuint8)cpu_readport(port & 0xFF);
}

static void out(void *context, zuint16 port, zuint8 value)
{
    (void)context;
    update_icount();
    cpu_writeport(port & 0xFF, value);
}

/* the data bus during the interrupt acknowledge (IM 2 / IM 0) */
static zuint8 inta(void *context, zuint16 address)
{
    (void)context;
    (void)address;
    return irq_callback ? (zuint8)irq_callback(0) : 0xFF;
}

void z80_reset(void *param)
{
    (void)param;
    cpu.context = 0;
    cpu.fetch_opcode = rd;
    cpu.fetch = rd;
    cpu.read = rd;
    cpu.write = wr;
    cpu.in = in;
    cpu.out = out;
    cpu.halt = 0;
    cpu.nop = 0;
    cpu.nmia = 0;
    cpu.inta = inta;
    cpu.int_fetch = 0;
    cpu.ld_i_a = 0;
    cpu.ld_r_a = 0;
    cpu.reti = 0;
    cpu.retn = 0;
    cpu.hook = 0;
    cpu.illegal = 0;
    cpu.options = Z80_MODEL_ZILOG_NMOS;
    z80_power(&cpu, Z_TRUE);
    nmi_state = CLEAR_LINE;
    debt = 0;
}

void z80_set_reg(int regnum, unsigned val)
{
    if (regnum == MAME_Z80_PC)
        Z80_PC(cpu) = (zuint16)val;
    else if (regnum == MAME_Z80_SP)
        Z80_SP(cpu) = (zuint16)val;
}

void z80_set_irq_callback(int (*callback)(int))
{
    irq_callback = callback;
}

void z80_set_irq_line(int irqline, int state)
{
    (void)irqline;
    z80_int(&cpu, state != CLEAR_LINE);
}

/* edge-triggered: SMS Plus pulses it for PAUSE */
void z80_set_nmi_line(int state)
{
    if (state != CLEAR_LINE && nmi_state == CLEAR_LINE)
        z80_nmi(&cpu);
    nmi_state = state;
}

/* Runs about `cycles` T-states; what the last instruction ran over is
 * taken off the next slice. Returns the T-states run. */
int z80_execute(int cycles)
{
    int want = cycles - debt;
    if (want <= 0) {                    /* a long instruction paid it already */
        debt = -want;
        z80_ICount = 0;
        return 0;
    }
    int ran = (int)z80_run(&cpu, (zusize)want);
    debt = ran - want;
    z80_ICount = -debt;
    return ran;
}

void *z80_context(void)
{
    return &cpu;
}

unsigned z80_context_size(void)
{
    return sizeof cpu;
}
