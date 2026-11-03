/* No save states, no SSF2 mapper and no battery RAM on the PC. */
#include "gwenesis_savestate.h"
SaveState* saveGwenesisStateOpenForRead(const char* f) { (void)f; return 0; }
SaveState* saveGwenesisStateOpenForWrite(const char* f) { (void)f; return 0; }
int saveGwenesisStateGet(SaveState* s, const char* t) { (void)s; (void)t; return 0; }
void saveGwenesisStateSet(SaveState* s, const char* t, int v) { (void)s; (void)t; (void)v; }
void saveGwenesisStateGetBuffer(SaveState* s, const char* t, void* b, int l) { (void)s; (void)t; (void)b; (void)l; }
void saveGwenesisStateSetBuffer(SaveState* s, const char* t, void* b, int l) { (void)s; (void)t; (void)b; (void)l; }
void gwenesis_mapper_write(unsigned int a, unsigned int v) { (void)a; (void)v; }

/* No battery RAM on the PC either (src/sram.c): ROM up to 8 MB. */
#include <stdbool.h>
extern unsigned char *ROM_DATA;
unsigned int m68k_rom_end = 0x800000;
int sram_read8(unsigned int a) { (void)a; return -1; }
bool sram_write8(unsigned int a, unsigned int v) { (void)a; (void)v; return false; }
void sram_control(unsigned int v) { (void)v; }
unsigned int m68ki_read_high_8(unsigned int a) { return ROM_DATA[a ^ 1]; }
unsigned int m68ki_read_high_16(unsigned int a) { return *(const unsigned short *)(ROM_DATA + a); }
unsigned int m68ki_read_high_32(unsigned int a)
{
    return (unsigned int)*(const unsigned short *)(ROM_DATA + a) << 16 | *(const unsigned short *)(ROM_DATA + a + 2);
}
