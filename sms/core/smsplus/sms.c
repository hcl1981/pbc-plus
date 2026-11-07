
#include "shared.h"

/* SMS context */
t_sms sms;

/* Run the virtual console emulation for one frame */
/* The Super Game Module's AY on top of the PSG (both unipolar; main.c
   takes the offset out) */
static void in_ram(coleco_mix_ay)(signed short *left, signed short *right, int n) {
    while (n > 0) {
        int32 v[8] = { 0 };
        int k = n < 8 ? n : 8;
        ay_render(&coleco_ay, v, k);
        for (int i = 0; i < k; i++) {
            int32 l = left[i] + v[i], r = right[i] + v[i];
            left[i] = (signed short)(l > 32767 ? 32767 : l);
            right[i] = (signed short)(r > 32767 ? 32767 : r);
        }
        left += k;
        right += k;
        n -= k;
    }
}

/* This frame's samples in snd.buffer so far. PBC: rendered lazily, up to
   the line being run whenever the Z80 writes to a sound chip (the PSG, the
   Game Gear stereo port, the FM or AY chip; sms_sound_sync()), and the rest
   at the end of the frame. Same samples as rendering after every line: a
   line's samples got the registers as they were at its end, which they
   still are at the first write in a later line. */
static int samples_rendered;

static void in_ram(render_audio_to)(int target) {
    int n = target - samples_rendered;
    if (n > 0) {
        INT16 *part[2] = {
            snd.buffer[0] + samples_rendered,
            snd.buffer[1] + samples_rendered
        };
        SN76496Update(0, part, n, sms.psg_mask);
        if (coleco_ay_on) coleco_mix_ay(part[0], part[1], n);
        samples_rendered = target;
    }
}

/* Before a write that changes the sound: the samples of the lines before
   this one, with the registers as they are */
void in_ram(sms_sound_sync)(void) {
    if (snd.enabled && vdp.line < 262)
        render_audio_to(vdp.line * snd.bufsize / 262);
}

void in_ram(sms_frame)(int skip_render) {
    /* Take care of hard resets */
    if (input.system & INPUT_HARD_RESET) {
        system_reset();
    }

    /* Debounce pause key */
    if (input.system & INPUT_PAUSE) {
        if (!sms.paused) {
            sms.paused = 1;

            z80_set_nmi_line(ASSERT_LINE);
            z80_set_nmi_line(CLEAR_LINE);
        }
    } else {
        sms.paused = 0;
    }

    if (snd.log) snd.callback(0x00);

    /* Audio follows the PSG writes line by line, so that Z80 writes to its
       registers (e.g. Game Gear PCM speech streaming) modulate the output
       within a frame instead of being collapsed to a single end-of-frame
       sample-and-hold: 262 lines x 60 fps = 15.72 kHz update rate. */
    samples_rendered = 0;

    for (vdp.line = 0; vdp.line < 262; vdp.line += 1) {
        /* Handle VDP line events */
        vdp_run();

        /* Draw the current frame */
        if (!skip_render) render_line(vdp.line);

        /* Run the Z80 for a line */
        z80_execute(227);
    }

    /* The samples since the last sound write */
    if (snd.enabled)
        render_audio_to(snd.bufsize);
}


/* Unmapped ROM space (SG-1000, ColecoVision) reads as open bus: 0xFF, set
 * in sms_reset (as a const table it took 8 KB of flash). */
static uint8 sg_open_bus[0x2000];

void sms_init(void) {
    cpu_reset();
    sms_reset();
}


void sms_reset(void) {
    /* Clear SMS context */

    __builtin_memset(sg_open_bus, 0xFF, sizeof sg_open_bus);

    __builtin_memset(sms.sram, 0, SRAM_BYTES);
    __builtin_memset(sms.ram, 0, RAMSIZEBYTES);

    sms.paused = sms.save = sms.port_3F = sms.port_F2 = sms.irq = 0x00;
    sms.psg_mask = 0xFF;

    if (IS_COLECO) {
        coleco_sgm_reset();
        coleco_memory_map();
        return;
    }

    if (IS_SG) {
        /* Linear paging for the > 48 KB images that use the Sega mapper */
        sms.fcr[0] = 0x00;
        sms.fcr[1] = 0x00;
        sms.fcr[2] = 0x01;
        sms.fcr[3] = 0x02;
        sg_memory_map();
        return;
    }

    sms.fcr[0] = 0x00;
    sms.fcr[1] = 0x00;
    sms.fcr[2] = 0x01;
    sms.fcr[3] = 0x00;
    sms_memory_map();
}


