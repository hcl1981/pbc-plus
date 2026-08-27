/*
 * pbc_audio -- Tonausgabe ueber den Piezo an GP15.
 *
 * OpenTyrians loudness.c bleibt unveraendert: es ist ein reiner Mischer (OPL-
 * Emulation fuer die Musik plus acht Klangkanaele) und beruehrt SDL nur an
 * einer Stelle -- es oeffnet ein Audiogeraet und laesst sich von dessen
 * Rueckruf Puffer abholen. Genau dieses Geraet wird hier nachgebildet.
 *
 * Der Weg zum Lautsprecher:
 *
 *   loudness.c mischt Sint16 mono
 *        -> in PWM-Stellwerte 0..1023 umgerechnet
 *        -> DMA schiebt sie im Takt von 11025 Hz in den PWM-Vergleichswert
 *        -> PWM laeuft mit ~146 kHz als Traeger, der Piezo mittelt ihn weg
 *
 * Zwei Puffer im Wechsel: waehrend der eine ausgegeben wird, fuellt die
 * DMA-Unterbrechung den anderen. Es gibt keinen DAC und kein I2S auf diesem
 * Geraet -- PWM ist der einzige Weg.
 *
 * WICHTIG, und zwar aus fremdem Schaden gelernt: die Unterbrechung laeuft auf
 * der NIEDRIGSTEN Prioritaet. Sie rechnet je Aufruf einige Millisekunden
 * (OPL-Emulation), und gleichrangige Unterbrechungen verdraengen einander
 * nicht. Laege sie auf derselben Stufe wie der Multiplayer-Link, muesste
 * dessen flankenkritischer Code auf sie warten -- das zerreisst Uebertragungen
 * und sieht von aussen wie ein Wackelkontakt aus. Der Tonpuffer hat dagegen
 * Millisekunden Vorlauf und vertraegt jede Unterbrechung.
 *
 * GPLv2, wie OpenTyrian.
 */

#include "SDL.h"

#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"

#include "pbc_config.h"
#include "pbc_audio.h"

/*
 * 256 Werte je Puffer sind bei 11025 Hz rund 23 ms. Gross genug, dass eine
 * verspaetete Unterbrechung nichts ausmacht (der Multiplayer-Link sperrt die
 * Unterbrechungen bis zu 1 ms am Stueck), klein genug, dass die OPL-Emulation
 * je Aufruf nicht zu lange rechnet.
 */
#define AUDIO_BUF_SAMPLES 256

static uint16_t pwm_buf[2][AUDIO_BUF_SAMPLES];
static Sint16   mix_buf[AUDIO_BUF_SAMPLES];

static void (*audio_cb)(void *userdata, Uint8 *stream, int len);
static void *audio_cb_user;

static int  dma_chan = -1;
static int  dma_timer = -1;
static int  audio_irq = -1;   /* DMA_IRQ_0 oder _1, oder -1 = kein Ton */
static uint pwm_slice, pwm_chan;
static volatile int filling;      /* welcher Puffer gerade neu befuellt wird */
static bool running;
static bool paused = true;
static volatile bool locked;      /* SDL_LockAudioDevice */

/* --------------------------------------------------------- Umrechnung */

static void fill_buffer(int idx)
{
	uint16_t *out = pwm_buf[idx];

	if (audio_cb == NULL || paused || locked)
	{
		/* Stille ist die Mitte des PWM-Bereichs, nicht null: null hiesse
		   Dauerausschlag in eine Richtung und damit ein hoerbarer Knacks. */
		for (int i = 0; i < AUDIO_BUF_SAMPLES; ++i)
			out[i] = 512;
		return;
	}

	audio_cb(audio_cb_user, (Uint8 *)mix_buf, AUDIO_BUF_SAMPLES * (int)sizeof(Sint16));

	for (int i = 0; i < AUDIO_BUF_SAMPLES; ++i)
	{
		/*
		 * 16 Bit vorzeichenbehaftet -> 10 Bit ohne Vorzeichen, mit der
		 * Zusatzverstaerkung aus pbc_config.h. Begrenzt wird VOR dem Versatz
		 * auf die Mitte, damit ein zu lautes Signal sauber an der Grenze
		 * stehenbleibt statt auf die andere Seite umzuschlagen.
		 */
		int v = ((int)mix_buf[i] * PBC_FX_GAIN) >> 6;
		if (v < -512)
			v = -512;
		else if (v > 511)
			v = 511;
		out[i] = (uint16_t)(v + 512);
	}
}

/* ------------------------------------------------------ Unterbrechung */

static void __isr audio_dma_irq(void)
{
	if (audio_irq == DMA_IRQ_1)
	{
		if (!dma_channel_get_irq1_status(dma_chan))
			return;
		dma_channel_acknowledge_irq1(dma_chan);
	}
	else
	{
		if (!dma_channel_get_irq0_status(dma_chan))
			return;
		dma_channel_acknowledge_irq0(dma_chan);
	}

	/* Den gerade fertigen Puffer erneut anstossen ist falsch -- der andere ist
	   der naechste. Erst umschalten, dann fuellen. */
	int next = filling;
	filling ^= 1;

	dma_channel_set_read_addr(dma_chan, pwm_buf[next], true);

	fill_buffer(filling);
}

