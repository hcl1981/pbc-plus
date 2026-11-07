/* ======================================================================== */
/* ========================= LICENSING & COPYRIGHT ======================== */
/* ======================================================================== */
/*
 *                                  MUSASHI
 *                                Version 4.60
 *
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

/* PBC: Musashi 4.10 reduced to the 68000 for the Mega Drive emulator of the
 * PicoBoy Color Plus. This file keeps Musashi's tables; the rest is written
 * for the way the emulator drives the CPU (m68k.h): a master clock count,
 * m68k_run() to a target, the instruction table of tools/m68k_gen.py, and
 * the save state format of the emulator. */

#include <stddef.h>
#include "m68kcpu.h"
#include "gwenesis_savestate.h"

m68ki_cpu_core m68k;
unsigned int m68ki_unused_mode;

#ifdef M68K_TRACE_HOOK
void M68K_TRACE_HOOK(void);
#endif

/* ======================================================================== */
/* ================================= DATA ================================= */
/* ======================================================================== */

/* Used by shift & rotate instructions */
const uint8 m68ki_shift_8_table[65] =
{
	0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff
};

const uint16 m68ki_shift_16_table[65] =
{
	0x0000, 0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
	0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc, 0xfffe, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff
};

const uint m68ki_shift_32_table[65] =
{
	0x00000000, 0x80000000, 0xc0000000, 0xe0000000, 0xf0000000, 0xf8000000,
	0xfc000000, 0xfe000000, 0xff000000, 0xff800000, 0xffc00000, 0xffe00000,
	0xfff00000, 0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000, 0xffff8000,
	0xffffc000, 0xffffe000, 0xfffff000, 0xfffff800, 0xfffffc00, 0xfffffe00,
	0xffffff00, 0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0, 0xfffffff8,
	0xfffffffc, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};


/* Number of clock cycles to use for exception processing (68000).
 * I used 4 for any vectors that are undocumented for processing times.
 * PBC: autovectored interrupts 54 instead of 44: with VPA the acknowledge
 * cycle waits for the E clock (CPU clock / 10), about 10 clocks more on
 * average than the manual's 44, which assumes a 4-clock acknowledge.
 */
const uint8 m68ki_exception_cycle_table[256] =
{
		 40, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 34, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 34, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 34, /*  9: Trace                                              */
		 34, /* 10: 1010                                               */
		 34, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 44, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 44, /* 24: Spurious Interrupt                                 */
		 54, /* 25: Level 1 Interrupt Autovector                       */
		 54, /* 26: Level 2 Interrupt Autovector                       */
		 54, /* 27: Level 3 Interrupt Autovector                       */
		 54, /* 28: Level 4 Interrupt Autovector                       */
		 54, /* 29: Level 5 Interrupt Autovector                       */
		 54, /* 30: Level 6 Interrupt Autovector                       */
		 54, /* 31: Level 7 Interrupt Autovector                       */
		 34, /* 32: TRAP #0                                            */
		 34, /* 33: TRAP #1                                            */
		 34, /* 34: TRAP #2                                            */
		 34, /* 35: TRAP #3                                            */
		 34, /* 36: TRAP #4                                            */
		 34, /* 37: TRAP #5                                            */
		 34, /* 38: TRAP #6                                            */
		 34, /* 39: TRAP #7                                            */
		 34, /* 40: TRAP #8                                            */
		 34, /* 41: TRAP #9                                            */
		 34, /* 42: TRAP #10                                           */
		 34, /* 43: TRAP #11                                           */
		 34, /* 44: TRAP #12                                           */
		 34, /* 45: TRAP #13                                           */
		 34, /* 46: TRAP #14                                           */
		 34, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
};

/* only used by the full extension format of the 68020 (dead code here) */
const uint8 m68ki_ea_idx_cycle_table[64] =
{
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0, /* ..01.000 no memory indirect, base NULL             */
	 5, /* ..01..01 memory indirect,    base NULL, outer NULL */
	 7, /* ..01..10 memory indirect,    base NULL, outer 16   */
	 7, /* ..01..11 memory indirect,    base NULL, outer 32   */
	 0,  5,  7,  7,  0,  5,  7,  7,  0,  5,  7,  7,
	 2, /* ..10.000 no memory indirect, base 16               */
	 7, /* ..10..01 memory indirect,    base 16,   outer NULL */
	 9, /* ..10..10 memory indirect,    base 16,   outer 16   */
	 9, /* ..10..11 memory indirect,    base 16,   outer 32   */
	 0,  7,  9,  9,  0,  7,  9,  9,  0,  7,  9,  9,
	 6, /* ..11.000 no memory indirect, base 32               */
	11, /* ..11..01 memory indirect,    base 32,   outer NULL */
	13, /* ..11..10 memory indirect,    base 32,   outer 16   */
	13, /* ..11..11 memory indirect,    base 32,   outer 32   */
	 0, 11, 13, 13,  0, 11, 13, 13,  0, 11, 13, 13
};


