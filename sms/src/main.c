/*
 * PBC SMS/GG -- Sega Master System and Game Gear emulator for the
 * PicoBoy Color Plus (RP2350A, 520 KB SRAM, 16 MB flash, ST7789 240x280).
 *
 * Emulator core: SMS Plus (Charles MacDonald, GPL-2.0+) as ported in
 * pico-smsplus by the PicoPlus developers (GPL-3.0). The platform layer is
 * the one from nes and c64:
 *   - ROMs come over USB mass storage into a flash FatFs partition
 *     (from PicoBoyGB), the chosen game is copied into a flash region and
 *     SMS Plus maps its pages straight onto it
 *   - Master System 240 wide 1:1, Game Gear scaled by 1.5 to 240x216 (other
 *     display modes from the launcher), sent by core 1 (sms_video.c)
 *   - sound on the PWM speaker (audio_pwm.c), its sample timer on core 1;
 *     on/off and volume only in the options of the ROM list
 *   - battery-backed cartridge RAM (Master System, Game Gear) is kept as a
 *     .sav file next to the ROM, as in ../gbc
 *
 * Boot:
 *   A or B held  -> USB stick mode, eject restarts
 *   otherwise    -> ROM list, then the game (RESET leaves it)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "hardware/structs/qmi.h"

#include "ff.h"
#include "tusb.h"

#include "board.h"
#include "pbc_launch.h"
#include "color565.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#if PBC_MULTI
#include "pbc_loading.h"
#endif
#include "rom_store.h"
#include "sms_video.h"
#include "audio_pwm.h"
#include "settings.h"
#include "shared.h"           /* SMS Plus */
#include "pbc_link.h"

/* ---- build-time configuration (see CMakeLists.txt) ---------------------- */
#ifndef CPU_MHZ
#define CPU_MHZ 150
#endif
#ifndef CPU_VREG
#define CPU_VREG VREG_VOLTAGE_1_10
#endif
#ifndef FLASH_MAX_FREQ_MHZ
#define FLASH_MAX_FREQ_MHZ 88
#endif
#ifndef SHOW_FPS
#define SHOW_FPS 0
#endif

/* ---- FatFs volume -------------------------------------------------------- */
static FATFS g_fs;
extern volatile bool usb_msc_ejected;

static bool fs_mount(void)   { return f_mount(&g_fs, "", 1) == FR_OK; }
static void fs_unmount(void) { f_mount(NULL, "", 0); }

static bool fs_format(void)
{
    static BYTE work[FF_MAX_SS];
    /* FM_SFD: no partition table, one FAT volume filling the region. */
    MKFS_PARM opt = { FM_FAT | FM_SFD, 0, 0, 0, 0 };
    if (f_mkfs("", &opt, work, sizeof work) != FR_OK)
        return false;
    if (fs_mount()) {
        f_setlabel("PBC-ROMS");
        fs_unmount();
    }
    return true;
}

