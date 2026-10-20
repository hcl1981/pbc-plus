/* No save states and no SSF2 mapper on the PC. */
#include "gwenesis_savestate.h"
SaveState* saveGwenesisStateOpenForRead(const char* f) { (void)f; return 0; }
SaveState* saveGwenesisStateOpenForWrite(const char* f) { (void)f; return 0; }
int saveGwenesisStateGet(SaveState* s, const char* t) { (void)s; (void)t; return 0; }
void saveGwenesisStateSet(SaveState* s, const char* t, int v) { (void)s; (void)t; (void)v; }
void saveGwenesisStateGetBuffer(SaveState* s, const char* t, void* b, int l) { (void)s; (void)t; (void)b; (void)l; }
void saveGwenesisStateSetBuffer(SaveState* s, const char* t, void* b, int l) { (void)s; (void)t; (void)b; (void)l; }
void gwenesis_mapper_write(unsigned int a, unsigned int v) { (void)a; (void)v; }
