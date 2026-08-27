/*
 * pbc_link -- Multiplayer-Transport zwischen zwei PicoBoy Color Plus.
 *
 * Darunter liegt usblink (unveraendert aus der Doom-Portierung uebernommen):
 * ein getakteter Halbduplex-Link, der die beiden USB-PHY-Leitungen D+/D- als
 * gewoehnliche IOs benutzt. Kein USB-Protokoll, keine Enumeration, kein VBUS --
 * nur ein USB-C-Kabel zwischen den Geraeten.
 *
 * Darueber liegt OpenTyrians eigener Netzwerkcode, unveraendert. Diese Datei
 * ist die Schicht dazwischen: sie fuellt und leert die beiden Paket-
 * warteschlangen, die die SDL_net-Attrappe bedient.
 *
 * Rollen: der Gastgeber ("Host Game" im Menue) ist Master und taktet die
 * Leitung, der Beitretende ("Join Game") ist Slave und laeuft flanken-
 * gesteuert. Die Wahl faellt zur Laufzeit im Menue, beide Geraete tragen
 * dieselbe Firmware.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_LINK_H
#define PBC_LINK_H

#include <stdint.h>
#include <stdbool.h>

/* Zustand fuer die Anzeige in der Leiste. */
enum
{
	PBC_LINK_OFF = 0,   /* kein Netzspiel */
	PBC_LINK_WAIT,      /* eingeschaltet, aber noch nichts empfangen */
	PBC_LINK_UP,        /* Pakete fliessen */
	PBC_LINK_LOST       /* laengere Zeit nichts mehr gehoert */
};

typedef enum
{
	PBC_ROLE_NONE = 0,
	PBC_ROLE_HOST,      /* Master, taktet */
	PBC_ROLE_JOIN       /* Slave, flankengesteuert */
} pbc_link_role_t;

/* Rolle festlegen und den Link hochfahren. PBC_ROLE_NONE faehrt ihn herunter
   und gibt die PHY-Leitungen wieder frei. */
void pbc_link_start(pbc_link_role_t role);
void pbc_link_stop(void);

pbc_link_role_t pbc_link_role(void);

/*
 * Den Link bedienen: als Master einen Austausch anstossen bzw. abschliessen,
 * als Slave nachsehen, ob der Master anklopft.
 *
 * Muss oft gerufen werden -- schon 60 ms Pause koennen die Verbindung
 * festfahren. Der Port ruft ihn deshalb zwischen je zwei Bildstreifen (alle
 * ~0,5 ms) und zusaetzlich in jedem Durchlauf der Spielschleife. Tut nichts,
 * solange kein Netzspiel laeuft.
 */
void pbc_link_pump(void);

/*
 * Warten, ohne den Link blind zu lassen. Ersatz fuer SDL_Delay an allen
 * Stellen, die im Netzspiel erreicht werden koennen -- ein schlichtes
 * sleep_ms() waere dort genau die Pause, die die Verbindung reisst.
 */
void pbc_link_delay(uint32_t ms);

/* ------------------------------------------------------------- Anzeige */

int pbc_link_state(void);
const char *pbc_link_state_name(int state);

/* Zaehler erfolgreicher Austausche. In der Leiste sichtbar, damit man beim
   Fehlersuchen Messwerte hat statt Vermutungen -- die serielle Schnittstelle
   ist waehrend eines Netzspiels vom Link belegt. */
uint32_t pbc_link_counter(void);

#endif /* PBC_LINK_H */
