#ifndef PBC_DISPLAY_H
#define PBC_DISPLAY_H
#include <stdint.h>
#include <stdbool.h>
#include "../font.h"

void pbc_display_init(void);
void pbc_display_backlight(uint8_t level);          /* 0..255 */

/* Fenster in sichtbaren Koordinaten (0..239 x 0..279); der Y-Versatz von
 * 20 Zeilen des Panels wird hier zugerechnet. */
void pbc_display_window(int x, int y, int w, int h);

void pbc_display_begin(void);                       /* CS an, 16-Bit-Format  */
void pbc_display_send(const uint16_t *px, int n);   /* per DMA, kehrt sofort zurueck */
void pbc_display_wait(void);                        /* letzten Transfer abwarten */
void pbc_display_end(void);                         /* abwarten, CS aus      */

/* blockierende Notausgabe -- ohne DMA, fuer den Fehlerbildschirm */
void pbc_display_fill(int x, int y, int w, int h, uint16_t colour);
/* y ist die Oberkante der Textzeile; die Grundlinie ergibt sich aus der Schrift. */
void pbc_display_text(int x, int y, const font_t *f, const char *s, uint16_t fg, uint16_t bg);
#endif