/* ------------------------------------------------------------- Aufbau */

void pbc_audio_init(void)
{
	if (running)
		return;

	gpio_set_function(PBC_PIN_SPEAKER, GPIO_FUNC_PWM);
	pwm_slice = pwm_gpio_to_slice_num(PBC_PIN_SPEAKER);
	pwm_chan  = pwm_gpio_to_channel(PBC_PIN_SPEAKER);

	/*
	 * Wrap 1023 bei ungeteiltem Systemtakt: 150 MHz / 1024 = 146,5 kHz
	 * Traegerfrequenz. Weit oberhalb des Hoerbaren, sodass der Traeger selbst
	 * nicht zu hoeren ist, und fein genug fuer 10 Bit Aufloesung.
	 */
	pwm_config cfg = pwm_get_default_config();
	pwm_config_set_clkdiv(&cfg, 1.0f);
	pwm_config_set_wrap(&cfg, 1023);
	pwm_init(pwm_slice, &cfg, true);
	pwm_set_chan_level(pwm_slice, pwm_chan, 512);

	for (int i = 0; i < 2; ++i)
		for (int s = 0; s < AUDIO_BUF_SAMPLES; ++s)
			pwm_buf[i][s] = 512;

	dma_chan = dma_claim_unused_channel(true);
	dma_timer = dma_claim_unused_timer(true);

	/*
	 * Der DMA-Zeitgeber taktet die Ausgabe: Rate = Systemtakt * X / Y.
	 * 150 MHz * 3 / 40816 = 11025,1 Hz. Zaehler und Nenner muessen beide in
	 * 16 Bit passen -- deshalb nicht einfach 11025/150000000.
	 *
	 * Bei einer Aenderung von PBC_AUDIO_RATE gehoert dieses Bruchpaar mit
	 * angepasst; die Werte lassen sich nicht aus dem Makro herleiten, ohne
	 * die 16-Bit-Grenze zu verletzen.
	 */
	_Static_assert(PBC_AUDIO_RATE == 11025, "Bruchpaar des DMA-Zeitgebers passt nicht mehr");
	dma_timer_set_fraction(dma_timer, 3, 40816);

	dma_channel_config c = dma_channel_get_default_config(dma_chan);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
	channel_config_set_read_increment(&c, true);
	channel_config_set_write_increment(&c, false);
	channel_config_set_dreq(&c, dma_get_timer_dreq(dma_timer));

	/*
	 * Ziel ist die Haelfte des Vergleichsregisters, die zu diesem Kanal
	 * gehoert. GP15 ist ein ungerader Pin und damit Kanal B, also die obere
	 * Haelfte -- das falsch zu treffen ergibt einen stummen Ausgang, an dem
	 * man lange sucht.
	 */
	volatile uint16_t *cc = (volatile uint16_t *)&pwm_hw->slice[pwm_slice].cc;
	volatile void *dst = (volatile void *)(cc + (pwm_chan == PWM_CHAN_B ? 1 : 0));

	dma_channel_configure(dma_chan, &c, dst, pwm_buf[0], AUDIO_BUF_SAMPLES, false);

	/*
	 * Unterbrechung belegen -- aber ohne das Programm anzuhalten, wenn es
	 * nicht geht.
	 *
	 * Beide Wege der SDK (irq_add_shared_handler und irq_set_exclusive_handler)
	 * pruefen mit hard_assert, ob der Vektor noch frei ist, und beenden das
	 * Programm, wenn nicht. Fuer einen Tonausgang ist das die falsche
	 * Reaktion: ohne Ton laesst sich spielen, ohne Start nicht.
	 *
	 * Deshalb wird vorher selbst nachgesehen. Als Vergleichswert dient der
	 * Vektor einer Unterbrechung, die dieses Programm garantiert nicht
	 * benutzt -- steht dort derselbe Eintrag, ist der Platz noch unbelegt.
	 */
	{
		const irq_handler_t unhandled = irq_get_vtable_handler(SPI1_IRQ);

		if (irq_get_vtable_handler(DMA_IRQ_1) == unhandled)
		{
			audio_irq = DMA_IRQ_1;
			dma_channel_set_irq1_enabled(dma_chan, true);
		}
		else if (irq_get_vtable_handler(DMA_IRQ_0) == unhandled)
		{
			audio_irq = DMA_IRQ_0;
			dma_channel_set_irq0_enabled(dma_chan, true);
		}
		else
		{
			/* Kein freier Vektor: stumm weiterspielen. */
			pwm_set_chan_level(pwm_slice, pwm_chan, 512);
			return;
		}

		irq_set_exclusive_handler((uint)audio_irq, audio_dma_irq);
		irq_set_enabled((uint)audio_irq, true);

		/* Siehe den Kommentar oben: unter allem anderen einsortieren. */
		irq_set_priority((uint)audio_irq, PICO_LOWEST_IRQ_PRIORITY);
	}

	filling = 1;
	running = true;

	dma_channel_set_read_addr(dma_chan, pwm_buf[0], true);
}

