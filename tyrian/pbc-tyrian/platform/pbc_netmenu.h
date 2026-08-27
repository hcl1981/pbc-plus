/*
 * pbc_netmenu -- Rollenwahl für ein Netzspiel. Siehe pbc_netmenu.c.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_NETMENU_H
#define PBC_NETMENU_H

#include <stdbool.h>

enum
{
	PBC_NET_CANCEL = 0,
	PBC_NET_HOST,
	PBC_NET_JOIN
};

/* Nur die Auswahl, ohne Wirkung. */
int pbc_choose_network_role(void);

/*
 * Auswahl zeigen und, falls nicht abgebrochen, alles für ein Netzspiel
 * vorbereiten: Rolle, Spielername, USB-Link und OpenTyrians Netzcode.
 *
 * Gibt false zurück, wenn der Spieler abgebrochen hat.
 */
bool pbc_setup_network(void);

#endif /* PBC_NETMENU_H */
