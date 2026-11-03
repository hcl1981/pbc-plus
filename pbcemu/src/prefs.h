/* prefs.h -- the launcher's own options, kept in flash (0xFF7000).
 *
 * Sound, volume, the display mode of each device and the performance
 * overlay go to the emulators in the launch record; they are only changed
 * here, never during a game. */
#ifndef PBC_PREFS_H
#define PBC_PREFS_H

#include <stdbool.h>
#include <stdint.h>

#define PREFS_OFFSET   0xFF7000u
#define VOLUME_MIN     1
#define VOLUME_MAX     12
#define VOLUME_DEFAULT 8

#define PREFS_DEVICES  16

#define BRIGHT_MIN     1
#define BRIGHT_MAX     8
#define BRIGHT_DEFAULT 6            /* the old fixed 200 */

typedef struct {
    bool sound;
    int  volume;
    bool stats;                     /* fps / speed / CPU overlay in the games */
    int  brightness;                /* BRIGHT_MIN..BRIGHT_MAX, for all systems */
    uint8_t video[PREFS_DEVICES];   /* display mode per device (devices.c) */
} prefs_t;

extern prefs_t g_prefs;

void prefs_load(void);
void prefs_save(void);              /* only writes when something changed */

/* backlight level (pbc_display_backlight) of g_prefs.brightness */
uint8_t prefs_backlight(void);

int  prefs_video(int device);       /* always a valid mode of the device */
void prefs_set_video(int device, int mode);

#endif /* PBC_PREFS_H */