static void fs_write_readme(void)
{
    /* static: a FIL carries a 4 KB sector buffer, the stack is only 4 KB */
    static FILINFO fi;
    static FIL f;
    if (f_stat("/SMS.TXT", &fi) == FR_OK)
        return;
    if (f_open(&f, "/SMS.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return;
    f_puts("PBC SMS/GG - Sega Master System and Game Gear emulator\r\n"
           "for the PicoBoy Color Plus\r\n"
           "\r\n"
           "Copy Master System (.sms) or Game Gear (.gg) ROMs to this drive,\r\n"
           "then EJECT it. The PicoBoy restarts by itself and shows the game list.\r\n"
           "\r\n"
           "Back to this mode: \"Add games via USB\" at the top of the game list,\r\n"
           "or hold A or B while switching on.\r\n"
           "\r\n"
           "Controls in game:\r\n"
           "  Joystick       D-pad\r\n"
           "  B              button 1\r\n"
           "  A              button 2\r\n"
           "  Center + B     START (Game Gear) / PAUSE (Master System)\r\n"
           "\r\n"
           "Back to the game list: RESET.\r\n", &f);
    f_close(&f);
}

/* The PBC emulators share one drive. Whichever formatted it first used to
 * name it after itself; give it the common name in either case. */
static void fs_ensure_label(void)
{
    char label[24];
    if (f_getlabel("", label, NULL) == FR_OK && strcmp(label, "PBC-ROMS") != 0)
        f_setlabel("PBC-ROMS");
}

/* One diagnostic line per step: the panel is the only console. */
static int diag_y;

static void diag(const char *s, uint16_t colour)
{
    pbc_display_text(8, diag_y, s, colour, PBC_BLACK, 1);
    diag_y += 11;
}

static void __attribute__((noreturn)) usb_stick_mode(void)
{
    menu_message("USB DRIVE MODE",
                 "Copy .sms/.gg ROMs to the",
                 "drive, then eject it.",
                 "The PicoBoy restarts.");
    diag_y = 168;

    diag("Checking drive...", PBC_GREY);
    FRESULT fr = f_mount(&g_fs, "", 1);
    if (fr != FR_OK) {
        char m[32];
        snprintf(m, sizeof m, "empty (%d), formatting...", (int)fr);
        diag(m, PBC_YELLOW);
        if (fs_format() && fs_mount())
            fr = FR_OK;
    }
    if (fr != FR_OK) {
        diag("ERROR: drive unusable", PBC_RED);
        for (;;) tight_loop_contents();
    }
    fs_ensure_label();
    fs_write_readme();
    fs_unmount();               /* the host owns the medium from now on */

    tud_init(BOARD_TUD_RHPORT);
    diag("USB started", PBC_GREY);

    int status_y = diag_y;
    uint64_t next_draw = 0;
    for (;;) {
        tud_task();
        if (time_us_64() >= next_draw) {
            next_draw = time_us_64() + 500000;
            int state = tud_mounted() ? 2 : tud_connected() ? 1 : 0;
            static const char *const names[] = {
                "PC: not connected  ", "PC: connected      ", "PC: drive mounted  " };
            pbc_display_text(8, status_y, names[state],
                             state == 2 ? PBC_YELLOW : PBC_WHITE, PBC_BLACK, 1);
        }
        if (usb_msc_ejected) {
            uint64_t t = time_us_64() + 300000;
            while (time_us_64() < t)
                tud_task();     /* let the host see the eject complete */
            watchdog_reboot(0, 0, 10);
            for (;;) tight_loop_contents();
        }
    }
}

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

/* Raised only after all flash writing is done. */
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
    /* clk_peri follows clk_sys: re-derive the SPI divider. */
    spi_set_baudrate(PBC_SPI, PBC_SPI_HZ);
}

/* ---- emulator ------------------------------------------------------------ */

/* SMS Plus takes its memory through these (system.c). */
void *frens_f_malloc(size_t size) { return malloc(size); }
void frens_f_free(void *ptr)      { free(ptr); }

/* Called on every reset; the .sav file is read after it (save_load). */
void system_load_sram(void)
{
    if (sms.sram)
        memset(sms.sram, 0, SRAM_BYTES);
}

/* ---- battery saves ---------------------------------------------------------
 *
 * Master System and Game Gear cartridges with a battery map up to 32 KB of
 * RAM (sms.sram) over $8000-$BFFF; SMS Plus sets sms.save once a game does.
 * It goes to "<rom>.sav" on the drive: 16 KB, or 32 KB once the game used
 * the upper half. The game is left with RESET, so it is written while the
 * game runs, as in ../gbc: a checksum twice a second notices a change; the
 * file is written once the RAM has stayed the same for a second (or has
 * been changing for 20 s). Unchanged flash sectors are skipped
 * (flash_fs.c); core 1 is parked for each changed one, the game stalls for
 * about 50 ms per 4 KB. Not in link play: the stall would lose the partner. */
static char save_path[ROM_NAME_MAX + 8];
static bool save_on;                    /* Master System / Game Gear */
static uint32_t save_len;               /* bytes in the file, 0: none yet */
static uint32_t save_sum, written_sum;
static bool save_dirty;
static uint64_t save_first_change, save_last_change, save_next_check, status_until;

/* checksum of the first len bytes of the RAM, or of as many zeros */
static uint32_t sram_sum(uint32_t len, bool zeros)
{
    const uint32_t *w = (const uint32_t *)sms.sram;
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < len / 4; i++)
        h = (h ^ (zeros ? 0 : w[i])) * 16777619u;
    return h;
}