/* Build the Master System / Game Gear memory map from the mapper registers.
   Called after reset and after a state load. */
void sms_memory_map(void) {
    int page;

    for (page = 0; page < 6; page++)
        cpu_writemap[page] = sms.dummy;
    cpu_readmap[6] = sms.ram;
    cpu_readmap[7] = sms.ram;
    cpu_writemap[6] = sms.ram;
    cpu_writemap[7] = sms.ram;

    if (cart.mapper == MAPPER_NONE) {
        /* The rom's 8K pages in order, a smaller rom repeating to fill the
           48K, as it does on a cartridge that leaves the upper address lines
           unconnected. Counting whole 16K pages, as this did, left the
           partial last page of homebrew such as Bomberman Boom (44920 bytes)
           and Ultima III (31233) out of reach, and both have code in it; an
           8K rom read the 8K after it at $2000. 8K pages mirror the same way
           for 16K, 32K and 48K roms. A partial page is mapped whole, so up to
           8K past the end of the rom is read: in flash that is the rest of the
           block the rom was programmed in, in PSRAM the next heap block.
           Neither is $FF, but a game does not run past its own end. */
        int pages8k = (cart.size + 0x1FFF) >> 13;
        for (page = 0; page < 6; page++)
            cpu_readmap[page] = &cart.rom[(page % pages8k) << 13];
        return;
    }

    /* The RAM control register goes first: after it, the bank registers
       only map ROM where it left ROM, and the Codemasters slot 1 register
       can put its cartridge RAM over $A000-$BFFF without being undone. */
    sms_mapper_w(0, sms.fcr[0]);
    sms_mapper_w(3, sms.fcr[3]);
    sms_mapper_w(2, sms.fcr[2]);
    sms_mapper_w(1, sms.fcr[1]);
}


/* Reset Z80 emulator */
void cpu_reset(void) {
    z80_reset(0);
    /* The console's BIOS leaves the stack in work RAM before it starts the
       cartridge; without a BIOS, SP would be 0 and the first push would land on
       the mapper registers at $FFFE-$FFFF. Ecco the Dolphin (GG) uses PUSH/POP as
       VDP delays before it sets SP, and jumped into the wrong bank. SMS Plus GX
       and Mesen2 use the same value. */
    z80_set_reg(Z80_SP, 0xDFF0);
    z80_set_irq_callback(sms_irq_callback);
}


/*--------------------------------------------------------------------------*/
/* SG-1000                                                                  */
/*--------------------------------------------------------------------------*/


/* Pages ($2000 and/or $8000) where a Taiwanese RAM adaptor may sit: bit n
   set = page n. Derived from the ROM image, so not part of the save state. */
static uint8 sg_adaptor_pages;

/* True when the ROM image has no data in this 8 KB page (only $00/$FF) */
static int sg_page_is_blank(int page) {
    int base = page << 13;
    int len = cart.size - base;
    int i;

    if (len > 0x2000) len = 0x2000;
    for (i = 0; i < len; i++) {
        uint8 b = cart.rom[base + i];
        if (b != 0x00 && b != 0xFF) return 0;
    }
    return 1;
}

/* Build the SG-1000 memory map. Called after reset and after a state load.
   $0000-$BFFF: cartridge ROM, 8 KB pages. Pages the ROM does not cover read
   open bus below $8000 and are cartridge RAM (sms.sram, 8 KB mirrored) from
   $8000 up (The Castle, Othello Multivision). Pages flagged in sms.port_3F
   were found to be Taiwanese RAM adaptor pages by sg_writemem.
   $C000-$FFFF: 8 KB work RAM, mirrored. */
void sg_memory_map(void) {
    int page;

    sg_adaptor_pages = 0;
    if (cart.size <= 0xC000) {
        if (sg_page_is_blank(1)) sg_adaptor_pages |= (1 << 1);
        if (sg_page_is_blank(4)) sg_adaptor_pages |= (1 << 4);
    }

    for (page = 0; page < 6; page++) {
        if ((page << 13) < cart.size) {
            cpu_readmap[page] = cart.rom + (page << 13);
            cpu_writemap[page] = sms.dummy;
        } else if (page < 4) {
            cpu_readmap[page] = (uint8 *)sg_open_bus;
            cpu_writemap[page] = sms.dummy;
        } else {
            cpu_readmap[page] = sms.sram;
            cpu_writemap[page] = sms.sram;
        }
        if (sms.port_3F & (1 << page)) {
            cpu_readmap[page] = sms.sram;
            cpu_writemap[page] = sms.sram;
        }
    }
    cpu_readmap[6] = sms.ram;
    cpu_readmap[7] = sms.ram;
    cpu_writemap[6] = sms.ram;
    cpu_writemap[7] = sms.ram;

    /* Images above 48 KB page through the Sega mapper (slots 1-3 only) */
    if (cart.size > 0xC000) {
        sms_mapper_w(1, sms.fcr[1]);
        sms_mapper_w(2, sms.fcr[2]);
        sms_mapper_w(3, sms.fcr[3]);
    }
}

