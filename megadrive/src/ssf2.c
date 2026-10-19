/* ssf2.c -- see ssf2.h */
#include "pico/stdlib.h"
#include "hardware/structs/qmi.h"
#include "hardware/xip_cache.h"
#include "hardware/regs/addressmap.h"
#include "rom_store.h"
#include "ssf2.h"

extern unsigned char *ROM_DATA;

#define BANK_SIZE   0x80000u                    /* 512 KB */
#define MB          0x100000u
/* 68K ROM address 0 sits 3 MB below the start of XIP window 2: addresses
 * 0..3 MB fall into window 1, 3..4 MB into window 2. */
#define ROM_VIRT    (XIP_BASE + 5 * MB)

static bool active;
static uint8_t bank[8];                         /* bank of each 512 KB slot */
static int last;                                /* last slot written */

static void atrans(int window, uint32_t phys, uint32_t size)
{
    if (phys + size > 16 * MB)
        size = 16 * MB - phys;
    qmi_hw->atrans[window] = ((size >> 12) << QMI_ATRANS0_SIZE_LSB) | (phys >> 12);
}

/* Window 2 shows slots 6 and 7 ($300000-$3FFFFF) and can only show two
 * banks that lie next to each other in flash. The game switches both
 * slots one after the other (bank 6/7 <-> 8/9); in between, the slot
 * written last wins. */
static void map_upper(void)
{
    uint32_t phys;
    if (last == 7)
        phys = rom_store_phys(bank[7] * BANK_SIZE) - BANK_SIZE;
    else
        phys = rom_store_phys(bank[6] * BANK_SIZE);
    atrans(2, phys, 4 * MB);
    xip_cache_invalidate_all();                 /* the cache sees virtual addresses */
}

bool ssf2_init(void)
{
    active = rom_store_size() > 4 * MB;
    if (!active)
        return false;
    for (int i = 0; i < 8; i++)
        bank[i] = (uint8_t)i;
    last = 6;
    /* window 1 (virtual 4..8 MB): its last 3 MB are banks 0..5 */
    atrans(1, rom_store_phys(0) - MB, 4 * MB);
    map_upper();
    ROM_DATA = (unsigned char *)ROM_VIRT;
    return true;
}

void gwenesis_mapper_write(unsigned int addr, unsigned int value)
{
    /* $A130F3, F5 ... FF: slots 1..7; $A130F1 (SRAM control) is ignored */
    addr &= 0xFF;
    if (!active || addr < 0xF3 || !(addr & 1))
        return;
    int slot = (int)(addr - 0xF1) / 2;
    uint8_t b = (uint8_t)(value & 0x3F);
    if (bank[slot] == b)
        return;
    bank[slot] = b;
    /* Slots 1..5 stay fixed: the game only switches the upper megabyte,
     * and two windows are all the linear ROM pointer allows. */
    if (slot >= 6) {
        last = slot;
        map_upper();
    }
}
