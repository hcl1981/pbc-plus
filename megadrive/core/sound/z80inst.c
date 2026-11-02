/*
Gwenesis : Genesis & megadrive Emulator.

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.
This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.

__author__ = "bzhxx"
__contact__ = "https://github.com/bzhxx"
__license__ = "GPLv3"

*/
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
/* PBC: the Z80 is redcode/Z80 (core/cpus/redcode, LGPL-3.0-or-later) instead
 * of Marat Fayzullin's core, which is free for non-commercial use only. Its
 * z80_run/z80_execute have the names of the functions below: renamed. */
#define z80_run redcode_z80_run
#define z80_execute redcode_z80_execute
#include "Z80.h"
#undef z80_run
#undef z80_execute
#include "z80inst.h"
#include "z80_benchmark.h"
#include "m68k.h"
#include "gwenesis_bus.h"
#include "ym2612.h"
#include "gwenesis_sn76489.h"
#include "gwenesis_savestate.h"


/* Always optimize Z80 functions for speed - critical path */
#pragma GCC optimize("Ofast")

static volatile int bus_ack = 0;
static volatile int reset = 0;
static volatile int reset_once = 0;
volatile int zclk = 0;
static int initialized = 0;

unsigned char *Z80_RAM;

/* PBC: no bank cache. frank-genesis copied each 32 KB bank out of PSRAM
 * into one of two SRAM slots; here the ROM is in XIP flash and every bank
 * change that missed both slots copied 32 KB. A driver that takes music
 * and samples from more than two banks (Sonic 2) spent most of the frame
 * doing that. zbank_mem_r8() reads the ROM directly instead. */

Z80 cpu;
int current_timeslice = 0;

static zuint8 rd_cb(void *context, zuint16 address) { (void)context; return z80_read(address); }
static void wr_cb(void *context, zuint16 address, zuint8 value) { (void)context; z80_write(address, value); }
static zuint8 io_in(void *context, zuint16 port) { (void)context; (void)port; return 0xFF; }
static void io_out(void *context, zuint16 port, zuint8 value) { (void)context; (void)port; (void)value; }
static zuint8 inta(void *context, zuint16 address) { (void)context; (void)address; return 0xFF; }

static void ResetZ80(Z80 *R)
{
    R->context = 0;
    R->fetch_opcode = rd_cb;
    R->fetch = rd_cb;
    R->read = rd_cb;
    R->write = wr_cb;
    R->in = io_in;
    R->out = io_out;
    R->halt = 0;
    R->nop = 0;
    R->nmia = 0;
    R->inta = inta;
    R->int_fetch = 0;
    R->ld_i_a = 0;
    R->ld_r_a = 0;
    R->reti = 0;
    R->retn = 0;
    R->hook = 0;
    R->illegal = 0;
    R->options = Z80_MODEL_ZILOG_NMOS;
    z80_power(R, Z_TRUE);
}

#define Z80_INST_DISABLE_LOGGING 1

#if !Z80_INST_DISABLE_LOGGING
#include <stdarg.h>
void z80_log(const char *subs, const char *fmt, ...) {
  extern int frame_counter;
  extern int scan_line;

  va_list va;

  printf("%06d:%03d :[%s]", frame_counter, scan_line, subs);

  va_start(va, fmt);
  vfprintf(stdout, fmt, va);
  va_end(va);
  printf("\n");
}
#else
	#define z80_log(...)  do {} while(0)
#endif

// Bank register used by Z80 to access M68K Memory space 1 BANK=32KByte
volatile int Z80_BANK;


void z80_start() {
    z80_benchmark_init();
    ResetZ80(&cpu);
    reset=1;
    reset_once=0;
    bus_ack=0;
    zclk=0;
    /* The bank register is part of the Z80 subsystem's reset state.
     *
     * It is BSS, so on the master it happens to be zero for the first
     * game and nobody noticed it is never cleared. The C2 slave does not
     * reboot between ROM loads, so it inherited the previous game's
     * bank: its Z80 then read ROM through the wrong 32 KB window from
     * the very first frame, took a different branch, and diverged —
     * while every register and the cycle count still matched exactly,
     * which is what made this so hard to see. Loading a second game on
     * the master without a reboot has the same latent bug. */
    Z80_BANK = 0;
}

void z80_pulse_reset() {
  ResetZ80(&cpu);
}

