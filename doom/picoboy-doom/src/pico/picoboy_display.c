//
// PicoBoy Color ST7789 driver -- implementation.
//
// Reproduces the SPI/init sequence that has been verified to drive the
// PicoBoy Color panel correctly:
//   * SPI0 at 62.5 MHz, mode 0,0 (CPOL=0/CPHA=0)
//   * SCK=GP18, MOSI=GP19, CS=GP10 (plain GPIO), DC=GP8, RST=GP9
//   * MADCTL=0x08 -- portrait, BGR colour order (the panel is BGR-wired)
//   * Backlight on GP26 driven by PWM (it is otherwise stuck in ADC mode)
//
// A test-pattern mode (PICOBOY_TEST_PATTERN, in picoboy_config.h) lets us
// paint a fixed RGB stripe instead of Doom pixels -- this isolates the
// display path from the renderer.
//
// GPLv2 (same as rp2040-doom).
//

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include "picoboy_config.h"
#include "picoboy_display.h"

// ---- ST7789 commands -------------------------------------------------------
#define ST_SWRESET  0x01
#define ST_SLPOUT   0x11
#define ST_NORON    0x13
#define ST_INVON    0x21
#define ST_DISPON   0x29
#define ST_CASET    0x2A
#define ST_RASET    0x2B
#define ST_RAMWR    0x2C
#define ST_MADCTL   0x36
#define ST_COLMOD   0x3A

// ---- Framebuffer (240*180*2 = 86400 bytes) --------------------------------
static uint16_t fb[PICOBOY_VIEW_W * PICOBOY_VIEW_H];
static int      dma_chan = -1;
static bool     panel_inited = false;
static bool     dma_in_flight = false;

static void picoboy_wait_dma(void);

// ---- Pin / SPI helpers -----------------------------------------------------
static inline void cs_low (void) { gpio_put(PICOBOY_PIN_CS, 0); }
static inline void cs_high(void) { gpio_put(PICOBOY_PIN_CS, 1); }
static inline void dc_cmd (void) { gpio_put(PICOBOY_PIN_DC, 0); }
static inline void dc_data(void) { gpio_put(PICOBOY_PIN_DC, 1); }

static void st_cmd(uint8_t c)
{
    dc_cmd(); cs_low();
    spi_write_blocking(PICOBOY_SPI_PORT, &c, 1);
    cs_high();
}

static void st_data1(uint8_t d)
{
    dc_data(); cs_low();
    spi_write_blocking(PICOBOY_SPI_PORT, &d, 1);
    cs_high();
}

static void st_data(const uint8_t *d, size_t n)
{
    dc_data(); cs_low();
    spi_write_blocking(PICOBOY_SPI_PORT, d, n);
    cs_high();
}

// Panel-internal Y offset. The ST7789 controller has 240x320 RAM but the
// physical panel here is only 240x280, often physically aligned to the
// bottom of the controller's address space (or the top, depending on the
// glass). Tweak this if the picture is shifted up or down on the screen.
#ifndef PICOBOY_PANEL_Y_OFFSET
#define PICOBOY_PANEL_Y_OFFSET 20
#endif

static void st_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Apply panel Y offset (controller has 240x320 RAM, panel is 240x280)
    y0 += PICOBOY_PANEL_Y_OFFSET;
    y1 += PICOBOY_PANEL_Y_OFFSET;
    uint8_t b[4];
    b[0] = x0 >> 8; b[1] = x0 & 0xFF; b[2] = x1 >> 8; b[3] = x1 & 0xFF;
    st_cmd(ST_CASET); st_data(b, 4);
    b[0] = y0 >> 8; b[1] = y0 & 0xFF; b[2] = y1 >> 8; b[3] = y1 & 0xFF;
    st_cmd(ST_RASET); st_data(b, 4);
    st_cmd(ST_RAMWR);
}

