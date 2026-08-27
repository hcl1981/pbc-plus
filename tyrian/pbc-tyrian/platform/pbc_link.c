/*
 * pbc_link -- Umsetzung. Enthaelt zugleich die SDL_net-Attrappe, weil beide
 * dieselben zwei Warteschlangen bedienen.
 *
 * Aufbau von unten nach oben:
 *
 *   usblink.c            Bits ueber D+/D-, ein Austausch je Aufruf
 *   pbc_link_pump()      stoesst Austausche an, fuellt/leert die Schlangen
 *   SDLNet_UDP_Send/Recv legen Pakete hinein bzw. holen sie heraus
 *   network.c            OpenTyrians Netzcode, unveraendert
 *
 * GPLv2, wie OpenTyrian.
 */

#include "SDL.h"
#include "SDL_net.h"

#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"

#include "pbc_link.h"
#include "usblink.h"

/* ------------------------------------------------------ Warteschlangen */

/*
 * Acht Plaetze je Richtung. OpenTyrian haelt selbst eine Warteschlange von 16
 * unquittierten Paketen; hier geht es nur um die wenigen, die zwischen zwei
 * Austauschen anfallen. Mehr zu puffern wuerde die Verzoegerung erhoehen, ohne
 * etwas zu retten.
 */
#define QUEUE_LEN 8

typedef struct
{
	Uint8 data[PBC_NET_MAX_PAYLOAD];
	int len;
} queued_packet;

typedef struct
{
	queued_packet slot[QUEUE_LEN];
	int head, count;
} packet_queue;

static packet_queue tx_queue, rx_queue;

static bool queue_push(packet_queue *q, const Uint8 *data, int len)
{
	if (q->count >= QUEUE_LEN || len > PBC_NET_MAX_PAYLOAD)
		return false;

	int slot = (q->head + q->count) % QUEUE_LEN;
	memcpy(q->slot[slot].data, data, (size_t)len);
	q->slot[slot].len = len;
	++q->count;
	return true;
}

static bool queue_peek(packet_queue *q, const Uint8 **data, int *len)
{
	if (q->count == 0)
		return false;
	*data = q->slot[q->head].data;
	*len = q->slot[q->head].len;
	return true;
}

static void queue_pop(packet_queue *q)
{
	if (q->count == 0)
		return;
	q->head = (q->head + 1) % QUEUE_LEN;
	--q->count;
}

/* ------------------------------------------------------------ Zustand */

static pbc_link_role_t role = PBC_ROLE_NONE;
static int link_state = PBC_LINK_OFF;
static uint32_t ok_counter;
static uint32_t last_rx_ms;
static uint32_t last_attempt_ms;
static bool attention_pending;

/*
 * Wie lange darf nichts kommen, bevor die Verbindung als weg gilt?
 *
 * Grosszuegig gewaehlt, und zwar aus einem konkreten Grund: der Levelwechsel
 * und der Ladenbildschirm koennen das Spiel Sekunden beschaeftigen. Eine kurze
 * Schwelle reisst die Verbindung dann ausgerechnet an der Stelle, an der sie
 * gebraucht wird. Zusaetzlich wird unten die eigene Blindheit verrechnet.
 */
#define LINK_LOST_MS 4000

/* Wie oft der Master anklopft, wenn niemand antwortet. Waehrend des Spiels
   laeuft ohnehin je Bild ein Austausch; das hier bremst nur die Lobby. */
#define IDLE_POLL_MS 30

static uint32_t now_ms(void)
{
	return (uint32_t)(time_us_64() / 1000u);
}

/* ------------------------------------------------------------- Start */

void pbc_link_start(pbc_link_role_t r)
{
	if (r == PBC_ROLE_NONE)
	{
		pbc_link_stop();
		return;
	}

	role = r;
	memset(&tx_queue, 0, sizeof tx_queue);
	memset(&rx_queue, 0, sizeof rx_queue);
	ok_counter = 0;
	attention_pending = false;
	link_state = PBC_LINK_WAIT;
	last_rx_ms = now_ms();
	last_attempt_ms = 0;

	usblink_init();
}

