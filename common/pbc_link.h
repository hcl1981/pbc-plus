/* pbc_link.h -- link play as the emulators see it (common/pbc_lockstep.h).
 *
 * The launcher finds the partner (common/pbc_lobby.h) and puts the controller
 * and the role on the link into the launch record. Without them everything
 * here does nothing and costs nothing.
 */
#ifndef PBC_LINK_H
#define PBC_LINK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FNV-1a, for the configuration both devices have to agree on. */
#define PBC_LINK_HASH_START 2166136261u

#if PBC_MULTI

uint32_t pbc_link_hash(uint32_t h, const void *data, size_t len);

/* Before the first frame, on core 0, with core 1 not started: waits for the
 * partner on a card of its own. Returns with the link up, or with link play
 * dropped when the player chooses to play alone; `config` differing on the
 * two devices goes back to the launcher. */
void pbc_link_start(uint32_t config);

bool pbc_link_active(void);

/* The controller of this device: 1, 2, or 0 without link play. */
int pbc_link_player(void);

/* The device that did not start the game takes the pace from the other and
 * does not wait for itself. Nothing to do with the controller. */
bool pbc_link_follows(void);

/* The frame's inputs of both players, `local` being this device's; without
 * link play p1 = local and p2 = 0. A lost partner ends the game here. */
void pbc_link_frame(uint16_t local, const uint8_t *ram, size_t ram_size,
                    uint16_t *p1, uint16_t *p2);

/* The two emulations differ; _news: true once, for the status line. */
bool pbc_link_out_of_sync(void);
bool pbc_link_out_of_sync_news(void);

#else   /* the single firmwares have no launcher and no link play */

static inline uint32_t pbc_link_hash(uint32_t h, const void *data, size_t len) { (void)data; (void)len; return h; }
static inline void pbc_link_start(uint32_t config) { (void)config; }
static inline bool pbc_link_active(void) { return false; }
static inline int pbc_link_player(void) { return 0; }
static inline bool pbc_link_follows(void) { return false; }
static inline void pbc_link_frame(uint16_t local, const uint8_t *ram, size_t ram_size,
                                  uint16_t *p1, uint16_t *p2)
{
    (void)ram; (void)ram_size;
    *p1 = local;
    *p2 = 0;
}
static inline bool pbc_link_out_of_sync(void) { return false; }
static inline bool pbc_link_out_of_sync_news(void) { return false; }

#endif

#ifdef __cplusplus
}
#endif

#endif
