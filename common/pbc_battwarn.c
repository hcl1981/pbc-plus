/* pbc_battwarn.c -- see pbc_battwarn.h */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "display.h"
#include "color565.h"
#include "pbc_battwarn.h"
#include "../pbcemu/src/battery.h"

/* The box around the symbol, and the symbol itself: the launcher's battery
 * (pbcemu/src/ui.c, battery_icon) with no bar, struck through in red. */
#define BX 199
#define BY 10
#define BW 30
#define BH 18
#define RED 0xe5484d                    /* the launcher's LOW_RED */
#include "pbc_battwarn_mask.h"

static bool ready, low, shown;
static int rx, ry, rw, rh;              /* the picture */
static int margin_rows = -1;            /* rows of the box in the border, as last drawn */

/* 5/6/5 blend whatever the field order, as gfx.c; a 0..15 */
static inline uint16_t blend(uint16_t d, uint16_t c, unsigned a)
{
    uint32_t dd = (d | (uint32_t)d << 16) & 0x07E0F81Fu, cc = (c | (uint32_t)c << 16) & 0x07E0F81Fu;
    uint32_t m = (dd + ((cc - dd) * (a * 2 + (a >> 3)) >> 5)) & 0x07E0F81Fu;
    return (uint16_t)(m | m >> 16);
}

static uint16_t pixel(uint16_t under, int i, int j)
{
    static uint16_t red;
    if (!red)
        red = pbc_rgb24(RED);
    uint8_t m = battwarn_mask[j][i];
    if (m & 15)
        under = blend(under, 0, m & 15);
    if (m >> 4)
        under = blend(under, red, m >> 4);
    return under;
}

/* the box's rows above the picture (all of it if the picture is elsewhere) */
static int border_rows(void)
{
    bool cols = rx < BX + BW && rx + rw > BX;
    bool rows = ry < BY + BH && ry + rh > BY;
    if (!cols || !rows)
        return BH;
    return ry > BY ? ry - BY : 0;
}

void pbc_battwarn_frame(int x, int y, int w, int h)
{
    if (!ready) {
        /* Most emulators stop clk_adc when they take pll_usb for clk_peri
         * (their overclock()); adc_init() would wait for the ADC forever.
         * The crystal's 12 MHz will do: 17 readings take 140 us then. */
        if (clock_get_hz(clk_adc) == 0)
            clock_configure(clk_adc, 0, CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
                            XOSC_HZ, XOSC_HZ);
        battery_init();
        ready = true;
    }
    battery_poll();                     /* four times a second */
    low = !battery_on_usb() && battery_bars() == 0;
    bool on = low && ((time_us_64() / 500000u) & 1) == 0;
    rx = x; ry = y; rw = w; rh = h;
    int n = border_rows();
    if (on == shown && n == margin_rows)
        return;
    shown = on;
    margin_rows = n;
    if (n <= 0)
        return;
    /* into the border: the symbol or black */
    static uint16_t px[BW * BH];
    for (int j = 0; j < n; j++)
        for (int i = 0; i < BW; i++)
            px[j * BW + i] = on ? pixel(0, i, j) : 0;
    pbc_display_window(BX, BY, BW, n);
    pbc_display_begin();
    pbc_display_send(px, BW * n);
    pbc_display_wait();
    pbc_display_end();
}

bool pbc_battwarn_rows(void)
{
    return shown && border_rows() < BH;
}

void pbc_battwarn_row(uint16_t *row, int y, int x, int w)
{
    if (!shown || y < BY || y >= BY + BH)
        return;
    int j = y - BY;
    for (int i = 0; i < BW; i++) {
        int c = BX + i - x;
        if (c >= 0 && c < w)
            row[c] = pixel(row[c], i, j);
    }
}
