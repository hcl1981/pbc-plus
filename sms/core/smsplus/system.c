/*
    Copyright (C) 1998, 1999, 2000  Charles Mac Donald

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "shared.h"
#include "ff.h"
#include <stdbool.h>

t_bitmap bitmap;
t_cart cart;
t_snd snd;
t_input input;

/* PBC: no YM2413 (FM unit). pico-smsplus built it but never switched it on
   (sms.use_fm was always 0), so it only took flash and RAM; removed in
   November 2026. Save states keep their 64 bytes for it (zeros). */
static const uint8_t ym2413_none[0x40];

void *frens_f_malloc(size_t size);
void frens_f_free(void *ptr);
void system_init(int rate) {

    // initialize memory. cart.type is already set by load_rom().
    // The SG-1000 renderer does not use the Mode 4 tile cache, and its
    // cartridge RAM is 8 KB instead of the 32 KB SMS battery RAM.
    if (!IS_TMS_SYS) {
        cachePtr = (int16 *)frens_f_malloc(512 * 4 * sizeof(int16));
        cacheStore = (uint8 *)frens_f_malloc(CACHEDTILES * 64 * sizeof(uint8));
        cacheStoreUsed = (uint8 *)frens_f_malloc(CACHEDTILES * sizeof(uint8));
        memset(cachePtr, 0, 512 * 4 * sizeof(int16));
        memset(cacheStore, 0, CACHEDTILES * 64 * sizeof(uint8));
        memset(cacheStoreUsed, 0, CACHEDTILES * sizeof(uint8));
    }
    sms.ram  = (uint8 *)frens_f_malloc(RAMSIZEBYTES);
    sms.sram = (uint8 *)frens_f_malloc(SRAM_BYTES);
    /* ColecoVision: the Super Game Module's RAM, used once a game asks */
    if (IS_COLECO) {
        coleco_sgm_ram = (uint8 *)frens_f_malloc(0x8000);
        if (coleco_sgm_ram) __builtin_memset(coleco_sgm_ram, 0, 0x8000);
    }
    /* Initialize the VDP emulation */
    vdp_init();

    /* Initialize the SMS emulation */
    sms_init();

    /* Initialize the look-up tables and related data */
    render_init();

    /* Enable sound emulation if the sample rate was specified */
    system_audio_init(rate);

    /* Don't save SRAM by default */
    sms.save = 0;

    /* Clear emulated button state */
    __builtin_memset(&input, 0, sizeof(t_input));
}

void system_audio_init(int rate) {
    /* Clear sound context */
    __builtin_memset(&snd, 0, sizeof(t_snd));

    /* Reset logging data */
    snd.log = 0;
    snd.callback = NULL;

    /* Oops.. sound is disabled */
    if (!rate) return;

    /* Calculate buffer size in samples */
    snd.bufsize = (rate / 60);

    /* Sound output */
    snd.buffer[0] = (signed short int *) frens_f_malloc(snd.bufsize * 2);
    snd.buffer[1] = (signed short int *) frens_f_malloc(snd.bufsize * 2);
    if (!snd.buffer[0] || !snd.buffer[1]) return;
    __builtin_memset(snd.buffer[0], 0, snd.bufsize * 2);
    __builtin_memset(snd.buffer[1], 0, snd.bufsize * 2);

    /* Set up SN76489 emulation */
    SN76496_init(0, MASTER_CLOCK, 255, rate);
    /* ColecoVision Super Game Module: AY-3-8910 at half the CPU clock */
    if (IS_COLECO) ay_init(&coleco_ay, MASTER_CLOCK / 2, rate);

#if PICO_RP2350
#endif

    /* Inform other functions that we can use sound */
    snd.enabled = 1;
}

void system_shutdown(void)
{
    // free memory
    printf("Freeing memory allocated to emulator.\n");
    frens_f_free(snd.buffer[1]);
    frens_f_free(snd.buffer[0]);
    frens_f_free(cacheStoreUsed);
    frens_f_free(cacheStore);
    frens_f_free(cachePtr);
    frens_f_free(sms.sram);
    frens_f_free(sms.ram);
    frens_f_free(coleco_sgm_ram);
    coleco_sgm_ram = NULL;
    snd.buffer[0] = NULL;
    snd.buffer[1] = NULL;
    cacheStoreUsed = NULL;
    cacheStore = NULL;
    cachePtr = NULL;
    sms.sram = NULL;
    sms.ram = NULL;
}

void system_reset(void) {
   
    
    cpu_reset();
    vdp_reset();
    sms_reset();
    render_reset();
    if (!IS_TMS_SYS) system_load_sram();
}