static void save_load(const char *game)
{
    static FIL f;
    snprintf(save_path, sizeof save_path, "/%s", game);
    char *dot = strrchr(save_path, '.');
    if (dot) *dot = 0;
    strcat(save_path, ".sav");
    save_on = true;
    if (f_open(&f, save_path, FA_READ) != FR_OK)
        return;
    UINT got = 0;
    uint32_t n = f_size(&f) < SRAMSIZEBYTES ? f_size(&f) : SRAMSIZEBYTES;
    f_read(&f, sms.sram, n, &got);
    f_close(&f);
    save_len = got > 0x4000 ? SRAMSIZEBYTES : 0x4000;
    written_sum = save_sum = sram_sum(save_len, false);
}

static void save_write(void)
{
    static FIL f;
    UINT put = 0;
    save_dirty = false;
    written_sum = save_sum;
    if (f_open(&f, save_path, FA_WRITE | FA_OPEN_ALWAYS) == FR_OK) {
        f_write(&f, sms.sram, save_len, &put);
        f_truncate(&f);
        f_close(&f);
    }
    if (put != save_len) {
        sms_video_set_status("SAVE FAILED: DRIVE FULL?");
        status_until = time_us_64() + 4000000;
    }
}

static void save_poll(void)
{
    if (!save_on || !sms.save || pbc_link_active())
        return;
    uint64_t now = time_us_64();
    if (now < save_next_check)
        return;
    save_next_check = now + 500000;
    if (status_until && now > status_until) {
        status_until = 0;
        sms_video_set_status("");
    }
    /* sms.save bit 1: the game used the upper 16 KB (sms.c) */
    uint32_t len = (sms.save & 2) || save_len > 0x4000 ? SRAMSIZEBYTES : 0x4000;
    bool grown = save_len && len != save_len;
    if (len != save_len) {
        /* no file yet counts as zeros; a bigger file is written at once */
        written_sum = grown ? ~sram_sum(len, false) : sram_sum(len, true);
        save_len = len;
    }
    uint32_t sum = sram_sum(save_len, false);
    if (sum != save_sum) {
        save_sum = sum;
        save_last_change = now;
        if (!save_dirty)
            save_first_change = now;
    }
    save_dirty = sum != written_sum;
    if (grown)
        save_first_change = 0;
    if (save_dirty && (now - save_last_change >= 1000000 || now - save_first_change >= 20000000))
        save_write();
}

/* One-pole DC blocker, from pico-smsplus: the PSG alone outputs 0..0x7FFF,
 * PSG plus FM a signed mix; either way the offset has to go before the
 * samples reach the speaker. Mono: left and right are averaged. */
static int16_t dc_block(int32_t x)
{
    static int32_t prev, dc;
    int32_t y = x - prev + dc - (dc >> 9);
    prev = x;
    dc = y;
    if (y > 32767) y = 32767;
    if (y < -32768) y = -32768;
    return (int16_t)y;
}

/* Bit 8 of a player's word: the start button; bits 9-12 the ColecoVision
 * keypad key (coleco_key) */
#define LINK_START 0x100
#define LINK_KEY_SHIFT 9

static bool coleco;