/* ======================================================================== */
/* ================================= API ================================== */
/* ======================================================================== */

/* Without an acknowledge callback: autovector, and the request goes away */
static int default_int_ack_callback(int int_level)
{
	(void)int_level;
	CPU_INT_LEVEL = 0;
	return M68K_INT_ACK_AUTOVECTOR;
}

void m68k_set_int_ack_callback(int (*callback)(int int_level))
{
	CALLBACK_INT_ACK = callback ? callback : default_int_ack_callback;
}

unsigned int m68k_get_reg(m68k_register_t regnum)
{
	switch(regnum)
	{
		case M68K_REG_D0: case M68K_REG_D1: case M68K_REG_D2: case M68K_REG_D3:
		case M68K_REG_D4: case M68K_REG_D5: case M68K_REG_D6: case M68K_REG_D7:
		case M68K_REG_A0: case M68K_REG_A1: case M68K_REG_A2: case M68K_REG_A3:
		case M68K_REG_A4: case M68K_REG_A5: case M68K_REG_A6: case M68K_REG_A7:
			return REG_DA[regnum - M68K_REG_D0];
		case M68K_REG_PC:  return REG_PC;
		case M68K_REG_SR:  return m68ki_get_sr();
		case M68K_REG_SP:  return REG_SP;
		case M68K_REG_USP: return FLAG_S ? REG_USP : REG_SP;
		case M68K_REG_ISP: return FLAG_S ? REG_SP : REG_ISP;
		case M68K_REG_PPC: return REG_PPC;
		case M68K_REG_IR:  return REG_IR;
		default:           return 0;
	}
}

void m68k_set_reg(m68k_register_t regnum, unsigned int value)
{
	switch(regnum)
	{
		case M68K_REG_D0: case M68K_REG_D1: case M68K_REG_D2: case M68K_REG_D3:
		case M68K_REG_D4: case M68K_REG_D5: case M68K_REG_D6: case M68K_REG_D7:
		case M68K_REG_A0: case M68K_REG_A1: case M68K_REG_A2: case M68K_REG_A3:
		case M68K_REG_A4: case M68K_REG_A5: case M68K_REG_A6: case M68K_REG_A7:
			REG_DA[regnum - M68K_REG_D0] = value; return;
		case M68K_REG_PC:  m68ki_jump(value); return;
		case M68K_REG_SR:  m68ki_set_sr_noint(value); return;
		case M68K_REG_SP:  REG_SP = value; return;
		case M68K_REG_USP: if(FLAG_S) REG_USP = value; else REG_SP = value; return;
		case M68K_REG_ISP: if(FLAG_S) REG_SP = value; else REG_ISP = value; return;
		case M68K_REG_PPC: REG_PPC = value; return;
		case M68K_REG_IR:  REG_IR = MASK_OUT_ABOVE_16(value); return;
		default:           return;
	}
}

/* The VDP drives the IPL lines; the level is taken between instructions
 * when it is above the interrupt mask (m68k_run, or an SR change). */
/* PBC: these and the clock queries below in RAM: called per line and per
 * sound chip access */
#define M68K_RAM_FN __attribute__((section(".time_critical.m68k_irq")))

M68K_RAM_FN void m68k_set_irq(unsigned int int_level)
{
	CPU_INT_LEVEL = int_level << 8;
}

M68K_RAM_FN void m68k_update_irq(unsigned int int_level)
{
	CPU_INT_LEVEL |= int_level << 8;
}

/* m68ki_poll_skip() below: its state, and the slices counted */
static unsigned int m68ki_slice;
static unsigned int poll_pc = 1, poll_slice;
static int poll_at;

/* PBC: in RAM like the instruction table, so the dispatch loop never waits
 * for flash. */
