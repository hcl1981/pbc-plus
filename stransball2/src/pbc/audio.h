#ifndef PBC_AUDIO_H
#define PBC_AUDIO_H
#include <stdbool.h>
#include "../game.h"

/* Gibt false zurueck, wenn der Tonpfad nicht startet -- das Spiel laeuft dann
 * stumm weiter statt haengenzubleiben (CLAUDE.md Abschnitt 4). */
bool pbc_audio_init(void);
void pbc_audio_frame(const stb_audio_t *a);     /* einmal je Bild aufrufen */
void pbc_audio_silence(void);
bool pbc_audio_ok(void);
#endif