static void sg_writemem(int address, int data) {
    int page = address >> 13;

    if (cpu_writemap[page] != sms.dummy) {
        cpu_writemap[page][address & 0x1FFF] = data;
        /* $FFFC (RAM paging) does not exist on SG carts */
        if (address >= 0xFFFD && cart.size > 0xC000) sms_mapper_w(address & 3, data);
        return;
    }

    /* A write into $2000 or $8000 means the Taiwanese 8 KB RAM adaptor: map
       RAM over that page from now on (idea from picodrive's write_bank_x8k).
       Only pages without ROM data qualify; Sega carts such as Pop Flamer
       write stray bytes into their own code, which must stay mapped. */
    if (sg_adaptor_pages & (1 << page)) {
        printf("SG: RAM adaptor detected at $%04X (write $%02X to $%04X)\n", page << 13, data, address);
        sms.port_3F |= (1 << page);
        cpu_readmap[page] = sms.sram;
        cpu_writemap[page] = sms.sram;
        sms.sram[address & 0x1FFF] = data;
    }
}

/* ColecoVision                                                             */
/*
 * $0000 the BIOS (8 KB), $2000-$5FFF nothing, $6000-$7FFF 1 KB of RAM eight
 * times over, $8000-$FFFF the cartridge (up to 32 KB). Ports by A7-A5:
 * $80 keypad mode, $C0 joystick mode, $A0 the video chip (A0: data or
 * control), $E0 the PSG (write) and the controllers (read, A1: which).
 *
 * The Super Game Module (Opcode Games) adds, on exact ports: an AY-3-8910
 * at $50 (register), $51 (write) and $52 (read); 24 KB of RAM over
 * $2000-$7FFF while bit 0 of port $53 is set; 8 KB of RAM instead of the
 * BIOS while bit 1 of port $7F is clear (games write $0D; $0F brings the
 * BIOS back). The RAM is coleco_sgm_ram, indexed by the address.
 */
const uint8 *coleco_bios;
uint8 coleco_key[2];
uint8 *coleco_sgm_ram;                  /* 32 KB, from system_init */
ay8910_t coleco_ay;
uint8 coleco_ay_on;                     /* the AY was written to: mix it in */
static uint8 coleco_keypad_mode, sgm_upper, sgm_lower;

void coleco_memory_map(void) {
    int page;
    cpu_readmap[0] = (uint8 *)coleco_bios;
    cpu_readmap[1] = (uint8 *)sg_open_bus;
    cpu_readmap[2] = (uint8 *)sg_open_bus;
    cpu_readmap[3] = sms.sram;
    for (page = 4; page < 8; page++) {
        uint32 off = (uint32)(page - 4) << 13;
        cpu_readmap[page] = off < (uint32)cart.size ? &cart.rom[off] : (uint8 *)sg_open_bus;
    }
    for (page = 0; page < 8; page++)
        cpu_writemap[page] = sms.dummy;
    if (!coleco_sgm_ram)
        return;
    if (sgm_lower)
        cpu_readmap[0] = coleco_sgm_ram;
    if (sgm_upper)
        for (page = 1; page < 4; page++)
            cpu_readmap[page] = coleco_sgm_ram + (page << 13);
}

void coleco_sgm_reset(void) {
    sgm_upper = sgm_lower = 0;
    coleco_ay_on = 0;
    ay_reset(&coleco_ay);
}

static void coleco_writemem(int address, int data) {
    int page = address >> 13;
    if ((page == 0 && sgm_lower) || (page >= 1 && page < 4 && sgm_upper)) {
        coleco_sgm_ram[address] = data;
        return;
    }
    if ((address & 0xE000) == 0x6000) {
        /* PBC: the eight mirrors in a row, the pointer loaded once */
        uint8 *p = sms.sram + (address & 0x3FF);
        p[0x0000] = data; p[0x0400] = data; p[0x0800] = data; p[0x0C00] = data;
        p[0x1000] = data; p[0x1400] = data; p[0x1800] = data; p[0x1C00] = data;
    }
}

