/* settings.h -- options kept in flash across power cycles. */
#ifndef PBC_SETTINGS_H
#define PBC_SETTINGS_H

#include <stdbool.h>

#define SETTINGS_OFFSET 0xFF1000u    /* one 4 KB sector after the ROM info */

typedef struct {
    bool sound;
} pbc_settings_t;

extern pbc_settings_t g_settings;

void settings_load(void);
/* Writes flash: only from the menu (stock clock, core 1 not running). */
void settings_save(void);

#endif /* PBC_SETTINGS_H */
