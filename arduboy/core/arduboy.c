/* arduboy.c -- see arduboy.h */
#include <string.h>
#include "arduboy.h"

#ifndef ARD_RAM                         /* the platform puts hot code in RAM */
#define ARD_RAM
#endif
#ifndef ARD_EXEC                        /* where exec() goes: as the rest, or not */
#define ARD_EXEC ARD_RAM
#endif

/* ---- the machine ------------------------------------------------------------- */

static uint8_t flash[0x8000];           /* 32 KB program */
typedef struct { uint8_t op, d; uint16_t x; } insn_t;
/* the program, decoded once: each instruction the address of its code in
 * exec() and op | d << 8 | x << 16; code[0x4000] wraps the PC to 0 */
typedef struct { const void *h; uint32_t w; } slot_t;
static slot_t code[0x4002] __attribute__((aligned(8)));   /* [0x4001]: read ahead past WRAP */
static const void *const *exec_ops;     /* exec()'s code for each op */
#ifdef ARD_PROF
unsigned long long ard_prof_op[256], ard_prof_pair[256][256];
unsigned ard_prof_last;
#endif
/* the data space, index = address: the registers 0x00..0x1F, the I/O
 * registers 0x20..0xFF, the SRAM 0x100..0xAFF */
static uint8_t mem[0xB00] __attribute__((aligned(4)));
#define R  mem
#define io mem
static uint8_t eeprom[ARD_EEPROM];
static bool eeprom_dirty;
static uint32_t eempe_at;               /* cycle EEMPE was set */
static uint16_t pc, sp;
static uint8_t sreg;
static uint32_t cyc;                    /* CPU cycles, wrapping */
static bool sleeping;
static uint8_t irq_hold;                /* instructions to run before an interrupt (after SEI, RETI) */
static bool irq_pending;                /* some enabled interrupt flag is set */
static uint32_t next_event;             /* the first cycle a timer has something to do */
static uint32_t run_limit;              /* the inner loop runs to here: lowered by anything that
                                           needs the outer loop (an interrupt, SLEEP, an event) */
static uint8_t buttons;
static uint8_t pin_keep[13] = { 0xFF, 0, 0, 0xFF, 0, 0, 0xFF, 0, 0, 0xFF, 0, 0, 0xFF };
                                        /* PINB..PINF (every third): PORTx & this, the buttons low */
#define PIN_PORT(a) ((unsigned)(a) - PINB <= 12u && (0x1249u >> ((unsigned)(a) - PINB) & 1))
static uint32_t idle_cycles;
static uint8_t adc_noise = 0x5a;

enum { FC = 0x01, FZ = 0x02, FN = 0x04, FV = 0x08, FS = 0x10, FH = 0x20, FT = 0x40, FI = 0x80 };

/* registers by data address (avr-libc iom32u4.h) */
enum {
    PINB = 0x23, DDRB, PORTB, PINC, DDRC, PORTC, PIND, DDRD, PORTD, PINE, DDRE, PORTE,
    PINF, DDRF, PORTF,
    TIFR0 = 0x35, TIFR1, TIFR3 = 0x38, TIFR4, EECR = 0x3F, EEDR, EEARL, EEARH,
    TCCR0A = 0x44, TCCR0B, TCNT0, OCR0A, OCR0B, PLLCSR, SPCR = 0x4C, SPSR, SPDR,
    PLLFRQ = 0x52, SMCR, MCUSR, MCUCR, SPL = 0x5D, SPH, SREG,
    WDTCSR = 0x60, CLKPR, TIMSK0 = 0x6E, TIMSK1, TIMSK3 = 0x71, TIMSK4,
    ADCL = 0x78, ADCH, ADCSRA, ADCSRB, ADMUX,
    TCCR1A = 0x80, TCCR1B, TCCR1C, TCNT1L = 0x84, TCNT1H, ICR1L, ICR1H, OCR1AL, OCR1AH,
    OCR1BL, OCR1BH, OCR1CL, OCR1CH,
    TCCR3A = 0x90, TCCR3B, TCCR3C, TCNT3L = 0x94, TCNT3H, ICR3L, ICR3H, OCR3AL, OCR3AH,
    OCR3BL, OCR3BH, OCR3CL, OCR3CH,
    TCNT4 = 0xBE, TC4H, TCCR4A, TCCR4B, TCCR4C, TCCR4D, TCCR4E,
    OCR4A = 0xCF, OCR4B, OCR4C, OCR4D,
};

/* ---- sound: the speaker between PC6 and PC7 ----------------------------------------
 *
 * Each pin drives 0..256 (a PWM output its duty), the speaker gets their
 * difference. Changes are kept with their cycle and turned into samples
 * (box filter) at the end of ard_run. */
#define SND_MAX 4096
static uint32_t snd_t[SND_MAX];
static int16_t snd_v[SND_MAX];
static int snd_n;
static int16_t snd_level;               /* -256..256 */

static int16_t speaker_level(void);

static ARD_RAM void snd_update(void)
{
    int16_t v = speaker_level();
    if (v == snd_level)
        return;
    snd_level = v;
    if (snd_n < SND_MAX) {
        snd_t[snd_n] = cyc;
        snd_v[snd_n++] = v;
    }
}

/* ---- timers 0, 1, 3 ---------------------------------------------------------------- */

enum { K_NORMAL, K_CTC, K_FAST, K_PHASE };

typedef struct {
    uint8_t a, b, tifr, timsk;          /* TCCRnA, TCCRnB, TIFRn, TIMSKn addresses */
    bool wide;                          /* 16 bit */
    uint16_t tcnt, ocr[3], icr;
    bool down;                          /* phase correct, counting down */
    uint32_t last;                      /* cycle up to which it has counted */
    uint8_t temp;                       /* the 16-bit registers' TEMP */
    bool oc_a;                          /* output compare A, toggled (non-PWM) */
} tmr_t;

static tmr_t t0 = { TCCR0A, TCCR0B, TIFR0, TIMSK0, false };
static tmr_t t1 = { TCCR1A, TCCR1B, TIFR1, TIMSK1, true };
static tmr_t t3 = { TCCR3A, TCCR3B, TIFR3, TIMSK3, true };

static const uint16_t presc_tab[8] = { 0, 1, 8, 64, 256, 1024, 0, 0 };

static int tmr_mode(const tmr_t *t, uint16_t *top)
{
    if (!t->wide) {
        int w = (io[t->a] & 3) | (io[t->b] >> 1 & 4);
        switch (w) {
        case 1: *top = 0xFF; return K_PHASE;
        case 2: *top = t->ocr[0]; return K_CTC;
        case 3: *top = 0xFF; return K_FAST;
        case 5: *top = t->ocr[0]; return K_PHASE;
        case 7: *top = t->ocr[0]; return K_FAST;
        default: *top = 0xFF; return K_NORMAL;
        }
    }
    int w = (io[t->a] & 3) | (io[t->b] >> 1 & 0x0C);
    switch (w) {
    case 1: *top = 0xFF; return K_PHASE;
    case 2: *top = 0x1FF; return K_PHASE;
    case 3: *top = 0x3FF; return K_PHASE;
    case 4: *top = t->ocr[0]; return K_CTC;
    case 5: *top = 0xFF; return K_FAST;
    case 6: *top = 0x1FF; return K_FAST;
    case 7: *top = 0x3FF; return K_FAST;
    case 8: case 10: *top = t->icr; return K_PHASE;
    case 9: case 11: *top = t->ocr[0]; return K_PHASE;
    case 12: *top = t->icr; return K_CTC;
    case 14: *top = t->icr; return K_FAST;
    case 15: *top = t->ocr[0]; return K_FAST;
    default: *top = 0xFFFF; return K_NORMAL;
    }
}

static inline uint32_t tmr_presc(const tmr_t *t)
{
    return presc_tab[io[t->b] & 7];
}

/* compare match on channel ch: the flag, and channel A's pin if it toggles */
static ARD_RAM void tmr_match(tmr_t *t, int ch, int kind)
{
    io[t->tifr] |= (uint8_t)(2 << ch);
    if (ch == 0 && kind != K_FAST && kind != K_PHASE && ((io[t->a] >> 6) & 3) == 1) {
        t->oc_a = !t->oc_a;                 /* COMnA = 01: toggle */
        if (t == &t3)
            snd_update();
    }
}

/* count `ticks` timer clocks; `at` is the cycle of the first one */
static ARD_RAM void tmr_count(tmr_t *t, uint32_t ticks, uint32_t presc)
{
    uint16_t top;
    int kind = tmr_mode(t, &top);
    uint32_t max = t->wide ? 0xFFFF : 0xFF;
    int nch = t->wide ? 3 : 2;
    uint32_t period = kind == K_PHASE ? 2u * top : (kind == K_NORMAL ? max : top) + 1u;
    if (period == 0)
        period = 1;
    while (ticks) {
        uint32_t c = t->tcnt;
        if (kind == K_PHASE) {
            /* up to top, down to 0: the next turn or match */
            uint32_t dist = t->down ? c : (top > c ? top - c : 0);
            if (dist == 0) {
                t->down = !t->down;
                if (!t->down)
                    io[t->tifr] |= 1;       /* TOV at BOTTOM */
                if (t->down && top == 0)
                    t->down = false;
                dist = 1;
            }
            uint32_t step = ticks < dist ? ticks : dist;
            for (int ch = 0; ch < nch; ch++) {
                uint32_t o = t->ocr[ch];
                if (!t->down ? (o > c && o <= c + step) : (o < c && o + step >= c))
                    tmr_match(t, ch, kind);
            }
            t->tcnt = (uint16_t)(t->down ? c - step : c + step);
            ticks -= step;
            if (ticks > 2 * period)         /* nothing new in whole periods: skip */
                ticks %= period;
            continue;
        }
        uint32_t wrap = kind == K_NORMAL ? max : top;     /* last value before 0 */
        if (c > wrap) {                     /* above TOP (written so): runs to MAX */
            wrap = max;
        }
        uint32_t dist = wrap - c + 1;       /* ticks to reach 0 again */
        uint32_t step = ticks < dist ? ticks : dist;
        for (int ch = 0; ch < nch; ch++) {
            uint32_t o = t->ocr[ch];
            if (o > c && o <= c + step)
                tmr_match(t, ch, kind);
        }
        if (step == dist) {                 /* wrapped */
            if (kind != K_CTC || wrap == max)
                io[t->tifr] |= 1;           /* TOV */
            t->tcnt = 0;
            /* a match on 0 happens when the counter gets there */
            for (int ch = 0; ch < nch; ch++)
                if (t->ocr[ch] == 0)
                    tmr_match(t, ch, kind);
        } else {
            t->tcnt = (uint16_t)(c + step);
        }
        ticks -= step;
        if (ticks > 2 * period)
            ticks %= period;
    }
    (void)presc;
}

