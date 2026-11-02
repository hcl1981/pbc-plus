/* ======================================================================== */
/*                                 MUSASHI                                  */
/*                    Version 4.10, 68000 only, for the                     */
/*                  Mega Drive emulator of the PicoBoy Color Plus           */
/* ======================================================================== */
/*
 * A portable Motorola M680x0 processor emulation engine.
 * Copyright Karl Stenerud.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* PBC: the public interface of this port (see readme.txt). It differs from
 * Musashi's in the way the Mega Drive emulator drives the CPU:
 *   - time is a master clock count (7 per 68000 clock) that only grows
 *     within a frame: m68k_run(t) executes until m68k.cycles >= t
 *   - ROM (below 0x800000) and work RAM (0xFF0000 up) are read directly,
 *     everything else through m68k_read/write_memory_* (gwenesis_bus.c)
 *   - the interrupt level is set by the VDP and cleared by its acknowledge
 *     callback (m68k_set_irq, m68k_update_irq, m68k_set_int_ack_callback)
 */

#ifndef M68K__HEADER
#define M68K__HEADER

#include <stdint.h>
#include "m68kconf.h"

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#define M68K_IRQ_NONE 0
#define M68K_IRQ_1    1
#define M68K_IRQ_2    2
#define M68K_IRQ_3    3
#define M68K_IRQ_4    4
#define M68K_IRQ_5    5
#define M68K_IRQ_6    6
#define M68K_IRQ_7    7

/* Special interrupt acknowledge values */
#define M68K_INT_ACK_AUTOVECTOR    0xffffffff
#define M68K_INT_ACK_SPURIOUS      0xfffffffe

/* Registers used by m68k_get_reg() and m68k_set_reg() */
typedef enum
{
	M68K_REG_D0, M68K_REG_D1, M68K_REG_D2, M68K_REG_D3,
	M68K_REG_D4, M68K_REG_D5, M68K_REG_D6, M68K_REG_D7,
	M68K_REG_A0, M68K_REG_A1, M68K_REG_A2, M68K_REG_A3,
	M68K_REG_A4, M68K_REG_A5, M68K_REG_A6, M68K_REG_A7,
	M68K_REG_PC,        /* Program Counter */
	M68K_REG_SR,        /* Status Register */
	M68K_REG_SP,        /* The current Stack Pointer (located in A7) */
	M68K_REG_USP,       /* User Stack Pointer */
	M68K_REG_ISP,       /* Interrupt Stack Pointer */
	M68K_REG_PPC,       /* Previous value in the program counter */
	M68K_REG_IR         /* Instruction register */
} m68k_register_t;

/* The CPU state. Only `cycles' is meant for the host (it subtracts the
 * frame length from it at the end of each frame). */
typedef struct
{
	int  cycles;       /* master clock count */
	int  cycle_end;    /* target of the current m68k_run() */
	unsigned int dar[16];   /* Data and Address Registers */
	unsigned int ppc;       /* Previous program counter */
	unsigned int pc;        /* Program Counter */
	unsigned int sp[7];     /* User and Interrupt Stack Pointers (USP, ISP: 0, 4) */
	unsigned int ir;        /* Instruction Register */
	unsigned int t1_flag;   /* Trace 1 */
	unsigned int s_flag;    /* Supervisor */
	unsigned int x_flag;    /* Extend */
	unsigned int n_flag;    /* Negative */
	unsigned int not_z_flag; /* Zero, inverted for speedups */
	unsigned int v_flag;    /* Overflow */
	unsigned int c_flag;    /* Carry */
	unsigned int int_mask;  /* I0-I2 */
	unsigned int int_level; /* State of interrupt pins IPL0-IPL2, << 8 */
	unsigned int stopped;   /* Stopped state */
	int  (*int_ack_callback)(int int_line);  /* Interrupt Acknowledge */
} m68ki_cpu_core;

extern m68ki_cpu_core m68k;

/* ======================================================================== */
/* ============================== CALLBACKS =============================== */
/* ======================================================================== */

/* Supplied by the host (gwenesis_bus.c) for everything that is not ROM or
 * work RAM. Addresses are 24 bits, 16- and 32-bit accesses even. */
unsigned int  m68k_read_memory_8(unsigned int address);
unsigned int  m68k_read_memory_16(unsigned int address);
unsigned int  m68k_read_memory_32(unsigned int address);
void m68k_write_memory_8(unsigned int address, unsigned int value);
void m68k_write_memory_16(unsigned int address, unsigned int value);
void m68k_write_memory_32(unsigned int address, unsigned int value);
unsigned int m68k_read_disassembler_16(unsigned int address);
unsigned int m68k_read_disassembler_32(unsigned int address);

/* ROM and work RAM, read and written directly by the CPU. Both hold 16-bit
 * words in the host's byte order: byte A is at [A ^ 1]. */
extern unsigned char *ROM_DATA;
extern unsigned char M68K_RAM[];

/* The same direct access for the bus and the VDP's DMA (A: 68000 address) */
static inline unsigned int FETCH8ROM(unsigned int a)  { return ROM_DATA[a ^ 1]; }
static inline unsigned int FETCH16ROM(unsigned int a) { return *(const uint16_t *)(ROM_DATA + a); }
static inline unsigned int FETCH8RAM(unsigned int a)  { return M68K_RAM[(a ^ 1) & 0xffff]; }
static inline unsigned int FETCH16RAM(unsigned int a) { return *(const uint16_t *)(M68K_RAM + (a & 0xffff)); }
static inline unsigned int FETCH32RAM(unsigned int a)
{
	uint32_t v = *(const uint32_t *)(M68K_RAM + (a & 0xffff));
	return (v << 16) | (v >> 16);
}
static inline void WRITE8RAM(unsigned int a, unsigned int v)  { M68K_RAM[(a ^ 1) & 0xffff] = (uint8_t)v; }
static inline void WRITE16RAM(unsigned int a, unsigned int v) { *(uint16_t *)(M68K_RAM + (a & 0xffff)) = (uint16_t)v; }
static inline void WRITE32RAM(unsigned int a, unsigned int v)
{
	*(uint32_t *)(M68K_RAM + (a & 0xffff)) = (v << 16) | (v >> 16);
}

/* Interrupt acknowledge: returns M68K_INT_ACK_AUTOVECTOR (the Mega Drive
 * only uses autovectors). */
void m68k_set_int_ack_callback(int (*callback)(int int_level));

/* ======================================================================== */
/* ====================== FUNCTIONS TO ACCESS THE CPU ===================== */
/* ======================================================================== */

void m68k_init(void);
void m68k_pulse_reset(void);

/* Run until m68k.cycles >= cycles (master clocks) */
void m68k_run(unsigned int cycles);

int m68k_cycles_master(void);   /* m68k.cycles */
int m68k_cycles_run(void);      /* clocks left until the target of m68k_run() */

/* Set the IPL lines to int_level (0-7); m68k_update_irq ORs a level in */
void m68k_set_irq(unsigned int int_level);
void m68k_update_irq(unsigned int int_level);

void m68k_pulse_halt(void);
void m68k_clear_halt(void);

unsigned int m68k_get_reg(m68k_register_t reg);
void m68k_set_reg(m68k_register_t reg, unsigned int value);

void gwenesis_m68k_save_state(void);
void gwenesis_m68k_load_state(void);

#endif /* M68K__HEADER */
