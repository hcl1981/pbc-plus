#ifndef PBC_HW_BATTERY_H
#define PBC_HW_BATTERY_H

// One-time ADC + GPIO bring-up, plus a settling delay so the on-board
// filter cap on the sense divider has time to charge to its
// steady-state level. Idempotent. Called from pbc_hw_init() at boot
// so that the very first user-visible battery_voltage() read sees a
// fully-settled circuit; calling it later is harmless.
void battery_init(void);

// Battery / VBUS voltage in volts. Averages 16 samples to take the
// edge off ADC noise; the call returns inside ~150 µs once warm.
float battery_voltage(void);

#endif // PBC_HW_BATTERY_H
