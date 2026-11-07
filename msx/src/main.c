/*
 * PBC MSX -- MSX1 emulator for the PicoBoy Color Plus (RP2350A, 520 KB SRAM,
 * 16 MB flash, ST7789 240x280), part of the combined firmware only.
 *
 * Emulator core: core/msx1, written for the PicoBoy (TMS9918A video,
 * AY-3-8910 sound, Konami SCC, slots, PPI, MegaROM mappers), with the Z80
 * redcode/Z80 (core/redcode, LGPL-3.0-or-later) and C-BIOS 0.29a as the
 * system ROM (core/cbios, BSD-style). The platform layer follows the other
 * emulators here:
 *   - the launcher picks the game; the cartridge (.rom .mx1) is copied into
 *     the flash ROM region and used in place
 *   - every scanline goes to the panel as it is drawn, sent by core 1
 *     (msx_video.c); sound on the PWM speaker (audio_pwm.c)
 *   - link play: player 1 joystick 1 and keyboard, player 2 joystick 2
 *
 * Buttons: msx_input.h. RESET goes back to the launcher.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/vreg.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/structs/qmi.h"

#include "ff.h"

#include "board.h"
#include "pbc_launch.h"
#include "pbc_loading.h"
#include "pbc_link.h"
#include "color565.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#include "rom_store.h"
#include "audio_pwm.h"
#include "settings.h"
#include "msx_video.h"
#include "msx_input.h"
#include "msx1.h"
#include "v9938.h"

#if !PBC_MULTI
#error "PBC MSX exists only as part of the combined firmware (pbcemu/build.sh)"
#endif

/* ---- build-time configuration (see CMakeLists.txt) ---------------------- */
#ifndef CPU_MHZ
#define CPU_MHZ 200
#endif
#ifndef CPU_VREG
#define CPU_VREG VREG_VOLTAGE_1_15
#endif
#ifndef FLASH_MAX_FREQ_MHZ
#define FLASH_MAX_FREQ_MHZ 88
#endif

/* C-BIOS for MSX1 and MSX2 and its disk ROM C-DISK, built in (core/cbios/cbios.S),
 * as 8 KB pages: one page is shared by the main ROMs and the sub-ROM */
extern const uint8_t cbios_shared[], cbios_main01[], cbios_main3[], cbios_logo[];
extern const uint8_t cbios2_main01[], cbios2_main3[], cbios2_logo[], cbios2_sub0[];
extern const uint8_t cbios_disk[], cbios_disk_tail[];
static const uint8_t *const cbios1_pages[4] = { cbios_main01, cbios_main01 + 0x2000, cbios_shared, cbios_main3 };
static const uint8_t *const cbios2_pages[4] = { cbios2_main01, cbios2_main01 + 0x2000, cbios_shared, cbios2_main3 };
static const uint8_t *const cbios2_sub_pages[2] = { cbios2_sub0, cbios_shared };

static FATFS g_fs;

static bool fs_mount(void) { return f_mount(&g_fs, "", 1) == FR_OK; }

/* ---- clocks (as in megadrive) -------------------------------------------- */

static void __no_inline_not_in_flash_func(set_flash_timings)(int cpu_mhz)
{
    const int clock_hz = cpu_mhz * 1000000;
    const int max_flash_freq = FLASH_MAX_FREQ_MHZ * 1000000;

    int divisor = (clock_hz + max_flash_freq - (max_flash_freq >> 4) - 1) / max_flash_freq;
    if (divisor == 1 && clock_hz >= 166000000)
        divisor = 2;

    int rxdelay = divisor;
    if (clock_hz / divisor > 100000000 && clock_hz >= 166000000)
        rxdelay += 1;

    qmi_hw->m[0].timing = 0x60007000 |
                          rxdelay << QMI_M0_TIMING_RXDELAY_LSB |
                          divisor << QMI_M0_TIMING_CLKDIV_LSB;
}

/* The SPI clock is clk_peri divided by an even number, and clk_peri
 * follows clk_sys: at 200 MHz the display got only 50 MHz, too slow for
 * 192 lines a frame. USB is used only before the game (stick mode), so in
 * the game the USB PLL runs at 150 MHz and drives clk_peri alone: 75 MHz
 * SPI whatever the system clock (as in megadrive and pce). */
