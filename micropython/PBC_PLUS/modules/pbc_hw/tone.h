#ifndef PBC_HW_TONE_H
#define PBC_HW_TONE_H

#include <stdint.h>

// Start a square-wave tone on the speaker pin.
// freq_hz   — clamped to [20, 20000]; 0 silences the output.
// duration_ms == 0 → tone runs until tone_off().
// duration_ms  > 0 → an SDK alarm calls tone_off() after that time
//                    (non-blocking).
void tone_play(uint32_t freq_hz, uint32_t duration_ms);

// Cut output. Cancels any pending duration alarm and drives the pin
// low so the speaker doesn't sit at half-rail.
void tone_off(void);

#endif // PBC_HW_TONE_H
