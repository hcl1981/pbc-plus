/*
 * Optimized VDP line rendering for ARM Cortex-M33
 * Renders entire tile rows in assembly for maximum performance
 *
 * PBC (November 2026): the eight pixels of a tile row are made as two
 * words with the DSP instructions instead of one byte at a time:
 *   - the pattern word (VRAM bytes b0..b3, PIX0 = b0 bits 7:4, PIX1 = b0
 *     bits 3:0, ...) is spread into PIX0..PIX3 and PIX4..PIX7, one pixel
 *     per byte (and/orr with shifts, pkhbt/pkhtb); horizontal flip
 *     reverses them (rev, words swapped)
 *   - USUB8 against 0x01010101 sets the GE flag of every byte whose pixel
 *     is not transparent, SEL then takes pixel|attributes there and the
 *     background (plane B) or what is already in the line (plane A,
 *     window) elsewhere
 *   - plane A of low priority must not cover a plane B pixel of high
 *     priority: UQSUB8 first takes 0x10 off its pixels there, which
 *     leaves 0 (transparent)
 * The bytes in the line come out as before; the stores are words, which
 * may be unaligned (fine on the M33).
 *
 * Each routine is a macro, made twice: as before, and for interlace mode 2
 * (*_im2_asm: tiles 8 x 16, 64 bytes, name bits 9:0, row 0..15).
 */

    .syntax unified
    .cpu cortex-m33
    .thumb

    .section .time_critical.vdp_line_opt, "ax", %progbits

/* External symbols */
    .extern VRAM

/* Constants */
    .equ PIXATTR_HIPRI, 0x80

/*
 * Attribute byte of a tile, by name bits 15:13 (priority, palette):
 * palette << 4 | priority << 7, in all four bytes
 */
    .balign 4
attr_word:
    .word 0x00000000, 0x10101010, 0x20202020, 0x30303030
    .word 0x80808080, 0x90909090, 0xA0A0A0A0, 0xB0B0B0B0

/*
 * TILE_PIXELS: pattern in r3 -> PIX0..PIX3 in r11, PIX4..PIX7 in r3 (one
 * per byte), horizontally flipped if the flags say NE. Uses lr.
 */
    .macro TILE_PIXELS
    and     lr, r3, #0x000F000F     @ PIX1, PIX5 (bits 3:0, 19:16)
    and     r11, r3, #0x00F000F0    @ PIX0, PIX4 (bits 7:4, 23:20)
    orr     lr, r11, lr, lsl #12
    lsr     lr, lr, #4              @ a = PIX0 PIX1 PIX4 PIX5
    and     r11, r3, #0xF000F000    @ PIX2, PIX6
    and     r3, r3, #0x0F000F00     @ PIX3, PIX7 (already in place)
    orr     r3, r3, r11, lsr #12    @ b = PIX2 PIX3 PIX6 PIX7
    pkhbt   r11, lr, r3, lsl #16    @ PIX0 PIX1 PIX2 PIX3
    pkhtb   r3, r3, lr, asr #16     @ PIX4 PIX5 PIX6 PIX7
    ittt    ne                      @ horizontal flip:
    revne   lr, r3                  @   PIX7 PIX6 PIX5 PIX4
    revne   r3, r11                 @   PIX3 PIX2 PIX1 PIX0
    movne   r11, lr
    .endm

/*
 * draw_line_b_simple_asm - Simplified Plane B renderer
 *
 * Optimized for the common case: no per-column scrolling, constant vertical scroll.
 *
 * C signature:
 *   void draw_line_b_simple_asm(uint8_t* scr, uint8_t* vram_nt, uint32_t col, uint32_t paty,
 *                               uint32_t ntw_mask, uint32_t background, uint32_t tile_count);
 *
 * Parameters (ARM calling convention):
 *   r0 = scr pointer (render_buffer + PIX_OVERFLOW - patx)
 *   r1 = VRAM pointer + nametable row offset (for fetching tile names)
 *   r2 = initial column (already masked)
 *   r3 = paty (0-7)
 *
 * Stack (after push, sp+36 is first stack arg):
 *   [sp+36] = ntw_mask
 *   [sp+40] = background color
 *   [sp+44] = tile count
 *
 * Registers in the loop:
 *   r0 = VRAM, r1 = name table row, r2 = col, r4 = ntw_mask,
 *   r5 = background in all four bytes, r6 = tiles left, r7 = scr,
 *   r8 = paty * 4, r9 = (7 - paty) * 4, r10 = 0x01010101,
 *   r3, r11, r12, lr = scratch
 */
    .macro LINE_B name, bits, shift, lastrow
    .global \name
    .thumb_func
    .type \name, %function
