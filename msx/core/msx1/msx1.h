/* msx1.h -- an MSX1 computer for the PicoBoy Color Plus
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). The machine of
 * C-BIOS' MSX1 configuration: slot 0 the BIOS (C-BIOS main ROM at 0000h,
 * its logo ROM, 8 KB at 8000h; A000h-BFFFh read FFh), slots 1 and 2
 * cartridges, slot 3 64 KB of RAM;
 * TMS9918A video, AY-3-8910 sound with the joysticks on its port A,
 * 8255 PPI for the slots and the keyboard. Cartridges: plain ROMs up to
 * 64 KB and the Konami, Konami SCC, ASCII 8 KB and ASCII 16 KB MegaROM
 * mappers (guessed from the bank switching code in the ROM).
 *
 * Z80: redcode/Z80 (LGPL-3.0-or-later) through an 8 KB page map.
 */
#ifndef MSX1_H
#define MSX1_H

#include <stdint.h>
#include <stdbool.h>

enum { MAP_PLAIN, MAP_KONAMI, MAP_KONAMI_SCC, MAP_ASCII8, MAP_ASCII16 };

/* joystick bits (active high) */
#define MSX_JOY_UP    0x01
#define MSX_JOY_DOWN  0x02
#define MSX_JOY_LEFT  0x04
#define MSX_JOY_RIGHT 0x08
#define MSX_JOY_A     0x10
#define MSX_JOY_B     0x20

/* keyboard matrix position: row << 4 | bit */
#define MSX_KEY(row, bit) ((row) << 4 | (bit))
enum {
    MSXK_0 = MSX_KEY(0, 0), MSXK_SHIFT = MSX_KEY(6, 0), MSXK_CTRL = MSX_KEY(6, 1),
    MSXK_GRAPH = MSX_KEY(6, 2), MSXK_CAPS = MSX_KEY(6, 3), MSXK_CODE = MSX_KEY(6, 4),
    MSXK_F1 = MSX_KEY(6, 5), MSXK_F2 = MSX_KEY(6, 6), MSXK_F3 = MSX_KEY(6, 7),
    MSXK_F4 = MSX_KEY(7, 0), MSXK_F5 = MSX_KEY(7, 1), MSXK_ESC = MSX_KEY(7, 2),
    MSXK_TAB = MSX_KEY(7, 3), MSXK_STOP = MSX_KEY(7, 4), MSXK_BS = MSX_KEY(7, 5),
    MSXK_SELECT = MSX_KEY(7, 6), MSXK_RETURN = MSX_KEY(7, 7), MSXK_SPACE = MSX_KEY(8, 0),
    MSXK_HOME = MSX_KEY(8, 1), MSXK_INS = MSX_KEY(8, 2), MSXK_DEL = MSX_KEY(8, 3),
    MSXK_LEFT = MSX_KEY(8, 4), MSXK_UP = MSX_KEY(8, 5), MSXK_DOWN = MSX_KEY(8, 6),
    MSXK_RIGHT = MSX_KEY(8, 7),
};
/* the matrix position of a digit or letter ('0'-'9', 'A'-'Z') */
int msx_key_of_char(char c);

/* Input for the next frame: keys pressed (a bit per matrix position,
 * rows 0..10) and the two joysticks. */
extern uint8_t msx_keys[11];
extern uint8_t msx_joy[2];
static inline void msx_key_set(int k) { msx_keys[k >> 4] |= (uint8_t)(1 << (k & 15)); }

/* The system ROMs come as 8 KB pages, which need not lie together (the
 * built-in C-BIOS shares one between its ROMs); MSX_PAGES(p) for a ROM in
 * one piece. */
#define MSX_PAGES(p) { (p), (p) + 0x2000, (p) + 0x4000, (p) + 0x6000 }

/* main ROM 32 KB (4 pages), logo ROM 8 KB at 8000h (may be NULL) */
void msx1_init(const uint8_t *const bios_main[4], const uint8_t *bios_logo);

/* MSX2: main ROM 32 KB (slot 0; `bios_logo` at 8000h may be NULL), sub-ROM
 * 16 KB (2 pages, slots 3-0 and 3-1), disk ROM 16 KB (slot 3-3 at 4000h,
 * patched by msx2_patch_disk_rom first; NULL: no disk), 128 KB RAM mapper
 * in slot 3-2, V9938 with 128 KB VRAM, RP5C01 clock. Lines go to
 * msx_line565. */
void msx2_init(const uint8_t *const bios_main[4], const uint8_t *bios_logo,
               const uint8_t *const sub_rom[2], const uint8_t *disk_rom);
bool msx_is_msx2(void);
typedef void (*msx_line565_fn)(int y, int lines, const uint16_t *pixels);
extern msx_line565_fn msx_line565;

/* The disk ROM's driver entries become traps (opcode 64h); the whole
 * 16 KB image, which C-DISK (C-BIOS) needs patched in its body too */
void msx2_patch_disk_rom(uint8_t *rom16k);
/* Drive A: one 512-byte sector in or out; false on failure (a write:
 * write protected). */
typedef bool (*msx_disk_io_fn)(bool write, uint32_t sector, uint8_t *buf);
void msx2_insert_disk(msx_disk_io_fn io, uint32_t sectors);
/* the cartridge in slot 1 (stays where it is, e.g. in flash); returns the
 * mapper, -1 if it is no MSX cartridge */
int msx1_insert(const uint8_t *rom, uint32_t size);
void msx1_reset(void);

/* One frame (262 lines, 60 Hz): `line` gets each of the 192 visible lines
 * as colour indices 0..15 (256 pixels, TMS9918 palette); `audio` gets
 * `samples` samples. */
typedef void (*msx_line_fn)(int y, const uint8_t *pixels, int backdrop);
void msx1_frame(msx_line_fn line, int16_t *audio, int samples);

/* the RAM (MSX1 64 KB in slot 3, MSX2 128 KB), for the link's comparison */
uint8_t *msx1_ram(void);

const char *msx1_mapper_name(int mapper);

#endif
