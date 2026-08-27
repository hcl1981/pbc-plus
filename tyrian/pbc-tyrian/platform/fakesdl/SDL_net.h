/*
 * SDL_net-Attrappe -- setzt OpenTyrians Netzwerkcode auf den USB-Link zwischen
 * zwei PicoBoys.
 *
 * Der Kernbefund, der diesen Weg ueberhaupt moeglich macht: OpenTyrians
 * Mehrspielermodus ist ein deterministischer Gleichschritt. Beide Geraete
 * rechnen dieselbe Simulation und tauschen je Bild nur ein 28-Byte-Paket mit
 * den Tasteneingaben aus. Es werden also KEINE Objektlisten uebertragen -- das
 * waere ueber diese Leitung aussichtslos, 28 Byte dagegen sind muehelos.
 *
 * Damit bleibt network.c unveraendert: es ruft weiter SDLNet_UDP_Send und
 * SDLNet_UDP_Recv, nur liegt darunter kein UDP-Stapel mehr, sondern zwei
 * Warteschlangen, die pbc_link.c ueber das USB-C-Kabel leert und fuellt.
 * Auch die Absicherung von network.c (Quittungen, Wiederholungen, das
 * XOR-Ersatzpaket gegen Verluste) bleibt damit erhalten -- und die brauchen
 * wir hier mehr als in einem LAN.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_FAKE_SDL_NET_H
#define PBC_FAKE_SDL_NET_H

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Groesste Nutzlast, die ein Austausch traegt. Der Link fasst 60 Byte in der
 * einen und 40 in der anderen Richtung; 36 ist das gemeinsame Maass abzueglich
 * des eigenen Kopfes. OpenTyrians groesstes Paket ist das Verbindungspaket mit
 * 12 + Namenslaenge + 1 Byte -- deshalb vergibt dieser Port feste kurze Namen
 * (P1/P2) statt der Namenseingabe, die sich mit sieben Knoepfen ohnehin nicht
 * bedienen liesse. Die Zustandspakete im Spiel sind 28 Byte.
 */
#define PBC_NET_MAX_PAYLOAD 36

typedef struct
{
	Uint32 host;
	Uint16 port;
} IPaddress;

typedef struct
{
	int channel;
	Uint8 *data;
	int len;
	int maxlen;
	int status;
	IPaddress address;
} UDPpacket;

typedef struct _UDPsocket *UDPsocket;

int  SDLNet_Init(void);
void SDLNet_Quit(void);
const char *SDLNet_GetError(void);

UDPsocket SDLNet_UDP_Open(Uint16 port);
void SDLNet_UDP_Close(UDPsocket sock);
int  SDLNet_UDP_Bind(UDPsocket sock, int channel, const IPaddress *address);
int  SDLNet_UDP_Send(UDPsocket sock, int channel, UDPpacket *packet);
int  SDLNet_UDP_Recv(UDPsocket sock, UDPpacket *packet);

UDPpacket *SDLNet_AllocPacket(int size);
void SDLNet_FreePacket(UDPpacket *packet);

int SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port);

/* Die beiden Zahlenfunktionen sind im Original Big Endian ("network byte
   order"). Das bleibt so -- beide Seiten sind zwar derselbe Prozessor, aber
   ein stillschweigender Formatwechsel waere genau die Art Aenderung, die man
   spaeter beim Fehlersuchen nicht mehr findet. */
static inline void SDLNet_Write16(Uint16 value, void *area)
{
	Uint8 *p = (Uint8 *)area;
	p[0] = (Uint8)(value >> 8);
	p[1] = (Uint8)(value & 0xFF);
}

static inline Uint16 SDLNet_Read16(const void *area)
{
	const Uint8 *p = (const Uint8 *)area;
	return (Uint16)((p[0] << 8) | p[1]);
}

#ifdef __cplusplus
}
#endif

#endif /* PBC_FAKE_SDL_NET_H */
