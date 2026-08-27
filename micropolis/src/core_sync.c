/* core_sync.c — runs the Micropolis simulation on core1 so the UI, input and
 * display stay responsive on core0 even as the city (and thus the periodic
 * map scans) grow heavier.
 *
 * Model:
 *   core1  owns ALL simulation writes: it loops engine_tick() at FRAME_MS.
 *   core0  renders + handles input/UI; its bulk mutators (build a tool,
 *          trigger a disaster, new game, save/load) go through engine_lock()/
 *          engine_unlock(), which serialize them against engine_tick().
 *   reads  used only for drawing (engine_tile/query/funds/sprites) are NOT
 *          locked — a half-updated tile is at worst a one-frame cosmetic
 *          glitch, never a crash, since the map storage is fixed after init.
 *   save   flash erase/program parks core1 via multicore_lockout so it is not
 *          executing from XIP while flash is busy (see save.c).
 *
 * This file is only built for the device (it pulls in pico_multicore); the
 * host harness never compiles it and the lock macros compile away there.
 */
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "config.h"
#include "engine.h"
#include "ui.h"

static mutex_t g_engine_mtx;

void engine_lock(void)   { mutex_enter_blocking(&g_engine_mtx); }
void engine_unlock(void) { mutex_exit(&g_engine_mtx); }

/* core1 entry: step the simulation at the reference cadence. */
static void core1_main(void) {
    /* let core0 park us cleanly (in a RAM-resident handler) during flash writes */
    multicore_lockout_victim_init();

    absolute_time_t next = get_absolute_time();
    for (;;) {
        next = delayed_by_ms(next, FRAME_MS);

        /* don't advance the sim while the title screen is up (matches the
         * old single-core behaviour). UI.mode is written by core0; a stale
         * read here only delays the first tick by one frame. */
        if (UI.mode != UI_TITLE) {
            engine_lock();
            engine_tick();
            engine_unlock();
        }

        sleep_until(next);
    }
}

void core_sync_start(void) {
    mutex_init(&g_engine_mtx);
    multicore_launch_core1(core1_main);
}