static ARD_RAM void tmr_sync(tmr_t *t)
{
    uint32_t presc = tmr_presc(t);
    if (!presc) {
        t->last = cyc;
        return;
    }
    uint32_t el = cyc - t->last;
    if (el < presc)
        return;
    uint32_t ticks = el / presc;
    t->last += ticks * presc;
    tmr_count(t, ticks, presc);
}

/* cycles until the timer next sets a flag that matters (an enabled
 * interrupt, or channel A toggling the speaker pin); ~0 if never */
static ARD_RAM uint32_t tmr_next(tmr_t *t)
{
    uint32_t presc = tmr_presc(t);
    if (!presc)
        return ~0u;
    uint8_t want = io[t->timsk] & (t->wide ? 0x0F : 0x07);
    bool toggles = t == &t3 && ((io[t->a] >> 6) & 3) == 1;
    if (toggles)
        want |= 2;
    if (!want)
        return ~0u;
    uint16_t top;
    int kind = tmr_mode(t, &top);
    uint32_t max = t->wide ? 0xFFFF : 0xFF;
    uint32_t c = t->tcnt, best = ~0u;
    if (kind == K_PHASE) {
        /* ticks to the next of: BOTTOM (TOV), each OCR */
        uint32_t up = t->down ? c + top : top - c;          /* to the next TOP */
        uint32_t to_bottom = t->down ? c : (top - c) + top;
        if (want & 1) best = to_bottom ? to_bottom : 2u * top;
        for (int ch = 0; ch < (t->wide ? 3 : 2); ch++) {
            if (!(want & (2 << ch)))
                continue;
            uint32_t o = t->ocr[ch], d;
            if (!t->down)
                d = o > c ? o - c : (top - c) + (top - o);
            else
                d = o < c ? c - o : c + o;
            if (d == 0) d = 2u * top;
            if (d < best) best = d;
        }
        (void)up;
    } else {
        uint32_t wrap = kind == K_NORMAL ? max : top;
        if (c > wrap) wrap = max;
        uint32_t period = wrap + 1;
        if (want & 1) {
            uint32_t d = wrap - c + 1;
            if (kind != K_CTC || wrap == max)
                best = d;
        }
        for (int ch = 0; ch < (t->wide ? 3 : 2); ch++) {
            if (!(want & (2 << ch)))
                continue;
            uint32_t o = t->ocr[ch], d;
            if (o > c) d = o - c;
            else if (o <= wrap) d = (wrap - c + 1) + o;
            else continue;
            if (d == 0) d = period;
            if (d < best) best = d;
        }
    }
    if (best == ~0u)
        return ~0u;
    uint32_t done = cyc - t->last;              /* cycles into the current tick */
    uint64_t c64 = (uint64_t)best * presc - done;
    return c64 > 0x7FFFFFFF ? 0x7FFFFFFF : (uint32_t)c64;
}

/* ---- timer 4 (10 bit, maybe clocked from the PLL) -------------------------------- */

static struct {
    uint16_t tcnt, ocr_a, ocr_b, ocr_c, ocr_d;
    bool down;
    uint32_t last, resid;               /* resid: in 1/(2*presc) timer clocks */
    bool oc_a;
} t4;

/* timer clocks per 2 CPU cycles: 2 = system clock, 6/8/12 from the PLL */
static uint32_t t4_rate(void)
{
    static const uint8_t r[4] = { 2, 12, 8, 6 };
    return r[(io[PLLFRQ] >> 4) & 3];
}

static uint32_t t4_presc(void)
{
    int cs = io[TCCR4B] & 0x0F;
    return cs ? 1u << (cs - 1) : 0;
}

static bool t4_pwm_a(void) { return io[TCCR4A] & 0x02; }

static ARD_RAM void t4_count(uint32_t ticks)
{
    uint32_t top = t4.ocr_c ? t4.ocr_c : 1;
    bool phase = (io[TCCR4D] & 3) == 1;
    uint32_t period = phase ? 2 * top : top + 1;
    while (ticks) {
        uint32_t c = t4.tcnt;
        if (phase) {
            uint32_t dist = t4.down ? c : (top > c ? top - c : 0);
            if (dist == 0) {
                t4.down = !t4.down;
                if (!t4.down)
                    io[TIFR4] |= 0x04;
                dist = 1;
            }
            uint32_t step = ticks < dist ? ticks : dist;
            uint16_t o[3] = { t4.ocr_a, t4.ocr_b, t4.ocr_d };
            static const uint8_t fl[3] = { 0x40, 0x20, 0x80 };
            for (int ch = 0; ch < 3; ch++)
                if (!t4.down ? (o[ch] > c && o[ch] <= c + step) : (o[ch] < c && o[ch] + step >= c))
                    io[TIFR4] |= fl[ch];
            t4.tcnt = (uint16_t)(t4.down ? c - step : c + step);
            ticks -= step;
        } else {
            if (c > top) top = 0x3FF;
            uint32_t dist = top - c + 1;
            uint32_t step = ticks < dist ? ticks : dist;
            uint16_t o[3] = { t4.ocr_a, t4.ocr_b, t4.ocr_d };
            static const uint8_t fl[3] = { 0x40, 0x20, 0x80 };
            for (int ch = 0; ch < 3; ch++)
                if (o[ch] > c && o[ch] <= c + step) {
                    io[TIFR4] |= fl[ch];
                    if (ch == 0 && !t4_pwm_a() && ((io[TCCR4A] >> 6) & 3) == 1) {
                        t4.oc_a = !t4.oc_a;
                        snd_update();
                    }
                }
            if (step == dist) {
                io[TIFR4] |= 0x04;
                t4.tcnt = 0;
                /* a match on 0 happens when the counter gets there */
                for (int ch = 0; ch < 3; ch++)
                    if (o[ch] == 0) {
                        io[TIFR4] |= fl[ch];
                        if (ch == 0 && !t4_pwm_a() && ((io[TCCR4A] >> 6) & 3) == 1) {
                            t4.oc_a = !t4.oc_a;
                            snd_update();
                        }
                    }
            } else {
                t4.tcnt = (uint16_t)(c + step);
            }
            ticks -= step;
            top = t4.ocr_c ? t4.ocr_c : 1;
        }
        if (ticks > 2 * period)
            ticks %= period;
    }
}

static ARD_RAM void t4_sync(void)
{
    uint32_t presc = t4_presc();
    uint32_t el = cyc - t4.last;
    t4.last = cyc;
    if (!presc)
        return;
    uint64_t units = (uint64_t)el * t4_rate() + t4.resid;
    uint32_t div = 2 * presc;
    uint64_t ticks = units / div;
    t4.resid = (uint32_t)(units % div);
    if (ticks)
        t4_count(ticks > 0xFFFFFFF ? 0xFFFFFFF : (uint32_t)ticks);
}

static ARD_RAM uint32_t t4_next(void)
{
    uint32_t presc = t4_presc();
    if (!presc)
        return ~0u;
    uint8_t want = io[TIMSK4] & 0xE4;
    if (!t4_pwm_a() && ((io[TCCR4A] >> 6) & 3) == 1)
        want |= 0x40;
    if (!want)
        return ~0u;
    uint32_t top = t4.ocr_c ? t4.ocr_c : 1, c = t4.tcnt, best = ~0u;
    bool phase = (io[TCCR4D] & 3) == 1;
    if (phase) {
        if (want & 0x04) best = t4.down ? c : (top - c) + top;
        if (!best) best = 2 * top;
        uint16_t o[3] = { t4.ocr_a, t4.ocr_b, t4.ocr_d };
        static const uint8_t fl[3] = { 0x40, 0x20, 0x80 };
        for (int ch = 0; ch < 3; ch++) {
            if (!(want & fl[ch])) continue;
            uint32_t d = !t4.down ? (o[ch] > c ? o[ch] - c : (top - c) + (top - o[ch]))
                                  : (o[ch] < c ? c - o[ch] : c + o[ch]);
            if (!d) d = 2 * top;
            if (d < best) best = d;
        }
    } else {
        if (c > top) top = 0x3FF;
        if (want & 0x04) best = top - c + 1;
        uint16_t o[3] = { t4.ocr_a, t4.ocr_b, t4.ocr_d };
        static const uint8_t fl[3] = { 0x40, 0x20, 0x80 };
        for (int ch = 0; ch < 3; ch++) {
            if (!(want & fl[ch])) continue;
            uint32_t d;
            if (o[ch] > c) d = o[ch] - c;
            else if (o[ch] <= top) d = (top - c + 1) + o[ch];
            else continue;
            if (!d) d = top + 1;
            if (d < best) best = d;
        }
    }
    if (best == ~0u)
        return ~0u;
    /* cycles until `best` more ticks: ticks = (el*rate + resid) / (2*presc) */
    uint64_t need = (uint64_t)best * 2 * presc;
    need = need > t4.resid ? need - t4.resid : 0;
    uint64_t cy = (need + t4_rate() - 1) / t4_rate();
    return cy > 0x7FFFFFFF ? 0x7FFFFFFF : (uint32_t)cy;
}

/* ---- the speaker's pins --------------------------------------------------------------- */

static int16_t pwm_duty(uint32_t ocr, uint32_t top, int com)
{
    uint32_t d = top ? (ocr >= top ? 256 : ocr * 256 / top) : 0;
    return (int16_t)(com == 3 ? 256 - d : d);
}

