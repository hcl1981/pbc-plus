/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "pico.h"
#include "net_defs.h"

typedef struct {
    uint32_t client_id;
    char name[MAXPLAYERNAME];
} lobby_player_t;

typedef enum {
    lobby_no_connection,
    lobby_waiting_for_start,
    lobby_game_started,
    lobby_game_not_compatible,
} piconet_lobby_status_t;

typedef struct {
    uint32_t compat_hash; // version and whd hash
    uint32_t seq;
    piconet_lobby_status_t status;
    uint8_t nplayers;
    int8_t deathmatch;
    int8_t epi;
    int8_t skill;
    lobby_player_t players[NET_MAXPLAYERS];
} lobby_state_t;

// one time initialization (set pulls etc)
void piconet_init();
void piconet_start_host(int8_t deathmatch, int8_t epi, int8_t skill);
void piconet_start_client();
void piconet_stop();
// periodically poll to check connection hasn't dropped
bool piconet_client_check_for_dropped_connection();
void piconet_start_game();
// returns which player you are
int piconet_get_lobby_state(lobby_state_t *state);
void piconet_new_local_tic(int tic);
// Zusaetzlicher Pollpfad aus der Spielschleife (unabhaengig vom Timer-IRQ)
void piconet_poll(void);
int piconet_maybe_recv_tic(int fromtic);

extern char player_name[MAXPLAYERNAME];

// Diagnosezaehler des USB-Links -- werden im Netzwerk-Menue eingeblendet.
// Warum der Link zuletzt abgeraeumt wurde (piconet_stop):
//   1 = d_loop: Verbindung galt als tot   2 = d_loop: weniger als 2 Spieler
//   3 = Menue "End Game"                  4 = Programmende
// piconet_dbg_stop_info traegt den Zusatzwert (bei 2: die Spielerzahl).
extern uint8_t piconet_dbg_stop_reason;
extern uint8_t piconet_dbg_stop_info;

extern uint32_t piconet_dbg_ticks;
extern uint32_t piconet_dbg_polls;
extern uint32_t piconet_dbg_exchanges;
extern uint32_t piconet_dbg_missed;
extern int32_t  piconet_dbg_last_rc;
extern uint32_t piconet_dbg_last_msg;
// Bit1 = CLK gesehen, Bit0 = DATA gesehen; -1 wenn der Link nicht laeuft.
int piconet_debug_line_state(void);
bool piconet_debug_is_host(void);
// true im laufenden Spiel; liefert eigenen und entfernten Tic-Stand
bool piconet_debug_tics(int *own, int *remote);
// true, solange die Lobby laeuft; liefert Spielerzahl und eigene Nummer
bool piconet_debug_lobby(int *nplayers, int *pnum);
// Millisekunden seit dem letzten zustande gekommenen Tic (0 = laeuft)
uint32_t piconet_stalled_ms(void);

#if USE_PICO_NET
// same var as used by regular networking
extern boolean net_client_connected; // basically whether events are sync-ing amongst the players
#endif