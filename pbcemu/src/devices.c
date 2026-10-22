/* devices.c -- see devices.h */
#include <string.h>
#include <strings.h>
#include "devices.h"
#include "pbc_devices.h"
/* Offsets of the emulators: build.sh writes pbc_apps.h into the build
 * directory. Built on its own, the launcher has none. */
#if __has_include("pbc_apps.h")
#include "pbc_apps.h"
#else
#define PBC_APP_MD  0
#define PBC_APP_SMS 0
#define PBC_APP_NES 0
#define PBC_APP_PCE 0
#define PBC_APP_GBC 0
#define PBC_APP_C64 0
#endif

/* Same as the emulators map them (their input.c). */
static const control_t c_c64[] = {
    { "Joystick", "Joystick" }, { "Fire", "B" }, { "RETURN", "A" }, { "SPACE", "Center + B" },
    { "Disk", "Center + A" }, { NULL } };
static const control_t c_nes[] = {
    { "D-pad", "Joystick" }, { "A", "B" }, { "B", "A" }, { "START", "Center + B" },
    { "SELECT", "Center + A" }, { NULL } };
static const control_t c_sms[] = {
    { "D-pad", "Joystick" }, { "1", "B" }, { "2", "A" }, { "PAUSE", "Center + B" }, { NULL } };
static const control_t c_pce[] = {
    { "D-pad", "Joystick" }, { "I", "B" }, { "II", "A" }, { "RUN", "Center + B" },
    { "SELECT", "Center + A" }, { NULL } };
static const control_t c_md[] = {
    { "D-pad", "Joystick" }, { "A", "B" }, { "B", "A" }, { "C", "Center" },
    { "START", "Center + B" }, { NULL } };
static const control_t c_gg[] = {
    { "D-pad", "Joystick" }, { "1", "B" }, { "2", "A" }, { "START", "Center + B" }, { NULL } };
static const control_t c_gbc[] = {
    { "D-pad", "Joystick" }, { "A", "B" }, { "B", "A" }, { "START", "Center + B" },
    { "SELECT", "Center + A" }, { NULL } };
static const control_t c_none[] = { { NULL } };

/* Display modes, in the order the emulators number them (their video.c);
 * the first is each emulator's default. */
static const char *const m_c64[] = { "Standard", "Full border", "No border", NULL };
static const char *const m_nes[] = { "Crop 1:1", "Fit width", "No overscan", NULL };
static const char *const m_sms[] = { "Crop 1:1", "Fit width", "Zoom 1.25x", NULL };
static const char *const m_pce[] = { "Standard", "Crop 1:1", NULL };
static const char *const m_md[]  = { "Fit width", "Crop 1:1", NULL };
static const char *const m_gg[]  = { "Scale 1.5x", "Original", NULL };
static const char *const m_gbc[] = { "Scale 1.5x", "Original", NULL };

/* The list itself is in common/pbc_devices.h: the emulators' loading
 * screen takes name, colour and icon from it by the same index. */
#define DEVICE(name, maker, exts, app, accent, pic, icon, controls, modes) \
    { name, maker, exts, app, accent, &pic, &icon, controls, modes },
const device_t devices[] = { PBC_DEVICES(DEVICE) };
const int device_count = sizeof devices / sizeof devices[0];

bool device_has_ext(const device_t *d, const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || !d->exts[0])
        return false;
    size_t n = strlen(dot);
    for (const char *e = d->exts; (e = strchr(e, '.')) != NULL; e++) {
        const char *next = strchr(e + 1, '.');
        size_t len = next ? (size_t)(next - e) : strlen(e);
        if (len == n && strncasecmp(e, dot, n) == 0)
            return true;
    }
    return false;
}

uint32_t app_offset(int app)
{
    static const uint32_t offsets[APP_COUNT] = {
        [APP_MD] = PBC_APP_MD, [APP_SMS] = PBC_APP_SMS, [APP_NES] = PBC_APP_NES,
        [APP_PCE] = PBC_APP_PCE, [APP_GBC] = PBC_APP_GBC, [APP_C64] = PBC_APP_C64,
    };
    return app >= 0 && app < APP_COUNT ? offsets[app] : 0;
}
