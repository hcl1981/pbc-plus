/* pbc_launch.h -- hand-over between the PBCemu launcher and the emulators
 *
 * In the combined firmware (pbcemu/) the launcher sits at the start of the
 * flash and the emulators behind it, each linked to its own address. The
 * launcher writes the chosen game into the launch record, leaves the address
 * of the emulator in the watchdog scratch registers and restarts the chip;
 * right after the restart it jumps there. Every later restart (RESET, "back
 * to the list") lands in the launcher again.
 *
 * Flash map of the combined firmware (16 MB):
 *   0x000000 .. 0x1FFFFF   launcher + emulators (pbcemu/build.sh packs them)
 *   0x200000 .. 0x6FFFFF   ROM region: the game being played (5 MB)
 *   0x700000 .. 0xFEFFFF   FatFs = the USB drive PBC-ROMS (9 MB; the
 *                          standalone firmwares have it at 0x600000)
 *   0xFF0000               ROM info of the standalone firmwares (erased by
 *                          the launcher: its ROM region is elsewhere)
 *   0xFF1000 .. 0xFF5000   settings of the emulators
 *   0xFF6000               launch record (this file)
 *   0xFF7000               launcher settings
 *   0xFF8000               ROM info of the combined firmware
 *   0xFF9000               launcher favourites (pbcemu/src/favs.h)
 */
#ifndef PBC_LAUNCH_H
#define PBC_LAUNCH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "hardware/regs/addressmap.h"
#include "hardware/structs/watchdog.h"
#include "hardware/clocks.h"

#define PBC_LAUNCH_OFFSET   0xFF6000u
#define PBC_LAUNCH_MAGIC    0x484E4C50u     /* "PLNH" */
#define PBC_LAUNCH_VERSION  2u
#define PBC_LAUNCH_NAME_MAX 96              /* = ROM_NAME_MAX of the emulators */

/* watchdog scratch[0] = PBC_JUMP_MAGIC, scratch[1] = flash offset of the
 * emulator: set by the launcher right before its restart. */
#define PBC_JUMP_MAGIC      0x504D554Au     /* "JUMP" */

/* watchdog scratch[2] = address of the launcher's pbc_shared_t, set at
 * every start: its fonts, which the emulators draw their loading screens
 * and messages with (common/pbc_loading.c) instead of a copy of their own.
 * The launcher fills the first PBC_LAUNCHER_SIZE bytes of flash. */
#define PBC_SHARED_MAGIC    0x544E4F46u     /* "FONT" */
#define PBC_LAUNCHER_SIZE   0x45000u        /* = LAUNCHER_SIZE, pbcemu/build.sh */
typedef struct {
    uint32_t magic;
    const struct pbc_font_s *font_head, *font_body, *font_small;
} pbc_shared_t;

/* flags */
#define PBC_LAUNCH_STATS    0x01            /* show fps, speed and CPU load */
#define PBC_LAUNCH_LINK1    0x02            /* link play, this device is player 1 */
#define PBC_LAUNCH_LINK2    0x04            /* ... player 2 */
#define PBC_LAUNCH_LINK_MASTER 0x08         /* ... and it started the game: keeps the pace */
#define PBC_LAUNCH_SCANLINES 0x10           /* every second panel row dimmed (pbc_scanline.h) */