void __attribute__((section(".time_critical.m68k_run"))) m68k_run(unsigned int cycles)
{
	const int end = (int)cycles;

	/* Make sure the CPU is not already ahead */
	if(m68ki_cpu.cycles >= end)
		return;

	/* Take a pending interrupt (this also ends STOP) */
	m68ki_check_interrupts();

	if(CPU_STOPPED)
	{
		m68ki_cpu.cycles = end;
		return;
	}

	m68ki_cpu.cycle_end = end;
	m68ki_slice++;

	while(m68ki_cpu.cycles < end)
	{
		/* Record previous program counter (exceptions stack it, a branch
		 * to itself compares with it) */
		REG_PPC = REG_PC;

#ifdef M68K_TRACE_HOOK
		M68K_TRACE_HOOK();      /* host tests (tools/host) */
#endif

		/* Read an instruction and call its handler */
		REG_IR = m68ki_read_imm_16();
		const m68ki_op_t *op = m68ki_decode(REG_IR);
		op->handler();
		m68ki_cpu.cycles += op->cycles;
	}
}

/* PBC: waiting loops, taken in one step.
 *
 * Within one m68k_run() slice nothing but the 68000 changes its RAM:
 * interrupts are taken at the start of a slice or when an instruction
 * lowers the mask, the Z80 runs between slices, and DMA only starts on a
 * VDP write. A loop that only tests a work RAM location and branches back
 *     loop:  tst.b/w/l (xxx).w | tst.b/w/l (xxx).l
 *            btst #n,(xxx).w   | cmpi.b/w #n,(xxx).w
 *            bcc.s loop
 * therefore goes round the same way until the end of the slice: same
 * registers and flags each time, the same number of clocks. Its whole
 * rounds that still start before the end are added at once; the last,
 * partial one runs as usual. Registers, flags, PC and clock at the end of
 * the slice are exactly as without (only an instruction trace would
 * notice); games wait for the VBlank interrupt like this (Sonic 2, Streets
 * of Rage 2: half of all the instructions in a level).
 * Only once the branch has been taken twice in a row in the same slice:
 * then the test ran in between and the branch followed what it read now.
 * The first time it may follow flags from before an interrupt (RTE back
 * to the branch), and the RAM may have changed since.
 * Called by Bcc.S right after it branched back by 6 or 8 bytes: REG_PPC
 * is the branch, REG_PC the loop's first instruction; the run loop still
 * adds the branch's own clocks. */
void __attribute__((section(".time_critical.m68k_run"))) m68ki_poll_skip(void)
{
	uint pc = REG_PC;
	uint len = REG_PPC - pc;                /* bytes before the branch */
	uint op = m68k_read_immediate_16(pc);
	uint addr;

	switch(op)
	{
		case 0x4a38: case 0x4a78: case 0x4ab8:  /* tst.b/w/l (xxx).w */
			if(len != 4)
				return;
			addr = MAKE_INT_16(m68k_read_immediate_16(pc + 2));
			break;
		case 0x4a39: case 0x4a79: case 0x4ab9:  /* tst.b/w/l (xxx).l */
			if(len != 6)
				return;
			addr = m68k_read_immediate_32(pc + 2);
			break;
		case 0x0838:                            /* btst #n,(xxx).w */
		case 0x0c38: case 0x0c78:               /* cmpi.b/w #n,(xxx).w */
			if(len != 6)
				return;
			addr = MAKE_INT_16(m68k_read_immediate_16(pc + 4));
			break;
		default:
			return;
	}
	if(ADDRESS_68K(addr) < 0xff0000)        /* only work RAM */
		return;

	int branch = (int)m68ki_op_cycles(REG_IR);
	int period = (int)m68ki_op_cycles(op) + branch;
	int now = m68ki_cpu.cycles;
	if(REG_PPC != poll_pc || m68ki_slice != poll_slice || now != poll_at + period)
	{
		poll_pc = REG_PPC;                  /* first time round: next time */
		poll_slice = m68ki_slice;
		poll_at = now;
		return;
	}
	int left = m68ki_cpu.cycle_end - (now + branch);
	if(left >= period)
		m68ki_cpu.cycles += left - left % period;
}

/* DBF Dn,* (a delay loop): its rounds to come until the end of the slice,
 * or until Dn runs out, at once. Called after it branched to itself and
 * counted its clocks but for the base time the run loop adds. */