/* External Z80 enable flag from main.c */
extern bool z80_enabled;

/* Z80 cycles actually asked for and actually executed, cumulative.
 * Shared by both halves, so master-local and slave-side rates can be
 * compared directly: a slave that executes fewer cycles per frame is a
 * music driver that ticks slower. */
#ifdef SOUND_CAPTURE
uint32_t z80_cycles_req, z80_cycles_exec, z80_run_calls;
#define Z80_COUNT(x) do { x; } while (0)
#else
#define Z80_COUNT(x) do { } while (0)
#endif

volatile uint32_t z80_live[4];

void z80_run(int target) {
    Z80_COUNT(z80_run_calls++);

  // Skip Z80 execution if disabled (for performance)
  if (!z80_enabled) {
    zclk = target;  // Advance clock without execution
    return;
  }

  // we are in advance,nothind to do
  current_timeslice = 0;
  if (zclk >= target) {
 // z80_log("z80_skip time","%1d%1d%1d||zclk=%d,tgt=%d",reset_once,bus_ack,reset, zclk, target);
    return;
  }

  current_timeslice = target - zclk;

  /* If we have less than one Z80 cycle worth of time, do nothing and
     accumulate until we can run at least 1 cycle. This avoids calling the
     core with RunCycles=0 on very small sync deltas. */
  if (current_timeslice < Z80_FREQ_DIVISOR) {
    return;
  }

  int rem = 0;
  if ((reset_once == 1) && (bus_ack == 0) && (reset == 0)) {

    /* (a HALTed Z80 skips to the end of the slice inside redcode) */
    int cycles_to_run = current_timeslice / Z80_FREQ_DIVISOR;
    Z80_COUNT(z80_cycles_req += (uint32_t)cycles_to_run);
    Z80_COUNT(z80_cycles_exec += (uint32_t)cycles_to_run);
#if Z80_BENCHMARK
    uint64_t bench_start = z80_benchmark_start();
#endif

   // z80_log("z80_run", "%1d%1d%1d||zclk=%d,tgt=%d",reset_once, bus_ack, reset, zclk, target);
    /* what it ran beyond the slice comes back negative, as from ExecZ80 */
    rem = cycles_to_run - (int)redcode_z80_run(&cpu, (zusize)cycles_to_run);

#if Z80_BENCHMARK
    z80_benchmark_end(bench_start, cycles_to_run - rem);
#endif
  }

  Z80_COUNT(z80_cycles_exec -= (uint32_t)rem);
  /* Live snapshot for diagnosis: where the driver is, and whether it can
   * take the 60 Hz interrupt an SMPS driver ticks on. */
  z80_live[0] = Z80_PC(cpu);
  z80_live[1] = cpu.iff1 | cpu.halt_line << 7;
  z80_live[2] = cpu.int_line;
  z80_live[3] = (uint32_t)(bus_ack | (reset << 1) | (reset_once << 2));

  zclk = target - rem * Z80_FREQ_DIVISOR;
}

/* Per-frame signature of the Z80's architectural state.
 *
 * The point is differential testing: the same ROM run with the Z80 on
 * the master (known good) and on the slave must produce the same
 * sequence of these. The first frame where they differ localises the
 * divergence exactly, instead of inferring a cause from how the audio
 * sounds. Registers plus the bank register only — no host pointers or
 * cycle counters, which legitimately differ between the two builds. */
uint32_t z80_state_signature(void) {
    uint32_t h = 2166136261u;
#define SIGMIX(v) do { h ^= (uint32_t)(v); h *= 16777619u; } while (0)
    SIGMIX(Z80_PC(cpu));  SIGMIX(Z80_SP(cpu));
    SIGMIX(Z80_AF(cpu));  SIGMIX(Z80_BC(cpu));
    SIGMIX(Z80_DE(cpu));  SIGMIX(Z80_HL(cpu));
    SIGMIX(Z80_IX(cpu));  SIGMIX(Z80_IY(cpu));
    SIGMIX(Z80_AF_(cpu)); SIGMIX(Z80_BC_(cpu));
    SIGMIX(Z80_DE_(cpu)); SIGMIX(Z80_HL_(cpu));
    SIGMIX(cpu.iff1);     SIGMIX(cpu.i);
    SIGMIX(Z80_BANK);
#undef SIGMIX
    return h;
}

