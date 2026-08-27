/*
 * pbc_input -- Steuerkreuz und Knoepfe des PicoBoy. Siehe pbc_input.c fuer die
 * Tastenzuordnung und die beiden Kombinationen, die ESC und den zweiten
 * Begleiter erreichbar machen.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_INPUT_H
#define PBC_INPUT_H

#include <stdbool.h>

void pbc_input_init(void);

/* Rueckgabewerte von pbc_input_wait_choice(). */
enum
{
	PBC_CHOICE_A = 0,
	PBC_CHOICE_B,
	PBC_CHOICE_CENTER
};

/*
 * Auf A, B oder Mitte warten und melden, welche Taste es war.
 *
 * Nur fuer die Auswahl beim Start gedacht, bevor OpenTyrian laeuft: dessen
 * Netzwerkmodus ist im Original ueber die Kommandozeile zu waehlen, und die
 * gibt es hier nicht. Blockiert und entprellt grob ueber die Wartezeit.
 */
int pbc_input_wait_choice(void);

/* Ist diese Taste (PBC_CHOICE_*) gerade gedrueckt? Fuer Abfragen beim Start. */
bool pbc_input_button_held(int choice);

#endif /* PBC_INPUT_H */
