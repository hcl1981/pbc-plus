/*
 * PBCemu -- one firmware with every emulator for the PicoBoy Color Plus.
 *
 * Sits at the start of the flash, the emulators behind it (build.sh packs
 * them; common/pbc_launch.h has the flash map). The launcher shows the
 * devices and their games; when one is chosen it writes the launch record,
 * puts the emulator's address into the watchdog scratch registers and
 * restarts. Straight after that restart, before touching anything else, it
 * jumps into the emulator, which starts as if it were alone on the chip.
 * Any later restart -- RESET, "back to the list", power -- lands here again.
 *
 *   A or B held at power-on  -> USB drive mode
 */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "pico/unique_id.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#ifdef PBC_AUTOTEST
#include "pico/bootrom.h"
#endif
#include "hardware/structs/watchdog.h"
#include "hardware/structs/powman.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/systick.h"

#include "ff.h"
#include "tusb.h"

#include "display.h"
#include "input.h"
#include "gfx.h"
#include "ui.h"
#include "devices.h"
#include "games.h"
#include "prefs.h"
#include "favs.h"
#include "battery.h"
#include "sound.h"
#include "pbc_launch.h"
#include "pbc_lobby.h"
#include "pbc_rgb.h"
#include "c64/settings.h"

static FATFS g_fs;
extern volatile bool usb_msc_ejected;
void usb_msc_flush(void);

static bool fs_mount(void)   { return f_mount(&g_fs, "", 1) == FR_OK; }
static void fs_unmount(void) { f_mount(NULL, "", 0); }

/* ---- into an emulator ------------------------------------------------------- */

/* Plausible vector table at `offset`: stack in SRAM, reset handler right
 * behind the vector table -- an image linked to another address fails. */
static bool app_valid(uint32_t offset)
{
    const uint32_t *vt = (const uint32_t *)(XIP_BASE + offset);
    return offset && vt[0] > SRAM_BASE && vt[0] <= SRAM_END &&
           vt[1] > XIP_BASE + offset && vt[1] < XIP_BASE + offset + 0x400u;
}

/* Called first thing after the restart: interrupts off and cleared, vector
 * table, stack and stack limit of the emulator, go. Its own runtime init
 * resets the peripherals and sets up the clocks again. */
static void __attribute__((noreturn)) jump_to(uint32_t offset)
{
    const uint32_t *vt = (const uint32_t *)(XIP_BASE + offset);
    __asm volatile("cpsid i");
    systick_hw->csr = 0;
    volatile uint32_t *icer = (volatile uint32_t *)0xE000E180u;   /* NVIC_ICER0.. */
    volatile uint32_t *icpr = (volatile uint32_t *)0xE000E280u;   /* NVIC_ICPR0.. */
    for (int i = 0; i < 16; i++) {
        icer[i] = 0xFFFFFFFFu;
        icpr[i] = 0xFFFFFFFFu;
    }
    scb_hw->vtor = (uint32_t)vt;
    __asm volatile(
        "msr msplim, %2 \n"
        "msr msp, %0    \n"
        "cpsie i        \n"
        "bx %1          \n"
        :: "r"(vt[0]), "r"(vt[1]), "r"(0u));
    __builtin_unreachable();
}