\name:
    push    {r4-r11, lr}

    ldr     r4, [sp, #36]           @ r4 = ntw_mask
    ldr     r5, [sp, #40]           @ r5 = background
    ldr     r6, [sp, #44]           @ r6 = tile_count

    mov     r7, r0                  @ r7 = scr
    lsl     r8, r3, #2              @ r8 = paty * 4
    rsb     r9, r8, #\lastrow            @ r9 = (last row - paty) * 4
    ldr     r0, =VRAM
    mov     r10, #0x01010101
    and     r5, r5, #0xFF
    mul     r5, r5, r10             @ r5 = bg|bg|bg|bg

.Lb_tile_loop\@:
    /* tile name, big-endian in VRAM */
    ldrh    r11, [r1, r2, lsl #1]
    rev16   r11, r11
    /* pattern row: tile * 32 + row * 4, row flipped vertically */
    ubfx    r12, r11, #0, #\bits
    tst     r11, #0x1000
    ite     eq
    addeq   r12, r8, r12, lsl #\shift
    addne   r12, r9, r12, lsl #\shift
    ldr     r3, [r0, r12]
    cbz     r3, .Lb_background\@      @ all transparent

    ldr     r12, =attr_word
    lsr     lr, r11, #13
    ldr     r12, [r12, lr, lsl #2]  @ r12 = attributes, four times
    tst     r11, #0x0800            @ horizontal flip (flags survive TILE_PIXELS)
    TILE_PIXELS

    usub8   lr, r11, r10            @ GE: pixel not transparent
    orr     r11, r11, r12
    sel     r11, r11, r5            @ pixel | attributes, else background
    usub8   lr, r3, r10
    orr     r3, r3, r12
    sel     r3, r3, r5
    str     r11, [r7, #0]
    str     r3, [r7, #4]

.Lb_next_tile\@:
    add     r7, r7, #8              @ scr += 8
    add     r2, r2, #1              @ col++
    and     r2, r2, r4              @ col &= ntw_mask
    subs    r6, r6, #1              @ tile_count--
    bne     .Lb_tile_loop\@

    pop     {r4-r11, pc}

.Lb_background\@:
    str     r5, [r7, #0]
    str     r5, [r7, #4]
    b       .Lb_next_tile\@

    .ltorg
    .size \name, .-\name
    .endm
    LINE_B draw_line_b_simple_asm, 11, 5, 28
    LINE_B draw_line_b_im2_asm, 10, 6, 60          @ interlace mode 2: tiles 8 x 16


/*
 * draw_line_a_simple_asm - Simplified Plane A renderer
 *
 * Plane A is drawn OVER Plane B, only non-transparent pixels are written;
 * a low-priority tile leaves high-priority plane B pixels alone.
 * Optimized for the common case: no per-column scrolling, constant vertical scroll.
 *
 * C signature:
 *   void draw_line_a_simple_asm(uint8_t* scr, uint8_t* vram_nt, uint32_t col, uint32_t paty,
 *                               uint32_t ntw_mask, uint32_t tile_count);
 *
 * Parameters (ARM calling convention):
 *   r0 = scr pointer
 *   r1 = VRAM + nametable row offset
 *   r2 = initial column (already masked)
 *   r3 = paty (0-7)
 *
 * Stack (after push, sp+36):
 *   [sp+36] = ntw_mask
 *   [sp+40] = tile count
 *
 * Registers as in draw_line_b_simple_asm, r5 = scratch.
 */
    .macro LINE_A name, bits, shift, lastrow
    .global \name
    .thumb_func
    .type \name, %function
\name:
    push    {r4-r11, lr}

    ldr     r4, [sp, #36]           @ r4 = ntw_mask
    ldr     r6, [sp, #40]           @ r6 = tile_count

    mov     r7, r0                  @ r7 = scr
    lsl     r8, r3, #2              @ r8 = paty * 4
    rsb     r9, r8, #\lastrow            @ r9 = (last row - paty) * 4
    ldr     r0, =VRAM
    mov     r10, #0x01010101

.La_tile_loop\@:
    ldrh    r11, [r1, r2, lsl #1]
    rev16   r11, r11
    ubfx    r12, r11, #0, #\bits
    tst     r11, #0x1000
    ite     eq
    addeq   r12, r8, r12, lsl #\shift
    addne   r12, r9, r12, lsl #\shift
    ldr     r3, [r0, r12]
    cmp     r3, #0
    beq     .La_next_tile\@           @ all transparent: nothing to draw

    ldr     r12, =attr_word
    lsr     lr, r11, #13
    ldr     r12, [r12, lr, lsl #2]
    tst     r11, #0x0800
    TILE_PIXELS

    tst     r12, #PIXATTR_HIPRI
    bne     .La_hipri\@

    /* low priority: not over a high-priority pixel of plane B */
    ldr     lr, [r7, #0]
    and     r5, lr, #0x80808080
    lsr     r5, r5, #3              @ 0x10 under a priority pixel
    uqsub8  r5, r11, r5             @ ... which makes this pixel 0
    usub8   r5, r5, r10             @ GE: draw
    orr     r11, r11, r12
    sel     r11, r11, lr
    str     r11, [r7, #0]
    ldr     lr, [r7, #4]
    and     r5, lr, #0x80808080
    lsr     r5, r5, #3
    uqsub8  r5, r3, r5
    usub8   r5, r5, r10
    orr     r3, r3, r12
    sel     r3, r3, lr
    str     r3, [r7, #4]
    b       .La_next_tile\@

.La_hipri\@:
    /* high priority: every non-transparent pixel */
    ldr     lr, [r7, #0]
    usub8   r5, r11, r10
    orr     r11, r11, r12
    sel     r11, r11, lr
    str     r11, [r7, #0]
    ldr     lr, [r7, #4]
    usub8   r5, r3, r10
    orr     r3, r3, r12
    sel     r3, r3, lr
    str     r3, [r7, #4]

.La_next_tile\@:
    add     r7, r7, #8              @ scr += 8
    add     r2, r2, #1              @ col++
    and     r2, r2, r4              @ col &= ntw_mask
    subs    r6, r6, #1              @ tile_count--
    bne     .La_tile_loop\@

    pop     {r4-r11, pc}

    .ltorg
    .size \name, .-\name
    .endm
    LINE_A draw_line_a_simple_asm, 11, 5, 28
    LINE_A draw_line_a_im2_asm, 10, 6, 60          @ interlace mode 2: tiles 8 x 16


/*
 * draw_window_line_asm - Window plane renderer
 *
 * Window plane has no scrolling - tiles are rendered sequentially.
 * Every non-transparent pixel is written, whatever is below.
 *
 * C signature:
 *   void draw_window_line_asm(uint8_t* scr, uint8_t* vram_nt, uint32_t paty, uint32_t tile_count);
 *
 * Parameters:
 *   r0 = scr pointer
 *   r1 = VRAM + window nametable offset
 *   r2 = paty (0-7)
 *   r3 = tile count
 *
 * Registers: r0 = VRAM, r1 = name (advances by 2), r6 = tiles left,
 * r7 = scr, r8/r9 = row offsets, r10 = 0x01010101, rest scratch.
 */
    .macro LINE_W name, bits, shift, lastrow
    .global \name
    .thumb_func
    .type \name, %function
\name:
    push    {r4-r11, lr}

    mov     r7, r0                  @ r7 = scr
    mov     r6, r3                  @ r6 = tile_count
    lsl     r8, r2, #2              @ r8 = paty * 4
    rsb     r9, r8, #\lastrow            @ r9 = (last row - paty) * 4
    ldr     r0, =VRAM
    mov     r10, #0x01010101

.Lw_tile_loop\@:
    ldrh    r11, [r1], #2
    rev16   r11, r11
    ubfx    r12, r11, #0, #\bits
    tst     r11, #0x1000
    ite     eq
    addeq   r12, r8, r12, lsl #\shift
    addne   r12, r9, r12, lsl #\shift
    ldr     r3, [r0, r12]
    cbz     r3, .Lw_next_tile\@

    ldr     r12, =attr_word
    lsr     lr, r11, #13
    ldr     r12, [r12, lr, lsl #2]
    tst     r11, #0x0800
    TILE_PIXELS

    ldr     lr, [r7, #0]
    usub8   r5, r11, r10
    orr     r11, r11, r12
    sel     r11, r11, lr
    str     r11, [r7, #0]
    ldr     lr, [r7, #4]
    usub8   r5, r3, r10
    orr     r3, r3, r12
    sel     r3, r3, lr
    str     r3, [r7, #4]

.Lw_next_tile\@:
    add     r7, r7, #8              @ scr += 8
    subs    r6, r6, #1              @ tile_count--
    bne     .Lw_tile_loop\@

    pop     {r4-r11, pc}

    .ltorg
    .size \name, .-\name
    .endm
    LINE_W draw_window_line_asm, 11, 5, 28
    LINE_W draw_window_line_im2_asm, 10, 6, 60          @ interlace mode 2: tiles 8 x 16