void __attribute__((section(".time_critical.m68k_run"))) m68ki_dbf_skip(uint *r_dst)
{
	int base = (int)m68ki_op_cycles(REG_IR);
	int period = base + CYC_DBCC_F_NOEXP * MUL;     /* one more round, taken */
	int left = m68ki_cpu.cycle_end - (m68ki_cpu.cycles + base);
	uint rounds = MASK_OUT_ABOVE_16(*r_dst);        /* taken ones still to come */
	if(left < period || rounds == 0)
		return;
	uint k = (uint)(left / period);
	if(k > rounds)
		k = rounds;
	*r_dst = MASK_OUT_BELOW_16(*r_dst) | (rounds - k);
	m68ki_cpu.cycles += (int)k * period;
}

/* Master clocks left until the target of m68k_run() (VDP H counter) */
M68K_RAM_FN int m68k_cycles_run(void)
{
	return m68ki_cpu.cycle_end - m68ki_cpu.cycles;
}

M68K_RAM_FN int m68k_cycles_master(void)
{
	return m68ki_cpu.cycles;
}

void m68k_init(void)
{
	m68k_set_int_ack_callback(NULL);
}

/* Pulse the RESET line on the CPU */
void m68k_pulse_reset(void)
{
	/* Clear all stop levels */
	CPU_STOPPED = 0;

	/* Turn off tracing */
	FLAG_T1 = 0;
	/* Interrupt mask to level 7 */
	FLAG_INT_MASK = 0x0700;
	CPU_INT_LEVEL = 0;
	/* Go to supervisor mode */
	m68ki_set_s_flag(SFLAG_SET);

	/* Read the initial stack pointer and program counter */
	m68ki_jump(0);
	REG_SP = m68ki_read_imm_32();
	REG_PC = m68ki_read_imm_32();
	m68ki_jump(REG_PC);

	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_RESET]);
}

/* Pulse the HALT line on the CPU */
void m68k_pulse_halt(void)
{
	CPU_STOPPED |= STOP_LEVEL_HALT;
}

void m68k_clear_halt(void)
{
	CPU_STOPPED &= ~STOP_LEVEL_HALT;
}

/* Same entries as the emulator's previous 68000 core */
void gwenesis_m68k_save_state(void)
{
	SaveState *state = saveGwenesisStateOpenForWrite("m68k");

	saveGwenesisStateSetBuffer(state, "REG_D", REG_D, sizeof(REG_DA));
	saveGwenesisStateSet(state, "SR", m68ki_get_sr());
	saveGwenesisStateSet(state, "REG_PC", REG_PC);
	saveGwenesisStateSet(state, "REG_SP", REG_SP);
	saveGwenesisStateSet(state, "REG_USP", REG_USP);
	saveGwenesisStateSet(state, "REG_ISP", REG_ISP);
	saveGwenesisStateSet(state, "REG_IR", REG_IR);

	saveGwenesisStateSet(state, "m68k_cycle_end", m68ki_cpu.cycle_end);
	saveGwenesisStateSet(state, "m68k_cycles", m68ki_cpu.cycles);
	saveGwenesisStateSet(state, "m68k_int_level", CPU_INT_LEVEL);
	saveGwenesisStateSet(state, "m68k_stopped", CPU_STOPPED);
}

void gwenesis_m68k_load_state(void)
{
	SaveState *state = saveGwenesisStateOpenForRead("m68k");

	saveGwenesisStateGetBuffer(state, "REG_D", REG_D, sizeof(REG_DA));
	m68ki_set_sr_noint_nosp(saveGwenesisStateGet(state, "SR"));
	REG_PC = saveGwenesisStateGet(state, "REG_PC");
	REG_SP = saveGwenesisStateGet(state, "REG_SP");
	REG_USP = saveGwenesisStateGet(state, "REG_USP");
	REG_ISP = saveGwenesisStateGet(state, "REG_ISP");
	REG_IR = saveGwenesisStateGet(state, "REG_IR");

	m68ki_cpu.cycle_end = saveGwenesisStateGet(state, "m68k_cycle_end");
	m68ki_cpu.cycles = saveGwenesisStateGet(state, "m68k_cycles");
	CPU_INT_LEVEL = saveGwenesisStateGet(state, "m68k_int_level");
	CPU_STOPPED = saveGwenesisStateGet(state, "m68k_stopped");
}

/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */
