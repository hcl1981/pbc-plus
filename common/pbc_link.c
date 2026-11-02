/* pbc_link.c -- see pbc_link.h */
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include "pbc_launch.h"
#include "pbc_loading.h"
#include "pbc_lockstep.h"
#include "pbc_link.h"
#include "audio_pwm.h"
#include "display.h"
#include "input.h"
#include "menu.h"

static int player;                      /* 0: no link play */
static bool master;

uint32_t pbc_link_hash(uint32_t h, const void *data, size_t len)
{
    const uint8_t *p = data;
    while (len--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

static void __attribute__((noreturn)) back_to_launcher(void)
{
    menu_wait_release();
    watchdog_reboot(0, 0, 10);
    for (;;) tight_loop_contents();
}

static void __attribute__((noreturn)) stop_with(const char *l1, const char *l2)
{
    pbc_loading_message("Link play", l1, l2, "Press a button");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    back_to_launcher();
}

void pbc_link_start(uint32_t config)
{
    const pbc_launch_t *l = pbc_launch_get();
    int want = !l ? 0 : (l->flags & PBC_LAUNCH_LINK1) ? 1 : (l->flags & PBC_LAUNCH_LINK2) ? 2 : 0;
    if (!want)
        return;

    master = (l->flags & PBC_LAUNCH_LINK_MASTER) != 0;
    pbc_loading_message("Link play", want == 1 ? "You are player 1" : "You are player 2",
                        "Waiting for partner ...", "B  play alone      A  back");
    menu_wait_release();
    ls_begin(master);
    for (;;) {
        int r = ls_connect(config);
        if (r > 0)
            break;
        if (r < 0) {
            ls_end();
            stop_with("Game or settings differ", "on the two devices");
        }
        uint32_t e = input_menu_edges();
        if (e & IN_B) {
            ls_end();
            menu_wait_release();
            return;                     /* alone: an ordinary game */
        }
        if (e & IN_A) {
            ls_end();
            back_to_launcher();
        }
    }
    player = want;
}

bool pbc_link_active(void)  { return player != 0; }
int  pbc_link_player(void)  { return player; }
bool pbc_link_follows(void) { return player && !master; }

void pbc_link_frame(uint16_t local, const uint8_t *ram, size_t ram_size,
                    uint16_t *p1, uint16_t *p2)
{
    if (!player) {
        *p1 = local;
        *p2 = 0;
        return;
    }
    uint16_t remote;
    if (!ls_frame(local, ram, ram_size, &remote)) {
        /* as the crash screens: the panel is core 0's from now on */
        multicore_reset_core1();
        pbc_display_end();
        audio_pwm_set_volume(0);
        ls_end();
        stop_with("Connection lost", "Check the cable");
    }
    *p1 = player == 1 ? local : remote;
    *p2 = player == 1 ? remote : local;
}

bool pbc_link_out_of_sync(void)
{
    return player && ls_out_of_sync();
}

bool pbc_link_out_of_sync_news(void)
{
    static bool told;
    if (told || !pbc_link_out_of_sync())
        return false;
    told = true;
    return true;
}
