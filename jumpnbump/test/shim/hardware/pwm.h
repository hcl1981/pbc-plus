#pragma once
#include <stdint.h>
typedef struct { uint32_t csr, div, ctr, cc, top; } pwm_slice_hw_t;
typedef struct { pwm_slice_hw_t slice[12]; } pwm_hw_t;
extern pwm_hw_t *pwm_hw;
typedef struct { int dummy; } pwm_config;
static inline unsigned pwm_gpio_to_slice_num(unsigned g){return (g>>1)&7;}
static inline unsigned pwm_gpio_to_channel(unsigned g){return g&1;}
static inline pwm_config pwm_get_default_config(void){pwm_config c={0};return c;}
static inline void pwm_config_set_clkdiv(pwm_config*,float){}
static inline void pwm_config_set_wrap(pwm_config*,uint32_t){}
static inline void pwm_init(unsigned,pwm_config*,bool){}
static inline void pwm_set_chan_level(unsigned,unsigned,uint16_t){}
