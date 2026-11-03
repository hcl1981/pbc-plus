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
#define PBC_APP_MSX 0
#define PBC_APP_A26 0
#define PBC_APP_ZX  0
#define PBC_APP_CPC 0
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
static const control_t c_sg[] = {
    { "Stick", "Joystick" }, { "1", "B" }, { "2", "A" }, { "PAUSE", "Center + B" }, { NULL } };
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
static const control_t c_msx[] = {
    { "Stick", "Joystick" }, { "Fire 1", "B" }, { "Fire 2", "A" },
    { "F1", "Center + B" }, { "Menu", "Center + A" }, { NULL } };
static const control_t c_a26[] = {
    { "Joystick", "Joystick" }, { "Button", "B or A" }, { "RESET", "Center + B" },
    { "SELECT", "Center + A" }, { NULL } };
static const control_t c_zx[] = {
    { "Joystick", "Joystick" }, { "Fire", "B" }, { "SPACE", "A" }, { "ENTER", "Center + B" },
    { "Menu", "Center + A" }, { NULL } };
static const control_t c_cpc[] = {
    { "Joystick", "Joystick" }, { "Fire", "B" }, { "SPACE", "A" }, { "RETURN", "Center + B" },
    { "Menu", "Center + A" }, { NULL } };
static const control_t c_none[] = { { NULL } };

/* Display modes, in the order the emulators number them (their video.c);
 * the first is each emulator's default. */
static const char *const m_c64[] = { "Standard", "Full border", "No border", NULL };
static const char *const m_nes[] = { "Crop 1:1", "Fit width", "No overscan", NULL };
static const char *const m_sms[] = { "Crop 1:1", "Fit width", "Zoom 1.25x", NULL };
static const char *const m_sg[]  = { "Crop 1:1", "Fit width", "Zoom 1.25x", NULL };
static const char *const m_pce[] = { "Standard", "Crop 1:1", NULL };
static const char *const m_md[]  = { "Fit width", "Crop 1:1", NULL };
static const char *const m_gg[]  = { "Scale 1.5x", "Original", NULL };
static const char *const m_gbc[] = { "Scale 1.5x", "Original", NULL };
static const char *const m_msx[] = { "Fit width", "Crop 1:1", NULL };
static const char *const m_a26[] = { "Scale 1.5x", NULL };
static const char *const m_zx[]  = { "Fit width", "Crop 1:1", NULL };
static const char *const m_cpc[] = { "Fit width", "Crop 1:1", NULL };

/* The list itself is in common/pbc_devices.h: the emulators' loading
 * screen takes name, colour and icon from it by the same index. */
#define DEVICE(name, maker, exts, app, accent, pic, icon, controls, modes) \
    { name, maker, exts, app, accent, &pic, &icon, controls, modes },
const device_t devices[] = { PBC_DEVICES(DEVICE) };
const int device_count = sizeof devices / sizeof devices[0];

static const char *const system_files[] = {
    "kernal.rom", "kernal.bin", "basic.rom", "basic.bin", "chargen.rom", "chargen.bin",
    "1541.rom", "1541.bin", "dos1541.rom",                          /* C64 */
    "msx2.rom", "msx2ext.rom", "disk.rom",                          /* MSX2 */
    NULL };

bool device_has_ext(const device_t *d, const char *filename)
{
    for (int i = 0; system_files[i]; i++)
        if (strcasecmp(filename, system_files[i]) == 0)
            return false;
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
        [APP_MSX] = PBC_APP_MSX, [APP_A26] = PBC_APP_A26, [APP_ZX] = PBC_APP_ZX,
        [APP_CPC] = PBC_APP_CPC,
    };
    return app >= 0 && app < APP_COUNT ? offsets[app] : 0;
}

bool device_link(const device_t *d)
{
    switch (d->app) {
    case APP_NES: case APP_PCE: case APP_C64: case APP_MSX: case APP_A26: case APP_MD:
        return true;
    case APP_SMS:
        return d->controls == c_sms || d->controls == c_sg;    /* not the Game Gear */
    default:
        return false;
    }
}
