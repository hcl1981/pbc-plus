// Kleiner Kompatibilitaets-Shim fuer den whd_gen-Build ausserhalb von glibc
// (z.B. Windows/mingw). Wird per -include vor allen Quellen eingezogen.
#ifndef WHD_GEN_COMPAT_H
#define WHD_GEN_COMPAT_H

// glibc definiert __STRING(x) in <sys/cdefs.h>; mingw/andere nicht.
#ifndef __STRING
#define __STRING(x) #x
#endif

#endif