/* PBC: SG-1000 (up to 48 KB, no mapper) and ColecoVision cartridges into
   the heap, if `margin` bytes stay free; after system_reset(). Their whole
   8 KB pages are mapped, so the copy takes as many bytes past the end of
   the image as the page holds: the same bytes as read from flash before.
   The game then reads no ROM through the XIP cache, which the emulator's
   flash code needs. Returns the bytes copied, 0 if it stays in flash. */
int sms_cart_to_ram(int margin) {
    if (!(IS_COLECO || IS_SG) || cart.size > (IS_COLECO ? 0x8000 : 0xC000))
        return 0;
    uint32 n = ((uint32)cart.size + 0x1FFF) & ~0x1FFFu;
    uint8 *ram = (uint8 *)malloc(n);
    if (!ram)
        return 0;
    void *probe = malloc(margin);
    if (!probe) {
        free(ram);
        return 0;
    }
    free(probe);
    memcpy(ram, cart.rom, n);
    for (int page = 0; page < 8; page++) {
        uintptr_t off = (uintptr_t)cpu_readmap[page] - (uintptr_t)cart.rom;
        if (off < n)
            cpu_readmap[page] = ram + off;
    }
    cart.rom = ram;
    return (int)n;
}

/* The controller in its current mode: joystick (directions, left fire) or
   keypad (the key's code, right fire); low active */
static uint8 coleco_pad(int p) {
    static const uint8 code[13] = {
        0x0F, 0x0D, 0x07, 0x0C, 0x02, 0x03, 0x0E, 0x05, 0x01, 0x0B, 0x0A, 0x09, 0x06,
    };
    uint8 v;
    if (coleco_keypad_mode) {
        v = 0xF0 | code[coleco_key[p] <= 12 ? coleco_key[p] : 0];
        if (input.pad[p] & INPUT_BUTTON1) v &= ~0x40;
    } else {
        v = 0xFF;
        if (input.pad[p] & INPUT_UP) v &= ~0x01;
        if (input.pad[p] & INPUT_RIGHT) v &= ~0x02;
        if (input.pad[p] & INPUT_DOWN) v &= ~0x04;
        if (input.pad[p] & INPUT_LEFT) v &= ~0x08;
        if (input.pad[p] & INPUT_BUTTON2) v &= ~0x40;
    }
    return v;
}

static void coleco_writeport(int port, int data) {
    switch (port) {
        case 0x50: ay_write_addr(&coleco_ay, data); return;
        case 0x51:
            sms_sound_sync();
            ay_write_data(&coleco_ay, data);
            coleco_ay_on = 1;
            return;
        case 0x53:
        case 0x7F:
            if (!coleco_sgm_ram) return;
            if (port == 0x53) sgm_upper = data & 1;
            else sgm_lower = !(data & 2);
            coleco_memory_map();
            return;
    }
    switch (port & 0xE0) {
        case 0x80: coleco_keypad_mode = 1; break;
        case 0xC0: coleco_keypad_mode = 0; break;
        case 0xA0:
            if (port & 1) vdp_ctrl_w(data);
            else vdp_data_w(data);
            break;
        case 0xE0:
            sms_sound_sync();
            if (snd.enabled) SN76496Write(0, data);
            break;
    }
}

static int coleco_readport(int port) {
    if (port == 0x52) return ay_read_data(&coleco_ay, NULL);
    switch (port & 0xE0) {
        case 0xA0: return (port & 1) ? vdp_ctrl_r() : vdp_data_r();
        case 0xE0: return coleco_pad((port >> 1) & 1);
    }
    return 0xFF;
}

/* Pad bits shared by the SMS port $DC and the SG-1000 even $C0-$FF ports */
static uint8 read_port_dc(void) {
    uint8 temp = 0xFF;
    if (input.pad[0] & INPUT_UP) temp &= ~0x01;
    if (input.pad[0] & INPUT_DOWN) temp &= ~0x02;
    if (input.pad[0] & INPUT_LEFT) temp &= ~0x04;
    if (input.pad[0] & INPUT_RIGHT) temp &= ~0x08;
    if (input.pad[0] & INPUT_BUTTON2) temp &= ~0x10;
    if (input.pad[0] & INPUT_BUTTON1) temp &= ~0x20;
    if (input.pad[1] & INPUT_UP) temp &= ~0x40;
    if (input.pad[1] & INPUT_DOWN) temp &= ~0x80;
    return temp;
}

