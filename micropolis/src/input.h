#pragma once
#include <stdint.h>

void     input_init(void);

// Raw button state as an IN_* bitmask (bits set = pressed).
uint32_t input_raw(void);

// Edge + auto-repeat. Call once per frame. Returns the bits that should
// "fire" this frame: a button fires immediately on press, then repeats
// after a short hold. Good for cursor movement and menu navigation.
uint32_t input_poll(void);
