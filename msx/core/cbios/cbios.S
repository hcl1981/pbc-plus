/* cbios.S -- C-BIOS 0.29a for MSX1 (BSD-style licence, see LICENSE.cbios),
 * built into the firmware: the international main ROM (60 Hz) for slot 0,
 * 0000h-7FFFh, and the logo ROM for 8000h-BFFFh. */
    .section .rodata.cbios, "a"
    .balign 4
    .global cbios_main
cbios_main:
    .incbin "cbios_main_msx1.rom"
    .global cbios_logo
cbios_logo:
    .incbin "cbios_logo_msx1.rom"