/* Display modes: each emulator numbers its own, 0 is its default, and the
 * launcher's device table (pbcemu/src/devices.c) has the names in the same
 * order. An emulator treats an unknown number as 0. */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t  device;                        /* launcher's device index */
    uint8_t  sound;                         /* 0/1 */
    uint8_t  volume;                        /* VOLUME_MIN..VOLUME_MAX */
    uint8_t  flags;                         /* PBC_LAUNCH_* below */
    uint8_t  video;                         /* display mode, per emulator (0 = its default) */
    uint8_t  brightness;                    /* backlight 1..255, 0 = PBC_BACKLIGHT */
    uint8_t  key;                           /* the game's own options, per system:
                                               CPC: bits 0-5 the key on Center + B (cpc_keys_menu
                                               index, 0 = RETURN), 6-7 the joystick kind;
                                               ZX: bits 0-5 the key on Center + B (zx_keys_menu,
                                               0 = ENTER), 6-7 the joystick (zx_joy_names);
                                               Mega Drive: button layout 0..5 (megadrive/src/input.h);
                                               NES, SMS/GG/SG: bit 0 buttons swapped;
                                               Game Boy: bits 0-2 palette of an old game
                                               (gbc/src/main.c), bit 3 A/B swapped,
                                               bit 4 link cable (pbc_cable.h);
                                               Atari: bit 0/1 left/right difficulty A, bit 2 b/w */
    uint8_t  opt2;                          /* more of the game's options: ZX bit 0 a tape game
                                               starts as a 48K (else 128K); 0 elsewhere */
    char     game[PBC_LAUNCH_NAME_MAX];     /* file in the FatFs root */
    uint32_t check;                         /* FNV-1a over everything above */
} pbc_launch_t;

static inline uint32_t pbc_launch_hash(const pbc_launch_t *l)
{
    const uint8_t *p = (const uint8_t *)l;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < offsetof(pbc_launch_t, check); i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

/* The record in flash, or NULL if there is none. */
static inline const pbc_launch_t *pbc_launch_get(void)
{
    const pbc_launch_t *l = (const pbc_launch_t *)(XIP_BASE + PBC_LAUNCH_OFFSET);
    if (l->magic != PBC_LAUNCH_MAGIC || l->version != PBC_LAUNCH_VERSION ||
        l->check != pbc_launch_hash(l) || !l->game[0])
        return NULL;
    return l;
}

/* Convenience for the emulators: settings from the record, or defaults. */
static inline unsigned pbc_launch_video(void)
{
    const pbc_launch_t *l = pbc_launch_get();
    return l ? l->video : 0;
}

static inline bool pbc_launch_stats(void)
{
    const pbc_launch_t *l = pbc_launch_get();
    return l && (l->flags & PBC_LAUNCH_STATS);
}

/* Launcher test mode (pbcemu built with PBC_AUTOTEST sets scratch[3]): the
 * emulators' stats line also into watchdog scratch[5..7], which the launcher
 * keeps in flash after the watchdog reset (picotool reads it): fps | MHz
 * << 16, speed in %, core 0 load in % | the highest so far << 16. */
#define PBC_TEST_MAGIC 0xB007B007u
static inline void pbc_test_stats(uint32_t fps, uint32_t speed, uint32_t cpu)
{
    if (watchdog_hw->scratch[3] != PBC_TEST_MAGIC)
        return;
    static uint32_t top;
    if (cpu > top)
        top = cpu;
    watchdog_hw->scratch[5] = fps | (clock_get_hz(clk_sys) / 1000000u) << 16;
    watchdog_hw->scratch[6] = speed;
    watchdog_hw->scratch[7] = cpu | top << 16;
}

/* "Sonic_2 (World).md" -> title "Sonic 2", tags "(World)": the launcher's
 * list and the emulators' loading screen show games this way. */
static inline void pbc_game_title(const char *file, char *title, char *tags, int size)
{
    snprintf(title, (size_t)size, "%s", file);
    char *dot = strrchr(title, '.');
    if (dot && dot != title)
        *dot = 0;
    for (char *p = title; *p; p++)
        if (*p == '_')
            *p = ' ';
    tags[0] = 0;
    /* region and dump tags go into the second, dimmer part */
    char *t = title;
    while ((t = strpbrk(t, "([")) != NULL) {
        if (t > title && t[-1] == ' ') {
            snprintf(tags, (size_t)size, "%s", t);
            t[-1] = 0;
            break;
        }
        t++;
    }
}

#endif /* PBC_LAUNCH_H */