static void display_clock(void)
{
    clock_stop(clk_usb);
    clock_stop(clk_adc);
    pll_init(pll_usb, 1, 1500 * MHZ, 5, 2);          /* 12 MHz x 125 / 10 */
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    150 * MHZ, 150 * MHZ);
    /* the USB block clocked again: the link cable (usblink) drives its PHY
     * and hangs without it */
    clock_configure(clk_usb, 0, CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    150 * MHZ, 50 * MHZ);
    spi_set_baudrate(PBC_SPI, PBC_SPI_HZ);
}

/* After the ROM region has been written. */
static void overclock(void)
{
    if (CPU_MHZ > 150) {
        if (CPU_VREG > VREG_VOLTAGE_1_30)
            vreg_disable_voltage_limit();
        vreg_set_voltage(CPU_VREG);
        sleep_ms(10);
        set_flash_timings(CPU_MHZ);
        if (!set_sys_clock_khz(CPU_MHZ * 1000, false)) {
            set_flash_timings(250);
            set_sys_clock_khz(250000, true);
        }
    }
    display_clock();
}

/* ---- the game being played ------------------------------------------------- */

static char game[ROM_NAME_MAX];

/* ---- MSX2: system ROMs ----------------------------------------------------------
 *
 * Built in is C-BIOS for MSX2 with C-DISK: cartridges run, disks only when
 * they boot by themselves (no BASIC, no MSX-DOS). If the drive's root holds
 * msx2.rom (main ROM, 32 KB), msx2ext.rom (sub-ROM, 16 KB) and, for disks,
 * disk.rom (16 KB), that set is used instead -- whole, never mixed with
 * C-BIOS. The disk ROM is patched (traps), so it goes into flash behind the
 * cartridge room of the ROM region, as do the drive's ROMs; written only
 * when they differ from what is there (another emulator's game may have
 * covered them). */
#define SYS_OFF   (ROM_FLASH_OFFSET + ROM_STORE_MAX)
#define SYS_MAIN  0
#define SYS_SUB   0x8000
#define SYS_DISK  0xc000
#define SYS_ROM(o) ((const uint8_t *)(XIP_BASE + SYS_OFF + (o)))

static bool ends_ci(const char *s, const char *ext)
{
    size_t n = strlen(s), m = strlen(ext);
    return n >= m && strcasecmp(s + n - m, ext) == 0;
}

/* `size` bytes from the drive's file `name` (or from `rom` if name is NULL)
 * to flash at `off`, patched as a disk ROM if `disk`; false if the file is
 * missing or of another size. buf: room for size bytes. */
static bool sys_store(const char *name, const uint8_t *rom, uint32_t size, uint32_t off,
                      bool disk, uint8_t *buf)
{
    if (name) {
        static FIL f;                   /* a FIL carries a 4 KB sector buffer, the stack is 4 KB */
        UINT n;
        if (f_open(&f, name, FA_READ) != FR_OK)
            return false;
        bool ok = f_size(&f) == size && f_read(&f, buf, size, &n) == FR_OK && n == size;
        f_close(&f);
        if (!ok)
            return false;
    } else if (rom != buf) {
        memcpy(buf, rom, size);
    }
    if (disk)
        msx2_patch_disk_rom(buf);
    for (uint32_t done = 0; done < size; done += FLASH_SECTOR_SIZE) {
        if (memcmp(buf + done, SYS_ROM(off + done), FLASH_SECTOR_SIZE) != 0) {
            uint32_t irq = save_and_disable_interrupts();
            flash_range_erase(SYS_OFF + off + done, FLASH_SECTOR_SIZE);
            flash_range_program(SYS_OFF + off + done, buf + done, FLASH_SECTOR_SIZE);
            restore_interrupts(irq);
        }
    }
    return true;
}

/* C-DISK has no MSX-DOS: a boot sector that opens files through the BDOS
 * (LD C,fn / CALL F37Dh) right at its start, like the standard MSX-DOS
 * one, cannot boot, nor can a disk without boot code. Only the start of
 * the code is looked at: a game's own boot code may leave parts of a
 * standard one behind it, never run. C-DISK's BDOS knows 02h 09h 0Dh 19h
 * 1Ah 2Eh 2Fh (console, DTA, absolute sectors). */