void pbc_audio_stop(void)
{
	if (!running)
		return;

	dma_channel_abort(dma_chan);
	if (audio_irq == DMA_IRQ_1)
		dma_channel_set_irq1_enabled(dma_chan, false);
	else
		dma_channel_set_irq0_enabled(dma_chan, false);
	pwm_set_chan_level(pwm_slice, pwm_chan, 0);
	pwm_set_enabled(pwm_slice, false);
	running = false;
}

/* ------------------------------------------------- SDL-Audio-Attrappe */

SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int iscapture,
                                      const SDL_AudioSpec *desired,
                                      SDL_AudioSpec *obtained,
                                      int allowed_changes)
{
	(void)device; (void)iscapture; (void)allowed_changes;

	if (desired == NULL || desired->callback == NULL)
		return 0;

	audio_cb = desired->callback;
	audio_cb_user = desired->userdata;

	/*
	 * OpenTyrian moechte 44100 Hz. Es erlaubt aber ausdruecklich eine andere
	 * Rate (SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) und rechnet danach mit dem, was
	 * es bekommt -- deshalb genuegt es, hier die Wahrheit zu melden, statt
	 * loudness.c anzufassen. 44100 Hz waeren auf diesem Geraet Verschwendung:
	 * Tyrians Klaenge liegen als 11025-Hz-Material vor und der Ausgang ist ein
	 * Piezo.
	 */
	if (obtained != NULL)
	{
		*obtained = *desired;
		obtained->freq = PBC_AUDIO_RATE;
		obtained->channels = 1;
		obtained->samples = AUDIO_BUF_SAMPLES;
		obtained->size = AUDIO_BUF_SAMPLES * sizeof(Sint16);
	}

	pbc_audio_init();
	return 1;
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID dev)
{
	(void)dev;
	audio_cb = NULL;
	pbc_audio_stop();
}

void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on)
{
	(void)dev;
	paused = (pause_on != 0);
}

/*
 * Das Original sperrt hier den Rueckruf, waehrend es Mischerzustand aendert.
 * Ein Flag genuegt: die Unterbrechung prueft es und legt in dieser Zeit Stille
 * ab, statt in halb geaenderte Daten zu greifen. Die Unterbrechung ganz zu
 * sperren waere die Alternative, wuerde aber je nach Sperrdauer Aussetzer
 * erzeugen.
 */
void SDL_LockAudioDevice(SDL_AudioDeviceID dev)   { (void)dev; locked = true; }
void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev) { (void)dev; locked = false; }

/* ------------------------------------------------- Abtastratenwandlung */

/*
 * nortsong.c laedt Tyrians Klaenge als 8-Bit-Material mit 11025 Hz und laesst
 * sie von SDL auf die Geraeterate bringen. Hier ist das eine ganzzahlige
 * Verdopplung (11025 -> 22050); der Code kommt trotzdem mit beliebigen
 * Verhaeltnissen zurecht, damit ein spaeter geaendertes PBC_AUDIO_RATE nicht
 * still etwas kaputt macht.
 */

static int cvt_src_rate, cvt_dst_rate;

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 src_format, Uint8 src_channels,
                      int src_rate, Uint16 dst_format, Uint8 dst_channels,
                      int dst_rate)
{
	if (cvt == NULL || src_channels != 1 || dst_channels != 1)
		return -1;
	if (src_format != AUDIO_S8 || dst_format != AUDIO_S16SYS)
		return -1;
	if (src_rate <= 0 || dst_rate <= 0)
		return -1;

	memset(cvt, 0, sizeof *cvt);
	cvt->needed = 1;

	cvt_src_rate = src_rate;
	cvt_dst_rate = dst_rate;

	/* Je Eingangsbyte entstehen 2 Byte mal Ratenverhaeltnis, aufgerundet. */
	double ratio = 2.0 * dst_rate / src_rate;
	cvt->len_ratio = ratio;
	cvt->len_mult = (int)(ratio + 0.999);
	if (cvt->len_mult < 1)
		cvt->len_mult = 1;

	return 1;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
	if (cvt == NULL || cvt->buf == NULL || cvt->len <= 0)
		return -1;

	const int in_count = cvt->len;
	const int out_count = (int)((int64_t)in_count * cvt_dst_rate / cvt_src_rate);

	const Sint8 *in = (const Sint8 *)cvt->buf;
	Sint16 *out = (Sint16 *)cvt->buf;

	/*
	 * Umgewandelt wird IM SELBEN Puffer und deshalb von hinten nach vorn:
	 * ein Ausgangswert ist doppelt so breit wie ein Eingangswert, von vorn
	 * gerechnet wuerde man sich die noch ungelesenen Eingangsbytes
	 * ueberschreiben.
	 */
	for (int i = out_count - 1; i >= 0; --i)
	{
		int si = (int)((int64_t)i * cvt_src_rate / cvt_dst_rate);
		if (si >= in_count)
			si = in_count - 1;
		out[i] = (Sint16)(in[si] << 8);
	}

	cvt->len_cvt = out_count * (int)sizeof(Sint16);
	return 0;
}
