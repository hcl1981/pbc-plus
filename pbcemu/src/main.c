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
#include "hardware/sync.h"
#include "hardware/watchdog.h"
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
#include "pbc_launch.h"

static FATFS g_fs;
extern volatile bool usb_msc_ejected;

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
    l.video = (uint8_t)prefs_video(device);
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
           "  Commodore 64     .d64 .prg\r\n"
           "  NES              .nes\r\n"
           "  Master System    .sms\r\n"
           "  PC Engine        .pce   (HuCards)\r\n"
           "  Mega Drive       .md .gen .smd .bin\r\n"
           "  Game Gear        .gg\r\n"
           "  Game Boy (Color) .gb .gbc\r\n"
           "\r\n"
           "Commodore 64: the free MEGA65 OpenROMs are built in. If you own\r\n"
           "the Commodore ROMs, copy kernal.rom and basic.rom (optionally\r\n"
           "chargen.rom) here; some games only run with the original KERNAL.\r\n"
           "\r\n"
           "Back to this mode: \"Add Games\" (last page of the systems), or\r\n"
           "hold A or B while switching on.\r\n"
           "Back to the game list from a game: RESET.\r\n", &f);
    f_close(&f);
}

static void __attribute__((noreturn)) usb_mode(void)
{
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

    tud_init(BOARD_TUD_RHPORT);
    int shown = -1;
    for (;;) {
        tud_task();
        int state = tud_mounted() ? 2 : tud_connected() ? 1 : 0;
        if (state != shown) {
            ui_usb(state);
            gfx_finish();
            shown = state;
        }
        if (usb_msc_ejected) {
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

int main(void)
{
    if (watchdog_hw->scratch[0] == PBC_JUMP_MAGIC) {
        uint32_t offset = watchdog_hw->scratch[1];
        watchdog_hw->scratch[0] = 0;
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
    prefs_load();
    pbc_display_init();
    gfx_init();
    gfx_background(0x161d31, 0x06080e);
    gfx_present();
    pbc_display_backlight(200);

    unexpected_reset_report(reset);
    if ((reset & POWMAN_CHIP_RESET_HAD_POR_BITS) && (input_raw() & (IN_A | IN_B)))
        usb_mode();
    if (!fs_mount())
        usb_mode();                     /* fresh device: nothing on the drive yet */

    int device = 1;
    static char game[GAME_NAME_MAX];
    const pbc_launch_t *last = pbc_launch_get();
    if (last && last->device < device_count) {
        device = last->device;
        strncpy(game, last->game, sizeof game - 1);
    }

    for (;;) {
        if (ui_run(&device, game) == UI_USB) {
            fs_unmount();
            usb_mode();
        }
        uint32_t offset = app_offset(devices[device].app);
        if (!app_valid(offset)) {
            ui_message("Not available", "This build has no emulator", "for this system.", NULL);
            continue;
        }
        write_launch(device, game);
        ui_starting(device, game);
        fs_unmount();
        launch(offset);
    }
}