// Der Bildtransfer laeuft per DMA und wird NICHT mehr abgewartet: er belegt
// 86 KB ueber SPI, also rund 11 ms, und picoboy_present() wird von Core0
// aufgerufen -- derselben Rechenzeit, aus der auch die Tics kommen. Wer hier
// wartet, verschenkt sie. Stattdessen laufen Spiellogik und Rendern des
// naechsten Bildes weiter, und erst wer wieder an den Puffer oder an SPI
// muss, holt den Transfer ein.
static void picoboy_wait_dma(void)
{
    if (!dma_in_flight) return;
    dma_channel_wait_for_finish_blocking(dma_chan);
    while (spi_is_busy(PICOBOY_SPI_PORT)) tight_loop_contents();
    cs_high();
    spi_set_format(PICOBOY_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    dma_in_flight = false;
}

// ---- Backlight (PWM, because GP26 is otherwise an ADC pin) -----------------
static void picoboy_init_backlight(void)
{
    gpio_set_function(PICOBOY_PIN_BL, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PICOBOY_PIN_BL);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 4.0f);
    pwm_config_set_wrap(&cfg, 255);
    pwm_init(slice, &cfg, true);
    pwm_set_gpio_level(PICOBOY_PIN_BL, 255);
}

// ---- Init ------------------------------------------------------------------
void picoboy_display_init(void)
{
    if (panel_inited) return;
    panel_inited = true;

    // SPI bring-up. Mode 0,0 (CPOL=0/CPHA=0) is what the working PicoBoy
    // driver uses; the earlier attempt with CPOL=1/CPHA=1 was wrong.
    spi_init(PICOBOY_SPI_PORT, PICOBOY_SPI_HZ);
    spi_set_format(PICOBOY_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(PICOBOY_PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PICOBOY_PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PICOBOY_PIN_CS);  gpio_set_dir(PICOBOY_PIN_CS,  GPIO_OUT); cs_high();
    gpio_init(PICOBOY_PIN_DC);  gpio_set_dir(PICOBOY_PIN_DC,  GPIO_OUT); dc_data();
    gpio_init(PICOBOY_PIN_RST); gpio_set_dir(PICOBOY_PIN_RST, GPIO_OUT);

    // Hardware reset
    gpio_put(PICOBOY_PIN_RST, 1); sleep_ms(10);
    gpio_put(PICOBOY_PIN_RST, 0); sleep_ms(10);
    gpio_put(PICOBOY_PIN_RST, 1); sleep_ms(120);

    st_cmd(ST_SWRESET); sleep_ms(150);
    st_cmd(ST_SLPOUT);  sleep_ms(120);
    st_cmd(ST_COLMOD);  st_data1(0x55);   // 16bpp RGB565
    st_cmd(ST_MADCTL);  st_data1(0xC8);   // portrait + 180° rotation, BGR
                                          // (0x80=MY | 0x40=MX | 0x08=BGR)
    st_cmd(ST_INVON);   sleep_ms(10);     // IPS panel: invert
    st_cmd(ST_NORON);   sleep_ms(10);
    st_cmd(ST_DISPON);  sleep_ms(20);

    // Clear the FULL ST7789 RAM area (240 wide x 320 tall) -- not just the
    // visible 240x280, because some panels have an internal Y-offset and
    // the rows beyond 280 can still show through at the visible edges.
    // Also write a chunk of pixels each spi_write_blocking call (much
    // faster + makes sure no rows are skipped due to FIFO timing).
    st_set_window(0, 0, PICOBOY_TFT_W - 1, 320 - 1);
    dc_data(); cs_low();
    {
        // 64-pixel (128-byte) chunks of zero for the SPI clear.
        static const uint8_t zeros[128] = { 0 };
        const int total_pixels = PICOBOY_TFT_W * 320;
        const int chunk_pixels = 64;
        const int chunk_bytes  = chunk_pixels * 2;
        int remaining = total_pixels;
        while (remaining > 0) {
            int n = remaining > chunk_pixels ? chunk_pixels : remaining;
            spi_write_blocking(PICOBOY_SPI_PORT, zeros, n * 2);
            remaining -= n;
        }
    }
    cs_high();

    picoboy_init_backlight();

    // DMA channel for per-frame framebuffer push.
    dma_chan = dma_claim_unused_channel(true);

    memset(fb, 0, sizeof(fb));
}

// ---- Pre-computed source-row table for 200 -> 180 vertical decimation ------
static const uint8_t src_row_for_dst[PICOBOY_VIEW_H] = {
    0,1,2,3,4,5,6,7,8,
    10,11,12,13,14,15,16,17,18,
    20,21,22,23,24,25,26,27,28,
    30,31,32,33,34,35,36,37,38,
    40,41,42,43,44,45,46,47,48,
    50,51,52,53,54,55,56,57,58,
    60,61,62,63,64,65,66,67,68,
    70,71,72,73,74,75,76,77,78,
    80,81,82,83,84,85,86,87,88,
    90,91,92,93,94,95,96,97,98,
   100,101,102,103,104,105,106,107,108,
   110,111,112,113,114,115,116,117,118,
   120,121,122,123,124,125,126,127,128,
   130,131,132,133,134,135,136,137,138,
   140,141,142,143,144,145,146,147,148,
   150,151,152,153,154,155,156,157,158,
   160,161,162,163,164,165,166,167,168,
   170,171,172,173,174,175,176,177,178,
   180,181,182,183,184,185,186,187,188,
   190,191,192,193,194,195,196,197,198,
};
_Static_assert(sizeof(src_row_for_dst) == PICOBOY_VIEW_H, "row table size");

#define MAIN_VIEW_HEIGHT 168    // SCREENHEIGHT (200) - statusbar (32)

// ---- Test pattern: vertical RGB stripes, dark top / bright bottom ----------
//
// Mirrors the proven test pattern that confirmed the SPI/display path
// during the earlier port. If this comes out as three clean coloured
// stripes (red/green/blue, dark at top, bright at bottom), the display
// pipeline is healthy and the issue is upstream in the Doom renderer.
//
static void picoboy_compose_test_pattern(void)
{
    for (int y = 0; y < PICOBOY_VIEW_H; y++) {
        // 0..255 brightness over the height of the view
        uint8_t v = (uint8_t)((y * 255) / (PICOBOY_VIEW_H - 1));
        uint16_t red   = ((v & 0xF8) << 8);                 // R5_5 = v -> RGB565 R
        uint16_t green = ((v & 0xFC) << 3);                 // G6
        uint16_t blue  = ( v >> 3);                         // B5
        uint16_t *row = &fb[y * PICOBOY_VIEW_W];
        int third = PICOBOY_VIEW_W / 3;                     // 80
        for (int x = 0; x < third;             x++) row[x] = red;
        for (int x = third; x < 2 * third;     x++) row[x] = green;
        for (int x = 2 * third; x < PICOBOY_VIEW_W; x++) row[x] = blue;
    }
}

// ---- Simple drawing helpers (used by the USB-link test firmware) ----------
void picoboy_display_clear(uint16_t color)
{
    picoboy_wait_dma();
    for (int i = 0; i < PICOBOY_VIEW_W * PICOBOY_VIEW_H; i++) fb[i] = color;
}

void picoboy_display_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    picoboy_wait_dma();
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > PICOBOY_VIEW_W) w = PICOBOY_VIEW_W - x;
    if (y + h > PICOBOY_VIEW_H) h = PICOBOY_VIEW_H - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        uint16_t *row = &fb[yy * PICOBOY_VIEW_W];
        for (int xx = x; xx < x + w; xx++) row[xx] = color;
    }
}