bool system_save_state(FIL *fd) {
    UINT bw;
    FRESULT fr;
    /* Save VDP context */
    fr = f_write(fd, &vdp, sizeof(t_vdp), &bw);
    if (fr != FR_OK || bw != sizeof(t_vdp)) {
        printf("Error writing VDP context: fr=%d wrote=%u expected=%u\n", fr, bw, (unsigned)sizeof(t_vdp));
        return false;
    }

    /* Save SMS context */
    fr = f_write(fd, &sms, sizeof(t_sms), &bw);
    if (fr != FR_OK || bw != sizeof(t_sms)) {
        printf("Error writing SMS context: fr=%d wrote=%u expected=%u\n", fr, bw, (unsigned)sizeof(t_sms));
        return false;
    }

    /* Save SMS work RAM contents (sms.ram is a pointer; struct above only stores the address) */
    fr = f_write(fd, sms.ram, RAMSIZEBYTES, &bw);
    if (fr != FR_OK || bw != RAMSIZEBYTES) {
        printf("Error writing SMS RAM: fr=%d wrote=%u expected=%u\n", fr, bw, (unsigned)RAMSIZEBYTES);
        return false;
    }

    /* Save SMS SRAM contents (SG-1000: the smaller cartridge RAM) */
    fr = f_write(fd, sms.sram, SRAM_BYTES, &bw);
    if (fr != FR_OK || bw != SRAM_BYTES) {
        printf("Error writing SMS SRAM: fr=%d wrote=%u expected=%u\n", fr, bw, (unsigned)SRAM_BYTES);
        return false;
    }

    /* Save Z80 context */
    fr = f_write(fd, z80_context(), z80_context_size(), &bw);
    if (fr != FR_OK || bw != z80_context_size()) {
        printf("Error writing Z80 regs: fr=%d wrote=%u expected=%u\n", fr, bw, z80_context_size());
        return false;
    }

    /* The YM2413's 64 bytes: zeros, so the format stays the same */
    fr = f_write(fd, ym2413_none, 0x40, &bw);
    if (fr != FR_OK || bw != 0x40) {
        printf("Error writing YM2413 regs: fr=%d wrote=%u expected=%u\n", fr, bw, (unsigned)0x40);
        return false;
    }

    /* Save SN76489 context */
    fr = f_write(fd, &sn[0], sizeof(t_SN76496), &bw);
    if (fr != FR_OK || bw != sizeof(t_SN76496)) {
        printf("Error writing SN76489 context: fr=%d wrote=%u expected=%u\n", fr, bw, (unsigned)sizeof(t_SN76496));
        return false;
    }

    return true;
}


bool system_load_state(FIL *fd) {
    int i;

    uint8 *reg = (uint8 *)frens_f_malloc(0x40);

    UINT br;
    FRESULT fr;

    /* Initialize everything */
    // cpu_reset();
    system_reset();

    /* Load VDP context */
    fr = f_read(fd, &vdp, sizeof(t_vdp), &br);
    if (fr != FR_OK || br != sizeof(t_vdp)) {
        printf("Error reading VDP context: fr=%d read=%u expected=%u\n", fr, br, (unsigned)sizeof(t_vdp));
        return false;
    }

    /* Load SMS context — preserve runtime pointers across the struct read.
       sms.dummy/ram/sram are pointers into local heap/static memory; the values
       written to the save file at save time are stale and must not be used. */
    uint8 *saved_dummy = sms.dummy;
    uint8 *saved_ram   = sms.ram;
    uint8 *saved_sram  = sms.sram;
    fr = f_read(fd, &sms, sizeof(t_sms), &br);
    sms.dummy = saved_dummy;
    sms.ram   = saved_ram;
    sms.sram  = saved_sram;
    if (fr != FR_OK || br != sizeof(t_sms)) {
        printf("Error reading SMS context: fr=%d read=%u expected=%u\n", fr, br, (unsigned)sizeof(t_sms));
        return false;
    }

    /* Load SMS work RAM contents */
    fr = f_read(fd, sms.ram, RAMSIZEBYTES, &br);
    if (fr != FR_OK || br != RAMSIZEBYTES) {
        printf("Error reading SMS RAM: fr=%d read=%u expected=%u\n", fr, br, (unsigned)RAMSIZEBYTES);
        return false;
    }

    /* Load SMS SRAM contents (SG-1000: the smaller cartridge RAM) */
    fr = f_read(fd, sms.sram, SRAM_BYTES, &br);
    if (fr != FR_OK || br != SRAM_BYTES) {
        printf("Error reading SMS SRAM: fr=%d read=%u expected=%u\n", fr, br, (unsigned)SRAM_BYTES);
        return false;
    }

    /* Load Z80 context (its callback pointers are the same in the same firmware) */
    fr = f_read(fd, z80_context(), z80_context_size(), &br);
    if (fr != FR_OK || br != z80_context_size()) {
        printf("Error reading Z80 regs: fr=%d read=%u expected=%u\n", fr, br, z80_context_size());
        return false;
    }

    // not used
    /* Load YM2413 registers */
    fr = f_read(fd, reg, 0x40, &br);
    if (fr != FR_OK || br != 0x40) {
        printf("Error reading YM2413 regs: fr=%d read=%u expected=%u\n", fr, br, (unsigned)0x40);
        return false;
    }
    
    /* Load SN76489 context */
    fr = f_read(fd, &sn[0], sizeof(t_SN76496), &br);
    if (fr != FR_OK || br != sizeof(t_SN76496)) {
        printf("Error reading SN76489 context: fr=%d read=%u expected=%u\n", fr, br, (unsigned)sizeof(t_SN76496));
        return false;
    }

    /* Restore callbacks */
    z80_set_irq_callback(sms_irq_callback);

    if (IS_COLECO) {
        coleco_memory_map();
    } else if (IS_SG) {
        /* Rebuilds ROM/RAM pages, detected RAM adaptor and mapper paging */
        sg_memory_map();
    } else {
        /* Rebuilds ROM/RAM pages from the mapper registers; a rom without a
           mapper gets its fixed map whatever registers the state holds */
        sms_memory_map();
    }

    /* Force full pattern cache update */
//    is_vram_dirty = 1;
//    memset(vram_dirty, 1, 0x200);

    /* Restore palette */
    for (i = 0; i < PALETTE_SIZE; i += 1)
        palette_sync(i);

    frens_f_free(reg);
    return true;
}






