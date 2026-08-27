/*
 * pbc_display -- Umsetzung. Siehe pbc_display.h fuer das Warum.
 *
 * Die Init-Folge (Reset-Zeiten, MADCTL 0xC8, INVON, Y-Versatz 20, SPI-Modus
 * 0,0 bei 62,5 MHz) ist unveraendert aus der Doom-Portierung auf derselben
 * Hardware uebernommen -- doom/picoboy-doom/src/pico/picoboy_display.c im
 * selben Repo. Sie ist dort auf echtem Geraet verifiziert; ein guter Grund,
 * hier nichts "aufzuraeumen".
 *
 * GPLv2.
 */

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "pbc_config.h"
#include "pbc_display.h"

/* ------------------------------------------------------ ST7789-Befehle */

#define ST_SWRESET 0x01
#define ST_SLPOUT  0x11
#define ST_NORON   0x13
#define ST_INVON   0x21
#define ST_DISPON  0x29
#define ST_CASET   0x2A
#define ST_RASET   0x2B
#define ST_RAMWR   0x2C
#define ST_MADCTL  0x36
#define ST_COLMOD  0x3A

#define STRIP_PIXELS (PBC_TFT_W * PBC_STRIP_ROWS)

static uint16_t strip[2][STRIP_PIXELS];
static int      strip_cur;
static int      dma_chan = -1;
static bool     dma_running;
static bool     inited;

/* --------------------------------------------------------------- Pins */

static inline void cs_low(void)  { gpio_put(PBC_PIN_CS, 0); }
static inline void cs_high(void) { gpio_put(PBC_PIN_CS, 1); }
static inline void dc_cmd(void)  { gpio_put(PBC_PIN_DC, 0); }
static inline void dc_data(void) { gpio_put(PBC_PIN_DC, 1); }

static void st_cmd(uint8_t c)
{
	dc_cmd(); cs_low();
	spi_write_blocking(PBC_SPI_PORT, &c, 1);
	cs_high();
}

static void st_data(const uint8_t *d, size_t n)
{
	dc_data(); cs_low();
	spi_write_blocking(PBC_SPI_PORT, d, n);
	cs_high();
}

static void st_data1(uint8_t d) { st_data(&d, 1); }

static void st_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	y0 += PBC_PANEL_Y_OFFSET;
	y1 += PBC_PANEL_Y_OFFSET;

	uint8_t b[4];
	b[0] = x0 >> 8; b[1] = x0 & 0xFF; b[2] = x1 >> 8; b[3] = x1 & 0xFF;
	st_cmd(ST_CASET); st_data(b, 4);
	b[0] = y0 >> 8; b[1] = y0 & 0xFF; b[2] = y1 >> 8; b[3] = y1 & 0xFF;
	st_cmd(ST_RASET); st_data(b, 4);
	st_cmd(ST_RAMWR);
}

/* ------------------------------------------------------------- DMA */

static void wait_dma(void)
{
	if (!dma_running)
		return;
	dma_channel_wait_for_finish_blocking(dma_chan);
	while (spi_is_busy(PBC_SPI_PORT))
		tight_loop_contents();
	dma_running = false;
}

bool pbc_display_busy(void)
{
	return dma_running && dma_chan >= 0 && dma_channel_is_busy(dma_chan);
}

/* ------------------------------------------------------------- Init */

void pbc_display_backlight(uint8_t level)
{
	pwm_set_gpio_level(PBC_PIN_BL, level);
}