static bool boot_needs_dos(const uint8_t *s)
{
    if ((s[0] != 0xeb && s[0] != 0xe9) || (s[0x1e] == 0 && s[0x1f] == 0))
        return true;
    for (int i = 0x1e; i + 5 <= 0x1e + 48; i++) {
        if (s[i] == 0x0e && s[i + 2] == 0xcd && s[i + 3] == 0x7d && s[i + 4] == 0xf3) {
            switch (s[i + 1]) {
            case 0x02: case 0x09: case 0x0d: case 0x19: case 0x1a: case 0x2e: case 0x2f:
                break;
            default:
                return true;
            }
        }
    }
    return false;
}

/* ---- MSX2: the disk in drive A ---------------------------------------------------
 *
 * Sectors are read from and written to the .dsk file on the USB drive
 * itself; a write goes to flash at once (core 1 waits meanwhile). */
static FIL disk_file;

static bool disk_io(bool write, uint32_t sector, uint8_t *buf)
{
    UINT n;
    if (f_lseek(&disk_file, (FSIZE_t)sector * 512) != FR_OK)
        return false;
    if (write)
        return f_write(&disk_file, buf, 512, &n) == FR_OK && n == 512 && f_sync(&disk_file) == FR_OK;
    return f_read(&disk_file, buf, 512, &n) == FR_OK && n == 512;
}

static uint16_t __not_in_flash_func(rgb565)(int r, int g, int b)
{
    return PBC_RGB(r, g, b);
}

/* ---- the in-game menu (Center + A) -----------------------------------------------
 *
 * Pressing any key of the MSX keyboard. In link play both devices stop
 * after the same frame (MSX_WORD_MENU in one of the two words); the player
 * who asked chooses, the other one waits, and both go on swapping words
 * without emulating until the chooser sends DONE. The key goes out with
 * the chooser's next words, so both machines see it in the same frame. */
#define LINK_DONE 0x8000u

static uint16_t link_swap(uint16_t mine)
{
    uint16_t p1, p2;
    pbc_link_frame(mine, NULL, 0, &p1, &p2);
    return pbc_link_player() == 1 ? p2 : p1;
}

static void link_idle(void)
{
    link_swap(0);
}

/* ---- changing disks ---------------------------------------------------------------
 *
 * For a disk game the menu starts with "Insert disk": the drive's MSX disks
 * (CPC disks, which share the extension, left out), and the chosen one goes
 * into drive A: at once (DSKCHG then says "changed"). In link play the
 * chooser sends the hash of the file's name; both look for that file and
 * swap the hash of its whole contents, and only if they match do both
 * change -- the disks are written to, so equal names are not enough. */
#define DISK_LIST_MAX 100
#define LINK_DISK 0x0001u               /* with DONE: a disk change follows */

static bool is_disk_game;
static char disk_now[ROM_NAME_MAX];

static uint32_t fold15(uint32_t h)
{
    uint32_t v = (h ^ h >> 15 ^ h >> 30) & 0x7fff;
    return v ? v : 1;
}

static uint32_t name_hash(const char *name)
{
    return fold15(pbc_link_hash(PBC_LINK_HASH_START, name, strlen(name)));
}

static int disk_list(char (*files)[ROM_NAME_MAX], int max)
{
    static DIR dir;
    static FILINFO fi;
    static FIL f;
    int n = 0;
    if (f_opendir(&dir, "/") != FR_OK)
        return 0;
    while (n < max && f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        if ((fi.fattrib & (AM_DIR | AM_HID | AM_SYS)) || fi.fname[0] == '.' ||
            !ends_ci(fi.fname, ".dsk") || strlen(fi.fname) >= ROM_NAME_MAX)
            continue;
        char path[ROM_NAME_MAX + 2], head[8];
        UINT got = 0;
        snprintf(path, sizeof path, "/%s", fi.fname);
        if (f_open(&f, path, FA_READ) != FR_OK)
            continue;
        f_read(&f, head, sizeof head, &got);
        f_close(&f);
        if (got == 8 && (memcmp(head, "MV - CPC", 8) == 0 || memcmp(head, "EXTENDED", 8) == 0))
            continue;                   /* an Amstrad CPC disk */
        strcpy(files[n++], fi.fname);
    }
    f_closedir(&dir);
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && strcasecmp(files[j - 1], files[j]) > 0; j--) {
            char t[ROM_NAME_MAX];
            strcpy(t, files[j]);
            strcpy(files[j], files[j - 1]);
            strcpy(files[j - 1], t);
        }
    return n;
}

