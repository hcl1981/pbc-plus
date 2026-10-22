/* settings.h -- options kept in flash across power cycles.
 *
 * Sound and volume apply to every game; the joystick port and the program
 * to load from the disk are remembered per game (by a hash of its file
 * name). Own sector, so the NES firmware's
 * settings at 0xFF1000 are left alone. */
#ifndef PBC_SETTINGS_H
#define PBC_SETTINGS_H

#include <stdbool.h>

#define SETTINGS_OFFSET 0xFF2000u

#define VOLUME_MIN     1
#define VOLUME_MAX     12
#define VOLUME_DEFAULT 8
#define PORT_DEFAULT   2            /* most single-player games use port 2 */

typedef struct {
    bool sound;
    int  volume;                    /* VOLUME_MIN..VOLUME_MAX */
} pbc_settings_t;

extern pbc_settings_t g_settings;

void settings_load(void);

/* Joystick port (1 or 2) for a game; PORT_DEFAULT if never set. */
int  settings_port(const char *game);
void settings_set_port(const char *game, int port);

/* Program to load from the game's disk: "" means the first one ("*"). */
#define SETTINGS_FILE_MAX 16
void settings_file(const char *game, char *out);    /* out: SETTINGS_FILE_MAX + 1 */
void settings_set_file(const char *game, const char *file);

/* Key on Center + B, per game; Center + A changes the disk. Name and C64
 * keyboard matrix position (row, column as Frodo's key_matrix); the first
 * is the default. */
#define C64_KEYS(X) \
    X("SPACE", 7, 4) X("RUN/STOP", 7, 7) X("F1", 0, 4) X("F3", 0, 5) \
    X("F5", 0, 6) X("F7", 0, 3) X("Y", 3, 1) X("N", 4, 7) \
    X("1", 7, 0) X("2", 7, 3) X("3", 1, 0) X("4", 1, 3)
#define C64_KEY_NAME(name, row, col) name,
#define C64_KEY_COUNT 12
int  settings_key(const char *game);                /* 0..C64_KEY_COUNT-1 */
void settings_set_key(const char *game, int key);

/* Processor-level 1541 ("True drive") for a game: needed by fast loaders,
 * costs time; needs 1541.rom on the drive. Off by default. */
bool settings_true_drive(const char *game);
void settings_set_true_drive(const char *game, bool on);

/* Writes flash: only from the menu (stock clock, core 1 not running). */
void settings_save(void);

#endif /* PBC_SETTINGS_H */
