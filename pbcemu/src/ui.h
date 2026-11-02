/* ui.h -- the launcher's screens. */
#ifndef PBC_UI_H
#define PBC_UI_H

#include <stdbool.h>
#include "games.h"

typedef enum { UI_PLAY, UI_USB } ui_result_t;

/* After UI_PLAY: this device's controller in link play, 0 for a game alone,
 * and whether this device started it (it is then the link's master). */
extern int  ui_link_player;
extern bool ui_link_master;

/* Device carousel and game list until a game is chosen (device and file
 * name returned) or "Add Games" is picked. The volume must be mounted.
 * Starts at the given device/game (from the last launch), if any. */
ui_result_t ui_run(int *device, char game[GAME_NAME_MAX]);

/* A card with a title and up to three lines; waits for a button. */
void ui_message(const char *title, const char *l1, const char *l2, const char *l3);

/* Shown while the chosen game is handed to its emulator. */
void ui_starting(int device, const char *game);

/* USB drive mode screen; state 0 = not connected, 1 = connected, 2 = mounted. */
void ui_usb(int state);

void ui_wait_release(void);

#endif /* PBC_UI_H */
