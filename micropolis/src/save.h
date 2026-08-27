#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Minimal persistence into the top SAVE_REGION_BYTES of flash.
// Good enough for the city state (~tens of KB); a LittleFS port can
// replace this later without touching callers.

void save_init(void);

// Write up to SAVE_REGION_BYTES. Returns false on size overflow.
// NOTE: erases/programs flash with interrupts disabled. On a dual-core
// build the other core must be parked during this call.
bool save_write(const void *buf, size_t len);

// Read len bytes from the save region (memcpy from XIP-mapped flash).
// Returns false if the region's magic header is absent.
bool save_read(void *buf, size_t len);

bool save_present(void);