static int16_t pin_pc6(void)
{
    int com3 = (io[TCCR3A] >> 6) & 3;
    if (com3) {
        uint16_t top;
        int kind = tmr_mode(&t3, &top);
        if (kind == K_FAST || kind == K_PHASE)
            return pwm_duty(t3.ocr[0], top, com3);
        return t3.oc_a ? 256 : 0;
    }
    int com4 = (io[TCCR4A] >> 6) & 3;
    if (com4 == 1 && t4_pwm_a())            /* /OC4A, the complement */
        return (int16_t)(256 - pwm_duty(t4.ocr_a, t4.ocr_c, 2));
    if (io[DDRC] & 0x40)
        return (io[PORTC] & 0x40) ? 256 : 0;
    return 0;
}

static int16_t pin_pc7(void)
{
    int com4 = (io[TCCR4A] >> 6) & 3;
    if (com4) {
        if (t4_pwm_a())
            return pwm_duty(t4.ocr_a, t4.ocr_c, com4 == 3 ? 3 : 2);
        return t4.oc_a ? 256 : 0;
    }
    if (io[DDRC] & 0x80)
        return (io[PORTC] & 0x80) ? 256 : 0;
    return 0;
}

static int16_t speaker_level(void)
{
    return (int16_t)(pin_pc6() - pin_pc7());
}

/* ---- interrupts and events ------------------------------------------------------------- */

static ARD_RAM void irq_update(void)
{
    irq_pending = (io[TIFR0] & io[TIMSK0] & 7) || (io[TIFR1] & io[TIMSK1] & 0x2F) ||
                  (io[TIFR3] & io[TIMSK3] & 0x2F) || (io[TIFR4] & io[TIMSK4] & 0xE4) ||
                  ((io[SPSR] & 0x80) && (io[SPCR] & 0x80)) ||
                  ((io[ADCSRA] & 0x18) == 0x18) || (io[EECR] & 0x08);
    if (irq_pending && (sreg & FI))
        run_limit = cyc;                /* to the outer loop, which takes it */
}

static ARD_RAM void events_update(void)
{
    uint32_t n = tmr_next(&t0), m;
    if ((m = tmr_next(&t1)) < n) n = m;
    if ((m = tmr_next(&t3)) < n) n = m;
    if ((m = t4_next()) < n) n = m;
    if (n > 0x7FFFFFFF) n = 0x7FFFFFFF;
    next_event = cyc + (n ? n : 1);
    if ((int32_t)(next_event - run_limit) < 0)
        run_limit = next_event;
}

static ARD_RAM void timers_sync(void)
{
    tmr_sync(&t0);
    tmr_sync(&t1);
    tmr_sync(&t3);
    t4_sync();
}

/* ---- the SSD1306 ----------------------------------------------------------------------- */

static struct {
    uint8_t ram[8][128];
    uint8_t col, page, col_lo, col_hi, page_lo, page_hi, mode;
    uint8_t cmd, args, arg[7];
    bool on, invert, seg_remap, com_remap;
    uint8_t contrast, start;
} oled;

static void oled_reset(void)
{
    memset(&oled.col, 0, sizeof oled - offsetof(typeof(oled), col));
    oled.col_hi = 127;
    oled.page_hi = 7;
    oled.mode = 2;                      /* page addressing */
    oled.contrast = 0x7F;
}

static ARD_RAM void oled_data(uint8_t v)
{
    oled.ram[oled.page & 7][oled.col & 127] = v;
    if (oled.mode == 2) {               /* page: along the page, wrapping in it */
        oled.col = (uint8_t)((oled.col + 1) & 127);
        return;
    }
    if (oled.mode == 0) {               /* horizontal */
        if (oled.col >= oled.col_hi) {
            oled.col = oled.col_lo;
            oled.page = oled.page >= oled.page_hi ? oled.page_lo : oled.page + 1;
        } else {
            oled.col++;
        }
    } else {                            /* vertical */
        if (oled.page >= oled.page_hi) {
            oled.page = oled.page_lo;
            oled.col = oled.col >= oled.col_hi ? oled.col_lo : oled.col + 1;
        } else {
            oled.page++;
        }
    }
}

static int oled_args(uint8_t c)
{
    switch (c) {
    case 0x20: case 0x81: case 0x8D: case 0xA8: case 0xD3: case 0xD5: case 0xD9:
    case 0xDA: case 0xDB:
        return 1;
    case 0x21: case 0x22: case 0xA3:
        return 2;
    case 0x26: case 0x27:
        return 6;
    case 0x29: case 0x2A:
        return 5;
    default:
        return 0;
    }
}

static void oled_command(uint8_t c)
{
    if (oled.args) {
        oled.arg[oled_args(oled.cmd) - oled.args] = c;
        if (--oled.args)
            return;
        switch (oled.cmd) {
        case 0x20: oled.mode = oled.arg[0] & 3; break;
        case 0x21: oled.col_lo = oled.arg[0] & 127; oled.col_hi = oled.arg[1] & 127;
                   oled.col = oled.col_lo; break;
        case 0x22: oled.page_lo = oled.arg[0] & 7; oled.page_hi = oled.arg[1] & 7;
                   oled.page = oled.page_lo; break;
        case 0x81: oled.contrast = oled.arg[0]; break;
        }
        return;
    }
    int n = oled_args(c);
    if (n) {
        oled.cmd = c;
        oled.args = (uint8_t)n;
        return;
    }
    if (c < 0x10)       oled.col = (uint8_t)((oled.col & 0xF0) | c);
    else if (c < 0x20)  oled.col = (uint8_t)((oled.col & 0x0F) | (c & 7) << 4);
    else if (c >= 0x40 && c < 0x80) oled.start = c & 0x3F;
    else if (c == 0xA0 || c == 0xA1) oled.seg_remap = c & 1;
    else if (c == 0xA6 || c == 0xA7) oled.invert = c & 1;
    else if (c == 0xAE || c == 0xAF) oled.on = c & 1;
    else if (c >= 0xB0 && c < 0xB8) oled.page = c & 7;
    else if (c == 0xC0 || c == 0xC8) oled.com_remap = (c & 8) != 0;
}

/* ---- SPI and the display's pins ---------------------------------------------------------- */

static bool spsr_seen;

static ARD_RAM void spi_write(uint8_t v)
{
    io[SPDR] = v;
    if (!(io[SPCR] & 0x40))             /* SPE off */
        return;
    bool cs_low = !(io[PORTD] & 0x40), dc = io[PORTD] & 0x10;
    if (cs_low && (io[PORTD] & 0x80)) {  /* selected, not in reset */
        if (dc)
            oled_data(v);
        else
            oled_command(v);
    }
    io[SPDR] = 0;                       /* nothing comes back */
    io[SPSR] |= 0x80;                   /* SPIF: the byte went out (in 16 cycles; as good as at once) */
    spsr_seen = false;
    irq_update();
}

/* ---- the data space ------------------------------------------------------------------------- */

/* outputs read back as driven, inputs as their pull-ups; a pressed button
 * pulls its pin low (pin_keep, set by ard_buttons) */
static ARD_RAM uint8_t pins_read(int port)
{
    return io[port + 2] & pin_keep[port - PINB];
}

static ARD_RAM uint8_t io_read(uint16_t a)
{
    switch (a) {
    case PINB: case PINC: case PIND: case PINE: case PINF:
        return pins_read(a);
    case SPL: return (uint8_t)sp;
    case SPH: return (uint8_t)(sp >> 8);
    case SREG: return sreg;
    case TIFR0: case TCNT0: tmr_sync(&t0); return a == TCNT0 ? (uint8_t)t0.tcnt : io[a];
    case TIFR1: tmr_sync(&t1); return io[a];
    case TIFR3: tmr_sync(&t3); return io[a];
    case TIFR4: t4_sync(); return io[a];
    case OCR0A: return (uint8_t)t0.ocr[0];
    case OCR0B: return (uint8_t)t0.ocr[1];
    case TCNT1L: tmr_sync(&t1); t1.temp = (uint8_t)(t1.tcnt >> 8); return (uint8_t)t1.tcnt;
    case TCNT3L: tmr_sync(&t3); t3.temp = (uint8_t)(t3.tcnt >> 8); return (uint8_t)t3.tcnt;
    case TCNT1H: return t1.temp;
    case TCNT3H: return t3.temp;
    case ICR1L: t1.temp = (uint8_t)(t1.icr >> 8); return (uint8_t)t1.icr;
    case ICR3L: t3.temp = (uint8_t)(t3.icr >> 8); return (uint8_t)t3.icr;
    case ICR1H: return t1.temp;
    case ICR3H: return t3.temp;
    case OCR1AL: case OCR1BL: case OCR1CL: return (uint8_t)t1.ocr[(a - OCR1AL) >> 1];
    case OCR1AH: case OCR1BH: case OCR1CH: return (uint8_t)(t1.ocr[(a - OCR1AH) >> 1] >> 8);
    case OCR3AL: case OCR3BL: case OCR3CL: return (uint8_t)t3.ocr[(a - OCR3AL) >> 1];
    case OCR3AH: case OCR3BH: case OCR3CH: return (uint8_t)(t3.ocr[(a - OCR3AH) >> 1] >> 8);
    case TCNT4: t4_sync(); io[TC4H] = (uint8_t)(t4.tcnt >> 8); return (uint8_t)t4.tcnt;
    case OCR4A: return (uint8_t)t4.ocr_a;
    case OCR4B: return (uint8_t)t4.ocr_b;
    case OCR4C: return (uint8_t)t4.ocr_c;
    case OCR4D: return (uint8_t)t4.ocr_d;
    case SPSR: spsr_seen = (io[SPSR] & 0x80) != 0; return io[SPSR];
    case SPDR:
        if (spsr_seen) { io[SPSR] &= ~0x80; spsr_seen = false; irq_update(); }
        return io[SPDR];
    case PLLCSR:
        return (uint8_t)((io[PLLCSR] & ~1) | ((io[PLLCSR] >> 1) & 1));   /* PLOCK = PLLE */
    case EECR: return io[EECR] & ~0x02;
    case ADCL: return io[ADCL];
    case ADCH: return io[ADCH];
    case 0xD9: return 0;                /* USBSTA: no VBUS */
    default:
        return io[a];
    }
}

static ARD_RAM void tmr16_write(tmr_t *t, int reg, uint8_t lo)
{
    uint16_t v = (uint16_t)(lo | t->temp << 8);
    tmr_sync(t);
    if (reg < 0) {
        t->tcnt = v;
    } else if (reg == 3) {
        t->icr = v;
    } else {
        t->ocr[reg] = v;
    }
    if (t == &t3) snd_update();
}

