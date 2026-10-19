/* ui.h -- the launcher's screens. */
#ifndef PBC_UI_H
#define PBC_UI_H

#include "games.h"

typedef enum { UI_PLAY, UI_USB } ui_result_t;

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