static uint32_t file_print(const char *name)
{
    static FIL f;
    static uint8_t buf[512];
    char path[ROM_NAME_MAX + 2];
    UINT got;
    snprintf(path, sizeof path, "/%s", name);
    if (f_open(&f, path, FA_READ) != FR_OK)
        return 0;
    uint32_t h = PBC_LINK_HASH_START;
    while (f_read(&f, buf, sizeof buf, &got) == FR_OK && got)
        h = pbc_link_hash(h, buf, got);
    f_close(&f);
    return fold15(h);
}

/* `name`: the chooser's file; NULL on the partner */
static const char *disk_change(const char *name)
{
    static char files[DISK_LIST_MAX][ROM_NAME_MAX];
    static char shown[32];
    const char *pick = name;
    if (pbc_link_active()) {
        uint16_t got = link_swap((uint16_t)(LINK_DONE | (name ? name_hash(name) : 0)));
        if (!name) {
            int n = disk_list(files, DISK_LIST_MAX);
            for (int i = 0; i < n && !pick; i++)
                if (name_hash(files[i]) == (got & 0x7fffu))
                    pick = files[i];
        }
        uint16_t mine = pick ? (uint16_t)file_print(pick) : 0;
        uint16_t other = link_swap((uint16_t)(LINK_DONE | mine)) & 0x7fff;
        if (!pick || !mine || mine != other)
            return "Partner has no such disk";
    }
    char path[ROM_NAME_MAX + 2];
    snprintf(path, sizeof path, "/%s", pick);
    f_close(&disk_file);
    if (f_open(&disk_file, path, FA_READ | FA_WRITE) != FR_OK &&
        f_open(&disk_file, path, FA_READ) != FR_OK)
        return "Cannot open disk";
    msx2_insert_disk(disk_io, (uint32_t)(f_size(&disk_file) / 512));
    strcpy(disk_now, pick);
    snprintf(shown, sizeof shown, "%s", pick);
    char *dot = strrchr(shown, '.');
    if (dot)
        *dot = 0;
    return shown;
}

static const char *disk_menu(void)
{
    static char files[DISK_LIST_MAX][ROM_NAME_MAX];
    static const char *rows[DISK_LIST_MAX];
    int n = disk_list(files, DISK_LIST_MAX), cur = -1;
    for (int i = 0; i < n; i++) {
        rows[i] = files[i];
        if (strcmp(files[i], disk_now) == 0)
            cur = i;
    }
    if (n == 0)
        return NULL;
    int k = menu_pick("Insert disk", rows, n, cur >= 0 ? cur : 0, cur, "in drive",
                      pbc_link_active() ? link_idle : NULL);
    return k >= 0 && k != cur ? files[k] : NULL;
}

/* Center + A: swap the disk (disk games), press a key once, back to the
 * game (also A). */
static const char *menu(void)
{
    static const char *plain[64];
    void (*idle)(void) = pbc_link_active() ? link_idle : NULL;
    enum { R_DISK, R_KEY, R_BACK };
    for (;;) {
        const char *rows[3];
        int what[3], n = 0;
        if (is_disk_game) { rows[n] = "Swap disk"; what[n++] = R_DISK; }
        rows[n] = "Press a key";  what[n++] = R_KEY;
        rows[n] = "Back to game"; what[n++] = R_BACK;
        int r = menu_pick("MSX", rows, n, 0, -1, NULL, idle);
        if (r < 0 || what[r] == R_BACK)
            break;
        if (what[r] == R_DISK) {
            const char *disk = disk_menu();
            if (!disk)
                continue;
            if (pbc_link_active())
                link_swap((uint16_t)(LINK_DONE | LINK_DISK));
            return disk_change(disk);
        }
        for (int i = 0; i < msx_key_count && i < 64; i++)
            plain[i] = msx_keys_menu[i].name;
        int k = menu_pick("Press a key", plain, msx_key_count, 0, -1, NULL, idle);
        if (k >= 0) {
            msx_input_press(k);         /* goes out with the next words */
            if (pbc_link_active())
                link_swap(LINK_DONE);
            return msx_keys_menu[k].name;
        }
    }
    if (pbc_link_active())
        link_swap(LINK_DONE);
    return NULL;
}