static ARD_RAM void io_write(uint16_t a, uint8_t v)
{
    switch (a) {
    case PINB: case PINC: case PIND: case PINE: case PINF:
        io[a + 2] ^= v;                 /* writing PINx toggles PORTx */
        if (a == PINC) snd_update();
        return;
    case PORTC: case DDRC:
        io[a] = v;
        snd_update();
        return;
    case PORTD:
        if ((io[PORTD] & 0x80) && !(v & 0x80))
            oled_reset();               /* RST went low */
        io[a] = v;
        return;
    case SPL: sp = (uint16_t)((sp & 0xFF00) | v); return;
    case SPH: sp = (uint16_t)((sp & 0x00FF) | v << 8); return;
    case SREG:
        if ((v & FI) && !(sreg & FI))
            irq_hold = 1;
        sreg = v;
        run_limit = cyc;
        return;
    case TIFR0: tmr_sync(&t0); io[a] &= ~v; irq_update(); return;
    case TIFR1: tmr_sync(&t1); io[a] &= ~v; irq_update(); return;
    case TIFR3: tmr_sync(&t3); io[a] &= ~v; irq_update(); return;
    case TIFR4: t4_sync(); io[a] &= ~v; irq_update(); return;
    case TCCR0A: case TCCR0B: tmr_sync(&t0); io[a] = v; t0.last = cyc; break;
    case TCNT0: tmr_sync(&t0); t0.tcnt = v; break;
    case OCR0A: tmr_sync(&t0); t0.ocr[0] = v; break;
    case OCR0B: tmr_sync(&t0); t0.ocr[1] = v; break;
    case TCCR1A: case TCCR1B: case TCCR1C: tmr_sync(&t1); io[a] = v; break;
    case TCCR3A: case TCCR3B: case TCCR3C:
        tmr_sync(&t3); io[a] = v;
        if (a == TCCR3A && !((v >> 6) & 3)) t3.oc_a = false;
        snd_update();
        break;
    case TCNT1H: case ICR1H: case OCR1AH: case OCR1BH: case OCR1CH: t1.temp = v; return;
    case TCNT3H: case ICR3H: case OCR3AH: case OCR3BH: case OCR3CH: t3.temp = v; return;
    case TCNT1L: tmr16_write(&t1, -1, v); break;
    case ICR1L: tmr16_write(&t1, 3, v); break;
    case OCR1AL: case OCR1BL: case OCR1CL: tmr16_write(&t1, (a - OCR1AL) >> 1, v); break;
    case TCNT3L: tmr16_write(&t3, -1, v); break;
    case ICR3L: tmr16_write(&t3, 3, v); break;
    case OCR3AL: case OCR3BL: case OCR3CL: tmr16_write(&t3, (a - OCR3AL) >> 1, v); break;
    case TCNT4: t4_sync(); t4.tcnt = (uint16_t)(v | (io[TC4H] & 3) << 8); break;
    case OCR4A: t4_sync(); t4.ocr_a = (uint16_t)(v | (io[TC4H] & 3) << 8); snd_update(); break;
    case OCR4B: t4_sync(); t4.ocr_b = (uint16_t)(v | (io[TC4H] & 3) << 8); break;
    case OCR4C: t4_sync(); t4.ocr_c = (uint16_t)(v | (io[TC4H] & 3) << 8); snd_update(); break;
    case OCR4D: t4_sync(); t4.ocr_d = (uint16_t)(v | (io[TC4H] & 3) << 8); break;
    case TCCR4A: case TCCR4B: case TCCR4C: case TCCR4D: case TCCR4E: case PLLFRQ:
        t4_sync();
        if (a == TCCR4B && (v & 0x40)) { t4.resid = 0; v &= ~0x40; }   /* PSR4 */
        io[a] = v;
        if (a == TCCR4A && !((v >> 6) & 3)) t4.oc_a = false;
        snd_update();
        break;
    case TIMSK0: case TIMSK1: case TIMSK3: case TIMSK4:
        timers_sync();
        io[a] = v;
        irq_update();
        break;
    case SPDR: spi_write(v); return;
    case SPSR: io[a] = (uint8_t)((io[a] & 0xC0) | (v & 0x01)); return;
    case SPCR: io[a] = v; irq_update(); return;
    case EECR: {
        /* EEMPE stays set for 4 cycles; EEPE within them writes */
        bool armed = (io[EECR] & 0x04) && cyc - eempe_at <= 4;
        io[EECR] = (uint8_t)(v & 0x3D);
        if ((v & 0x04) && !armed)
            eempe_at = cyc;
        uint16_t ea = (uint16_t)((io[EEARL] | io[EEARH] << 8) & (ARD_EEPROM - 1));
        if ((v & 0x02) && armed) {      /* EEPE after EEMPE: write */
            uint8_t mode = (v >> 4) & 3, d = io[EEDR];
            uint8_t nv = mode == 1 ? 0xFF : mode == 2 ? (eeprom[ea] & d) : d;
            if (eeprom[ea] != nv) {
                eeprom[ea] = nv;
                eeprom_dirty = true;
            }
            io[EECR] &= ~0x06;
        }
        if (v & 0x01) {                 /* EERE: read */
            io[EEDR] = eeprom[ea];
            io[EECR] &= ~0x01;
        }
        irq_update();
        return;
    }
    case ADCSRA: {
        uint8_t flag = (v & 0x10) ? 0 : (io[a] & 0x10);   /* writing ADIF clears it */
        io[a] = (uint8_t)((v & ~0x10) | flag);
        if ((v & 0xC0) == 0xC0) {       /* ADEN, ADSC: done at once with some noise */
            adc_noise = (uint8_t)(adc_noise * 73 + (cyc & 0xFF) + 41);
            uint16_t r = (uint16_t)(0x180 + (adc_noise & 0x3F));
            io[ADCL] = (uint8_t)r;
            io[ADCH] = (uint8_t)(r >> 8);
            io[a] = (uint8_t)((io[a] & ~0x40) | 0x10);
        }
        irq_update();
        return;
    }
    default:
        io[a] = v;
        return;
    }
    events_update();
}

/* ---- decoding ----------------------------------------------------------------------------- */

#define OPS(X) \
    X(NOP) X(MOVW) X(MULS) X(MULSU) X(FMUL) X(FMULS) X(FMULSU) X(CPC) X(SBC) X(ADD) \
    X(CPSE) X(CP) X(SUB) X(ADC) X(AND) X(EOR) X(OR) X(MOV) X(CPI) X(SBCI) X(SUBI) X(ORI) \
    X(ANDI) X(LDDZ) X(LDDY) X(STDZ) X(STDY) X(LDS) X(LDZP) X(LDMZ) X(LPMZ) X(LPMZP) \
    X(LDYP) X(LDMY) X(LDX) X(LDXP) X(LDMX) X(POP) X(STS) X(STZP) X(STMZ) X(STYP) X(STMY) \
    X(STX) X(STXP) X(STMX) X(PUSH) X(COM) X(NEG) X(SWAP) X(INC) X(ASR) X(LSR) X(ROR) \
    X(DEC) X(JMP) X(CALL) X(BSET) X(BCLR) X(RET) X(RETI) X(SLEEP) X(BREAK) X(WDR) X(LPM0) \
    X(SPM) X(IJMP) X(ICALL) X(ADIW) X(SBIW) X(CBI) X(SBIC) X(SBI) X(SBIS) X(MUL) X(IN) \
    X(OUT) X(RJMP) X(RCALL) X(LDI) X(BRBS) X(BRBC) X(BLD) X(BST) X(SBRC) X(SBRS) \
    X(ILLEGAL) X(WRAP) X(JIT) X(JITX)
#define OP_ENUM(name) O_##name,
enum { OPS(OP_ENUM) };
#undef OP_ENUM

static void decode_one(int i, insn_t *c);

static void decode(int i)
{
    insn_t c;
    decode_one(i, &c);
    code[i].w = (uint32_t)c.op | (uint32_t)c.d << 8 | (uint32_t)c.x << 16;
}

