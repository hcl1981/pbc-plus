/* settings.h -- options kept in flash across power cycles. */
#ifndef PBC_SETTINGS_H
#define PBC_SETTINGS_H

#include <stdbool.h>

#define SETTINGS_OFFSET 0xFF5000u    /* own sector: nes 0xFF1000, c64 0xFF2000 */

#define VOLUME_MIN     1
#define VOLUME_MAX     12
#define VOLUME_DEFAULT 8

typedef struct {
    bool sound;
    int  volume;                    /* VOLUME_MIN..VOLUME_MAX */
} pbc_settings_t;

extern pbc_settings_t g_settings;

void settings_load(void);
/* Writes flash: only from the menu (stock clock, core 1 not running). */
void settings_save(void);

#endif /* PBC_SETTINGS_H */