// ---- Compose Doom frame ---------------------------------------------------
//
// Doom's frame buffer is 320x200, split into two parts the engine handles
// separately:
//   - Main view (320x168) -> frame_buffer[display_frame_index]
//   - Status bar (320x32) -> last 32 rows of frame_buffer[display_frame_index^1]
//
// In VIDEO_TYPE_SINGLE (menu/intermission/title/finale) the engine fills
// both parts itself. In VIDEO_TYPE_DOUBLE (gameplay) it normally relies
// on scanvideo overlays for the status bar; we don't have scanvideo, so
// pd_end_frame on the PicoBoy build calls draw_stbar_on_framebuffer
// before our finish_update so that fb_b's tail also has valid statusbar
// pixels in DOUBLE mode.
//
void picoboy_compose_frame_doom(const uint8_t *fb_a,
                                const uint8_t *fb_b,
                                const uint16_t palette[256])
{
    // Erst hier wird der laufende Transfer eingeholt: bis dahin hat Core0
    // Spiellogik und Rendern erledigt, die Wartezeit ist also schon genutzt.
    picoboy_wait_dma();
    while (spi_is_busy(PICOBOY_SPI_PORT)) tight_loop_contents();

#if PICOBOY_TEST_PATTERN
    (void) fb_a; (void) fb_b; (void) palette;
    picoboy_compose_test_pattern();
    return;
#else
    for (int y = 0; y < PICOBOY_VIEW_H; y++) {
        int src_row = src_row_for_dst[y];
        const uint8_t *src;
        if (src_row < MAIN_VIEW_HEIGHT) {
            src = fb_a + src_row * DOOM_SRC_W;
        } else {
            src = fb_b + (src_row - 32) * DOOM_SRC_W;
        }
        uint16_t *dst = &fb[y * PICOBOY_VIEW_W];

        // 320 -> 240: drop every 4th source pixel.
        int s = 0;
        for (int d = 0; d < PICOBOY_VIEW_W; d += 3) {
            dst[d  ] = palette[src[s    ]];
            dst[d+1] = palette[src[s + 1]];
            dst[d+2] = palette[src[s + 2]];
            s += 4;
        }
    }
#endif
}