void z80_state_regs(uint32_t out[6]) {
    out[0] = (uint32_t)Z80_PC(cpu) | ((uint32_t)Z80_SP(cpu) << 16);
    out[1] = (uint32_t)Z80_AF(cpu) | ((uint32_t)Z80_BC(cpu) << 16);
    out[2] = (uint32_t)Z80_DE(cpu) | ((uint32_t)Z80_HL(cpu) << 16);
    out[3] = (uint32_t)Z80_IX(cpu) | ((uint32_t)Z80_IY(cpu) << 16);
    out[4] = (uint32_t)Z80_BANK;
    out[5] = (uint32_t)zclk;
}

void z80_sync(void) {
  /*
  get M68K cycles 
  Execute cycles on z80 to sync with m68K
  */

  z80_run(m68k_cycles_master());
}

void z80_set_memory(unsigned char *buffer)
{
    Z80_RAM = buffer;
    initialized = 1;
}

uint32_t z80_ctrl_busreq, z80_ctrl_busrel, z80_ctrl_reset_assert, z80_ctrl_reset_release;

void z80_write_ctrl(unsigned int address, unsigned int value) {
  z80_sync();

  if (address == 0x1100) { if (value) z80_ctrl_busreq++; else z80_ctrl_busrel++; }
  else if (address == 0x1200) { if (value == 0) z80_ctrl_reset_assert++; else z80_ctrl_reset_release++; }


  if (address == 0x1100) // BUSREQ
  {
    // Bus request. Z80 bus on hold.
    if (value) {
      bus_ack = 1;


    // Bus request cancel. Z80 runs.
    } else {
            bus_ack = 0;
    }

  } else if (address == 0x1200) // RESET
  {
    /* The Z80 is held in reset while this line is low and starts from its
     * reset state when it goes high. Only the low->high edge restarts it.
     *
     * Pulsing on every write instead meant a game that keeps writing 1 —
     * which is normal, it is part of the usual sound handshake — restarted
     * the Z80 continuously. Measured on Ultimate Mortal Kombat 3: 1656
     * writes in 15 seconds, so the sound driver was reset 110 times a
     * second and never ran long enough to play anything. Its YM traffic
     * was 985 writes and 116 DAC writes in 20 s against Comix Zone's
     * 375884 and 88684. */
    if (value == 0) {
      reset = 1;
    } else {
      if (reset) {
        z80_pulse_reset();
        reset_once = 1;
      }
      reset = 0;
    }
  }
}

unsigned int z80_read_ctrl(unsigned int address) {

  z80_sync();

  if (address == 0x1100) {

    z80_log(__FUNCTION__,"RUNNING = %d ", bus_ack ? 0 : 1);
    return bus_ack == 1 ? 0 : 1;

  } else if (address == 0x1101) {
    return 0x00;

  } else if (address == 0x1200) {

    z80_log(__FUNCTION__,"RESET = %d ", reset );
    return reset;

  } else if (address == 0x1201) {
    return 0x00;
  }
  return 0xFF;
}

void z80_irq_line(unsigned int value)
{
    if (reset_once == 0) return;

    z80_int(&cpu, value ? Z_TRUE : Z_FALSE);

    z80_log(__FUNCTION__,"Interrupt = %d ", value);

}

#if 0

word z80_get_reg(int reg_i) {
    switch(reg_i) {
        case 0: return cpu.AF.W; break;
        case 1: return cpu.BC.W; break;
        case 2: return cpu.DE.W; break;
        case 3: return cpu.HL.W; break;
        case 4: return cpu.IX.W; break;
        case 5: return cpu.IY.W; break;
        case 6: return cpu.PC.W; break;
        case 7: return cpu.SP.W; break;
    }
}
#endif

/********************************************
 * Z80 Bank
 ********************************************/

unsigned int zbankreg_mem_r8(unsigned int address)
{
      z80_log(__FUNCTION__,"Z80 bank read pointer : %06x", Z80_BANK);

    return Z80_BANK;
}

/* Make this function non-inline and non-static so assembly can call it */
void zbankreg_mem_w8(unsigned int value) {
  Z80_BANK >>= 1;
  Z80_BANK |= (value & 1) << 8;
  z80_log(__FUNCTION__,"Z80 bank points to: %06x", Z80_BANK << 15);
  return;
}

