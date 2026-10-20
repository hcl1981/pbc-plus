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
 */
#ifndef PBC_LAUNCH_H
#define PBC_LAUNCH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "hardware/regs/addressmap.h"

#define PBC_LAUNCH_OFFSET   0xFF6000u
#define PBC_LAUNCH_MAGIC    0x484E4C50u     /* "PLNH" */
#define PBC_LAUNCH_VERSION  2u
#define PBC_LAUNCH_NAME_MAX 96              /* = ROM_NAME_MAX of the emulators */

/* watchdog scratch[0] = PBC_JUMP_MAGIC, scratch[1] = flash offset of the
 * emulator: set by the launcher right before its restart. */
#define PBC_JUMP_MAGIC      0x504D554Au     /* "JUMP" */

/* flags */
#define PBC_LAUNCH_STATS    0x01            /* show fps, speed and CPU load */

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
    uint8_t  reserved[3];
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
