/* vtest.c: drive src/video.c (included) over random frames in every mode, hash the panel stream */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "video.c"
static uint64_t h;
static inline uint64_t mix64(uint64_t a, uint64_t v) { a ^= v + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2); a *= 0xff51afd7ed558ccdull; return a ^ (a >> 31); }
void pbc_display_window(int x, int y, int w, int hh) { h = mix64(h, x * 1000003u + y * 1009u + w * 7u + hh); }
void pbc_display_begin(void) {}
void pbc_display_send(const uint16_t *px, int n) { for (int i = 0; i < n; i++) h = mix64(h, px[i]); }
bool pbc_display_busy(void) { return false; }
void pbc_display_end(void) {}
void pbc_display_fill(int x, int y, int w, int hh, uint16_t c) {}
void pbc_display_text_centered(int y, const char *s, uint16_t fg, uint16_t bg, int scale) {}
static uint32_t rng = 1;
static uint32_t rnd(void) { rng = rng * 1103515245u + 12345u; return rng >> 8; }
static uint8_t luts[8][256];
int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 200;
    hdmi_line_lut_reset();
    for (int k = 0; k < 8; k++) for (int i = 0; i < 256; i++) luts[k][i] = rnd() & 0xff;
    for (int f = 0; f < frames; f++) {
        int mode = f & 1 ? VIDEO_CROP : VIDEO_FIT;
        int w = (f >> 1) & 1 ? 256 : 320, hh = (f >> 2) & 1 ? 240 : 224;
        if (f % 16 == 0 || (f % 8) == 1) video_set_mode(mode); else video_set_mode(mode);
        for (int i = 0; i < 256; i++) pbc_palette[i] = (uint16_t)rnd();
        for (size_t i = 0; i < sizeof video_screen; i++) video_screen[0][i] = (uint8_t)rnd();
        for (int y = 0; y < HDMI_MAX_LINES; y++) hdmi_line_lut[y] = (rnd() & 3) ? hdmi_identity_lut : luts[rnd() & 7];
        video_frame_begin(w, hh);
        for (int y = 0; y < hh; y++) {
            video_line_done(y);
            if (y == hh - 3) video_palette_freeze();
            video_service();
        }
        while (!video_idle()) video_service();
        printf("%d %d %d %d %016llx\n", f, mode, w, hh, (unsigned long long)h);
    }
    return 0;
}
