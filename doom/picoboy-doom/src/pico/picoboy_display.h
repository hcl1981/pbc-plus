//
// PicoBoy ST7789 display driver -- public interface.
//
// GPLv2 (same as rp2040-doom).
//

#ifndef _PICOBOY_DISPLAY_H
#define _PICOBOY_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize SPI, ST7789, DMA channel, backlight. Call once at startup.
void picoboy_display_init(void);

// Compose one Doom frame into our 240x180 RGB565 framebuffer.
//
//   frame_buffer  -- pointer to Doom's two 320x168 byte buffers, packed as
//                    frame_buffer[2][320*168]. Caller passes the same array
//                    used by the renderer.
//   display_idx   -- which of the two buffers holds the active frame body
//                    (0 or 1). The status bar lives in the OTHER buffer.
//   palette       -- 256-entry RGB565 palette currently active.
//
void picoboy_compose_frame_doom(const uint8_t *fb_a,
                                const uint8_t *fb_b,
                                const uint16_t palette[256]);

// Simple drawing helpers (framebuffer only -- call picoboy_present() after).
void picoboy_display_clear(uint16_t color);
void picoboy_display_fill_rect(int x, int y, int w, int h, uint16_t color);

// Push the current framebuffer to the ST7789 via SPI+DMA. Blocks until the
// previous transfer has finished; returns once the new transfer is queued.
void picoboy_present(void);

// Diagnostic helper: init display, paint the RGB test stripes, push them.
// Call this early in main() (right after the backlight ramp-up) to verify
// the SPI/display path independently of the Doom renderer. Also handy as
// a "I made it this far" signal if the engine hangs further along.
void picoboy_display_show_test_pattern(void);

// Zwei zentrierte Textzeilen in den schwarzen Streifen unter dem Doom-Bild
// schreiben (5x7-Font, doppelt skaliert, max. 20 Zeichen je Zeile). Wird nur
// tatsaechlich uebertragen, wenn sich der Text geaendert hat.
void picoboy_status_lines(const char *l1, const char *l2);
void picoboy_status_lines3(const char *l1, const char *l2, const char *l3);

// True, solange der Framebuffer-Transfer zum Panel laeuft.
bool picoboy_display_dma_busy(void);

// PWM-driven beep on the piezo. Blocks for dur_ms.
void picoboy_tone(uint32_t freq_hz, uint32_t dur_ms);

#ifdef __cplusplus
}
#endif

#endif // _PICOBOY_DISPLAY_H