void pbc_link_stop(void)
{
	if (role != PBC_ROLE_NONE)
		usblink_deinit();

	role = PBC_ROLE_NONE;
	link_state = PBC_LINK_OFF;
	attention_pending = false;
}

pbc_link_role_t pbc_link_role(void) { return role; }

/* -------------------------------------------------------------- Pumpe */

/*
 * Ein Rahmen traegt [len][payload]. len == 0 heisst "ich habe gerade nichts zu
 * sagen" -- das kommt oft vor und ist kein Fehler: der Takt laeuft weiter,
 * damit die Gegenseite ihrerseits senden kann.
 */
#define FRAME_HDR 1

static int build_frame(Uint8 *frame, int frame_max)
{
	const Uint8 *data;
	int len;

	if (!queue_peek(&tx_queue, &data, &len) || len + FRAME_HDR > frame_max)
	{
		frame[0] = 0;
		return FRAME_HDR;
	}

	frame[0] = (Uint8)len;
	memcpy(frame + FRAME_HDR, data, (size_t)len);
	return FRAME_HDR + len;
}

static void consume_frame(const Uint8 *frame, int len)
{
	if (len < FRAME_HDR)
		return;

	int payload = frame[0];
	if (payload == 0 || payload > len - FRAME_HDR)
		return;

	queue_push(&rx_queue, frame + FRAME_HDR, payload);
}

/* Rueckruf fuer den Slave: zwischen Empfangs- und Sendephase darf er seine
   Antwort zusammenstellen, sodass beides in denselben Austausch passt. */
static int slave_make_reply(int rxlen, void *user)
{
	(void)rxlen;
	Uint8 *tx = (Uint8 *)user;
	return build_frame(tx, USBLINK_S2M_PAYLOAD);
}

static void note_success(int rxlen, const Uint8 *rx)
{
	++ok_counter;
	last_rx_ms = now_ms();
	link_state = PBC_LINK_UP;

	/* Der eigene Rahmen ist raus -- erst jetzt darf er aus der Schlange. */
	queue_pop(&tx_queue);

	if (rxlen > 0)
		consume_frame(rx, rxlen);
}

void pbc_link_pump(void)
{
	if (role == PBC_ROLE_NONE)
		return;

	uint32_t t = now_ms();

	if (role == PBC_ROLE_HOST)
	{
		if (!attention_pending)
		{
			/* Wenn nichts zu senden ist und nichts ankommt, nicht dauernd
			   anklopfen -- das haelt die Lobby fluessig. */
			if (tx_queue.count == 0 && link_state != PBC_LINK_UP &&
			    (t - last_attempt_ms) < IDLE_POLL_MS)
				return;

			last_attempt_ms = t;
			attention_pending = usblink_master_begin_attention();
		}

		if (attention_pending && usblink_master_slave_ready())
		{
			Uint8 tx[USBLINK_M2S_PAYLOAD];
			Uint8 rx[USBLINK_S2M_PAYLOAD];

			int txlen = build_frame(tx, sizeof tx);
			int rxlen = usblink_master_finish_exchange(tx, txlen, rx, sizeof rx);

			attention_pending = false;

			if (rxlen >= 0)
				note_success(rxlen, rx);
		}
		else if (attention_pending && (t - last_attempt_ms) > 150)
		{
			/* Der Slave hat sich nicht gemeldet. Anfrage zuruecknehmen, sonst
			   bleibt die Taktleitung hoch stehen und der Slave sieht beim
			   naechsten Mal eine Flanke, die keine ist. */
			usblink_master_abort();
			attention_pending = false;
		}
	}
	else /* PBC_ROLE_JOIN */
	{
		if (usblink_slave_request_pending())
		{
			Uint8 tx[USBLINK_S2M_PAYLOAD];
			Uint8 rx[USBLINK_M2S_PAYLOAD];

			int rxlen = usblink_slave_exchange_cb(rx, sizeof rx, tx, sizeof tx,
			                                      slave_make_reply, tx);
			if (rxlen >= 0)
				note_success(rxlen, rx);
		}
	}

	if (link_state == PBC_LINK_UP && (t - last_rx_ms) > LINK_LOST_MS)
		link_state = PBC_LINK_LOST;
}

