/*
 * piconet ueber den USB-C-Link (usblink.c) statt ueber I2C.
 *
 * Protokoll und Semantik sind aus Graham Sandersons piconet.c uebernommen
 * (Lobby-Sync per seq-Zaehler, Tic-Austausch mit Acks und Retransmit), nur
 * dass hier genau zwei Geraete miteinander reden: an einem Kabel haengt
 * nichts Drittes, also entfallen Adressvergabe und Polling komplett.
 *
 * Rollen:
 *   Host   = usblink-Master, stoesst alle PERIOD_MS einen Austausch an.
 *   Client = usblink-Slave, prueft alle CLIENT_POLL_MS, ob der Host anklopft.
 *
 * Ein Austausch ist immer Host-Paket raus / Client-Paket rein. Geht einer
 * verloren (CRC, Timeout), passiert nichts Schlimmes: beide Seiten schicken
 * unbestaetigte Daten einfach beim naechsten Mal wieder.
 *
 * Copyright (c) 2022 Graham Sanderson (Originalprotokoll)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "piconet.h"

boolean net_client_connected;

#if PICO_ON_DEVICE

#include "pico/sync.h"
#include "pico/binary_info.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

#include "usblink.h"
#include "picoboy_display.h"
#include "d_loop.h"
#include "doom/doomstat.h"
#include "whddata.h"
#include <stddef.h>

#define PICONET_VERSION 1

// Ein Austausch dauert bei gemaechlichem Takt ein paar Millisekunden, und
// waehrenddessen sind die Interrupts aus. Gebraucht werden 35 Austausche je
// Sekunde -- alle 15 ms ist also reichlich Luft, ohne das Spiel auszubremsen.
#define PERIOD_US 12000         // Host: Abstand zwischen zwei Austauschen (Lobby)
// 35 Tics je Sekunde brauchen 35 Austausche -- jeder traegt einen. Alle 18 ms
// sind es 55, also reichlich Reserve. Mehr kostet nur Rechenzeit: waehrend
// eines Austauschs sind auf beiden Geraeten die Interrupts aus.
#define GAME_PERIOD_US 24000
#define IDLE_PERIOD_US 20000    // Host ohne Gegenstelle: Pause nach Fehlversuch
#define ATTN_POLL_US 250        // Host: wie oft er auf das Ready des Clients schaut
#define ATTN_HOLD_US 150000     // Host: so lange bleibt eine Anfrage stehen
#define CLIENT_POLL_US 500      // Client: wie oft er nach dem Host schaut
#define PERIODIC_ALARM_NUM 1

// Wie lange Funkstille sein darf, bevor das Spiel die Verbindung fuer tot
// erklaert. Das ist keine Kosmetik: d_loop ruft danach piconet_stop() auf und
// der Link ist endgueltig weg. 300 ms waren viel zu knapp -- allein das Laden
// eines Levels oder eine Handvoll gestoerter Austausche reisst so eine Luecke.
// Grosszuegig: auf dieser Leitung sind Fehlerserien normal (beide Boards
// haben eigene Serienwiderstaende in der Datenleitung). Wer hier zu frueh
// aufgibt, reisst das Spiel ab, obwohl die Verbindung gleich wieder traegt.
#define GAME_TIMEOUT_US 10000000
// Fuer die Anzeige in der Lobby darf es ruhig empfindlicher sein.
#define LOBBY_TIMEOUT_SHORT_US 500000
#define LOBBY_TIMEOUT_LONG_US 1500000

#if DOOM_DEBUG_INFO
#define piconet_info printf
#else
static inline void piconet_info(const char *fmt, ...) {}
#endif

typedef enum {
    piconet_msg_none,
    piconet_msg_client_lobby,
    piconet_msg_host_lobby,
    piconet_msg_game_full,
    piconet_msg_game_already_started,
    piconet_msg_client_tic,
    piconet_msg_host_tic,
    piconet_msg_game_not_compatible,
    piconet_msg_game_start,     // 8 Byte: "los geht's" -- der Client hat den
                                // Lobby-Zustand schon
} piconet_msg_type;

// ---------------------------------------------------------------------------
// Nachrichten (Layout wie im I2C-Original)
// ---------------------------------------------------------------------------
typedef struct {
    uint32_t id;
    uint32_t last_acked_by_client_seq;
    uint32_t last_rx_time;
    int last_acked_by_client_tic;   // welchen Host-Tic der Client zuletzt hat
    int last_sent_by_client_tic;    // welchen Tic der Client zuletzt geschickt hat
} remote_client_t;

typedef struct {
    uint8_t msg_type;
    uint32_t client_id;
} host_packet_header_t;

typedef struct {
    uint8_t msg_type;
    uint32_t compat_hash;
    uint32_t client_id;
} client_packet_header_t;

// Namen stehen nicht mehr auf der Leitung: sie ergeben sich aus der Rolle
// (Host = PLAYER 1, Beitretender = PLAYER 2), beide Seiten wissen das ohnehin.
// Das spart im groessten Paket 36 Byte -- und Laenge ist auf dieser Leitung
// die einzige Groesse, die ueber Erfolg entscheidet.
#define WIRE_MAX_PLAYERS 2

typedef struct {
    client_packet_header_t hdr;
    uint32_t last_rx_seq;
} client_lobby_msg_t;

// Im laufenden Spiel braucht kein Paket mehr Kennung oder Pruefhash: an einem
// Kabel haengt genau eine Gegenstelle, die Rollen stehen fest, und die
// Pruefsumme des Rahmens sichert den Inhalt. Beides wird nur beim
// Verbindungsaufbau gebraucht -- in den Tic-Paketen war es Ballast, den jeder
// Austausch 35-mal je Sekunde mitgeschleppt hat.
typedef struct {
    uint8_t msg_type;
    uint8_t pad[3];
    int last_rx_tic;
    int enclosed_tic;               // -1 = nichts dabei
    ticcmd_t ticcmd;
} client_tic_msg_t;

// Lobby auf der Leitung: nur die Felder, die der Client nicht selbst kennt.
typedef struct {
    host_packet_header_t hdr;
    uint32_t compat_hash;
    uint32_t seq;
    uint8_t status;
    uint8_t nplayers;
    int8_t deathmatch;
    int8_t epi;
    int8_t skill;
    uint8_t pad[3];
    uint32_t player_ids[WIRE_MAX_PLAYERS];
} host_lobby_msg_t;

typedef struct {
    uint8_t msg_type;
    uint8_t pad[3];
    int client_ack_tic;
    int enclosed_tic;               // -1 = nichts dabei
} host_tic_msg_hdr_t;

// Tics: nur der Befehl des Hosts. Seinen eigenen kennt der Client bereits --
// er hat ihn selbst erzeugt und dem Host geschickt, und der Host kann einen
// Tic ueberhaupt erst abschliessen, wenn er ihn hat. Ihn zurueckzuschicken
// waere die Haelfte der Nutzlast fuer nichts.
typedef struct {
    host_tic_msg_hdr_t hdr;
    ticcmd_t cmd_host;
} host_tic_msg_t;

// Kein Paket darf gross werden -- sonst faellt die Zustellrate in den Keller.
static_assert(sizeof(host_lobby_msg_t) <= 32, "host lobby msg zu gross");
static_assert(sizeof(host_tic_msg_t) <= 32, "host tic msg zu gross");
static_assert(sizeof(client_lobby_msg_t) <= 16, "client lobby msg zu gross");
static_assert(sizeof(client_tic_msg_t) <= 32, "client tic msg zu gross");


// ---------------------------------------------------------------------------
// Zustand
// ---------------------------------------------------------------------------
static struct {
    enum { in_none, in_lobby, in_game } status;
    lobby_state_t lobby;
} synced_state;

typedef struct {
    remote_client_t client;         // der eine Gegenspieler (Spieler 1)
    bool sent_tic;                  // im letzten Austausch ging ein Tic raus
    bool got_tic;                   // ... und einer kam herein
    bool attn_active;               // Anfrage steht (CLK liegt hoch)
    uint32_t attn_t0;
    uint32_t next_exchange;         // fruehester Zeitpunkt fuers naechste Anklopfen
    int local_tic;                  // eigener zuletzt erzeugter Tic
    int last_complete_tic;          // hoechster Tic, fuer den alle Daten da sind
    int limit_tic;
    piconet_msg_type reject_reason; // != 0: dem Anklopfer eine Absage schicken
} host_state_t;

typedef struct {
    uint32_t client_id;
    uint32_t last_rx_time;
    int8_t player_num;
    int last_local_tic;
    int last_server_acked_tic;
    int last_received_from_server_tic;
    int limit_tic;
} client_state_t;

// Namen werden lokal vergeben, nicht uebertragen.
static void set_local_player_names(void) {
    memset(synced_state.lobby.players[0].name, 0, MAXPLAYERNAME);
    memset(synced_state.lobby.players[1].name, 0, MAXPLAYERNAME);
    strcpy(synced_state.lobby.players[0].name, "PLAYER 1");
    strcpy(synced_state.lobby.players[1].name, "PLAYER 2");
}

static enum { role_none, role_host, role_client } role;

static union {
    host_state_t host;
    client_state_t client;
} local_state;

static critical_section_t critsec;
static uint32_t last_step_time;

// Wann kam zuletzt ein Tic zustande? Bleibt der Tic-Fluss stehen, waehrend
// Pakete weiter fliessen, warten beide Geraete fuer immer aufeinander -- das
// Spiel sieht dann aus, als haenge es. Diese Uhr macht den Zustand sichtbar
// und loest notfalls die Verbindung auf, damit weitergespielt werden kann.
static uint32_t last_tic_progress;

// --- Diagnose (wird im Netzwerk-Menue angezeigt) ---------------------------
uint8_t piconet_dbg_stop_reason;
uint8_t piconet_dbg_stop_info;
uint32_t piconet_dbg_ticks;      // wie oft der Timer-Callback lief
uint32_t piconet_dbg_polls;      // wie oft aus der Spielschleife gepollt wurde
uint32_t piconet_dbg_exchanges;  // begonnene Austausche
uint32_t piconet_dbg_arm;
uint32_t piconet_dbg_missed;     // verpasste Timerziele
int32_t  piconet_dbg_last_rc;    // Rueckgabe des letzten Austauschs
uint32_t piconet_dbg_last_msg;   // zuletzt empfangener Nachrichtentyp

static __aligned(4) uint8_t tx_buf[160];
static __aligned(4) uint8_t rx_buf[160];

static void host_check_tic_advance_locked(void);

// --- Selbstabgleich der Taktrate -------------------------------------------
// Nur der Master gibt den Takt vor, also kann er ihn auch allein nachregeln.
// Haeufen sich Fehler, wird langsamer getaktet; laeuft es lange sauber, wird
// vorsichtig wieder beschleunigt. Damit muss niemand von Hand einmessen.
// Gemessen (Testprogramm, beide Rollen fehlerfrei): 1500 ns je Halbbit laufen
// sauber. Gebraucht werden ohnehin nur ein paar hundert Byte je Sekunde --
// schneller zu takten bringt nichts ausser Rand, langsamer verlaengert nur die
// Zeit, in der die Interrupts aus sind.
#define LINK_SPEED_MIN_NS 800
#define LINK_SPEED_MAX_NS 2500
#define LINK_SPEEDUP_AFTER 300     // Erfolge in Folge

// Auf die Fehler_quote_ schauen, nicht auf Fehler in Folge: die Bitfehler
// treten verstreut auf, drei hintereinander kommen kaum vor -- so hat sich die
// Regelung immer weiter hochgeschaukelt, obwohl jeder achte Austausch scheiterte.
#define LINK_WINDOW 64
#define LINK_WINDOW_MAX_FAILS 4

static uint8_t link_window_n;
static uint8_t link_window_fails;
static uint16_t link_clean_run;

static void link_rate_feedback(int rc) {
    if (rc == -4) return;              // war schon erledigt, kein Fehler
    uint32_t ns = usblink_get_speed();

    if (rc < 0) {
        link_window_fails++;
        link_clean_run = 0;
    } else if (++link_clean_run >= LINK_SPEEDUP_AFTER) {
        link_clean_run = 0;
        if (ns > LINK_SPEED_MIN_NS) {
            uint32_t faster = ns - ns / 8;
            usblink_set_speed(faster < LINK_SPEED_MIN_NS ? LINK_SPEED_MIN_NS : faster);
        }
    }

    if (++link_window_n >= LINK_WINDOW) {
        if (link_window_fails > LINK_WINDOW_MAX_FAILS && ns < LINK_SPEED_MAX_NS) {
            uint32_t slower = ns + ns / 2;
            usblink_set_speed(slower > LINK_SPEED_MAX_NS ? LINK_SPEED_MAX_NS : slower);
            link_clean_run = 0;
        }
        link_window_n = 0;
        link_window_fails = 0;
    }
}

static uint32_t get_compat_hash(void) {
    return (PICONET_VERSION * 31 + whdheader->hash) & 0xffffff;
}

// hardware_alarm_set_target() liefert true, wenn der Zielzeitpunkt schon
// vorbei war -- dann wird KEIN Interrupt geplant und der Timer steht fuer
// immer still. Genau das passiert bei kurzen Perioden, wenn der Aufruf selbst
// durch einen anderen IRQ verzoegert wird. Deshalb so lange nachlegen, bis er
// sicher in der Zukunft liegt.
static void arm_alarm(uint32_t us) {
    piconet_dbg_arm++;
    for (int i = 0; i < 8; i++) {
        if (!hardware_alarm_set_target(PERIODIC_ALARM_NUM, make_timeout_time_us(us))) return;
        piconet_dbg_missed++;
        us *= 2;
    }
    // Letzter Versuch mit reichlich Abstand -- lieber traege als tot.
    hardware_alarm_set_target(PERIODIC_ALARM_NUM, make_timeout_time_us(20000));
}

// ---------------------------------------------------------------------------
// Host
// ---------------------------------------------------------------------------
static int host_build_packet_locked(void) {
    host_packet_header_t *hdr = (host_packet_header_t *)tx_buf;
    hdr->client_id = local_state.host.client.id;

    if (local_state.host.reject_reason) {
        hdr->msg_type = local_state.host.reject_reason;
        hdr->client_id = 0;         // gilt fuer jeden, der gerade anklopft
        local_state.host.reject_reason = piconet_msg_none;
        return sizeof(host_packet_header_t);
    }

    if (synced_state.status == in_lobby || local_state.host.client.last_sent_by_client_tic == -1) {
        // Laeuft das Spiel schon, der Client hat aber noch keinen Tic
        // geschickt, dann fehlt ihm nur das Startsignal -- und das geht als
        // 8-Byte-Paket raus statt als kompletter Lobby-Zustand. Auf einer
        // wackligen Leitung ist das der entscheidende Unterschied: kurze
        // Pakete kommen an, lange nicht. Den Lobby-Inhalt (Spieler, Skill,
        // Episode) hat der Client aus der Wartephase bereits.
        if (synced_state.status == in_game && local_state.host.client.id) {
            hdr->msg_type = piconet_msg_game_start;
            return sizeof(host_packet_header_t);
        }
        if (local_state.host.client.id &&
            local_state.host.client.last_acked_by_client_seq != synced_state.lobby.seq) {
            hdr->msg_type = piconet_msg_host_lobby;
            host_lobby_msg_t *msg = (host_lobby_msg_t *)hdr;
            msg->compat_hash = synced_state.lobby.compat_hash;
            msg->seq = synced_state.lobby.seq;
            msg->status = (uint8_t)synced_state.lobby.status;
            msg->nplayers = synced_state.lobby.nplayers;
            msg->deathmatch = synced_state.lobby.deathmatch;
            msg->epi = synced_state.lobby.epi;
            msg->skill = synced_state.lobby.skill;
            for (int i = 0; i < WIRE_MAX_PLAYERS; i++) {
                msg->player_ids[i] = synced_state.lobby.players[i].client_id;
            }
            return sizeof(host_lobby_msg_t);
        }
        if (synced_state.status == in_lobby) {
            hdr->msg_type = piconet_msg_none;
            return sizeof(host_packet_header_t);
        }
    }

    if (synced_state.status == in_game) {
        hdr->msg_type = piconet_msg_host_tic;
        host_tic_msg_hdr_t *tichdr = (host_tic_msg_hdr_t *)hdr;
        tichdr->client_ack_tic = local_state.host.client.last_sent_by_client_tic;
        if (local_state.host.client.last_acked_by_client_tic < local_state.host.last_complete_tic) {
            tichdr->enclosed_tic = local_state.host.client.last_acked_by_client_tic + 1;
            int slot = tichdr->enclosed_tic % BACKUPTICS;
            ((host_tic_msg_t *)tichdr)->cmd_host = ticdata[slot].cmds[0];
            local_state.host.sent_tic = true;
            return sizeof(host_tic_msg_t);
        }
        tichdr->enclosed_tic = -1;
        local_state.host.sent_tic = false;
        return sizeof(host_tic_msg_hdr_t);
    }

    hdr->msg_type = piconet_msg_none;
    return sizeof(host_packet_header_t);
}

static void host_handle_reply_locked(int len) {
    if (len < (int)sizeof(client_packet_header_t)) return;
    client_packet_header_t *pkt = (client_packet_header_t *)rx_buf;

    if (pkt->msg_type == piconet_msg_client_lobby) {
        if (len < (int)sizeof(client_lobby_msg_t)) return;
        client_lobby_msg_t *lobby_msg = (client_lobby_msg_t *)pkt;
        if (local_state.host.client.id == pkt->client_id && pkt->client_id) {
            // bekannter Mitspieler: nur Buchhaltung
            local_state.host.client.last_rx_time = time_us_32();
            local_state.host.client.last_acked_by_client_seq = lobby_msg->last_rx_seq;
            return;
        }
        // neuer Anklopfer
        if (synced_state.status != in_lobby) {
            local_state.host.reject_reason = piconet_msg_game_already_started;
            return;
        }
        if ((pkt->compat_hash & 0xffffff) != get_compat_hash()) {
            local_state.host.reject_reason = piconet_msg_game_not_compatible;
            return;
        }
        if (local_state.host.client.id) {
            local_state.host.reject_reason = piconet_msg_game_full;
            return;
        }
        piconet_info("USBLINK: client joined\n");
        local_state.host.client.id = pkt->client_id;
        local_state.host.client.last_rx_time = time_us_32();
        local_state.host.client.last_acked_by_client_seq = 0;
        local_state.host.client.last_sent_by_client_tic = -1;
        local_state.host.client.last_acked_by_client_tic = -1;
        synced_state.lobby.players[1].client_id = pkt->client_id;
        set_local_player_names();
        synced_state.lobby.nplayers = 2;
        synced_state.lobby.seq++;
    } else if (pkt->msg_type == piconet_msg_client_tic) {
        if (len < (int)sizeof(client_tic_msg_t)) return;
        // Kein Kennungsvergleich mehr: das Tic-Paket traegt keine, und es
        // braucht auch keine -- an einem Kabel haengt genau eine Gegenstelle,
        // und der Rahmen ist per Pruefsumme gesichert.
        if (!local_state.host.client.id) return;
        client_tic_msg_t *tic_msg = (client_tic_msg_t *)pkt;
        local_state.host.client.last_rx_time = time_us_32();
        local_state.host.client.last_acked_by_client_tic = tic_msg->last_rx_tic;
        local_state.host.got_tic = tic_msg->enclosed_tic != -1;
        if (tic_msg->enclosed_tic != -1) {
            if (tic_msg->enclosed_tic > local_state.host.last_complete_tic &&
                tic_msg->enclosed_tic < local_state.host.limit_tic) {
                ticdata[tic_msg->enclosed_tic % BACKUPTICS].cmds[1] = tic_msg->ticcmd;
                local_state.host.client.last_sent_by_client_tic = tic_msg->enclosed_tic;
                host_check_tic_advance_locked();
            }
        }
    }
}

// Nur noch der zweite Teil: der Slave hat bereits Ready gemeldet.
static void host_exchange(void) {
    critical_section_enter_blocking(&critsec);
    int txlen = host_build_packet_locked();
    critical_section_exit(&critsec);

    piconet_dbg_exchanges++;
    int rc = usblink_master_finish_exchange(tx_buf, txlen, rx_buf, sizeof(rx_buf));
    piconet_dbg_last_rc = rc;
    link_rate_feedback(rc);

    critical_section_enter_blocking(&critsec);
    if (rc >= 0) {
        piconet_dbg_last_msg = ((client_packet_header_t *)rx_buf)->msg_type;
        host_handle_reply_locked(rc);
    }
    critical_section_exit(&critsec);
}

// Ein Tic ist komplett, sobald er von uns und (falls verbunden) vom Client
// vorliegt.
static void host_check_tic_advance_locked(void) {
    bool advance;
    do {
        advance = local_state.host.local_tic > local_state.host.last_complete_tic;
        if (advance && local_state.host.client.id) {
            advance = local_state.host.client.last_sent_by_client_tic > local_state.host.last_complete_tic;
        }
        if (advance) {
            last_tic_progress = time_us_32();
            local_state.host.last_complete_tic++;
            int slot = local_state.host.last_complete_tic % BACKUPTICS;
            ticdata[slot].cmds[0].ingame = true;
            ticdata[slot].cmds[1].ingame = local_state.host.client.id != 0;
            for (int i = 2; i < NET_MAXPLAYERS; i++) ticdata[slot].cmds[i].ingame = false;
        }
    } while (advance);
}

// ---------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------
static int client_build_packet_locked(void) {
    client_packet_header_t *pkt = (client_packet_header_t *)tx_buf;

    if (synced_state.status == in_game) {
        client_tic_msg_t *tic_msg = (client_tic_msg_t *)tx_buf;
        tic_msg->msg_type = piconet_msg_client_tic;
        tic_msg->last_rx_tic = local_state.client.last_received_from_server_tic;
        if (local_state.client.last_server_acked_tic < local_state.client.last_local_tic) {
            tic_msg->enclosed_tic = local_state.client.last_server_acked_tic + 1;
            tic_msg->ticcmd = ticdata[tic_msg->enclosed_tic % BACKUPTICS].cmds[consoleplayer];
        } else {
            tic_msg->enclosed_tic = -1;
        }
        return sizeof(client_tic_msg_t);
    }

    client_lobby_msg_t *lobby_msg = (client_lobby_msg_t *)pkt;
    pkt->msg_type = piconet_msg_client_lobby;
    pkt->client_id = local_state.client.client_id;
    pkt->compat_hash = get_compat_hash();
    lobby_msg->last_rx_seq = synced_state.lobby.seq;
    return sizeof(client_lobby_msg_t);
}

static void client_handle_host_packet_locked(int len) {
    if (len < 4) return;
    host_packet_header_t *hdr = (host_packet_header_t *)rx_buf;

    // Tic-Pakete tragen keine Kennung mehr (sie sparen die vier Byte), also
    // muss der Nachrichtentyp vor jedem Zugriff auf den Kopf geprueft werden.
    if (hdr->msg_type != piconet_msg_host_tic) {
        if (len < (int)sizeof(host_packet_header_t)) return;
        if (hdr->client_id && hdr->client_id != local_state.client.client_id) {
            return; // gilt einem anderen Client
        }
    }
    local_state.client.last_rx_time = time_us_32();

    if (hdr->msg_type == piconet_msg_host_lobby) {
        if (len < (int)sizeof(host_lobby_msg_t)) return;
        if (synced_state.status == in_lobby) {
            const host_lobby_msg_t *msg = (const host_lobby_msg_t *)hdr;
            memset(&synced_state.lobby, 0, sizeof(lobby_state_t));
            synced_state.lobby.compat_hash = msg->compat_hash;
            synced_state.lobby.seq = msg->seq;
            synced_state.lobby.status = (piconet_lobby_status_t)msg->status;
            synced_state.lobby.nplayers = msg->nplayers;
            synced_state.lobby.deathmatch = msg->deathmatch;
            synced_state.lobby.epi = msg->epi;
            synced_state.lobby.skill = msg->skill;
            local_state.client.player_num = -1;
            for (int i = 0; i < WIRE_MAX_PLAYERS; i++) {
                synced_state.lobby.players[i].client_id = msg->player_ids[i];
                if (msg->player_ids[i] == local_state.client.client_id) {
                    local_state.client.player_num = (int8_t)i;
                }
            }
            set_local_player_names();
        }
    } else if (hdr->msg_type == piconet_msg_host_tic) {
        if (len < (int)sizeof(host_tic_msg_hdr_t)) return;
        // Tics vom Host koennen nur kommen, wenn das Spiel laeuft. Falls das
        // eigentliche Startpaket (Lobby mit game_started) unterwegs verloren
        // ging, ist das hier das Ersatzsignal fuer den Menue-Code -- sonst
        // bleibt der Client im Wartebildschirm stehen.
        if (synced_state.lobby.status != lobby_game_started) {
            synced_state.lobby.status = lobby_game_started;
        }
        synced_state.status = in_game;      // startet das Spiel beim Client
        host_tic_msg_t *tic_msg = (host_tic_msg_t *)hdr;
        local_state.client.last_server_acked_tic = tic_msg->hdr.client_ack_tic;
        if (tic_msg->hdr.enclosed_tic != -1 && len >= (int)sizeof(host_tic_msg_t)) {
            if (tic_msg->hdr.enclosed_tic == local_state.client.last_received_from_server_tic + 1 &&
                tic_msg->hdr.enclosed_tic < local_state.client.limit_tic) {
                last_tic_progress = time_us_32();
                local_state.client.last_received_from_server_tic = tic_msg->hdr.enclosed_tic;
                int slot = tic_msg->hdr.enclosed_tic % BACKUPTICS;
                // Befehl des Hosts uebernehmen; der eigene steht schon im
                // selben Platz (die Spielschleife hat ihn dort abgelegt, bevor
                // er an den Host ging).
                ticdata[slot].cmds[0] = tic_msg->cmd_host;
                ticdata[slot].cmds[1].ingame = true;
                for (int i = WIRE_MAX_PLAYERS; i < NET_MAXPLAYERS; i++) {
                    ticdata[slot].cmds[i].ingame = false;
                }
            }
        }
    } else if (hdr->msg_type == piconet_msg_game_start) {
        // Startsignal des Hosts -- der Menue-Code springt daraufhin ins Spiel.
        synced_state.lobby.status = lobby_game_started;
    } else if (synced_state.status == in_lobby) {
        if (hdr->msg_type == piconet_msg_game_already_started) {
            synced_state.lobby.status = lobby_game_started;
        } else if (hdr->msg_type == piconet_msg_game_not_compatible) {
            synced_state.lobby.status = lobby_game_not_compatible;
        }
    }

    if (synced_state.status == in_lobby && synced_state.lobby.status == lobby_no_connection) {
        synced_state.lobby.status = lobby_waiting_for_start;
    }
}

// Wird mitten im Austausch aufgerufen: das Paket des Hosts liegt bereits in
// rx_buf, die Antwort geht gleich darauf raus. Dadurch quittiert der Client
// einen Tic im SELBEN Austausch statt erst im naechsten -- das halbiert die
// Zahl der Runden, die ein Tic braucht.
static int client_make_reply(int rxlen, void *user) {
    (void)user;
    critical_section_enter_blocking(&critsec);
    if (rxlen >= 0) {
        piconet_dbg_last_msg = ((host_packet_header_t *)rx_buf)->msg_type;
        client_handle_host_packet_locked(rxlen);
    }
    int txlen = client_build_packet_locked();
    critical_section_exit(&critsec);
    return txlen;
}

static void client_exchange(void) {
    critical_section_enter_blocking(&critsec);
    int txlen = client_build_packet_locked();   // Vorbelegung, falls kein Paket kommt
    critical_section_exit(&critsec);

    piconet_dbg_exchanges++;
    int rc = usblink_slave_exchange_cb(rx_buf, sizeof(rx_buf), tx_buf, txlen,
                                       client_make_reply, NULL);
    if (rc != -4) piconet_dbg_last_rc = rc;   // -4 = war schon erledigt
}

// ---------------------------------------------------------------------------
// Takt
// ---------------------------------------------------------------------------
// Ein Schritt der Zustandsmaschine. Laeuft aus zwei Quellen: dem periodischen
// Timer UND direkt aus der Spielschleife (piconet_poll). Damit haengt die
// Verbindung nicht daran, dass der Timer-IRQ zuverlaessig feuert.
// Rueckgabe: Wartezeit in us bis zum naechsten sinnvollen Schritt.
// Waren wir selbst laenger nicht dran (Levelladen, langer Renderframe,
// gesperrte Interrupts), dann war das keine Funkstille der Gegenstelle --
// diese Zeit wird der Verbindung deshalb gutgeschrieben. Ohne das faellt der
// Link ausgerechnet beim Spielstart um, wo beide Geraete erst mal ein Level
// aufbauen und minutenlang wirken koennen.
static void piconet_credit_stall(void) {
    uint32_t now = time_us_32();
    uint32_t gap = now - last_step_time;
    last_step_time = now;
    if (gap < 100000) return;
    if (role == role_host) {
        local_state.host.client.last_rx_time += gap;
    } else if (role == role_client) {
        local_state.client.last_rx_time += gap;
    }
}

static uint32_t piconet_step(void) {
    piconet_credit_stall();
    if (role == role_host) {
        if (!local_state.host.attn_active) {
            // Takt einhalten. Der zurueckgegebene Wert steuert nur den Timer --
            // die Spielschleife ruft uns aber unabhaengig davon auf und hat
            // deshalb bisher sofort wieder angeklopft. Ergebnis: 82 Austausche
            // je Sekunde statt der eingestellten 55, und die Rechenzeit dafuer
            // fehlte dem Spiel.
            uint32_t now = time_us_32();
            if ((int32_t)(now - local_state.host.next_exchange) < 0) {
                return ATTN_POLL_US;
            }
            // Anfrage stellen und stehen lassen -- kein blockierendes Warten.
            if (usblink_master_begin_attention()) {
                local_state.host.attn_active = true;
                local_state.host.attn_t0 = time_us_32();
                return ATTN_POLL_US;
            }
            piconet_dbg_last_rc = -3;       // Leitung belegt
            return IDLE_PERIOD_US;
        }
        if (usblink_master_slave_ready()) {
            // Sofort loslegen: der Client haelt sein Ready nur wenige
            // Millisekunden und wartet dabei mit gesperrten Interrupts. Jedes
            // Zoegern hier kostet ihn einen Fehlschlag und dem Spiel Zeit.
            local_state.host.attn_active = false;
            host_exchange();
            usblink_master_abort();
            uint32_t period = synced_state.status == in_game ? GAME_PERIOD_US : PERIOD_US;
            // Hat keine Seite einen Tic beigesteuert, war die Runde umsonst --
            // dann lieber etwas warten, statt Rechenzeit fuer nichts
            // auszugeben. Sobald wieder etwas fliesst, geht es sofort im
            // normalen Takt weiter.
            if (synced_state.status == in_game &&
                !local_state.host.sent_tic && !local_state.host.got_tic) {
                period *= 2;
            }
            local_state.host.next_exchange = time_us_32() + period;
            return period;
        }
        if (time_us_32() - local_state.host.attn_t0 > ATTN_HOLD_US) {
            local_state.host.attn_active = false;
            usblink_master_abort();
            piconet_dbg_last_rc = -1;       // niemand da
            return IDLE_PERIOD_US;
        }
        return ATTN_POLL_US;
    }
    if (role == role_client) {
        if (usblink_slave_request_pending()) {
            client_exchange();
        }
        return CLIENT_POLL_US;
    }
    return PERIOD_US;
}

static void periodic_tick(uint timer) {
    (void)timer;
    piconet_dbg_ticks++;
    if (role == role_none) return;   // kein Nachlegen -> Timer bleibt stehen
    arm_alarm(piconet_step());
}

// Zweiter, vom Timer unabhaengiger Pollpfad aus der Spielschleife
// (Lobby-Bildschirm und jeder Tic).
void piconet_poll(void) {
    if (role == role_none) return;
    // d_loop ruft die Tic-Hooks in engen Schleifen auf; ohne Bremse liefe das
    // Spiel praktisch nur noch in der Verbindungslogik.
    static uint32_t last_poll;
    uint32_t now = time_us_32();
    if (now - last_poll < 400) return;
    last_poll = now;
    piconet_dbg_polls++;
    // Den Timer mitziehen: hat die Spielschleife gerade eine Anfrage gestellt,
    // muss der Timer ab jetzt im 250-us-Takt nach dem Ready schauen und nicht
    // erst in 12 ms. Sonst wartet der Client waehrend eines langen Frames
    // vergeblich und laeuft reihenweise in Timeouts -- er haelt sein Ready nur
    // wenige Millisekunden.
    arm_alarm(piconet_step());
}

// ---------------------------------------------------------------------------
// Oeffentliche API
// ---------------------------------------------------------------------------
void piconet_init(void) {
    bi_decl_if_func_used(bi_program_feature("USB-C link multi-player"));
    critical_section_init(&critsec);
    hardware_alarm_set_callback(PERIODIC_ALARM_NUM, periodic_tick);
    uint irq = TIMER_ALARM_IRQ_NUM(PICO_DEFAULT_TIMER_INSTANCE(), PERIODIC_ALARM_NUM);
    irq_set_priority(irq, 0xc0);
    irq_set_enabled(irq, true);
}

static void clear_state(void) {
    memset(&local_state, 0, sizeof(local_state));
    memset(&synced_state, 0, sizeof(synced_state));
    piconet_dbg_stop_reason = 0;
    piconet_dbg_stop_info = 0;
}

void piconet_start_host(int8_t deathmatch, int8_t epi, int8_t skill) {
    critical_section_enter_blocking(&critsec);
    hardware_alarm_cancel(PERIODIC_ALARM_NUM);
    clear_state();
    role = role_host;

    uint32_t host_id = time_us_32() | 1u;
    synced_state.status = in_lobby;
    synced_state.lobby.status = lobby_waiting_for_start;
    synced_state.lobby.compat_hash = get_compat_hash();
    synced_state.lobby.players[0].client_id = host_id;
    set_local_player_names();
    synced_state.lobby.nplayers = 1;
    synced_state.lobby.deathmatch = deathmatch;
    synced_state.lobby.epi = epi;
    synced_state.lobby.skill = skill;
    synced_state.lobby.seq = 1;

    local_state.host.local_tic = -1;
    local_state.host.last_complete_tic = -1;
    local_state.host.limit_tic = BACKUPTICS - 1;
    local_state.host.client.last_sent_by_client_tic = -1;
    local_state.host.client.last_acked_by_client_tic = -1;
    last_step_time = time_us_32();
    critical_section_exit(&critsec);

    usblink_init();
    link_window_n = link_window_fails = 0; link_clean_run = 0;
    arm_alarm(IDLE_PERIOD_US);
}

void piconet_start_client(void) {
    critical_section_enter_blocking(&critsec);
    hardware_alarm_cancel(PERIODIC_ALARM_NUM);
    clear_state();
    role = role_client;
    synced_state.status = in_lobby;
    synced_state.lobby.status = lobby_no_connection;
    local_state.client.client_id = time_us_32() | 1u;
    local_state.client.player_num = -1;
    local_state.client.last_local_tic = -1;
    local_state.client.last_server_acked_tic = -1;
    local_state.client.last_received_from_server_tic = -1;
    local_state.client.limit_tic = BACKUPTICS - 1;
    local_state.client.last_rx_time = time_us_32();
    last_step_time = time_us_32();
    critical_section_exit(&critsec);

    usblink_init();
    arm_alarm(CLIENT_POLL_US);
}

bool piconet_client_check_for_dropped_connection(void) {
    bool rc = false;
    critical_section_enter_blocking(&critsec);
    if (role == role_client && synced_state.status == in_lobby &&
        synced_state.lobby.status != lobby_game_started) {
        uint32_t timeout = synced_state.lobby.status == lobby_waiting_for_start
                           ? LOBBY_TIMEOUT_SHORT_US : LOBBY_TIMEOUT_LONG_US;
        if (time_us_32() - local_state.client.last_rx_time > timeout) {
            synced_state.lobby.status = lobby_no_connection;
            synced_state.lobby.seq = 0;
            synced_state.lobby.nplayers = 0;
            memset(synced_state.lobby.players, 0, sizeof(synced_state.lobby.players));
            local_state.client.player_num = -1;
            rc = true;
        }
    }
    critical_section_exit(&critsec);
    return rc;
}

void piconet_stop(void) {
    if (role == role_host && local_state.host.attn_active) {
        local_state.host.attn_active = false;
        usblink_master_abort();
    }
    critical_section_enter_blocking(&critsec);
    if (role != role_none) {
        hardware_alarm_cancel(PERIODIC_ALARM_NUM);
        role = role_none;
        usblink_deinit();
    }
    critical_section_exit(&critsec);
}

int piconet_get_lobby_state(lobby_state_t *state) {
    piconet_poll();
    critical_section_enter_blocking(&critsec);
    int rc;
    if (role == role_client) {
        rc = local_state.client.player_num;
    } else if (role == role_host) {
        rc = 0;
    } else {
        rc = -1;
    }
    memcpy(state, &synced_state.lobby, sizeof(lobby_state_t));
    critical_section_exit(&critsec);
    return rc;
}

void piconet_start_game(void) {
    critical_section_enter_blocking(&critsec);
    uint32_t now = time_us_32();
    last_step_time = now;
    last_tic_progress = now;
    if (role == role_host) local_state.host.client.last_rx_time = now;
    else if (role == role_client) local_state.client.last_rx_time = now;
    synced_state.status = in_game;
    synced_state.lobby.status = lobby_game_started;
    if (role == role_host) {
        synced_state.lobby.seq++;
    }
    critical_section_exit(&critsec);
}

void piconet_new_local_tic(int tic) {
    piconet_poll();
    critical_section_enter_blocking(&critsec);
    if (role == role_client) {
        local_state.client.last_local_tic = tic;
    } else if (role == role_host) {
        local_state.host.local_tic = tic;
        host_check_tic_advance_locked();
    }
    critical_section_exit(&critsec);
}

int piconet_debug_line_state(void) {
    if (!usblink_is_active() || role == role_none) return -1;
    return (int)usblink_line_state();
}

bool piconet_debug_is_host(void) {
    return role == role_host;
}

// Waehrend des Spiels ist interessanter als jede Fehlerstatistik, ob die Tics
// ueberhaupt vorankommen: eigener Tic gegen den der Gegenstelle.
// Nur lesen -- die Anzeige laeuft auf Core1, dort darf die Zustandsmaschine
// nicht angestossen werden.
bool piconet_debug_lobby(int *nplayers, int *pnum) {
    if (synced_state.status != in_lobby) return false;
    *nplayers = synced_state.lobby.nplayers;
    if (role == role_host) *pnum = 0;
    else if (role == role_client) *pnum = local_state.client.player_num;
    else *pnum = -1;
    return true;
}

// Millisekunden seit dem letzten zustande gekommenen Tic (0 = laeuft, oder
// kein Netzspiel).
uint32_t piconet_stalled_ms(void) {
    if (synced_state.status != in_game) return 0;
    return (time_us_32() - last_tic_progress) / 1000;
}

bool piconet_debug_tics(int *own, int *remote) {
    if (synced_state.status != in_game) return false;
    if (role == role_host) {
        *own = local_state.host.local_tic;
        *remote = local_state.host.client.last_sent_by_client_tic;
        return true;
    }
    if (role == role_client) {
        *own = local_state.client.last_local_tic;
        *remote = local_state.client.last_received_from_server_tic;
        return true;
    }
    return false;
}

int piconet_maybe_recv_tic(int fromtic) {
    piconet_poll();
    critical_section_enter_blocking(&critsec);
    if (role == role_host) {
        if (fromtic < local_state.host.last_complete_tic) {
            fromtic++;
        } else if (local_state.host.client.id &&
                   time_us_32() - local_state.host.client.last_rx_time > GAME_TIMEOUT_US) {
            piconet_info("USBLINK: client dropped\n");
            local_state.host.client.id = 0;
            for (int j = 0; j < BACKUPTICS; j++) {
                ticdata[j].cmds[1].ingame = false;
            }
            host_check_tic_advance_locked();
        }
        local_state.host.limit_tic = fromtic + BACKUPTICS - 1;
    } else if (role == role_client) {
        if (fromtic < local_state.client.last_received_from_server_tic) {
            fromtic++;
        } else if (time_us_32() - local_state.client.last_rx_time > GAME_TIMEOUT_US) {
            net_client_connected = false;
            for (int i = 0; i < NET_MAXPLAYERS; i++) {
                if (i != consoleplayer) {
                    for (int j = 0; j < BACKUPTICS; j++) {
                        ticdata[j].cmds[i].ingame = false;
                    }
                }
            }
        }
        local_state.client.limit_tic = fromtic + BACKUPTICS - 1;
    }
    critical_section_exit(&critsec);
    return fromtic;
}

#else // !PICO_ON_DEVICE  -- Stub fuer den SDL-Build

static enum {
    none,
    client,
    host,
    host_game,
} state;
static int host_tic;

uint8_t piconet_dbg_stop_reason;
uint8_t piconet_dbg_stop_info;
uint32_t piconet_dbg_ticks;
uint32_t piconet_dbg_polls;
uint32_t piconet_dbg_exchanges;
uint32_t piconet_dbg_missed;
int32_t  piconet_dbg_last_rc;
uint32_t piconet_dbg_last_msg;
int piconet_debug_line_state(void) { return -1; }
bool piconet_debug_is_host(void) { return false; }
bool piconet_debug_tics(int *own, int *remote) { (void)own; (void)remote; return false; }
bool piconet_debug_lobby(int *n, int *p) { (void)n; (void)p; return false; }
uint32_t piconet_stalled_ms(void) { return 0; }
void piconet_poll(void) {}

void piconet_init() {}
void piconet_start_host(int8_t deathmatch, int8_t epi, int8_t skill) {
    state = host;
}
void piconet_start_client() {
    state = client;
}
void piconet_stop() {
    state = none;
}
bool piconet_client_check_for_dropped_connection() {
    return true;
}

void piconet_start_game() {
    if (state == host) {
        state = host_game;
    }
}

void piconet_new_local_tic(int tic) {
    host_tic = tic;
}

int piconet_maybe_recv_tic(int fromtic) {
    if (host_tic > fromtic) fromtic++;
    return fromtic;
}

int piconet_get_lobby_state(lobby_state_t *ls) {
    memset(ls, 0, sizeof(lobby_state_t));
    if (state == host || state == host_game) {
        ls->status = state == host ? lobby_waiting_for_start : lobby_game_started;
        ls->players[0].client_id = 1;
        ls->nplayers = 1;
        memcpy(ls->players[0].name, player_name, MAXPLAYERNAME);
        return 0;
    }
    ls->status = lobby_no_connection;
    return -1;
}

#endif