void pbc_display_init(void)
{
	if (inited)
		return;
	inited = true;

	spi_init(PBC_SPI_PORT, PBC_SPI_HZ);
	spi_set_format(PBC_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

	gpio_set_function(PBC_PIN_SCK,  GPIO_FUNC_SPI);
	gpio_set_function(PBC_PIN_MOSI, GPIO_FUNC_SPI);

	gpio_init(PBC_PIN_CS);  gpio_set_dir(PBC_PIN_CS,  GPIO_OUT); cs_high();
	gpio_init(PBC_PIN_DC);  gpio_set_dir(PBC_PIN_DC,  GPIO_OUT); dc_data();
	gpio_init(PBC_PIN_RST); gpio_set_dir(PBC_PIN_RST, GPIO_OUT);

	gpio_put(PBC_PIN_RST, 1); sleep_ms(10);
	gpio_put(PBC_PIN_RST, 0); sleep_ms(10);
	gpio_put(PBC_PIN_RST, 1); sleep_ms(120);

	st_cmd(ST_SWRESET); sleep_ms(150);
	st_cmd(ST_SLPOUT);  sleep_ms(120);
	st_cmd(ST_COLMOD);  st_data1(0x55);   /* RGB565 */
	st_cmd(ST_MADCTL);  st_data1(0xC8);   /* MY|MX|BGR: hochkant, 180 gedreht */
	st_cmd(ST_INVON);   sleep_ms(10);     /* IPS-Panel: invertiert */
	st_cmd(ST_NORON);   sleep_ms(10);
	st_cmd(ST_DISPON);  sleep_ms(20);

	/*
	 * Den GANZEN Controller-Speicher loeschen (240x320), nicht nur die
	 * sichtbaren 280 Zeilen: je nach Glas schimmern die Zeilen dahinter am
	 * Rand durch, und das sieht aus wie ein Darstellungsfehler.
	 */
	st_set_window(0, (uint16_t)-PBC_PANEL_Y_OFFSET, PBC_TFT_W - 1,
	              (uint16_t)(320 - 1 - PBC_PANEL_Y_OFFSET));
	dc_data(); cs_low();
	{
		static const uint8_t zeros[128] = { 0 };
		int remaining = PBC_TFT_W * 320;
		while (remaining > 0)
		{
			int n = remaining > 64 ? 64 : remaining;
			spi_write_blocking(PBC_SPI_PORT, zeros, n * 2);
			remaining -= n;
		}
	}
	cs_high();

	/* Hintergrundlicht per PWM -- GP26 ist sonst ein ADC-Pin und bleibt dunkel. */
	gpio_set_function(PBC_PIN_BL, GPIO_FUNC_PWM);
	{
		uint slice = pwm_gpio_to_slice_num(PBC_PIN_BL);
		pwm_config cfg = pwm_get_default_config();
		pwm_config_set_clkdiv(&cfg, 4.0f);
		pwm_config_set_wrap(&cfg, 255);
		pwm_init(slice, &cfg, true);
	}
	pbc_display_backlight(255);

	dma_chan = dma_claim_unused_channel(true);
	strip_cur = 0;
	dma_running = false;
}

/* --------------------------------------------------------- Streifen */

void pbc_display_begin(int y0, int rows)
{
	wait_dma();
	cs_high();

	st_set_window(0, (uint16_t)y0, PBC_TFT_W - 1, (uint16_t)(y0 + rows - 1));

	/* 16 Bit, damit der DMA fertige Pixel schieben kann. */
	spi_set_format(PBC_SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	dc_data();
	cs_low();
}

uint16_t *pbc_display_strip_buffer(void)
{
	/*
	 * Der DMA liest gerade aus strip[strip_cur ^ 1] -- der andere Puffer ist
	 * also frei, ohne dass irgendetwas abgewartet werden muesste. Das ist der
	 * ganze Zweck der zwei Puffer.
	 */
	return strip[strip_cur];
}

void pbc_display_push_strip(uint16_t *buf, int npix)
{
	/* Erst hier auf den vorherigen Streifen warten: bis hierher hat der
	   Aufrufer diesen umgerechnet, die Wartezeit ist also schon genutzt. */
	wait_dma();

	dma_channel_config c = dma_channel_get_default_config(dma_chan);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
	channel_config_set_dreq(&c, spi_get_dreq(PBC_SPI_PORT, true));
	channel_config_set_read_increment(&c, true);
	channel_config_set_write_increment(&c, false);

	dma_channel_configure(dma_chan, &c, &spi_get_hw(PBC_SPI_PORT)->dr,
	                      buf, (uint)npix, true);
	dma_running = true;

	strip_cur ^= 1;
}

void pbc_display_end(void)
{
	wait_dma();
	cs_high();
	spi_set_format(PBC_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

/* ------------------------------------------------------- Notausgabe */

/* 5x7-Zeichensatz, ASCII 32..90; Kleinbuchstaben werden gross gezeichnet.
   Uebernommen aus der Doom-Portierung -- fuer eine Fehlermeldung reicht das. */
/* Nicht static: die Anzeigeleiste zeichnet damit ebenfalls (siehe pbc_hud.c). */
const uint8_t pbc_font5x7[59][5] = {
	{0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5f,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
	{0x14,0x7f,0x14,0x7f,0x14},{0x24,0x2a,0x7f,0x2a,0x12},{0x23,0x13,0x08,0x64,0x62},
	{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1c,0x22,0x41,0x00},
	{0x00,0x41,0x22,0x1c,0x00},{0x14,0x08,0x3e,0x08,0x14},{0x08,0x08,0x3e,0x08,0x08},
	{0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
	{0x20,0x10,0x08,0x04,0x02},{0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},
	{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},
	{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
	{0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e},{0x00,0x36,0x36,0x00,0x00},
	{0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
	{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3e},
	{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},
	{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},
	{0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},{0x00,0x41,0x7f,0x41,0x00},
	{0x20,0x40,0x41,0x3f,0x01},{0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
	{0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},
	{0x7f,0x09,0x09,0x09,0x06},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},
	{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},{0x3f,0x40,0x40,0x40,0x3f},
	{0x1f,0x20,0x40,0x20,0x1f},{0x7f,0x20,0x18,0x20,0x7f},{0x63,0x14,0x08,0x14,0x63},
	{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
};

#define MSG_SCALE 2
#define MSG_LINE_H (7 * MSG_SCALE)

static void msg_render_row(uint16_t *row, int y, const char *const lines[3])
{
	memset(row, 0, PBC_TFT_W * sizeof(uint16_t));

	/* rot / weiss / gelb -- ueber PBC_RGB, siehe pbc_config.h */
	static const uint16_t colors[3] = {
		PBC_RGB(255, 0, 0), PBC_RGB(255, 255, 255), PBC_RGB(255, 255, 0)
	};
	const int pitch = MSG_LINE_H + 4;

	for (int l = 0; l < 3; ++l)
	{
		int top = 4 + l * pitch;
		if (y < top || y >= top + MSG_LINE_H)
			continue;

		const char *text = lines[l];
		int n = (int)strlen(text);
		if (n > 20)
			n = 20;
		if (n == 0)
			return;

		int w = n * 6 * MSG_SCALE - MSG_SCALE;
		int x0 = (PBC_TFT_W - w) / 2;
		if (x0 < 0)
			x0 = 0;

		int font_row = (y - top) / MSG_SCALE;
		for (int i = 0; i < n; ++i)
		{
			char ch = text[i];
			if (ch >= 'a' && ch <= 'z')
				ch = (char)(ch - 32);
			int idx = (ch >= 32 && ch <= 90) ? ch - 32 : 0;
			int cx = x0 + i * 6 * MSG_SCALE;
			for (int col = 0; col < 5; ++col)
			{
				if (!(pbc_font5x7[idx][col] & (1 << font_row)))
					continue;
				for (int dx = 0; dx < MSG_SCALE; ++dx)
				{
					int px = cx + col * MSG_SCALE + dx;
					if (px >= 0 && px < PBC_TFT_W)
						row[px] = colors[l];
				}
			}
		}
		return;
	}
}

/* Ein Zeichen des 5x7-Satzes, 2-fach vergroessert, in eine Zeile malen. */
static void test_glyph(uint16_t *row, int font_row, int x0, char ch, uint16_t color)
{
	if (ch >= 'a' && ch <= 'z')
		ch = (char)(ch - 32);
	int idx = (ch >= 32 && ch <= 90) ? ch - 32 : 0;

	for (int col = 0; col < 5; ++col)
	{
		if (!(pbc_font5x7[idx][col] & (1 << font_row)))
			continue;
		for (int d = 0; d < 2; ++d)
		{
			int px = x0 + col * 2 + d;
			if (px >= 0 && px < PBC_TFT_W)
				row[px] = color;
		}
	}
}

void pbc_display_test_pattern(void)
{
	pbc_display_init();

	static const uint16_t bar[4] = {
		PBC_RGB(255,   0,   0),
		PBC_RGB(  0, 255,   0),
		PBC_RGB(  0,   0, 255),
		PBC_RGB(255, 255, 255),
	};
	static const char label[4] = { 'R', 'G', 'B', 'W' };

	wait_dma();
	cs_high();
	st_set_window(0, 0, PBC_TFT_W - 1, PBC_TFT_H - 1);
	spi_set_format(PBC_SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	dc_data();
	cs_low();

	uint16_t row[PBC_TFT_W];
	const int bar_w = PBC_TFT_W / 4;   /* 60 */

	for (int y = 0; y < PBC_TFT_H; ++y)
	{
		if (y < 120)
		{
			/* Vier reine Balken. */
			for (int x = 0; x < PBC_TFT_W; ++x)
				row[x] = bar[x / bar_w];

			/* Beschriftung mittig im Balken, in Schwarz bzw. Weiss. */
			if (y >= 50 && y < 64)
			{
				for (int b = 0; b < 4; ++b)
					test_glyph(row, y - 50, b * bar_w + bar_w / 2 - 5,
					           label[b], b == 3 ? PBC_RGB(0,0,0) : PBC_RGB(255,255,255));
			}
		}
		else if (y < 180)
		{
			/* Graukeil ueber die volle Breite. */
			for (int x = 0; x < PBC_TFT_W; ++x)
			{
				int v = x * 255 / (PBC_TFT_W - 1);
				row[x] = PBC_RGB(v, v, v);
			}
		}
		else if (y < 240)
		{
			/* Farbverlauf Rot -> Gruen -> Blau. */
			for (int x = 0; x < PBC_TFT_W; ++x)
			{
				int t = x * 3 * 255 / (PBC_TFT_W - 1);
				int r = t < 255 ? 255 - t : 0;
				int g = t < 255 ? t : (t < 510 ? 510 - t : 0);
				int b = t < 255 ? 0 : t - 255;
				if (b > 255) b = 255;
				row[x] = PBC_RGB(r, g, b);
			}
		}
		else
		{
			for (int x = 0; x < PBC_TFT_W; ++x)
				row[x] = PBC_RGB(0, 0, 0);

			/* Welche Reihenfolge gerade eingestellt ist. */
			if (y >= 250 && y < 264)
			{
				const char *t = PBC_PANEL_BGR ? "PANEL BGR" : "PANEL RGB";
				for (int i = 0; t[i]; ++i)
					test_glyph(row, y - 250, 60 + i * 12, t[i], PBC_RGB(255,255,255));
			}
		}

		spi_write16_blocking(PBC_SPI_PORT, row, PBC_TFT_W);
	}

	while (spi_is_busy(PBC_SPI_PORT))
		tight_loop_contents();
	cs_high();
	spi_set_format(PBC_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void pbc_display_message(const char *line1, const char *line2, const char *line3)
{
	pbc_display_init();

	const char *const lines[3] = {
		line1 ? line1 : "", line2 ? line2 : "", line3 ? line3 : ""
	};

	wait_dma();
	cs_high();
	st_set_window(0, 0, PBC_TFT_W - 1, PBC_TFT_H - 1);
	spi_set_format(PBC_SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	dc_data();
	cs_low();

	/*
	 * Blockierend und ohne DMA: das hier laeuft nur, wenn ohnehin nichts mehr
	 * weitergeht. Ein Zeilenpuffer statt eines Bildpuffers, damit die Ausgabe
	 * auch dann noch funktioniert, wenn der Speicher knapp ist.
	 */
	uint16_t row[PBC_TFT_W];
	const int block_top = (PBC_TFT_H - (3 * (MSG_LINE_H + 4))) / 2;
	for (int y = 0; y < PBC_TFT_H; ++y)
	{
		msg_render_row(row, y - block_top, lines);
		spi_write16_blocking(PBC_SPI_PORT, row, PBC_TFT_W);
	}

	while (spi_is_busy(PBC_SPI_PORT))
		tight_loop_contents();
	cs_high();
	spi_set_format(PBC_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}