void pbc_link_delay(uint32_t ms)
{
	/*
	 * Warten, ohne den Link blind zu lassen. Ein schlichtes sleep_ms() waere
	 * hier genau die Pause, die die Verbindung festfahren laesst -- deshalb
	 * wird die Wartezeit in kleine Scheiben zerlegt und dazwischen gepumpt.
	 */
	uint32_t end = now_ms() + ms;
	for (;;)
	{
		pbc_link_pump();

		uint32_t t = now_ms();
		if ((int32_t)(t - end) >= 0)
			return;

		uint32_t remaining = end - t;
		sleep_us(remaining > 1 ? 500 : 100);
	}
}

/* ------------------------------------------------------------ Anzeige */

int pbc_link_state(void) { return link_state; }
uint32_t pbc_link_counter(void) { return ok_counter; }

const char *pbc_link_state_name(int state)
{
	switch (state)
	{
		case PBC_LINK_WAIT: return "LINK WAIT";
		case PBC_LINK_UP:   return "LINK OK";
		case PBC_LINK_LOST: return "LINK WEG";
		default:            return "LINK AUS";
	}
}

/* ==================================================================== */
/*  SDL_net-Attrappe                                                     */
/* ==================================================================== */

/*
 * Es gibt keine Adressen, keine Kanaele und keine Sockets -- am Kabel haengt
 * genau eine Gegenstelle. Die Funktionen sind trotzdem alle da, damit
 * network.c unveraendert uebersetzt.
 */

static struct _UDPsocket { int dummy; } the_socket;

int  SDLNet_Init(void) { return 0; }
void SDLNet_Quit(void) { pbc_link_stop(); }
const char *SDLNet_GetError(void) { return "usblink"; }

UDPsocket SDLNet_UDP_Open(Uint16 port)
{
	(void)port;
	return &the_socket;
}

void SDLNet_UDP_Close(UDPsocket sock) { (void)sock; }

int SDLNet_UDP_Bind(UDPsocket sock, int channel, const IPaddress *address)
{
	(void)sock; (void)address;
	return channel;
}

int SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port)
{
	(void)host;
	if (address)
	{
		address->host = 0;
		address->port = port;
	}
	return 0;
}

int SDLNet_UDP_Send(UDPsocket sock, int channel, UDPpacket *packet)
{
	(void)sock; (void)channel;

	if (packet == NULL || packet->len <= 0)
		return 0;

	/*
	 * Zu grosse Pakete werden verworfen statt abgeschnitten. Abgeschnitten
	 * kaeme auf der Gegenseite als gueltig aussehender Unsinn an; verworfen
	 * wiederholt network.c von selbst, weil die Quittung ausbleibt.
	 * Betrifft in der Praxis nichts: das groesste Paket im Spiel ist der
	 * Zustand mit 28 Byte.
	 */
	if (packet->len > PBC_NET_MAX_PAYLOAD)
		return 0;

	if (!queue_push(&tx_queue, packet->data, packet->len))
		return 0;

	/* Sofort anstossen: je frueher der Austausch laeuft, desto kleiner die
	   Verzoegerung, und im Spiel haengt daran die Bildrate beider Geraete. */
	pbc_link_pump();
	return 1;
}

int SDLNet_UDP_Recv(UDPsocket sock, UDPpacket *packet)
{
	(void)sock;

	pbc_link_pump();

	const Uint8 *data;
	int len;
	if (!queue_peek(&rx_queue, &data, &len))
		return 0;

	if (packet == NULL || len > packet->maxlen)
	{
		queue_pop(&rx_queue);
		return 0;
	}

	memcpy(packet->data, data, (size_t)len);
	packet->len = len;
	packet->channel = 0;
	packet->status = len;
	queue_pop(&rx_queue);
	return 1;
}

UDPpacket *SDLNet_AllocPacket(int size)
{
	UDPpacket *p = calloc(1, sizeof *p);
	if (p == NULL)
		return NULL;

	p->data = calloc(1, (size_t)size);
	if (p->data == NULL)
	{
		free(p);
		return NULL;
	}

	p->maxlen = size;
	return p;
}

void SDLNet_FreePacket(UDPpacket *packet)
{
	if (packet == NULL)
		return;
	free(packet->data);
	free(packet);
}