/* Make these functions non-inline and non-static so assembly can call them */
unsigned int __attribute__((section(".time_critical.zbank_mem_r8"))) zbank_mem_r8(unsigned int address)
{
    address &= 0x7FFF;
    
    /* Check if we're reading from ROM (bank in ROM range) */
    unsigned int full_addr = address | (Z80_BANK << 15);
    if (full_addr < 0x800000) {
        /* ROM read, straight from XIP flash (byte-swapped image) */
        extern unsigned char* ROM_DATA;
        return ROM_DATA[full_addr ^ 1];
    }
    
    /* Non-ROM access (RAM mirror, etc) - use slow path */
    z80_log(__FUNCTION__,"Z80 bank read: %06x", full_addr);
    return m68k_read_memory_8(full_addr);
}

void zbank_mem_w8(unsigned int address, unsigned int value) {
  address &= 0x7FFF;
  address |= (Z80_BANK << 15);

  z80_log(__FUNCTION__,"Z80 bank write %06x: %02x", address, value);
  m68k_write_memory_8(address, value);

}

// TODO ??
/*
unsigned int zvdp_mem_r8(unsigned int address)
{
    if (address >= 0x7F00 && address < 0x7F20)
        return vdp_mem_r8(address);
    return 0xFF;
}

void zvdp_mem_w8(unsigned int address, unsigned int value)
{
    if (address >= 0x7F00 && address < 0x7F20)
        vdp_mem_w8(address, value);
}

*/

/* The Z80's memory map, for the core (was z80_mem_opt.S):
 *   0000-3FFF  Z80 RAM, 8 KB mirrored (redcode fetches its code straight
 *              from Z80_RAM, Z80_PBC_FETCH8K in CMakeLists.txt)
 *   4000-5FFF  YM2612
 *   6000       bank register        7F11  SN76489
 *   8000-FFFF  32 KB window into the 68K space (zbank_*)
 * YM2612 and PSG get the master clock of the access: the slice start plus
 * the Z80 cycles run in it so far. */
static inline int access_clock(void)
{
    return zclk + (int)cpu.cycles * Z80_FREQ_DIVISOR;
}

zuint8 __attribute__((section(".time_critical.z80_read"))) z80_read(zuint16 address)
{
    if (address < 0x4000)
        return Z80_RAM[address & 0x1FFF];
    if (address < 0x6000)
        return (zuint8)YM2612Read(access_clock());
    if (address >= 0x8000)
        return (zuint8)zbank_mem_r8(address);
    return 0xFF;
}

void __attribute__((section(".time_critical.z80_write"))) z80_write(int address, int value)
{
    address &= 0xFFFF;
    value &= 0xFF;
    if (address < 0x4000)
        Z80_RAM[address & 0x1FFF] = (unsigned char)value;
    else if (address < 0x6000)
        YM2612Write(address & 3, value, access_clock());
    else if (address == 0x6000)
        zbankreg_mem_w8(value);
    else if (address == 0x7F11)
        gwenesis_SN76489_Write(value, access_clock());
    else if (address >= 0x8000)
        zbank_mem_w8(address, value);
}

void gwenesis_z80inst_save_state() {
    SaveState* state;
    state = saveGwenesisStateOpenForWrite("z80inst");
    saveGwenesisStateSetBuffer(state, "cpu", &cpu, sizeof(Z80));
    saveGwenesisStateSet(state, "bus_ack", bus_ack);
    saveGwenesisStateSet(state, "reset", reset);
    saveGwenesisStateSet(state, "reset_once", reset_once);
    saveGwenesisStateSet(state, "zclk", zclk);
    saveGwenesisStateSet(state, "initialized", initialized);
    saveGwenesisStateSet(state, "Z80_BANK", Z80_BANK);
    saveGwenesisStateSet(state, "current_timeslice",current_timeslice);
}

void gwenesis_z80inst_load_state() {
    SaveState* state = saveGwenesisStateOpenForRead("z80inst");
    saveGwenesisStateGetBuffer(state, "cpu", &cpu, sizeof(Z80));
    bus_ack = saveGwenesisStateGet(state, "bus_ack");
    reset = saveGwenesisStateGet(state, "reset");
    reset_once = saveGwenesisStateGet(state, "reset_once");
    zclk = saveGwenesisStateGet(state, "zclk");
    initialized = saveGwenesisStateGet(state, "initialized");
    Z80_BANK = saveGwenesisStateGet(state, "Z80_BANK");
    current_timeslice = saveGwenesisStateGet(state, "current_timeslice");

}