/* SG-1000 I/O is decoded on A7/A6 only */
static void sg_writeport(int port, int data) {
    switch (port & 0xC0) {
        case 0x40: /* SN76489 PSG */
            if (snd.log) {
                snd.callback(0x03);
                snd.callback(data);
            }
            sms_sound_sync();
            if (snd.enabled) SN76496Write(0, data);
            break;

        case 0x80: /* TMS9918A: odd = control, even = data */
            if (port & 1)
                vdp_ctrl_w(data);
            else
                vdp_data_w(data);
            break;
    }
}

static int sg_readport(int port) {
    uint8 temp;

    switch (port & 0xC0) {
        case 0x80: /* TMS9918A: odd = status, even = data */
            return (port & 1) ? vdp_ctrl_r() : vdp_data_r();

        case 0xC0: /* Joypads: even = $DC layout, odd = $DD layout */
            if (!(port & 1)) return read_port_dc();
            temp = 0xFF;
            if (input.pad[1] & INPUT_LEFT) temp &= ~0x01;
            if (input.pad[1] & INPUT_RIGHT) temp &= ~0x02;
            if (input.pad[1] & INPUT_BUTTON2) temp &= ~0x04;
            if (input.pad[1] & INPUT_BUTTON1) temp &= ~0x08;
            return temp;
    }
    return 0xFF;
}


/* Write to memory */
void in_ram(cpu_writemem16)(int address, int data) {
    if (IS_COLECO) {
        coleco_writemem(address, data);
        return;
    }
    if (IS_SG) {
        sg_writemem(address, data);
        return;
    }
    /* Writes to ROM space are mapped to sms.dummy, a sentinel rather than a
       real 8K page: discard them instead of indexing up to 8K past its end.
       The Codemasters bank registers at $0000, $4000 and $8000 are in ROM space,
       so they are looked for only there. They select the same slots as the Sega
       registers at $FFFD-$FFFF, which Codemasters cartridges do not have. */
    uint8 *page = cpu_writemap[address >> 13];
    if (page != sms.dummy)
        page[address & 0x1FFF] = data;
    else if (cart.mapper == MAPPER_CODIES && !(address & 0x3FFF))
        sms_mapper_w(1 + (address >> 14), data);
    if (address >= 0xFFFC && cart.mapper == MAPPER_SEGA) sms_mapper_w(address & 3, data);
}


/* Write to an I/O port. Apart from the Game Gear ports at $00-$06 and the
   YM2413 at $F0-$F2, the console decodes only A7, A6 and A0, so every port
   repeats through its quarter of the range: the MSX conversions reach the
   video chip at $98/$99, where an MSX has it, instead of at $BE/$BF. */
void in_ram(cpu_writeport)(int port, int data) {
    if (IS_COLECO) {
        coleco_writeport(port & 0xFF, data);
        return;
    }
    if (IS_SG) {
        sg_writeport(port, data);
        return;
    }
    port &= 0xFF;

    switch (port) {
        case 0x00:
        case 0x01: /* GG SIO */
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
            return;

        case 0x06: /* GG STEREO */
            if (snd.log) {
                snd.callback(0x04);
                snd.callback(data);
            }
            sms_sound_sync();
            sms.psg_mask = (data & 0xFF);
            return;

        case 0xF0: /* YM2413: no FM unit here (PBC), writes are ignored */
        case 0xF1:
            if (snd.log) {
                snd.callback((port & 1) ? 0x06 : 0x05);
                snd.callback(data);
            }
            return;

        case 0xF2: /* YM2413 DETECT: no FM unit */
            return;
    }

    switch (port & 0xC1) {
        case 0x01: /* TERRITORY CTRL. ($3F) */
            sms.port_3F = ((data & 0x80) | (data & 0x20) << 1) & 0xC0;
            if (sms.country == TYPE_DOMESTIC) sms.port_3F ^= 0xC0;
            break;

        case 0x40: /* SN76489 PSG ($7E/$7F) */
        case 0x41:
            if (snd.log) {
                snd.callback(0x03);
                snd.callback(data);
            }
            sms_sound_sync();
            if (snd.enabled) SN76496Write(0, data);
            break;

        case 0x80: /* VDP DATA ($BE) */
            vdp_data_w(data);
            break;

        case 0x81: /* VDP CTRL ($BF) */
            vdp_ctrl_w(data);
            break;
    }
}


