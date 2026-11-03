/* battery.c -- see battery.h */
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "battery.h"

#define PIN      29
#define INPUT    3

static int mv = -1;                 /* smoothed, millivolts */
static bool usb;
static int bars;
static uint64_t next;

static int measure(void)
{
    adc_select_input(INPUT);
    (void)adc_read();
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++)
        sum += adc_read();
    return (int)(sum * 3300u * 3u / (16u * 4095u));
}

/* two AAA cells: 3.2 V fresh, about 2.2 V where they give up */
static int bars_for(int v)
{
    return v >= 2850 ? 4 : v >= 2650 ? 3 : v >= 2450 ? 2 : v >= 2250 ? 1 : 0;
}

void battery_init(void)
{
    adc_init();
    adc_gpio_init(PIN);
    sleep_ms(30);                   /* the filter capacitor on the divider */
    mv = measure();
    usb = mv > 3800;
    bars = bars_for(mv);
}

void battery_poll(void)
{
    uint64_t now = time_us_64();
    if (now < next)
        return;
    next = now + 250000;
    int v = measure();
    if (v > 3800 || usb) {          /* the BAT/USB switch shows at once */
        usb = v > 3800;
        mv = v;
        bars = bars_for(v);
        return;
    }
    mv += (v - mv) / 4;
    /* hysteresis: a bar goes or comes 40 mV past its boundary */
    int b = bars_for(mv);
    if (b < bars && bars_for(mv + 40) < bars)
        bars = b;
    else if (b > bars && bars_for(mv - 40) > bars)
        bars = b;
}

bool battery_on_usb(void) { return usb; }
int  battery_bars(void)   { return bars; }