/* ColecoVision keypad (index as coleco_key: 1-9, 10 '0', 11 '*', 12 '#'):
 *   Center + Up/Right/Down/Left       1 2 3 4
 *   Center + B + Up/Right/Down/Left   5 6 7 8
 *   Center + A + Up/Right/Down/Left   9 0 * #
 * A key counts once the buttons have stayed the same for three frames, so
 * the buttons of a chord pressed one after the other give no other key on
 * the way, and it is held for half a second after letting go (below). */
static int coleco_keypad(void)
{
    static uint32_t last;
    static int same;
    uint32_t r = input_raw() & (IN_CENTER | IN_A | IN_B | IN_UP | IN_RIGHT | IN_DOWN | IN_LEFT);
    if (r != last) {
        last = r;
        same = 0;
    } else if (same < 3) {
        same++;
    }
    /* A key stays down for half a second after the buttons let go: some
     * games look at the keypad only now and then -- Donkey Kong after the
     * last life about every third of a second, so a short * or # was
     * missed. A new key takes over at once. */
    static int held, held_frames;
    int k = 0;
    if ((r & IN_CENTER) && same >= 2) {
        int d = (r & IN_UP) ? 0 : (r & IN_RIGHT) ? 1 : (r & IN_DOWN) ? 2 : (r & IN_LEFT) ? 3 : -1;
        if (d >= 0)
            k = (r & IN_A) ? 9 + d : (r & IN_B) ? 5 + d : 1 + d;
    }
    if (k) {
        held = k;
        held_frames = 30;
    } else if (held_frames > 0) {
        held_frames--;
        k = held;
    }
    return k;
}

static uint16_t link_word(void)
{
    bool start;
    int pad = input_poll_sms(&start);
    if (coleco) {
        int k = coleco_keypad();
        if (input_raw() & IN_CENTER)
            pad &= ~0x0f;               /* Center + a direction is a key, not a move */
        return (uint16_t)(pad | k << LINK_KEY_SHIFT);
    }
    return (uint16_t)(pad | (start ? LINK_START : 0));
}

/* The ColecoVision's BIOS: coleco.rom from the drive, or a small one of our
 * own that starts the cartridge at the address in its header and sends the
 * NMI and RST vectors to the jumps there, as the real one does -- enough for
 * games that do not call the BIOS's routines. The rest of it is HALTs: a
 * game that calls a routine stops there, and coleco_bios_check says so. */
static uint8_t coleco_bios_buf[0x2000];

static bool coleco_bios_load(void)
{
    static FIL f;                       /* a FIL carries a 4 KB buffer */
    UINT got = 0;
    if (f_open(&f, "/coleco.rom", FA_READ) == FR_OK) {
        bool ok = f_size(&f) == sizeof coleco_bios_buf &&
                  f_read(&f, coleco_bios_buf, sizeof coleco_bios_buf, &got) == FR_OK &&
                  got == sizeof coleco_bios_buf;
        f_close(&f);
        if (ok)
            return true;
    }
    memset(coleco_bios_buf, 0x76, sizeof coleco_bios_buf);    /* HALT */
    static const uint8_t start[] = {
        0xf3,                           /* DI */
        0x31, 0x00, 0x74,               /* LD SP,7400h */
        0x2a, 0x0a, 0x80,               /* LD HL,(800Ah): the game's start */
        0xe9,                           /* JP (HL) */
    };
    memcpy(coleco_bios_buf, start, sizeof start);
    for (int v = 1; v < 8; v++) {       /* RST 08h-38h -> 800Ch, 800Fh, ... 801Eh */
        coleco_bios_buf[v * 8] = 0xc3;
        coleco_bios_buf[v * 8 + 1] = (uint8_t)(0x09 + 3 * v);
        coleco_bios_buf[v * 8 + 2] = 0x80;
    }
    coleco_bios_buf[0x66] = 0xc3;       /* NMI -> 8021h */
    coleco_bios_buf[0x67] = 0x21;
    coleco_bios_buf[0x68] = 0x80;
    return false;
}