/* The player who is not choosing. */
static const char *menu_wait(void)
{
    pbc_loading_message("MSX", "Your partner is", "in the menu", NULL);
    uint16_t w;
    while (!((w = link_swap(0)) & LINK_DONE))
        ;
    if (w & LINK_DISK)                  /* the partner chose a disk: the same one, if here */
        return disk_change(NULL);
    return NULL;
}

/* ---- emulation ------------------------------------------------------------- */

static void __attribute__((noreturn)) emulation_loop(bool show_stats, const char *start_status)
{
    static int16_t samples[1024];
    uint64_t next_frame = time_us_64();
    uint64_t stat_t0 = next_frame, stat_busy_us = 0;
    uint32_t stat_drawn = 0, stat_emulated = 0;
    uint64_t status_until = 0;
    uint32_t sample_acc = 0;
    bool menu_mine = true;

    if (start_status) {
        msx_video_set_status(start_status);
        status_until = time_us_64() + 4000000;
    }
    audio_pwm_set_volume(g_settings.sound ? g_settings.volume : 0);

    for (;;) {
        uint16_t p1, p2;
        pbc_link_frame(msx_input_word(), msx1_ram(), msx_is_msx2() ? 0x20000 : 0x10000, &p1, &p2);
        bool menu_now = ((p1 | p2) & MSX_WORD_MENU) != 0;
        if (menu_now && pbc_link_active())
            menu_mine = ((p1 & MSX_WORD_MENU) != 0) == (pbc_link_player() == 1);
        msx_input_apply(p1 & ~MSX_WORD_MENU, p2 & ~MSX_WORD_MENU);

        uint64_t frame_start = time_us_64();
        /* sound: the frame's share of the sample rate (60 frames per second) */
        sample_acc += AUDIO_RATE;
        unsigned n = sample_acc / 60;
        sample_acc -= n * 60;
        msx1_frame(msx_video_line_begin, msx_video_line_commit, samples, (int)n);
        audio_pwm_push(samples, (int)n);

        if (msx_video_frame_drawn())
            stat_drawn++;
        stat_emulated++;

        if (menu_now) {
            msx_video_pause();
            const char *s = menu_mine ? menu() : menu_wait();
            pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
            msx_video_resume();
            msx_video_set_status(s ? s : "");
            status_until = s ? time_us_64() + 2000000 : 0;
            next_frame = time_us_64();      /* the pause is not to be caught up */
            stat_t0 = next_frame;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
            continue;
        }

        const uint32_t period = 16688;      /* 262 lines */
        next_frame += period;
        uint64_t now = time_us_64();
        stat_busy_us += now - frame_start;
        if (pbc_link_follows())
            next_frame = now;               /* the partner's frames are the clock */
        else if (now < next_frame)
            sleep_until(from_us_since_boot(next_frame));
        else if (now > next_frame + 4 * period)
            next_frame = now;               /* hopelessly behind: stop catching up */

        if (pbc_link_out_of_sync_news())
            msx_video_set_status("LINK OUT OF SYNC");
        now = time_us_64();
        if (pbc_link_out_of_sync())
            continue;
        if (status_until && now >= status_until) {
            status_until = 0;
            msx_video_set_status("");
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
        if (show_stats && !status_until && now - stat_t0 >= 1000000) {
            char s[32];
            snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%%",
                     (unsigned long)stat_drawn,
                     (unsigned long)(stat_emulated * 100 / 60),
                     (unsigned long)(stat_busy_us * 100 / (now - stat_t0)));
            pbc_test_stats((unsigned long)stat_drawn, (unsigned long)(stat_emulated * 100 / 60), (unsigned long)(stat_busy_us * 100 / (now - stat_t0)));
            msx_video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
    }
}

/* ---- crash screen ----------------------------------------------------------
 *
 * There is no serial console on this board, so a fault is shown on the panel:
 * the faulting PC and LR go into arm-none-eabi-addr2line -e build-multi/pbc_msx.elf.
 * A stack-guard hit (MPU) arrives here too. */
void __attribute__((used, noreturn)) hardfault_report(uint32_t *frame, uint32_t exc_return)
{
    (void)exc_return;
    char l1[32], l2[32], l3[32];
    snprintf(l1, sizeof l1, "Core %u  PC %08lx", get_core_num(), (unsigned long)frame[6]);
    snprintf(l2, sizeof l2, "LR %08lx", (unsigned long)frame[5]);
    snprintf(l3, sizeof l3, "CFSR %08lx", (unsigned long)(*(volatile uint32_t *)0xE000ED28));
    if (get_core_num() == 0)
        multicore_reset_core1();        /* the panel is ours now */
    pbc_display_end();
    menu_message("CRASH", l1, l2, l3);
    for (;;) tight_loop_contents();
}

void __attribute__((naked)) isr_hardfault(void)
{
    __asm volatile(
        "tst lr, #4      \n"
        "ite eq          \n"
        "mrseq r0, msp   \n"
        "mrsne r0, psp   \n"
        "mov r1, lr      \n"
        "b hardfault_report \n");
}

/* ---- main ---------------------------------------------------------------- */

/* Back to the launcher, once every button is released. */
static void __attribute__((noreturn)) reboot_to_menu(void)
{
    menu_wait_release();
    watchdog_reboot(0, 0, 10);
    for (;;) tight_loop_contents();
}

static void __attribute__((noreturn)) fatal(const char *what, const char *detail, const char *l3)
{
    menu_message("MSX", what, detail, l3 ? l3 : "Press a button");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    reboot_to_menu();
}

/* All that makes the game start differently on two devices. */
/* Cartridges that exist only for the MSX2: the module tells nothing about
 * it, so the file name decides (a dump's name always carries the title).
 * The rest are MSX1 games, which the MSX2 would run too, but slower. */
static bool msx2_title(const char *name)
{
    static const char *const titles[] = {
        "metal gear", "space manbow", "vampire killer", "usas", "aleste 2",
        "psycho world", "undeadline", "super laydock", "zanac ex", "golvellius ii",
    };
    char low[ROM_NAME_MAX];
    size_t n = 0;
    for (; name[n] && n < sizeof low - 1; n++)
        low[n] = (char)tolower((unsigned char)name[n]);
    low[n] = 0;
    for (size_t i = 0; i < sizeof titles / sizeof titles[0]; i++)
        if (strstr(low, titles[i]))
            return true;
    return false;
}

static uint32_t link_config(bool msx2, bool disk, const uint8_t *const *main_rom, const uint8_t *const *sub_rom)
{
    uint32_t h = PBC_LINK_HASH_START ^ 7;
    if (disk) {
        static uint8_t buf[512];
        uint32_t n = (uint32_t)(f_size(&disk_file) / 512);
        for (uint32_t s = 0; s < n; s++)
            if (disk_io(false, s, buf))
                h = pbc_link_hash(h, buf, sizeof buf);
    } else {
        h = pbc_link_hash(h, rom_store_data(), rom_store_size());
    }
    if (msx2) {
        for (int p = 0; p < 4; p++)
            h = pbc_link_hash(h, main_rom[p], 0x2000);
        for (int p = 0; p < 2; p++)
            h = pbc_link_hash(h, sub_rom[p], 0x2000);
        if (disk)
            h = pbc_link_hash(h, SYS_ROM(SYS_DISK), 0x4000);
    } else
        for (int p = 0; p < 4; p++)
            h = pbc_link_hash(h, cbios1_pages[p], 0x2000);
    return h;
}

int main(void)
{
    input_init();
    settings_load();
    pbc_display_init();

    /* the launcher has picked the game and owns the USB drive mode */
    const pbc_launch_t *launch = pbc_launch_get();
    if (!launch || !fs_mount())
        reboot_to_menu();
    strncpy(game, launch->game, sizeof game - 1);

    /* .mx2: an MSX2 cartridge, .dsk: an MSX2 disk (C-BIOS for MSX2 or the
     * drive's system ROMs); a .rom with the name of a known MSX2 cartridge
     * too. Everything else runs on C-BIOS for MSX1. */
    bool disk = ends_ci(game, ".dsk");
    is_disk_game = disk;
    snprintf(disk_now, sizeof disk_now, "%s", game);
    bool msx2 = disk || ends_ci(game, ".mx2") || msx2_title(game);
    uint8_t *scratch = malloc(0x8000);
    if (!scratch)
        fatal("Out of memory", "for loading", NULL);
    if (!disk && !rom_store_matches(game)) {
        menu_progress_begin(game);
        const char *err = rom_store_program(game, scratch, menu_progress);
        if (err)
            fatal(err, game, NULL);
    }
    static const uint8_t *const own_pages[4] = MSX_PAGES(SYS_ROM(SYS_MAIN));
    static const uint8_t *const own_sub_pages[2] = { SYS_ROM(SYS_SUB), SYS_ROM(SYS_SUB + 0x2000) };
    const uint8_t *const *main_rom = cbios1_pages, *const *sub_rom = NULL;
    const uint8_t *logo_rom = NULL, *disk_rom = NULL;
    bool own = false;                   /* the drive's system ROMs */
    if (msx2) {
        own = sys_store("msx2.rom", NULL, 0x8000, SYS_MAIN, false, scratch) &&
              sys_store("msx2ext.rom", NULL, 0x4000, SYS_SUB, false, scratch) &&
              (!disk || sys_store("disk.rom", NULL, 0x4000, SYS_DISK, true, scratch));
        if (own) {
            main_rom = own_pages;
            sub_rom = own_sub_pages;
        } else {
            main_rom = cbios2_pages;
            logo_rom = cbios2_logo;
            sub_rom = cbios2_sub_pages;
            if (disk) {                 /* C-DISK without its FFh filling (cbios.S) */
                memset(scratch, 0xff, 0x4000);
                memcpy(scratch, cbios_disk, 0x500);
                memcpy(scratch + 0x3f00, cbios_disk_tail, 0x100);
                sys_store(NULL, scratch, 0x4000, SYS_DISK, true, scratch);
            }
        }
        if (disk)
            disk_rom = SYS_ROM(SYS_DISK);
    }
    free(scratch);
    if (disk) {
        /* the drive stays mounted: the game reads and writes its disk */
        if (f_open(&disk_file, game, FA_READ | FA_WRITE) != FR_OK &&
            f_open(&disk_file, game, FA_READ) != FR_OK)
            fatal("Cannot open", game, NULL);
        static uint8_t boot[512];
        if (disk_io(false, 0, boot) && (memcmp(boot, "MV - CPC", 8) == 0 || memcmp(boot, "EXTENDED", 8) == 0))
            fatal("Not an MSX disk:", "this is an Amstrad CPC disk", NULL);
        if (!own && (!disk_io(false, 0, boot) || boot_needs_dos(boot)))
            fatal("Disk needs MSX-DOS/BASIC:", "copy msx2.rom msx2ext.rom", "disk.rom to the drive");
    } else {
        f_unmount("");
    }

    overclock();
    audio_pwm_init();
    msx_video_init(pbc_launch_video());

    int mapper = 0;
    if (msx2) {
        v99_rgb565 = rgb565;
        msx2_init(main_rom, logo_rom, sub_rom, disk_rom);
        msx_line565_begin = msx_video_line565_begin;
        msx_line565_commit = msx_video_line565_commit;
    } else {
        msx1_init(cbios1_pages, cbios_logo);
    }
    if (disk) {
        msx2_insert_disk(disk_io, (uint32_t)(f_size(&disk_file) / 512));
    } else {
        mapper = msx1_insert(rom_store_data(), rom_store_size());
        if (mapper < 0)
            fatal("Not an MSX cartridge", game, NULL);
    }
    msx1_reset();

    pbc_link_start(link_config(msx2, disk, main_rom, sub_rom));
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    multicore_launch_core1(msx_video_core1_main);
    pbc_fade_in(false);                 /* over the first frames */
    emulation_loop(pbc_launch_stats(), mapper > 0 ? msx1_mapper_name(mapper) : msx2 ? (own ? "MSX2" : "MSX2 C-BIOS") : NULL);
}
