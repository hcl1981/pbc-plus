/*
 *  ROM_data.h - C64 ROMs for PBC C64 (MEGA65 OpenROMs, see ROM_data.cpp)
 *
 *  Replaces frank-c64's built-in Commodore ROMs, which are not free.
 */
#ifndef ROM_DATA_H
#define ROM_DATA_H

#include <stdint.h>
#include "../C64.h"

extern const uint8_t BuiltinBasicROM[BASIC_ROM_SIZE];
extern const uint8_t BuiltinKernalROM[KERNAL_ROM_SIZE];
extern const uint8_t BuiltinCharROM[CHAR_ROM_SIZE];
extern const uint8_t BuiltinDriveROM[DRIVE_ROM_SIZE];

/* The KERNAL bytes under the DOS-level drive traps (offset from $E000). */
typedef struct { uint16_t off; uint8_t orig[3]; } KernalTrap;
extern const KernalTrap BuiltinKernalTraps[6];

#endif
