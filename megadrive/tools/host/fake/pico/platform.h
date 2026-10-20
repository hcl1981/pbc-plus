/* Stand-ins for the Pico SDK headers the emulator core includes. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#define __not_in_flash_func(f) f
#define __no_inline_not_in_flash_func(f) __attribute__((noinline)) f
#define __time_critical_func(f) f
#define __not_in_flash(x)
#define __scratch_x(x)
#define __scratch_y(x)
#ifndef __aligned
#define __aligned(x) __attribute__((aligned(x)))
#endif
#define __fast_mul(a, b) ((a) * (b))
static inline void tight_loop_contents(void) {}
static inline void __dmb(void) {}
static inline unsigned get_core_num(void) { return 0; }