// Laeuft gerade ein Bildtransfer? Der schaufelt 86 KB ueber SPI und haelt
// dabei den Peripheriebus belegt -- wer selbst Flanken zaehlen muss, wartet
// besser eine Luecke ab.
bool picoboy_display_dma_busy(void)
{
    return dma_in_flight && dma_chan >= 0 && dma_channel_is_busy(dma_chan);
}

// ---- Push framebuffer to ST7789 -------------------------------------------
void picoboy_present(void)
{
    if (!panel_inited) return;

    picoboy_wait_dma();
    while (spi_is_busy(PICOBOY_SPI_PORT)) tight_loop_contents();
    cs_high();

    st_set_window(0, PICOBOY_VIEW_Y0,
                  PICOBOY_VIEW_W - 1,
                  PICOBOY_VIEW_Y0 + PICOBOY_VIEW_H - 1);

    // Switch SPI to 16-bit so DMA can push uint16_t pixels directly.
    spi_set_format(PICOBOY_SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    dc_data();
    cs_low();

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(PICOBOY_SPI_PORT, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);

    dma_channel_configure(dma_chan, &c,
        &spi_get_hw(PICOBOY_SPI_PORT)->dr,
        fb,
        PICOBOY_VIEW_W * PICOBOY_VIEW_H,
        true);

    // Und zurueck ins Spiel -- der Transfer laeuft nebenher weiter.
    dma_in_flight = true;
}

// ---- Status strip below the Doom view -------------------------------------
//
// The panel is 240x280, the Doom view occupies rows 50..229 -- so there are
// 50 unused rows underneath. We render up to two centred lines of 5x7 text
// (scaled 2x, i.e. 10x14 px) into that strip. Only pushed when the text
// actually changed, so this costs nothing per frame.
//
#define STRIP_Y      (PICOBOY_VIEW_Y0 + PICOBOY_VIEW_H + 2)   // 232
#define STRIP_H      48                                       // drei Zeilen
#define STRIP_SCALE  2
#define STRIP_MAXCH  20

static char strip_prev[3][STRIP_MAXCH + 1];

// 5x7 font, ASCII 32..90 (lowercase is folded to uppercase, rest blank).
static const uint8_t font5x7[59][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5f,0x00,0x00},{0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00},{0x23,0x13,0x08,0x64,0x62},
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00},{0x00,0x1c,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1c,0x00},{0x00,0x00,0x00,0x00,0x00},{0x08,0x08,0x3e,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x00,0x00,0x00,0x00},{0x02,0x01,0x51,0x09,0x06},{0x00,0x00,0x00,0x00,0x00},
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