static void decode_one(int i, insn_t *c)
{
    uint16_t o = (uint16_t)(flash[2 * i] | flash[2 * i + 1] << 8);
    uint16_t next = i + 1 < 0x4000 ? (uint16_t)(flash[2 * i + 2] | flash[2 * i + 3] << 8) : 0;
    uint8_t d5 = (o >> 4) & 0x1F, r5 = (uint8_t)((o & 0x0F) | ((o >> 5) & 0x10));
    uint8_t d4 = (uint8_t)(16 + ((o >> 4) & 0x0F)), k8 = (uint8_t)((o & 0x0F) | ((o >> 4) & 0xF0));
    c->op = O_ILLEGAL; c->d = 0; c->x = 0;
    switch (o >> 12) {
    case 0x0:
        if (o == 0) { c->op = O_NOP; return; }
        switch ((o >> 8) & 0x0F) {
        case 1: c->op = O_MOVW; c->d = (uint8_t)(((o >> 4) & 0x0F) * 2); c->x = (uint16_t)((o & 0x0F) * 2); return;
        case 2: c->op = O_MULS; c->d = d4; c->x = (uint16_t)(16 + (o & 0x0F)); return;
        case 3: {
            uint8_t d = (uint8_t)(16 + ((o >> 4) & 7)), r = (uint8_t)(16 + (o & 7));
            int k = ((o >> 6) & 2) | ((o >> 3) & 1);
            static const uint8_t ops[4] = { O_MULSU, O_FMUL, O_FMULS, O_FMULSU };
            c->op = ops[k]; c->d = d; c->x = r; return;
        }
        default: break;
        }
        switch ((o >> 10) & 3) {
        case 1: c->op = O_CPC; break;
        case 2: c->op = O_SBC; break;
        case 3: c->op = O_ADD; break;
        default: return;
        }
        c->d = d5; c->x = r5; return;
    case 0x1: {
        static const uint8_t ops[4] = { O_CPSE, O_CP, O_SUB, O_ADC };
        c->op = ops[(o >> 10) & 3]; c->d = d5; c->x = r5; return;
    }
    case 0x2: {
        static const uint8_t ops[4] = { O_AND, O_EOR, O_OR, O_MOV };
        c->op = ops[(o >> 10) & 3]; c->d = d5; c->x = r5; return;
    }
    case 0x3: c->op = O_CPI;  c->d = d4; c->x = k8; return;
    case 0x4: c->op = O_SBCI; c->d = d4; c->x = k8; return;
    case 0x5: c->op = O_SUBI; c->d = d4; c->x = k8; return;
    case 0x6: c->op = O_ORI;  c->d = d4; c->x = k8; return;
    case 0x7: c->op = O_ANDI; c->d = d4; c->x = k8; return;
    case 0x8: case 0xA: {
        uint8_t q = (uint8_t)((o & 7) | ((o >> 7) & 0x18) | ((o >> 8) & 0x20));
        bool st = o & 0x0200, y = o & 0x0008;
        c->op = st ? (y ? O_STDY : O_STDZ) : (y ? O_LDDY : O_LDDZ);
        c->d = d5; c->x = q; return;
    }
    case 0x9:
        if ((o & 0xFE00) == 0x9000 || (o & 0xFE00) == 0x9200) {
            bool st = o & 0x0200;
            c->d = d5;
            switch (o & 0x0F) {
            case 0x0: c->op = st ? O_STS : O_LDS; c->x = next; return;
            case 0x1: c->op = st ? O_STZP : O_LDZP; return;
            case 0x2: c->op = st ? O_STMZ : O_LDMZ; return;
            case 0x4: case 0x6: if (!st) c->op = O_LPMZ; return;
            case 0x5: case 0x7: if (!st) c->op = O_LPMZP; return;
            case 0x9: c->op = st ? O_STYP : O_LDYP; return;
            case 0xA: c->op = st ? O_STMY : O_LDMY; return;
            case 0xC: c->op = st ? O_STX : O_LDX; return;
            case 0xD: c->op = st ? O_STXP : O_LDXP; return;
            case 0xE: c->op = st ? O_STMX : O_LDMX; return;
            case 0xF: c->op = st ? O_PUSH : O_POP; return;
            default: return;
            }
        }
        if ((o & 0xFE00) == 0x9400) {
            c->d = d5;
            switch (o & 0x0F) {
            case 0x0: c->op = O_COM; return;
            case 0x1: c->op = O_NEG; return;
            case 0x2: c->op = O_SWAP; return;
            case 0x3: c->op = O_INC; return;
            case 0x5: c->op = O_ASR; return;
            case 0x6: c->op = O_LSR; return;
            case 0x7: c->op = O_ROR; return;
            case 0xA: c->op = O_DEC; return;
            case 0xC: case 0xD: c->op = O_JMP; c->x = next; return;
            case 0xE: case 0xF: c->op = O_CALL; c->x = next; return;
            case 0x8:
                if ((o & 0xFF8F) == 0x9408) { c->op = O_BSET; c->d = (o >> 4) & 7; return; }
                if ((o & 0xFF8F) == 0x9488) { c->op = O_BCLR; c->d = (o >> 4) & 7; return; }
                switch (o) {
                case 0x9508: c->op = O_RET; return;
                case 0x9518: c->op = O_RETI; return;
                case 0x9588: c->op = O_SLEEP; return;
                case 0x9598: c->op = O_BREAK; return;
                case 0x95A8: c->op = O_WDR; return;
                case 0x95C8: case 0x95D8: c->op = O_LPM0; return;
                case 0x95E8: case 0x95F8: c->op = O_SPM; return;
                }
                return;
            case 0x9:
                if (o == 0x9409 || o == 0x9419) c->op = O_IJMP;
                else if (o == 0x9509 || o == 0x9519) c->op = O_ICALL;
                return;
            default: return;
            }
        }
        if ((o & 0xFE00) == 0x9600) {
            c->op = (o & 0x0100) ? O_SBIW : O_ADIW;
            c->d = (uint8_t)(24 + ((o >> 4) & 3) * 2);
            c->x = (uint16_t)((o & 0x0F) | ((o >> 2) & 0x30));
            return;
        }
        if ((o & 0xFC00) == 0x9800) {
            static const uint8_t ops[4] = { O_CBI, O_SBIC, O_SBI, O_SBIS };
            c->op = ops[(o >> 8) & 3]; c->d = (uint8_t)(((o >> 3) & 0x1F) + 0x20); c->x = o & 7;
            return;
        }
        if ((o & 0xFC00) == 0x9C00) { c->op = O_MUL; c->d = d5; c->x = r5; return; }
        return;
    case 0xB: {
        uint8_t A = (uint8_t)(((o & 0x0F) | ((o >> 5) & 0x30)) + 0x20);
        c->op = (o & 0x0800) ? O_OUT : O_IN; c->d = d5; c->x = A; return;
    }
    case 0xC: case 0xD: {
        int16_t k = (int16_t)(o << 4) >> 4;
        c->op = (o >> 12) == 0xC ? O_RJMP : O_RCALL; c->x = (uint16_t)k; return;
    }
    case 0xE: c->op = O_LDI; c->d = d4; c->x = k8; return;
    case 0xF:
        if (!(o & 0x0800)) {
            int8_t k = (int8_t)(((o >> 3) & 0x7F) << 1) >> 1;
            c->op = (o & 0x0400) ? O_BRBC : O_BRBS; c->d = o & 7; c->x = (uint16_t)(int16_t)k;
            return;
        }
        if (o & 0x0008) return;
        {
            static const uint8_t ops[4] = { O_BLD, O_BST, O_SBRC, O_SBRS };
            c->op = ops[(o >> 9) & 3]; c->d = d5; c->x = o & 7;
        }
        return;
    }
}

/* LDS, STS, JMP, CALL: from the flash (a slot may hold a pattern's or the
 * translator's op) */
static bool two_words(int i)
{
    i &= 0x3FFF;
    uint16_t o = (uint16_t)(flash[2 * i] | flash[2 * i + 1] << 8);
    return (o & 0xFC0F) == 0x9000 || (o & 0xFE0C) == 0x940C;
}

/* ---- the CPU ----------------------------------------------------------------------------------- */

/* The flags of an 8-bit add or subtract (with carry in c), branch-free.
 * keep_z: SBC/SBCI/CPC only clear Z, never set it. */
#if defined(__arm__) || defined(ARD_TEST_NZCV)
/* The ARM's own flags: the operands in the top byte of 32-bit words, the
 * carry in spread over the low 24 bits so that it reaches bit 24 and leaves
 * the low bits 0. ADDS then gives N, Z, C, V of the 8-bit sum; a subtract
 * is a + ~b + !c, its C the inverse of the AVR's (borrow). The tables turn
 * NZCV into the AVR's C, Z, N, V, S. Checked for all a, b, c. */
static uint8_t nzcv_add[16] __attribute__((section(".data.ard_nzcv"))) =
    { 0, 24, 1, 25, 2, 26, 3, 27, 20, 12, 21, 13, 22, 14, 23, 15 };
static uint8_t nzcv_sub[16] __attribute__((section(".data.ard_nzcv"))) =
    { 1, 25, 0, 24, 3, 27, 2, 26, 21, 13, 20, 12, 23, 15, 22, 14 };

static inline uint32_t adds_nzcv(uint32_t a, uint32_t b, uint32_t *sum)
{
#if defined(__arm__)
    uint32_t s, ps;
    __asm__("adds %0, %2, %3\n\tmrs %1, APSR" : "=&r"(s), "=&r"(ps) : "r"(a), "r"(b) : "cc");
    *sum = s;
    return ps >> 28;
#else                                   /* the host test: the same in C */
    uint64_t S = (uint64_t)a + b;
    uint32_t r = (uint32_t)S, n = r >> 31, v = (a >> 31) == (b >> 31) && n != (a >> 31);
    *sum = r;
    return n << 3 | (uint32_t)(r == 0) << 2 | (uint32_t)(S >> 32) << 1 | v;
#endif
}

static inline uint8_t f_add(uint8_t f, unsigned a, unsigned b, unsigned c, uint8_t *res)
{
    uint32_t s, n = adds_nzcv(a << 24 | c, b << 24 | ((0u - c) >> 8), &s);
    unsigned r = s >> 24;
    *res = (uint8_t)r;
    return (uint8_t)((f & (FI | FT)) | nzcv_add[n] | ((a ^ b ^ r) & 0x10) << 1);
}

/* keep_z: SBC/SBCI/CPC only clear Z, never set it */
static inline uint8_t f_sub(uint8_t f, unsigned a, unsigned b, unsigned c, bool keep_z, uint8_t *res)
{
    unsigned nc = c ^ 1;
    uint32_t s, n = adds_nzcv(a << 24 | nc, (~b & 0xFF) << 24 | ((0u - nc) >> 8), &s);
    unsigned r = s >> 24, fl = nzcv_sub[n];
    if (keep_z)
        fl &= f | ~FZ;
    *res = (uint8_t)r;
    return (uint8_t)((f & (FI | FT)) | fl | ((a ^ b ^ r) & 0x10) << 1);
}
#else
/* The flags of an 8-bit add or subtract (with carry in c), branch-free.
 * keep_z: SBC/SBCI/CPC only clear Z, never set it. */
static inline uint8_t f_add(uint8_t f, unsigned a, unsigned b, unsigned c, uint8_t *res)
{
    unsigned s = a + b + c, r = s & 0xFF;
    unsigned n = r >> 7, v = (((a ^ r) & (b ^ r)) >> 7) & 1;
    *res = (uint8_t)r;
    return (uint8_t)((f & (FI | FT)) | ((s >> 8) & 1) | (unsigned)(r == 0) << 1 | n << 2 | v << 3 |
                     (n ^ v) << 4 | ((a ^ b ^ r) & 0x10) << 1);
}

static inline uint8_t f_sub(uint8_t f, unsigned a, unsigned b, unsigned c, bool keep_z, uint8_t *res)
{
    unsigned s = a - b - c, r = s & 0xFF;
    unsigned n = r >> 7, v = (((a ^ b) & (a ^ r)) >> 7) & 1;
    unsigned z = keep_z ? (r == 0 && (f & FZ)) : (r == 0);
    *res = (uint8_t)r;
    return (uint8_t)((f & (FI | FT)) | ((s >> 8) & 1) | z << 1 | n << 2 | v << 3 |
                     (n ^ v) << 4 | ((a ^ b ^ r) & 0x10) << 1);
}
#endif