static void write_launch(int device, const char *game)
{
    static uint8_t page[FLASH_PAGE_SIZE] __aligned(4);
    pbc_launch_t l;
    memset(&l, 0, sizeof l);
    l.magic = PBC_LAUNCH_MAGIC;
    l.version = PBC_LAUNCH_VERSION;
    l.device = (uint8_t)device;
    l.sound = g_prefs.sound;
    l.volume = (uint8_t)g_prefs.volume;
    l.flags = g_prefs.stats ? PBC_LAUNCH_STATS : 0;
    if (ui_link_player)
        l.flags |= (ui_link_player == 1 ? PBC_LAUNCH_LINK1 : PBC_LAUNCH_LINK2) |
                   (ui_link_master ? PBC_LAUNCH_LINK_MASTER : 0);
    l.video = (uint8_t)prefs_video(device);
    if (prefs_scanlines(device))
        l.flags |= PBC_LAUNCH_SCANLINES;
    l.brightness = prefs_backlight();
#ifdef PBC_AUTOTEST
    l.flags |= PBC_LAUNCH_STATS;        /* the test photos show the speed */
#endif
    if (devices[device].app != APP_C64) {
        /* the game's own options byte (pbc_launch.h); the C64 reads its
         * settings itself. In link play CPC, ZX and Atari ignore it. */
        settings_load();
        l.key = (uint8_t)settings_key_raw(game);
        if (devices[device].app == APP_ZX)  /* a tape game's machine (ui.c) */
            l.opt2 = settings_true_drive(game) ? 1 : 0;
    }
    snprintf(l.game, sizeof l.game, "%s", game);
    l.check = pbc_launch_hash(&l);
    if (memcmp((const void *)(XIP_BASE + PBC_LAUNCH_OFFSET), &l, sizeof l) == 0)
        return;                         /* same game again: spare the flash */
    memset(page, 0xFF, sizeof page);
    memcpy(page, &l, sizeof l);
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(PBC_LAUNCH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(PBC_LAUNCH_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
}

static void __attribute__((noreturn)) launch(uint32_t offset)
{
    pbc_rgb_set(0, 0, 0);               /* it would keep its colour in the game */
    sound_off();
    /* fade out; the emulator fades in again (common/pbc_loading.c), which
     * hides the panel's reset and clearing in between */
    for (int i = 24; i >= 0; i--) {
        pbc_display_backlight((uint8_t)(prefs_backlight() * i / 25));
        sleep_ms(10);
    }
    watchdog_hw->scratch[0] = PBC_JUMP_MAGIC;
    watchdog_hw->scratch[1] = offset;
    watchdog_reboot(0, 0, 10);
    for (;;) tight_loop_contents();
}

/* ---- USB drive mode ----------------------------------------------------------- */

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
    if (f_stat("/PBC.TXT", &fi) == FR_OK)
        return;
    if (f_open(&f, "/PBC.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return;
    f_puts("PBCemu - game systems for the PicoBoy Color Plus\r\n"
           "\r\n"
           "Copy games to this drive, then EJECT it. The PicoBoy restarts\r\n"
           "by itself; pick the system, then the game.\r\n"
           "\r\n"
           "  Atari 2600       .a26\r\n"
           "  Atari 7800       .a78\r\n"
           "  ColecoVision     .col .rom .bin  (coleco.rom here: the BIOS)\r\n"
           "  ZX Spectrum      .tap .tzx .z80 .sna\r\n"
           "  Commodore 64     .d64 .t64 .prg .crt\r\n"
           "  Vectrex          .vec\r\n"
           "  NES              .nes   (Game Genie codes: <game>.cht, one per line)\r\n"
           "  SG-1000          .sg\r\n"
           "  MSX              .rom .mx1   (MSX1 cartridges)\r\n"
           "                   .mx2 .dsk   (MSX2 cartridges and disks)\r\n"
           "  Amstrad CPC      .dsk .cdt .sna\r\n"
           "  Master System    .sms\r\n"
           "  PC Engine        .pce   (HuCards)\r\n"
           "  Mega Drive       .md .gen .smd .bin\r\n"
           "  Game Gear        .gg\r\n"
           "  Game Boy (Color) .gb .gbc\r\n"
           "  Arduboy          .hex   (Arduboy FX games not supported)\r\n"
           "\r\n"
           "Commodore 64: the free MEGA65 OpenROMs are built in. If you own\r\n"
           "the Commodore ROMs, copy kernal.rom and basic.rom (optionally\r\n"
           "chargen.rom) here; some games only run with the original KERNAL.\r\n"
           "\r\n"
           "MSX: the free C-BIOS is built in; it starts cartridges and\r\n"
           "disks that boot by themselves. BASIC and MSX-DOS disks need\r\n"
           "the system ROMs here: msx2.rom (32 KB), msx2ext.rom (16 KB)\r\n"
           "and disk.rom (16 KB).\r\n"
           "\r\n"
           "Back to this mode: \"Add Games\" (last page of the systems), or\r\n"
           "hold A or B while switching on.\r\n"
           "Back to the game list from a game: RESET.\r\n", &f);
    f_close(&f);
}

static void __attribute__((noreturn)) usb_mode(void)
{
    sound_off();
    ui_usb(0);
    FRESULT fr = f_mount(&g_fs, "", 1);
    if (fr != FR_OK && fs_format() && fs_mount())
        fr = FR_OK;
    if (fr != FR_OK) {
        ui_message("Drive unusable", "The flash drive could not", "be formatted.", NULL);
        watchdog_reboot(0, 0, 10);
        for (;;) tight_loop_contents();
    }
    char label[24];
    if (f_getlabel("", label, NULL) == FR_OK && strcmp(label, "PBC-ROMS") != 0)
        f_setlabel("PBC-ROMS");
    fs_write_readme();
    fs_unmount();                       /* the host owns the medium from now on */

    /* Back to the menu when the host ejects the drive -- or, once it had
     * the drive, when it lets go without saying so: it unconfigures the
     * device, switches the port off ("safely remove" on Linux does that
     * instead of an eject) or the cable comes out, and the bus stays
     * quiet. Every write is in flash already, so nothing is lost. */
    tud_init(BOARD_TUD_RHPORT);
    int shown = 0, seen = 0;            /* ui_usb(0) is on the panel */
    uint64_t seen_since = 0;
    bool had_host = false;
    uint64_t gone_since = 0;
    for (;;) {
        tud_task();
        int state = tud_mounted() ? 2 : tud_connected() ? 1 : 0;
        /* A new state is drawn once it has held for 0.3 s. Drawing takes
         * some 50 ms without tud_task(); while the host enumerates (it
         * resets the bus a few times and wants SET_ADDRESS answered within
         * 50 ms -- Windows is strict about it) that made it give up and
         * start again, over and over, and the text jumped. */
        if (state != seen) {
            seen = state;
            seen_since = time_us_64();
        }
        if (seen != shown && time_us_64() - seen_since > 300000) {
            ui_usb(seen);
            shown = seen;
        }
        if (state == 2 && !tud_suspended())
            had_host = true;
        bool gone = had_host && (state != 2 || tud_suspended());
        if (!gone)
            gone_since = 0;
        else if (!gone_since)
            gone_since = time_us_64();
        if (usb_msc_ejected || (gone_since && time_us_64() - gone_since > 3000000)) {
            usb_msc_flush();
            uint64_t t = time_us_64() + 300000;
            while (time_us_64() < t)
                tud_task();             /* let the host see the eject complete */
            watchdog_reboot(0, 0, 10);
            for (;;) tight_loop_contents();
        }
    }
}

/* ---- start --------------------------------------------------------------------- */

/* The chip restarted on its own (supply dip, glitch detector, reset
 * request) -- say so, that is the only trace such a restart leaves. */
static void unexpected_reset_report(uint32_t r)
{
    const char *why;
    if (r & POWMAN_CHIP_RESET_HAD_BOR_BITS)
        why = "Supply voltage dropped";
    else if (r & POWMAN_CHIP_RESET_HAD_GLITCH_DETECT_BITS)
        why = "Glitch detector";
    else if (r & POWMAN_CHIP_RESET_HAD_HZD_SYS_RESET_REQ_BITS)
        why = "System reset request";
    else
        return;
    char code[32];
    snprintf(code, sizeof code, "(reset %08lx)", (unsigned long)r);
    ui_message("Unexpected restart", why, code, "Batteries low?");
}

/* for the emulators, see pbc_launch.h */
static const pbc_shared_t shared = { PBC_SHARED_MAGIC, &font_head, &font_body, &font_small };

int main(void)
{
    watchdog_hw->scratch[2] = (uint32_t)&shared;
    if (watchdog_hw->scratch[0] == PBC_JUMP_MAGIC) {
        uint32_t offset = watchdog_hw->scratch[1];
        watchdog_hw->scratch[0] = 0;
#ifdef PBC_AUTOTEST
        /* test build: back to BOOTSEL after a while (watchdog, scratch[3]) */
        watchdog_hw->scratch[3] = 0xB007B007u;
        watchdog_hw->scratch[5] = watchdog_hw->scratch[6] = watchdog_hw->scratch[7] = 0;
        watchdog_enable(PBC_AUTOTEST_MS, false);
#endif
        if (app_valid(offset))
            jump_to(offset);
    }

    /* The standalone emulators note at 0xFF0000 which game lies in their ROM
     * region at 0x100000 -- which this firmware has just overwritten with
     * the emulators. Forget it, so that one flashed back reloads its game. */
    if (*(const uint32_t *)(XIP_BASE + 0xFF0000u) != 0xFFFFFFFFu) {
        uint32_t irq = save_and_disable_interrupts();
        flash_range_erase(0xFF0000u, FLASH_SECTOR_SIZE);
        restore_interrupts(irq);
    }

    uint32_t reset = powman_hw->chip_reset;
    input_init();
    pbc_rgb_set(0, 0, 0);               /* a game may have left it on (C64 drive LED) */
    prefs_load();
    gfx_backlight = prefs_backlight();
    favs_load();
    battery_init();
    sound_on();                     /* ramps the speaker up while still dark */
    pbc_display_init();             /* backlight off until the first picture */
    gfx_init();
#ifdef PBC_AUTOTEST
    if (watchdog_hw->scratch[3] == 0xB007B007u) {   /* the game's markers on screen */
        watchdog_hw->scratch[3] = 0;
        char s[3][40];
        snprintf(s[0], 40, "A %08lx", (unsigned long)watchdog_hw->scratch[5]);
        snprintf(s[1], 40, "B %08lx", (unsigned long)watchdog_hw->scratch[6]);
        snprintf(s[2], 40, "C %08lx", (unsigned long)watchdog_hw->scratch[7]);
        gfx_background(0, 0);
        for (int i = 0; i < 3; i++)
            gfx_text(&font_head, 20, 60 + 50 * i, s[i], 0xffffff, 255);
        gfx_present();
        pbc_display_backlight(200);
        static uint32_t keep[FLASH_PAGE_SIZE / 4];  /* for picotool save (0xFFB000) */
        memset(keep, 0xFF, sizeof keep);
        keep[0] = 0x7E577E57u;
        for (int i = 0; i < 3; i++)
            keep[1 + i] = watchdog_hw->scratch[5 + i];
        uint32_t irq = save_and_disable_interrupts();
        flash_range_erase(0xFFB000u, FLASH_SECTOR_SIZE);
        flash_range_program(0xFFB000u, (const uint8_t *)keep, FLASH_PAGE_SIZE);
        restore_interrupts(irq);
        busy_wait_ms(2000);
        reset_usb_boot(0, 0);
    }
#endif

    unexpected_reset_report(reset);
    if ((reset & POWMAN_CHIP_RESET_HAD_POR_BITS) && (input_raw() & (IN_A | IN_B)))
        usb_mode();
    if (!fs_mount())
        usb_mode();                     /* fresh device: nothing on the drive yet */
    /* switched on or RES pressed (not the restart after the USB drive) */
    ui_intro_pending = (reset & (POWMAN_CHIP_RESET_HAD_POR_BITS | POWMAN_CHIP_RESET_HAD_RUN_LOW_BITS)) != 0;

    int device = 1;
    static char game[GAME_NAME_MAX];
    const pbc_launch_t *last = pbc_launch_get();
    if (last && last->device < device_count) {
        device = last->device;
        strncpy(game, last->game, sizeof game - 1);
    }

    /* USB drive mode, above, has the port to itself */
    pico_unique_board_id_t uid;
    pico_get_unique_board_id(&uid);
    uint32_t id = 2166136261u;
    for (unsigned i = 0; i < sizeof uid.id; i++)
        id = (id ^ uid.id[i]) * 16777619u;
    lobby_begin(id);

#ifdef PBC_AUTOTEST
    {                                   /* test build: start the game at once */
        char (*names)[GAME_NAME_MAX];
        int n = games_list(PBC_AUTOTEST, &names);
        device = PBC_AUTOTEST;
        if (n > PBC_AUTOTEST_GAME) {
            strcpy(game, names[PBC_AUTOTEST_GAME]);
            lobby_end();
            write_launch(device, game);
            fs_unmount();
            launch(app_offset(devices[device].app));
        }
    }
#endif
    for (;;) {
        if (ui_run(&device, game) == UI_USB) {
            lobby_end();
            fs_unmount();
            usb_mode();
        }
        uint32_t offset = app_offset(devices[device].app);
        if (!app_valid(offset)) {
            ui_message("Not available", "This build has no emulator", "for this system.", NULL);
            continue;
        }
        lobby_end();
        write_launch(device, game);
        ui_starting(device, game);
        fs_unmount();
        launch(offset);
    }
}
