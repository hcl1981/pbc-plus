/* fdc.h -- the CPC 6128's floppy controller (NEC uPD765) with drive A
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). The disk is a
 * .dsk image (standard "MV - CPCEMU" or "EXTENDED CPC DSK"), read in place
 * from flash. The commands AMSDOS and most games use: specify, sense drive
 * and interrupt status, recalibrate, seek, read ID, read (deleted) data,
 * write data and format, the last two accepted and dropped (the image in
 * flash stays as it is). Bytes go through the data register on demand,
 * without the real controller's timing; the status bytes of the image's
 * sectors (copy protections with CRC errors and the like) are passed on.
 */
#ifndef CPC_FDC_H
#define CPC_FDC_H

#include <stdint.h>
#include <stdbool.h>

/* false: not a .dsk */
bool fdc_insert(const uint8_t *dsk, uint32_t size);
void fdc_reset(void);

uint8_t fdc_status(void);               /* port FB7Eh */
uint8_t fdc_read(void);                 /* port FB7Fh */
void fdc_write(uint8_t v);              /* port FB7Fh */
void fdc_motor(bool on);                /* port FA7Eh */

/* The disk's first program for RUN"..." (8.3 without spaces, e.g.
 * "NIBDX.BAS"), "" for a CP/M boot disk (|CPM), NULL if none was found. */
const char *fdc_first_program(void);

#endif
