#pragma once
#include <stdint.h>
typedef struct { int dummy; } dma_channel_config;
static inline unsigned dma_claim_unused_timer(bool){return 0;}
static inline void dma_timer_set_fraction(unsigned,uint16_t,uint16_t){}
static inline int dma_claim_unused_channel(bool){static int n=0;return n++;}
static inline dma_channel_config dma_channel_get_default_config(int){dma_channel_config c={0};return c;}
#define DMA_SIZE_16 1
static inline void channel_config_set_transfer_data_size(dma_channel_config*,int){}
static inline void channel_config_set_read_increment(dma_channel_config*,bool){}
static inline void channel_config_set_write_increment(dma_channel_config*,bool){}
static inline void channel_config_set_dreq(dma_channel_config*,unsigned){}
static inline void channel_config_set_chain_to(dma_channel_config*,int){}
static inline void dma_channel_configure(int,dma_channel_config*,volatile void*,const void*,uint32_t,bool){}
static inline unsigned dma_get_timer_dreq(unsigned){return 0;}
static inline void dma_channel_set_irq1_enabled(int,bool){}
static inline void dma_channel_start(int){}
static inline bool dma_channel_get_irq1_status(int){return false;}
static inline void dma_channel_acknowledge_irq1(int){}
static inline void dma_channel_set_read_addr(int,const void*,bool){}
static inline void dma_channel_set_trans_count(int,uint32_t,bool){}