/* Our BIOS only: the CPU stuck in its HALTs (past the vectors, BIOS paged
 * in) means the game wanted a routine of the real one. Once per game. */
static void coleco_bios_check(void)
{
    static bool told;
    unsigned pc = z80_get_pc();
    if (told || pc < 0x69 || pc >= 0x2000 || cpu_readmap[0] != coleco_bios_buf)
        return;
    told = true;
    sms_video_set_status("GAME NEEDS COLECO.ROM");
}

static void __attribute__((noreturn)) emulation_loop(bool gg, bool show_stats, bool own_bios)
{
    /* 262 lines of 228 Z80 cycles at 3.579545 MHz: 59.92 Hz */
    const uint32_t period = 16688;
    uint64_t next_frame = time_us_64();

    audio_pwm_set_volume(g_settings.sound ? g_settings.volume : 0);

    uint64_t stat_t0 = time_us_64(), stat_busy_us = 0;
    uint32_t stat_drawn = 0, stat_emulated = 0;

    for (;;) {
        uint16_t p1, p2;
        pbc_link_frame(link_word(), sms.ram, RAMSIZEBYTES, &p1, &p2);
        input.pad[0] = p1 & 0xFF;
        input.pad[1] = p2 & 0xFF;
        input.system = (p1 | p2) & LINK_START ? (gg ? INPUT_START : INPUT_PAUSE) : 0;
        coleco_key[0] = (uint8_t)((p1 >> LINK_KEY_SHIFT) & 15);
        coleco_key[1] = (uint8_t)((p2 >> LINK_KEY_SHIFT) & 15);

        uint64_t frame_start = time_us_64();
        sms_frame(0);
        if (coleco && !own_bios)
            coleco_bios_check();

        static int16_t samples[1024];
        int n = snd.bufsize < 1024 ? snd.bufsize : 1024;
        for (int i = 0; i < n; i++)
            samples[i] = dc_block(((int32_t)snd.buffer[0][i] + snd.buffer[1][i]) / 2);
        audio_pwm_push(samples, n);

        if (sms_video_present())
            stat_drawn++;
        stat_emulated++;
        save_poll();

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
            sms_video_set_status("LINK OUT OF SYNC");
        if (!show_stats || pbc_link_out_of_sync())
            continue;
        now = time_us_64();
        if (now - stat_t0 >= 1000000) {
            char s[32];
            snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%%",
                     (unsigned long)stat_drawn,
                     (unsigned long)(stat_emulated * 100 / 60),
                     (unsigned long)(stat_busy_us * 100 / (now - stat_t0)));
            pbc_test_stats((unsigned long)stat_drawn, (unsigned long)(stat_emulated * 100 / 60), (unsigned long)(stat_busy_us * 100 / (now - stat_t0)));
            sms_video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
    }
}

/* ---- crash screen ----------------------------------------------------------
 *
 * There is no serial console on this board, so a fault is shown on the panel:
 * the faulting PC and LR go into arm-none-eabi-addr2line -e build/pbc_sms.elf.
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

/* Restart into the ROM list. Waits for every button to be released first:
 * A or B still held at boot would land in USB stick mode instead. */
static void __attribute__((noreturn)) reboot_to_menu(void)
{
    menu_wait_release();
    watchdog_reboot(0, 0, 10);
    for (;;) tight_loop_contents();
}

static void __attribute__((noreturn)) fatal(const char *what, const char *detail)
{
    menu_message("ERROR", what, detail, "Press a button");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    reboot_to_menu();
}

