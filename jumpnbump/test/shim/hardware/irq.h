#pragma once
#include <stdint.h>
#define DMA_IRQ_1 1
#define PICO_HIGHEST_IRQ_PRIORITY 0
#define PICO_LOWEST_IRQ_PRIORITY 255
#define PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY 128
typedef void (*irq_handler_t)(void);
static inline void irq_add_shared_handler(unsigned,irq_handler_t,uint8_t){}
static inline void irq_set_enabled(unsigned,bool){}
static inline void irq_set_priority(unsigned,uint8_t){}
