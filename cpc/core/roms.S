/* roms.S -- the CPC 6128's ROMs, built in. They are Amstrad's and
 * Locomotive Software's (AMSDOS also Digital Research's); Amstrad allow
 * them to be passed on with emulators (see ../roms/LICENSE.roms). The
 * files are the originals, the copyright messages unchanged:
 *   cpc6128.rom  32 KB  the OS (lower ROM) and BASIC 1.1 (upper ROM 0)
 *   amsdos.rom   16 KB  AMSDOS (upper ROM 7) */
    .section .rodata.cpcroms, "a"
    .balign 4
    .global cpc_rom6128
cpc_rom6128:
    .incbin "cpc6128.rom"
    .global cpc_rom_amsdos
cpc_rom_amsdos:
    .incbin "amsdos.rom"