int main(void)
{
    input_init();
    settings_load();
    pbc_display_init();
#if !PBC_MULTI
    pbc_display_backlight(200);     /* combined firmware: pbc_fade_in() */
#endif

#if PBC_MULTI
    /* Combined firmware: the launcher has picked the game and owns the
     * USB drive mode and the game list. */
    static char rom[ROM_NAME_MAX];
    const pbc_launch_t *launch = pbc_launch_get();
    if (!launch || !fs_mount())
        reboot_to_menu();
    strncpy(rom, launch->game, sizeof rom - 1);
    input_set_swap(launch->key & 1);    /* the game's options: buttons swapped */
#else
    if (input_raw() & (IN_A | IN_B))
        usb_stick_mode();

    if (!fs_mount())
        usb_stick_mode();           /* fresh device: nothing on the stick yet */

    static char rom[ROM_NAME_MAX];
    if (!menu_pick_rom(rom, sizeof rom)) {
        fs_unmount();
        usb_stick_mode();
    }
#endif

    if (!rom_store_matches(rom)) {
        /* Heap, freed again before the emulator allocates its memory. */
        uint8_t *scratch = malloc(16384);
        if (!scratch)
            fatal("Out of memory", "for loading");
        menu_progress_begin(rom);
        const char *err = rom_store_program(rom, scratch, menu_progress);
        free(scratch);
        if (err)
            fatal(err, rom);
    }
    const char *ext = strrchr(rom, '.');
    bool gg = ext && strcasecmp(ext, ".gg") == 0;
    bool sg = ext && strcasecmp(ext, ".sg") == 0;     /* SG-1000 */
    /* ColecoVision: .col, or .rom/.bin, which the launcher gives this
     * emulator only for the ColecoVision (by their AA 55 / 55 AA header) */
    coleco = ext && (strcasecmp(ext, ".col") == 0 || strcasecmp(ext, ".rom") == 0 ||
                     strcasecmp(ext, ".bin") == 0);
    bool own_bios = coleco && coleco_bios_load();
    bool saves = !coleco && !sg;          /* the drive stays mounted for them */
    if (!saves)
        fs_unmount();

    const menu_control_t controls[] = {
        { "D-pad",   "Joystick" },
        { "1",       "Button B" },
        { "2",       "Button A" },
        { gg ? "START" : "PAUSE", "Center + B" },
    };
#if PBC_MULTI
    (void)controls;                 /* combined firmware: the launcher showed them */
#else
    menu_controls(controls, sizeof controls / sizeof controls[0]);
#endif

    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    menu_wait_release();

    overclock();
    audio_pwm_init();

#if PBC_MULTI
    /* combined firmware: display mode and status line from the launcher */
    unsigned video_mode = pbc_launch_video();
    bool show_stats = pbc_launch_stats();
#else
    unsigned video_mode = 0;
    bool show_stats = SHOW_FPS;
#endif
    sms_video_set_mode(gg, video_mode);
    coleco_bios = coleco_bios_buf;
    load_rom((uintptr_t)rom_store_data(), (int)rom_store_size(),
             gg ? TYPE_GG : sg ? TYPE_SG : coleco ? TYPE_COLECO : TYPE_SMS);
    system_init(AUDIO_RATE);
    if (!snd.enabled || (!cachePtr && !IS_TMS_SYS) || !sms.ram)
        fatal("Out of memory", "for the emulator");
    system_reset();
    /* SG-1000/ColecoVision: the cartridge (48/32 KB at most) into the heap,
     * which these systems leave mostly free (no tile cache) */
    sms_cart_to_ram(16 * 1024);

    pbc_link_start(pbc_link_hash(PBC_LINK_HASH_START ^ 2, rom_store_data(), rom_store_size()));
    if (saves && !pbc_link_active())    /* link play: same RAM on both, no saves */
        save_load(rom);
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    multicore_launch_core1(sms_video_core1_main);
    sms_video_wait_ready();         /* the sample timer belongs to core 1 */
#if PBC_MULTI
    pbc_fade_in(false);             /* over the first frames */
#endif
    emulation_loop(gg, show_stats, own_bios);
}