/* AND, OR, EOR, COM: V cleared, S = N */
static inline uint8_t f_logic(uint8_t f, unsigned r)
{
    return (uint8_t)((f & ~(FV | FZ | FN | FS)) | (unsigned)(r == 0) << 1 | ((r >> 7) & 1) * (FN | FS));
}

/* Z, N, and S = N ^ V with V as it is now */
static inline uint8_t f_zns(uint8_t f, unsigned r)
{
    unsigned n = (r >> 7) & 1, v = (f >> 3) & 1;
    return (uint8_t)((f & ~(FZ | FN | FS)) | (unsigned)(r == 0) << 1 | n << 2 | (n ^ v) << 4);
}

static inline ARD_RAM uint8_t rd(uint16_t a)
{
    if (a >= 0x100)
        return a < 0xB00 ? mem[a] : 0;
    if (a < 0x20)
        return R[a];
    return io_read(a);
}

static inline ARD_RAM void wr(uint16_t a, uint8_t v)
{
    if (a >= 0x100) {
        if (a < 0xB00)
            mem[a] = v;
        return;
    }
    if (a < 0x20)
        R[a] = v;
    else
        io_write(a, v);
}

static inline ARD_RAM void push(uint8_t v)
{
    wr(sp, v);
    sp--;
}

/* the interrupt with the lowest vector number of the pending ones */
static ARD_RAM int irq_vector(void)
{
    uint8_t f;
    if ((f = io[TIFR1] & io[TIMSK1] & 0x2F)) {
        if (f & 0x20) { io[TIFR1] &= ~0x20; return 16; }
        if (f & 0x02) { io[TIFR1] &= ~0x02; return 17; }
        if (f & 0x04) { io[TIFR1] &= ~0x04; return 18; }
        if (f & 0x08) { io[TIFR1] &= ~0x08; return 19; }
        io[TIFR1] &= ~0x01; return 20;
    }
    if ((f = io[TIFR0] & io[TIMSK0] & 7)) {
        if (f & 2) { io[TIFR0] &= ~2; return 21; }
        if (f & 4) { io[TIFR0] &= ~4; return 22; }
        io[TIFR0] &= ~1; return 23;
    }
    if ((io[SPSR] & 0x80) && (io[SPCR] & 0x80)) { io[SPSR] &= ~0x80; return 24; }
    if ((io[ADCSRA] & 0x18) == 0x18) { io[ADCSRA] &= ~0x10; return 29; }
    if (io[EECR] & 0x08) return 30;
    if ((f = io[TIFR3] & io[TIMSK3] & 0x2F)) {
        if (f & 0x20) { io[TIFR3] &= ~0x20; return 31; }
        if (f & 0x02) { io[TIFR3] &= ~0x02; return 32; }
        if (f & 0x04) { io[TIFR3] &= ~0x04; return 33; }
        if (f & 0x08) { io[TIFR3] &= ~0x08; return 34; }
        io[TIFR3] &= ~0x01; return 35;
    }
    if ((f = io[TIFR4] & io[TIMSK4] & 0xE4)) {
        if (f & 0x40) { io[TIFR4] &= ~0x40; return 38; }
        if (f & 0x20) { io[TIFR4] &= ~0x20; return 39; }
        if (f & 0x80) { io[TIFR4] &= ~0x80; return 40; }
        io[TIFR4] &= ~0x04; return 41;
    }
    return -1;
}

static ARD_RAM void take_irq(void)
{
    int v = irq_vector();
    irq_update();
    if (v < 0)
        return;
    sleeping = false;
    push((uint8_t)pc);
    push((uint8_t)(pc >> 8));
    sreg &= ~FI;
    pc = (uint16_t)(v * 2);
    cyc += 5;                           /* 4, plus 1 waking or not */
}

/* Run instructions until the cycle `limit`, or until something lowers
 * run_limit (an interrupt that can be taken, SLEEP, SEI, a timer event).
 *
 * One function, the state the instructions touch most (pc, SREG, the cycle
 * count, the limit) in locals the compiler keeps in registers, and each
 * instruction jumping straight to the next one's code (GCC's computed goto)
 * instead of a call and a switch per instruction: on the M33 that was ~70
 * clock cycles per AVR instruction. What may change the globals behind the
 * locals' back (the I/O registers, interrupts) goes through SYNC_OUT /
 * SYNC_IN. */
static ARD_EXEC void exec(uint32_t limit);
#ifdef ARD_JIT
#include "ard_jit.c"
#endif

static ARD_EXEC void exec(uint32_t limit)
{
#define OP_LABEL(name) &&L_##name,
    /* in RAM: GCC would put it with the constants, in flash */
    static void *tbl[] __attribute__((section(".data.ard_ops"))) = { OPS(OP_LABEL) };
#undef OP_LABEL
    if (!exec_ops) {
        exec_ops = (const void *const *)tbl;
        return;
    }
    /* the cycle count as what is left to the limit, less 1 (so that the
     * test is a sign: SUBS, BMI): c = base - left - 1 */
    uint32_t base = limit;
    int32_t left = (int32_t)(limit - cyc) - 1;
    const slot_t *ip = &code[pc];
    /* the data space's address in a register: hidden from GCC, which would
     * otherwise load the constant again in every instruction's code */
    uint8_t *dm = mem;
    __asm__("" : "+r"(dm));
#undef R
#define R dm
    uint8_t f = sreg;
    uint32_t w;
    unsigned d, x;
    run_limit = limit;

#define CYC() (base - (uint32_t)left - 1)
#define STOP() (base = CYC(), left = -1)         /* leave after this instruction */
#define PIDX() ((uint32_t)(ip - code))
#define SETP(v) (ip = &code[(v) & 0x3FFF])
#define SYNC_OUT() (pc = (uint16_t)PIDX(), sreg = f, cyc = CYC())
#define SYNC_IN()  (f = sreg, left = (int32_t)(run_limit - cyc) - 1, base = run_limit)
/* the PC runs past 0x3FFF only by one: code[0x4000] (WRAP) sets it back */
#ifdef ARD_PROF                         /* host profiling: ops and pairs as dispatched */
#define PROF_OP(o) (ard_prof_op[o]++, ard_prof_pair[ard_prof_last][o]++, ard_prof_last = (o))
#else
#define PROF_OP(o) ((void)0)
#endif
#define DISPATCH() do { if (left < 0) goto out; \
        const void *h_ = ip->h; w = ip->w; PROF_OP(w & 0xFF); ip++; d = (w >> 8) & 0xFF; x = w >> 16; \
        goto *h_; } while (0)
#define NEXT(n) do { left -= (n); DISPATCH(); } while (0)
#define RD(a, out) do { unsigned a_ = (a); \
        if (a_ - 0x100u < 0xA00u || a_ < 0x20) (out) = R[a_]; \
        else if (a_ >= 0xB00) (out) = 0; \
        else { SYNC_OUT(); (out) = io_read((uint16_t)a_); SYNC_IN(); } } while (0)
#define WR(a, v) do { unsigned a_ = (a); uint8_t v_ = (v); \
        if (a_ - 0x100u < 0xA00u || a_ < 0x20) R[a_] = v_; \
        else if (a_ < 0x100) { SYNC_OUT(); io_write((uint16_t)a_, v_); SYNC_IN(); } } while (0)
#define PUSH(v) do { WR(sp, (v)); sp--; } while (0)
#define POP(out) do { sp++; RD(sp, out); } while (0)
#define PUSH_PC(v) do { unsigned pv_ = (v); PUSH((uint8_t)pv_); PUSH((uint8_t)(pv_ >> 8)); } while (0)
#define POP_PC() do { uint8_t h_, l_; POP(h_); POP(l_); SETP((unsigned)(h_ << 8 | l_)); } while (0)
#define XR ((unsigned)(R[26] | R[27] << 8))
#define YR ((unsigned)(R[28] | R[29] << 8))
#define ZR ((unsigned)(R[30] | R[31] << 8))
#define SET16(lo, v) do { unsigned s_ = (v); R[lo] = (uint8_t)s_; R[(lo) + 1] = (uint8_t)(s_ >> 8); } while (0)
#define SKIP() do { unsigned n_ = two_words((int)PIDX()) ? 2 : 1; SETP(PIDX() + n_); NEXT(1 + n_); } while (0)
#define REL() SETP(PIDX() + (uint32_t)(int16_t)x)

    DISPATCH();

L_NOP: NEXT(1);
L_MOVW: R[d] = R[x]; R[d + 1] = R[x + 1]; NEXT(1);
#define MULOP(sa, sb, frac) do { \
        int32_t a_ = (sa) ? (int8_t)R[d] : R[d], b_ = (sb) ? (int8_t)R[x] : R[x]; \
        unsigned r_ = (uint16_t)(a_ * b_); \
        f = (uint8_t)((f & ~(FC | FZ)) | ((r_ >> 15) & 1)); \
        if (frac) r_ = (uint16_t)(r_ << 1); \
        if (!r_) f |= FZ; \
        R[0] = (uint8_t)r_; R[1] = (uint8_t)(r_ >> 8); NEXT(2); } while (0)