// Eine Bildschirmzeile des Streifens erzeugen (nur ein 240-Pixel-Puffer, damit
// der Doom-Zone kein Speicher weggenommen wird).
static void strip_render_row(uint16_t *row, int y,
                             const char *l1, const char *l2, const char *l3)
{
    memset(row, 0, PICOBOY_TFT_W * sizeof(uint16_t));

    const char *text;
    uint16_t color;
    int local_y;
    const int line_h = 7 * STRIP_SCALE;
    const int pitch = line_h + 2;

    if (y >= 1 && y < 1 + line_h) {
        text = l1; color = 0xFFFF; local_y = y - 1;
    } else if (y >= 1 + pitch && y < 1 + pitch + line_h) {
        text = l2; color = 0x07E0; local_y = y - (1 + pitch);
    } else if (y >= 1 + 2 * pitch && y < 1 + 2 * pitch + line_h) {
        text = l3; color = 0xFFE0; local_y = y - (1 + 2 * pitch);
    } else {
        return;
    }

    int n = 0;
    while (text[n] && n < STRIP_MAXCH) n++;
    if (!n) return;
    int w = n * 6 * STRIP_SCALE - STRIP_SCALE;
    int x0 = (PICOBOY_TFT_W - w) / 2;
    if (x0 < 0) x0 = 0;

    int font_row = local_y / STRIP_SCALE;
    for (int i = 0; i < n; i++) {
        char c = text[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        int idx = (c >= 32 && c <= 90) ? c - 32 : 0;
        int cx = x0 + i * 6 * STRIP_SCALE;
        for (int col = 0; col < 5; col++) {
            if (!(font5x7[idx][col] & (1 << font_row))) continue;
            for (int dx = 0; dx < STRIP_SCALE; dx++) {
                int px = cx + col * STRIP_SCALE + dx;
                if (px >= 0 && px < PICOBOY_TFT_W) row[px] = color;
            }
        }
    }
}

void picoboy_status_lines(const char *l1, const char *l2)
{
    picoboy_status_lines3(l1, l2, "");
}

void picoboy_status_lines3(const char *l1, const char *l2, const char *l3)
{
    if (!panel_inited) return;
    if (!l1) l1 = "";
    if (!l2) l2 = "";
    if (!l3) l3 = "";

    // Nur neu zeichnen, wenn sich der Text geaendert hat.
    if (!strncmp(strip_prev[0], l1, STRIP_MAXCH) && !strncmp(strip_prev[1], l2, STRIP_MAXCH) &&
        !strncmp(strip_prev[2], l3, STRIP_MAXCH)) {
        return;
    }
    strncpy(strip_prev[0], l1, STRIP_MAXCH); strip_prev[0][STRIP_MAXCH] = 0;
    strncpy(strip_prev[1], l2, STRIP_MAXCH); strip_prev[1][STRIP_MAXCH] = 0;
    strncpy(strip_prev[2], l3, STRIP_MAXCH); strip_prev[2][STRIP_MAXCH] = 0;

    picoboy_wait_dma();
    while (spi_is_busy(PICOBOY_SPI_PORT)) tight_loop_contents();
    cs_high();

    st_set_window(0, STRIP_Y, PICOBOY_TFT_W - 1, STRIP_Y + STRIP_H - 1);
    spi_set_format(PICOBOY_SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    dc_data();
    cs_low();
    {
        uint16_t row[PICOBOY_TFT_W];
        for (int y = 0; y < STRIP_H; y++) {
            strip_render_row(row, y, l1, l2, l3);
            spi_write16_blocking(PICOBOY_SPI_PORT, row, PICOBOY_TFT_W);
        }
    }
    while (spi_is_busy(PICOBOY_SPI_PORT)) tight_loop_contents();
    cs_high();
    spi_set_format(PICOBOY_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

// ---- Diagnostic: init + test pattern in one call --------------------------
//
// Call early in main() (right after the backlight ramp-up). If the panel is
// alive and SPI is wired correctly, you should see three vertical stripes
// red/green/blue, dark at top, bright at bottom. If you see nothing -- SPI
// pins or panel init are still wrong. If you see scrambled colours -- the
// MADCTL bit needs flipping (BGR vs RGB).
//
void picoboy_display_show_test_pattern(void)
{
    picoboy_display_init();
    picoboy_compose_test_pattern();
    picoboy_present();
}

// ---- Piezo beep ------------------------------------------------------------
void picoboy_tone(uint32_t freq_hz, uint32_t dur_ms)
{
    if (freq_hz == 0 || dur_ms == 0) return;

    gpio_set_function(PICOBOY_PIN_SPEAKER, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PICOBOY_PIN_SPEAKER);
    uint chan  = pwm_gpio_to_channel(PICOBOY_PIN_SPEAKER);

    uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t wrap   = sys_hz / freq_hz;
    uint8_t  div    = 1;
    while (wrap > 65535) { wrap /= 2; div *= 2; }

    pwm_set_clkdiv_int_frac(slice, div, 0);
    pwm_set_wrap(slice, wrap - 1);
    pwm_set_chan_level(slice, chan, wrap / 2);
    pwm_set_enabled(slice, true);

    sleep_ms(dur_ms);

    pwm_set_enabled(slice, false);
    gpio_init(PICOBOY_PIN_SPEAKER);
    gpio_set_dir(PICOBOY_PIN_SPEAKER, GPIO_OUT);
    gpio_put(PICOBOY_PIN_SPEAKER, 0);
}
