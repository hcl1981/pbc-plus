#include "battery.h"

#include "hardware/adc.h"
#include "pico/time.h"

#include "pbc_hw_config.h"

static bool battery_initialized = false;

void battery_init(void) {
    if (battery_initialized) {
        return;
    }
    adc_init();
    adc_gpio_init(BATTERY_PIN);

    // The PBC+ has a filter capacitor across the bottom of the sense
    // divider for noise rejection. Its RC time constant against the
    // divider's source impedance is on the order of milliseconds, so
    // the very first conversions after adc_gpio_init() come back well
    // below the actual voltage. Pause a comfortable margin here so
    // the cap is fully charged by the time we read.
    sleep_ms(50);

    // Belt-and-braces: also do a handful of throwaway reads to settle
    // the ADC's own internal sample/hold cap.
    adc_select_input(BATTERY_ADC_INPUT);
    for (int i = 0; i < 8; i++) {
        (void)adc_read();
    }

    battery_initialized = true;
}

float battery_voltage(void) {
    battery_init();
    adc_select_input(BATTERY_ADC_INPUT);

    // Discard two samples after channel selection so the S/H cap is
    // re-settled (cheap insurance against other code switching ADC
    // channels between calls).
    (void)adc_read();
    (void)adc_read();

    // 16-sample average -- enough to drop a few mV of jitter without
    // blocking long enough to be felt in a game loop.
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += adc_read();
    }
    float avg   = (float)(sum / 16);
    float v_pin = avg * BATTERY_VREF / BATTERY_ADC_MAX;

    // The PBC+ has a 1:3 step-down divider on the battery sense line:
    // the ADC sees one third of the actual VBATT/VBUS, so multiply by
    // 3 to recover. BATTERY_DIVIDER from pbc_hw_config.h is ignored
    // -- its factory value of 2.0 doesn't match this hardware.
    return v_pin * 3.0f;
}