L_MUL:    MULOP(0, 0, 0);
L_MULS:   MULOP(1, 1, 0);
L_MULSU:  MULOP(1, 0, 0);
L_FMUL:   MULOP(0, 0, 1);
L_FMULS:  MULOP(1, 1, 1);
L_FMULSU: MULOP(1, 0, 1);
L_CPC: { uint8_t r; f = f_sub(f, R[d], R[x], f & FC, true, &r); NEXT(1); }
L_SBC: f = f_sub(f, R[d], R[x], f & FC, true, &R[d]); NEXT(1);
L_ADD: f = f_add(f, R[d], R[x], 0, &R[d]); NEXT(1);
L_ADC: f = f_add(f, R[d], R[x], f & FC, &R[d]); NEXT(1);
L_CP: { uint8_t r; f = f_sub(f, R[d], R[x], 0, false, &r); NEXT(1); }
L_SUB: f = f_sub(f, R[d], R[x], 0, false, &R[d]); NEXT(1);
L_CPSE: if (R[d] == R[x]) SKIP(); NEXT(1);
L_AND: R[d] &= R[x]; f = f_logic(f, R[d]); NEXT(1);
L_EOR: R[d] ^= R[x]; f = f_logic(f, R[d]); NEXT(1);
L_OR:  R[d] |= R[x]; f = f_logic(f, R[d]); NEXT(1);
L_MOV: R[d] = R[x]; NEXT(1);
L_CPI: { uint8_t r; f = f_sub(f, R[d], x, 0, false, &r); NEXT(1); }
L_SBCI: f = f_sub(f, R[d], x, f & FC, true, &R[d]); NEXT(1);
L_SUBI: f = f_sub(f, R[d], x, 0, false, &R[d]); NEXT(1);
L_ORI:  R[d] |= (uint8_t)x; f = f_logic(f, R[d]); NEXT(1);
L_ANDI: R[d] &= (uint8_t)x; f = f_logic(f, R[d]); NEXT(1);
L_LDDZ: RD((ZR + x) & 0xFFFF, R[d]); NEXT(2);
L_LDDY: RD((YR + x) & 0xFFFF, R[d]); NEXT(2);
L_STDZ: WR((ZR + x) & 0xFFFF, R[d]); NEXT(2);
L_STDY: WR((YR + x) & 0xFFFF, R[d]); NEXT(2);
L_LDS: RD(x, R[d]); SETP(PIDX() + 1); NEXT(2);
L_STS: WR(x, R[d]); SETP(PIDX() + 1); NEXT(2);
L_LDZP: { unsigned a = ZR; RD(a, R[d]); SET16(30, a + 1); NEXT(2); }
L_LDMZ: { unsigned a = (ZR - 1) & 0xFFFF; SET16(30, a); RD(a, R[d]); NEXT(2); }
L_LDYP: { unsigned a = YR; RD(a, R[d]); SET16(28, a + 1); NEXT(2); }
L_LDMY: { unsigned a = (YR - 1) & 0xFFFF; SET16(28, a); RD(a, R[d]); NEXT(2); }
L_LDX:  RD(XR, R[d]); NEXT(2);
L_LDXP: { unsigned a = XR; RD(a, R[d]); SET16(26, a + 1); NEXT(2); }
L_LDMX: { unsigned a = (XR - 1) & 0xFFFF; SET16(26, a); RD(a, R[d]); NEXT(2); }
L_STZP: { unsigned a = ZR; WR(a, R[d]); SET16(30, a + 1); NEXT(2); }
L_STMZ: { unsigned a = (ZR - 1) & 0xFFFF; SET16(30, a); WR(a, R[d]); NEXT(2); }
L_STYP: { unsigned a = YR; WR(a, R[d]); SET16(28, a + 1); NEXT(2); }
L_STMY: { unsigned a = (YR - 1) & 0xFFFF; SET16(28, a); WR(a, R[d]); NEXT(2); }
L_STX:  WR(XR, R[d]); NEXT(2);
L_STXP: { unsigned a = XR; WR(a, R[d]); SET16(26, a + 1); NEXT(2); }
L_STMX: { unsigned a = (XR - 1) & 0xFFFF; SET16(26, a); WR(a, R[d]); NEXT(2); }
L_LPMZ: R[d] = flash[ZR & 0x7FFF]; NEXT(3);
L_LPMZP: { unsigned a = ZR; R[d] = flash[a & 0x7FFF]; SET16(30, a + 1); NEXT(3); }
L_LPM0: R[0] = flash[ZR & 0x7FFF]; NEXT(3);
L_POP: POP(R[d]); NEXT(2);
L_PUSH: PUSH(R[d]); NEXT(2);
L_COM: R[d] = (uint8_t)~R[d]; f = (uint8_t)(f_logic(f, R[d]) | FC); NEXT(1);
L_NEG: f = f_sub(f, 0, R[d], 0, false, &R[d]); NEXT(1);
L_SWAP: R[d] = (uint8_t)(R[d] << 4 | R[d] >> 4); NEXT(1);
L_INC: { unsigned r = (R[d] + 1) & 0xFF; f = (uint8_t)((f & ~FV) | (unsigned)(r == 0x80) << 3); R[d] = (uint8_t)r; f = f_zns(f, r); NEXT(1); }
L_DEC: { unsigned r = (R[d] - 1) & 0xFF; f = (uint8_t)((f & ~FV) | (unsigned)(r == 0x7F) << 3); R[d] = (uint8_t)r; f = f_zns(f, r); NEXT(1); }
#define SHIFTOP(expr) do { unsigned v_ = R[d], r_ = (expr) & 0xFF; \
        unsigned c_ = v_ & 1, n_ = r_ >> 7, ov_ = n_ ^ c_; \
        f = (uint8_t)((f & ~(FC | FZ | FN | FV | FS)) | c_ | (unsigned)(r_ == 0) << 1 | n_ << 2 | ov_ << 3 | (n_ ^ ov_) << 4); \
        R[d] = (uint8_t)r_; NEXT(1); } while (0)
L_ASR: SHIFTOP((v_ >> 1) | (v_ & 0x80));
L_LSR: SHIFTOP(v_ >> 1);
L_ROR: SHIFTOP((v_ >> 1) | (unsigned)(f & FC) << 7);
L_JMP: SETP(x); NEXT(3);
L_CALL: PUSH_PC((PIDX() + 1) & 0x3FFF); SETP(x); NEXT(4);
L_RJMP: REL(); NEXT(2);
L_RCALL: PUSH_PC(PIDX()); REL(); NEXT(3);
L_IJMP: SETP(ZR); NEXT(2);
L_ICALL: PUSH_PC(PIDX()); SETP(ZR); NEXT(3);
L_RET: POP_PC(); NEXT(4);
L_RETI: POP_PC(); f |= FI; irq_hold = 1; STOP(); NEXT(4);
L_BSET:
    /* SEI: only an interrupt already waiting needs the outer loop (the
     * instruction after SEI runs first); one raised later lowers run_limit */
    if (d == 7 && !(f & FI) && irq_pending) {
        irq_hold = 1;
        STOP();
    }
    f |= (uint8_t)(1 << d);
    NEXT(1);
L_BCLR: f &= (uint8_t)~(1 << d); NEXT(1);
L_SLEEP:
    if (io[SMCR] & 1) {
        sleeping = true;
        STOP();
    }
    NEXT(1);
L_BREAK: L_WDR: L_SPM: L_ILLEGAL: NEXT(1);
L_WRAP: SETP(0); DISPATCH();

#ifdef ARD_JIT
/* a block start (ard_jit.c): translated the first time it runs, then
 * entered when the whole block fits the budget, else the instruction in
 * the interpreter */
L_JITX: {
    unsigned i = PIDX() - 1;
    if (!jit_translate((int)i)) {
        insn_t c;
        decode_one((int)i, &c);
        code[i].w = (uint32_t)c.op | (uint32_t)c.d << 8 | (uint32_t)c.x << 16;
        code[i].h = tbl[c.op];
        w = code[i].w; d = c.d; x = c.x;
        goto *tbl[c.op];
    }
    w = code[i].w; d = (w >> 8) & 0xFF; x = w >> 16;
}
    /* fall through */
L_JIT: {
    if (left < (int32_t)d) {
        insn_t c;
        JIT_STAT(3);
        decode_one((int)PIDX() - 1, &c);
        w = (uint32_t)c.op | (uint32_t)c.d << 8 | (uint32_t)c.x << 16; d = c.d; x = c.x;
        goto *tbl[c.op];
    }
    JIT_STAT(2);
    jc.left = left; jc.base = base; jc.f = f; jc.sp = sp; jc.stop = 0;
    uint32_t npc = jit_enter(&jc, (uint32_t)(uintptr_t)(jbuf + 2 * x) | 1u);
    left = jc.left; base = jc.base; f = jc.f; sp = jc.sp;
    SETP(npc);
    if (jc.stop)
        STOP();
    DISPATCH();
}
#else
L_JIT: L_JITX: NEXT(1);
#endif

L_ADIW: {
    unsigned v = R[d] | R[d + 1] << 8, r = (v + x) & 0xFFFF;
    unsigned cf = (~r & v) >> 15 & 1, vf = (r & ~v) >> 15 & 1, n = r >> 15;
    f = (uint8_t)((f & ~(FC | FZ | FN | FV | FS)) | cf | (unsigned)(r == 0) << 1 | n << 2 | vf << 3 | (n ^ vf) << 4);
    R[d] = (uint8_t)r; R[d + 1] = (uint8_t)(r >> 8);
    NEXT(2);
}
L_SBIW: {
    unsigned v = R[d] | R[d + 1] << 8, r = (v - x) & 0xFFFF;
    unsigned cf = (r & ~v) >> 15 & 1, vf = (v & ~r) >> 15 & 1, n = r >> 15;
    f = (uint8_t)((f & ~(FC | FZ | FN | FV | FS)) | cf | (unsigned)(r == 0) << 1 | n << 2 | vf << 3 | (n ^ vf) << 4);
    R[d] = (uint8_t)r; R[d + 1] = (uint8_t)(r >> 8);
    NEXT(2);
}
L_CBI: { SYNC_OUT(); io_write((uint16_t)d, (uint8_t)(io_read((uint16_t)d) & ~(1 << x))); SYNC_IN(); NEXT(2); }
L_SBI: { SYNC_OUT(); io_write((uint16_t)d, (uint8_t)(io_read((uint16_t)d) | (1 << x))); SYNC_IN(); NEXT(2); }
L_SBIC: {
    unsigned v;
    if (PIN_PORT(d)) v = R[d + 2] & pin_keep[d - PINB];     /* the buttons: no side effects */
    else { SYNC_OUT(); v = io_read((uint16_t)d); SYNC_IN(); }
    if (!((v >> x) & 1)) SKIP();
    NEXT(1);
}
L_SBIS: {
    unsigned v;
    if (PIN_PORT(d)) v = R[d + 2] & pin_keep[d - PINB];
    else { SYNC_OUT(); v = io_read((uint16_t)d); SYNC_IN(); }
    if ((v >> x) & 1) SKIP();
    NEXT(1);
}
L_IN:
    if (PIN_PORT(x)) { R[d] = R[x + 2] & pin_keep[x - PINB]; NEXT(1); }
    if (x == SREG) { R[d] = f; NEXT(1); }
    SYNC_OUT(); R[d] = io_read((uint16_t)x); SYNC_IN();
    NEXT(1);
