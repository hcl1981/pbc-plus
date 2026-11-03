/* roms.S -- the ZX Spectrum ROMs, built in. They are Amstrad's, who allow
 * them to be passed on with emulators (see ../roms/LICENSE.roms); the
 * files are the originals, the copyright messages unchanged:
 *   48.rom     16 KB  the 48K ROM
 *   128-0.rom  16 KB  the 128K's editor ROM
 *   128-1.rom  16 KB  the 128K's 48 BASIC ROM */
    .section .rodata.zxroms, "a"
    .balign 4
    .global zx_rom48
zx_rom48:
    .incbin "48.rom"
    .global zx_rom128_0
zx_rom128_0:
    .incbin "128-0.rom"
    .global zx_rom128_1
zx_rom128_1:
    .incbin "128-1.rom"
