#pragma once
#include <stdint.h>
typedef struct { volatile uint32_t timerawl; } fake_timer_hw_t;
extern fake_timer_hw_t *timer_hw;
