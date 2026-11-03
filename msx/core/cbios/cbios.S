/* cbios.S -- C-BIOS 0.29a (BSD-style licence, see LICENSE.cbios), built
 * into the firmware:
 *   MSX1: the international main ROM (60 Hz) for slot 0, 0000h-7FFFh, and
 *         the logo ROM for 8000h-BFFFh;
 *   MSX2: main ROM and logo ROM likewise, the sub-ROM (slot 3-0) and
 *         C-DISK, the disk ROM (slot 3-3), which boots a disk's boot
 *         sector but has neither BASIC nor MSX-DOS.
 * The third 8 KB page of both main ROMs is the second of the sub-ROM (the
 * font); it is there once, cbios_shared (checked by msx/CMakeLists.txt).
 * Only what is not FFh filling goes in: of the logo ROMs the first 8 KB
 * (A000h-BFFFh are all FFh), of C-DISK the code at 4000h-44FFh and the
 * two bytes at 7F00h (msx/src/main.c puts it together again). */
    .section .rodata.cbios, "a"
    .balign 4
    .global cbios_shared
cbios_shared:                           /* main ROMs 4000h-5FFFh, sub-ROM 2000h-3FFFh */
    .incbin "cbios_main_msx1.rom", 0x4000, 0x2000
    .global cbios_main01
cbios_main01:                           /* MSX1 main ROM 0000h-3FFFh */
    .incbin "cbios_main_msx1.rom", 0, 0x4000
    .global cbios_main3
cbios_main3:                            /* MSX1 main ROM 6000h-7FFFh */
    .incbin "cbios_main_msx1.rom", 0x6000, 0x2000
    .global cbios_logo
cbios_logo:
    .incbin "cbios_logo_msx1.rom", 0, 0x2000
    .global cbios2_main01
cbios2_main01:
    .incbin "cbios_main_msx2.rom", 0, 0x4000
    .global cbios2_main3
cbios2_main3:
    .incbin "cbios_main_msx2.rom", 0x6000, 0x2000
    .global cbios2_logo
cbios2_logo:
    .incbin "cbios_logo_msx2.rom", 0, 0x2000
    .global cbios2_sub0
cbios2_sub0:                            /* sub-ROM 0000h-1FFFh */
    .incbin "cbios_sub.rom", 0, 0x2000
    .global cbios_disk
cbios_disk:
    .incbin "cbios_disk.rom", 0, 0x500
    .global cbios_disk_tail
cbios_disk_tail:
    .incbin "cbios_disk.rom", 0x3f00, 0x100
