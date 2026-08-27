#ifndef PBC_INPUT_H
#define PBC_INPUT_H
#include <stdint.h>
void     pbc_input_init(void);
/* Rohzustand der sieben Tasten, Bits wie PAD_* aus game.h */
uint16_t pbc_input_raw(void);
/* Entprellt, mit Flankenbits TRG_* in den oberen 8 Bit */
uint16_t pbc_input_read(void);
#endif