/* Read from an I/O port, decoded like cpu_writeport() */
int in_ram(cpu_readport)(int port) {
    uint8 temp = 0xFF;

    if (IS_COLECO) return coleco_readport(port & 0xFF);
    if (IS_SG) return sg_readport(port);
    port &= 0xFF;

    switch (port) {
        case 0x00: /* INPUT #2 */
            temp = 0xFF;
            if (input.system & INPUT_START) temp &= ~0x80;
            if (sms.country == TYPE_DOMESTIC) temp &= ~0x40;
            return (temp);

        case 0x01: /* GG SIO */
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
            return (0x00);

        case 0xF2: /* YM2413 DETECT: no FM unit, as with use_fm 0 before */
            break;
    }

    switch (port & 0xC1) {
        case 0x40: /* V COUNTER ($7E) */
            return (vdp_vcounter_r());

        case 0x41: /* H COUNTER ($7F) */
            return (vdp_hcounter_r());

        case 0x80: /* VDP DATA ($BE) */
            return (vdp_data_r());

        case 0x81: /* VDP CTRL ($BF) */
            return (vdp_ctrl_r());

        case 0xC0: /* INPUT #0 ($DC) */
            return (read_port_dc());

        case 0xC1: /* INPUT #1 ($DD) */
            temp = 0xFF;
            if (input.pad[1] & INPUT_LEFT) temp &= ~0x01;
            if (input.pad[1] & INPUT_RIGHT) temp &= ~0x02;
            if (input.pad[1] & INPUT_BUTTON2) temp &= ~0x04;
            if (input.pad[1] & INPUT_BUTTON1) temp &= ~0x08;
            if (input.system & INPUT_SOFT_RESET) temp &= ~0x10;
            return ((temp & 0x3F) | (sms.port_3F & 0xC0));
    }
    return (0xFF);
}


void in_ram(sms_mapper_w)(int address, int data) {
    /* Calculate ROM page index */
    uint8 page = (data % cart.pages);

    /* Save frame control register data */
    sms.fcr[address] = data;

    switch (address) {
        case 0:
            if (data & 8) {
                sms.save |= (data & 4) ? 3 : 1;     /* bit 1: the upper 16 KB (PBC) */
                /* Page in ROM */
                cpu_readmap[4] = &sms.sram[(data & 4) ? 0x4000 : 0x0000];
                cpu_readmap[5] = &sms.sram[(data & 4) ? 0x6000 : 0x2000];
                cpu_writemap[4] = &sms.sram[(data & 4) ? 0x4000 : 0x0000];
                cpu_writemap[5] = &sms.sram[(data & 4) ? 0x6000 : 0x2000];
            } else {
                /* Page in RAM */
                cpu_readmap[4] = &cart.rom[((sms.fcr[3] % cart.pages) << 14) + 0x0000];
                cpu_readmap[5] = &cart.rom[((sms.fcr[3] % cart.pages) << 14) + 0x2000];
                cpu_writemap[4] = sms.dummy;
                cpu_writemap[5] = sms.dummy;
            }
            break;

        case 1:
            cpu_readmap[0] = &cart.rom[(page << 14) + 0x0000];
            cpu_readmap[1] = &cart.rom[(page << 14) + 0x2000];
            break;

        case 2:
            cpu_readmap[2] = &cart.rom[(page << 14) + 0x0000];
            cpu_readmap[3] = &cart.rom[(page << 14) + 0x2000];
            /* Codemasters: bit 7 maps 8 KB of cartridge RAM over $A000-$BFFF
               (Ernie Els Golf) */
            if (cart.mapper == MAPPER_CODIES) {
                if (data & 0x80) {
                    sms.save |= 1;
                    cpu_readmap[5] = sms.sram;
                    cpu_writemap[5] = sms.sram;
                } else {
                    cpu_readmap[5] = &cart.rom[((sms.fcr[3] % cart.pages) << 14) + 0x2000];
                    cpu_writemap[5] = sms.dummy;
                }
            }
            break;

        case 3:
            if (!(sms.fcr[0] & 0x08)) {
                cpu_readmap[4] = &cart.rom[(page << 14) + 0x0000];
                /* Codemasters: $A000-$BFFF stays cartridge RAM while it is mapped */
                if (!(cart.mapper == MAPPER_CODIES && (sms.fcr[2] & 0x80)))
                    cpu_readmap[5] = &cart.rom[(page << 14) + 0x2000];
            }
            break;
    }
}


int sms_irq_callback(int param) {
    return (0xFF);
}

