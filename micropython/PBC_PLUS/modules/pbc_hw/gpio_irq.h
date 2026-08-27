// Shared declarations for the pbc_hw GPIO IRQ + lifecycle plumbing.

#ifndef PBC_HW_GPIO_IRQ_H
#define PBC_HW_GPIO_IRQ_H

#include "py/obj.h"

// One-time enable of IO_IRQ_BANK0 at the NVIC level. Idempotent.
void pbc_hw_gpio_irq_dispatcher_init(void);

// Defensive HW refresh -- reapplies pin config + IRQ enables that
// MicroPython's machine_pin_init() resets on every soft-reboot.
// Called from every public Python entry point in pbc_hw.c.
void pbc_hw_sync_lifecycle(void);

// Subsystem entry points called from raw IRQ handlers.
void buttons_handle_gpio_irq(uint32_t gpio, uint32_t events);
void accel_handle_gpio_irq(uint32_t gpio, uint32_t events);

// Drop cached Python callback references. Called from
// pbc_hw_init() (which pbc.py's module body runs on every reboot).
void buttons_clear_callbacks(void);

#endif // PBC_HW_GPIO_IRQ_H