L_OUT:
    if (x == SREG) {                    /* as SEI */
        if ((R[d] & FI) && !(f & FI) && irq_pending) {
            irq_hold = 1;
            STOP();
        }
        f = R[d];
        NEXT(1);
    }
    SYNC_OUT(); io_write((uint16_t)x, R[d]); SYNC_IN();
    NEXT(1);
L_LDI: R[d] = (uint8_t)x; NEXT(1);
L_BRBS: if (f & (1 << d)) { REL(); NEXT(2); } NEXT(1);
L_BRBC: if (!(f & (1 << d))) { REL(); NEXT(2); } NEXT(1);
L_BLD: R[d] = (uint8_t)((R[d] & ~(1 << x)) | ((f & FT) ? 1 << x : 0)); NEXT(1);
L_BST: f = (uint8_t)((f & ~FT) | ((R[d] >> x) & 1) << 6); NEXT(1);
L_SBRC: if (!((R[d] >> x) & 1)) SKIP(); NEXT(1);
L_SBRS: if ((R[d] >> x) & 1) SKIP(); NEXT(1);

out:
    SYNC_OUT();
#undef R
#define R mem
#undef CYC
#undef PIDX
#undef SETP
#undef STOP
#undef SYNC_OUT
#undef SYNC_IN
#undef DISPATCH
#undef NEXT
#undef RD
#undef WR
#undef PUSH
#undef POP
#undef PUSH_PC
#undef POP_PC
#undef XR
#undef YR
#undef ZR
#undef SET16
#undef SKIP
#undef REL
#undef MULOP
#undef SHIFTOP
}

/* ---- running --------------------------------------------------------------------------------- */

static ARD_RAM void do_events(void)
{
    timers_sync();
    irq_update();
    events_update();
}

#ifdef ARD_DEVPROF                      /* device test builds: cycles spent per part */
uint32_t ard_prof_cyc[2];
#define DWT_CYC (*(volatile uint32_t *)0xE0001004u)
#endif

void ARD_RAM ard_run(uint32_t cycles, int16_t *audio, int samples)
{
#ifdef ARD_DEVPROF
    uint32_t prof_t0 = DWT_CYC;
#endif
    uint32_t start = cyc, end = cyc + cycles;
    int16_t level0 = snd_level;
    snd_n = 0;
    while ((int32_t)(end - cyc) > 0) {
        if ((int32_t)(cyc - next_event) >= 0)
            do_events();
        if (irq_pending && (sreg & FI) && !irq_hold) {
            take_irq();
            continue;
        }
        if (sleeping) {
            /* nothing happens until the next timer event (or the end) */
            uint32_t to = (int32_t)(next_event - end) < 0 ? next_event : end;
            if ((int32_t)(to - cyc) > 0) {
                idle_cycles += to - cyc;
                cyc = to;
            }
            if (!irq_pending || !(sreg & FI)) {
                if ((int32_t)(cyc - next_event) >= 0)
                    do_events();
                if (!(irq_pending && (sreg & FI)) && (int32_t)(end - cyc) <= 0)
                    break;
                if (!(irq_pending && (sreg & FI)) && next_event == end)
                    break;
            }
            continue;
        }
        if (irq_hold) {
            irq_hold--;
            exec(cyc + 1);              /* exactly one instruction */
            continue;
        }
        /* nothing but instructions until the next event, the end, or
         * something that lowers run_limit */
        exec((int32_t)(next_event - end) < 0 ? next_event : end);
    }
#ifdef ARD_DEVPROF
    uint32_t prof_t1 = DWT_CYC;
    ard_prof_cyc[0] += prof_t1 - prof_t0;
#endif
    /* the speaker's changes as samples: each the mean level over its span */
    if (audio && samples > 0) {
        int ci = 0;
        int32_t level = level0;
        /* sample s spans cycles [span * s / samples, span * (s + 1) / samples):
         * stepped without dividing (q, r), the levels (+-256) times a span
         * fit 32 bits */
        uint32_t q = cycles / (uint32_t)samples, r = cycles % (uint32_t)samples;
        uint32_t a = 0, frac = 0;
        for (int s = 0; s < samples; s++) {
            uint32_t b = a + q;
            frac += r;
            if (frac >= (uint32_t)samples) {
                frac -= (uint32_t)samples;
                b++;
            }
            int32_t acc = 0;
            uint32_t t = a;
            while (ci < snd_n && snd_t[ci] - start < b) {
                uint32_t c = snd_t[ci] - start;
                if (c < t) c = t;
                acc += level * (int32_t)(c - t);
                t = c;
                level = snd_v[ci++];
            }
            acc += level * (int32_t)(b - t);
            int32_t v = b > a ? acc / (int32_t)(b - a) : level;
            audio[s] = (int16_t)(v * 24);       /* +-256 -> about +-6000 */
            a = b;
        }
    }
#ifdef ARD_DEVPROF
    ard_prof_cyc[1] += DWT_CYC - prof_t1;
#endif
}

/* ---- the outside ------------------------------------------------------------------------------- */

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool ard_load_hex(const char *text, size_t len)
{
    memset(flash, 0xFF, sizeof flash);
    memset(eeprom, 0xFF, sizeof eeprom);    /* erased, as a new Arduboy's: the platform loads a save over it */
    size_t i = 0;
    uint32_t base = 0;
    bool any = false;
    while (i < len) {
        while (i < len && text[i] != ':') i++;
        if (i >= len) break;
        i++;
        uint8_t b[260];
        int n = 0;
        while (i + 1 < len && hexval(text[i]) >= 0 && hexval(text[i + 1]) >= 0 && n < 260) {
            b[n++] = (uint8_t)(hexval(text[i]) << 4 | hexval(text[i + 1]));
            i += 2;
        }
        if (n < 5 || n != b[0] + 5)
            return false;
        uint8_t sum = 0;
        for (int k = 0; k < n; k++) sum = (uint8_t)(sum + b[k]);
        if (sum)
            return false;
        uint32_t addr = base + (uint32_t)(b[1] << 8 | b[2]);
        switch (b[3]) {
        case 0:
            for (int k = 0; k < b[0]; k++)
                if (addr + k < sizeof flash)
                    flash[addr + k] = b[4 + k];
            any = true;
            break;
        case 1: i = len; break;
        case 2: base = (uint32_t)(b[4] << 8 | b[5]) << 4; break;
        case 4: base = (uint32_t)(b[4] << 8 | b[5]) << 16; break;
        default: break;
        }
    }
    if (!any)
        return false;
    for (int k = 0; k < 0x4000; k++)
        decode(k);
    code[0x4000].w = code[0x4001].w = O_WRAP;
    if (!exec_ops)
        exec(0);                        /* hands over its table */
    for (int k = 0; k <= 0x4001; k++)
        code[k].h = exec_ops[code[k].w & 0xFF];
    return true;
}

void ard_reset(void)
{
    memset(mem, 0, sizeof mem);
    io[PLLFRQ] = 0x04;
    sp = 0x0AFF;
    sreg = 0;
    pc = 0;
    sleeping = false;
    irq_hold = 0;
    tmr_t *ts[3] = { &t0, &t1, &t3 };
    for (int i = 0; i < 3; i++) {
        ts[i]->tcnt = 0; ts[i]->ocr[0] = ts[i]->ocr[1] = ts[i]->ocr[2] = 0;
        ts[i]->icr = 0; ts[i]->down = false; ts[i]->last = cyc; ts[i]->temp = 0;
        ts[i]->oc_a = false;
    }
    memset(&t4, 0, sizeof t4);
    t4.ocr_c = 0xFF;
    t4.last = cyc;
    oled_reset();
    memset(oled.ram, 0, sizeof oled.ram);
    snd_level = 0;
    snd_n = 0;
    irq_update();
    events_update();
}

void ard_buttons(uint8_t pressed)
{
    buttons = pressed;
    pin_keep[PINF - PINB] = (uint8_t)~((pressed & ARD_UP ? 0x80 : 0) | (pressed & ARD_RIGHT ? 0x40 : 0) |
                                       (pressed & ARD_LEFT ? 0x20 : 0) | (pressed & ARD_DOWN ? 0x10 : 0));
    pin_keep[PINE - PINB] = (uint8_t)~(pressed & ARD_A ? 0x40 : 0);
    pin_keep[PINB - PINB] = (uint8_t)~(pressed & ARD_B ? 0x10 : 0);
}

const uint8_t *ard_screen(void)
{
    static uint8_t out[8][128];
    if (!oled.on)
        return NULL;
    bool hflip = !oled.seg_remap, vflip = !oled.com_remap;
    if (!hflip && !vflip && !oled.start)
        return &oled.ram[0][0];
    memset(out, 0, sizeof out);
    for (int y = 0; y < 64; y++) {
        int ry = (y + oled.start) & 63;
        int sy = vflip ? 63 - y : y;
        for (int x = 0; x < 128; x++) {
            if (oled.ram[ry >> 3][x] >> (ry & 7) & 1) {
                int sx = hflip ? 127 - x : x;
                out[sy >> 3][sx] |= (uint8_t)(1 << (sy & 7));
            }
        }
    }
    return &out[0][0];
}

bool ard_invert(void) { return oled.invert; }
uint8_t ard_contrast(void) { return oled.contrast; }

uint8_t ard_led(void)
{
    /* the LED's cathodes: on when the pin drives low */
    uint8_t b = io[PORTB], d = io[DDRB], v = 0;
    if ((d & 0x40) && !(b & 0x40)) v |= 1;
    if ((d & 0x80) && !(b & 0x80)) v |= 2;
    if ((d & 0x20) && !(b & 0x20)) v |= 4;
    return v;
}

uint8_t *ard_eeprom(void) { return eeprom; }

bool ard_eeprom_dirty(void)
{
    bool d = eeprom_dirty;
    eeprom_dirty = false;
    return d;
}

uint32_t ard_idle_cycles(void)
{
    uint32_t v = idle_cycles;
    idle_cycles = 0;
    return v;
}
