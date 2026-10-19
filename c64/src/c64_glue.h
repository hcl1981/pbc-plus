/* c64_glue.h -- C entry points into the Frodo core (core/rp2350/C64_rp2350.cpp)
 * and the small glue modules around it. */
#ifndef PBC_C64_GLUE_H
#define PBC_C64_GLUE_H

#include <stdint.h>
#include <stdbool.h>

/* Before c64_init: ROMs to use instead of the built-in OpenROMs (NULL: built-in) */
void      c64_set_roms(const uint8_t *basic, const uint8_t *kernal, const uint8_t *chargen);
void      c64_init(void);
bool      c64_run_frame(void);
uint8_t  *c64_get_ram(void);
uint16_t  c64_get_pc(void);
/* .d64: mount as drive 8 and type LOAD"*",8,1; .prg: load and type RUN */
void      c64_load_file(const char *path);
/* Only mount a disk image as drive 8 (data/size unused). */
void      c64_mount_disk(const uint8_t *data, uint32_t size, const char *path);
void      c64_type_string(const char *str);

/* VIC output buffer, 384 x 272, one colour index per pixel (Display_rp2350.cpp) */
extern uint8_t g_pixels[];
#define C64_FB_W 384
#define C64_FB_H 272

/* c64_audio.c: hand this frame's SID samples to the speaker */
void c64_audio_flush(void);

#endif /* PBC_C64_GLUE_H */
