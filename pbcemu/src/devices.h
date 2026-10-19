/* devices.h -- the systems the launcher offers, and which emulator runs them. */
#ifndef PBC_DEVICES_H
#define PBC_DEVICES_H

#include <stdint.h>
#include <stdbool.h>
#include "assets.h"

/* Emulator firmwares packed behind the launcher (pbcemu/build.sh). */
enum { APP_MD, APP_SMS, APP_NES, APP_PCE, APP_GBC, APP_C64, APP_COUNT, APP_NONE = APP_COUNT };

/* One line of the controls card: the console's button, the PicoBoy's. */
typedef struct { const char *button, *key; } control_t;

typedef struct {
    const char *name;
    const char *maker;              /* maker and year, the line under the name */
    const char *exts;               /* ".nes" or ".md.gen.smd.bin" */
    uint8_t app;
    uint32_t accent;                /* 0xRRGGBB */
    const pbc_picture_t *pic, *icon;
    const control_t *controls;      /* ends with { NULL } */
    const char *const *modes;       /* display modes as the emulator numbers them, NULL-terminated */
} device_t;

extern const device_t devices[];
extern const int device_count;      /* the last entry is "Add games" (USB) */

bool device_has_ext(const device_t *d, const char *filename);

/* Flash offset of an emulator, 0 if it is not part of this build. */
uint32_t app_offset(int app);

#endif /* PBC_DEVICES_H */
