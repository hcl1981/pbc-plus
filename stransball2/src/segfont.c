/* segfont.c -- Sechzehn-Segment-Schrift, zeilenweise gezeichnet. */

#include "segfont.h"
#include "color565.h"

/* Segmentbits.  Namen: T/M/B = oben/mitte/unten, L/R = linke/rechte Haelfte,
 * U/L vorne = obere/untere Haelfte bei den Senkrechten, M = Mittelsaeule,
 * D... = die vier Schraegen von den Ecken zur Mitte. */
#define S_TL  (1u <<  0)
#define S_TR  (1u <<  1)
#define S_ML  (1u <<  2)
#define S_MR  (1u <<  3)
#define S_BL  (1u <<  4)
#define S_BR  (1u <<  5)
#define S_UL  (1u <<  6)
#define S_LL  (1u <<  7)
#define S_UR  (1u <<  8)
#define S_LR  (1u <<  9)
#define S_UM  (1u << 10)
#define S_LM  (1u << 11)
#define S_DUL (1u << 12)
#define S_DUR (1u << 13)
#define S_DLL (1u << 14)
#define S_DLR (1u << 15)
#define S_DOTL (1u << 16)
#define S_DOTM (1u << 17)

#define FIRST 0x20
#define LAST  0x5A

/* V und Y sind auf einer Segmentanzeige nicht zu unterscheiden -- beide laufen
 * in der Mittelsaeule zusammen.  Das ist bei echten Anzeigen genauso. */
static const uint32_t seg[LAST - FIRST + 1] = {
    /* space */ 0,
    /* !  */ S_UM | S_DOTL,
    /* "  */ S_UM,
    /* #  */ S_ML | S_MR | S_UM | S_LM,
    /* $  */ S_TL | S_TR | S_UL | S_ML | S_MR | S_LR | S_BL | S_BR | S_UM | S_LM,
    /* %  */ S_UL | S_LR | S_DUR | S_DLL,
    /* &  */ S_TL | S_UL | S_ML | S_LL | S_BL | S_BR | S_DLR,
    /* '  */ S_UM,
    /* (  */ S_TL | S_UL | S_LL | S_BL,
    /* )  */ S_TR | S_UR | S_LR | S_BR,
    /* *  */ S_ML | S_MR | S_UM | S_LM | S_DUL | S_DUR | S_DLL | S_DLR,
    /* +  */ S_ML | S_MR | S_UM | S_LM,
    /* ,  */ S_DOTL,
    /* -  */ S_ML | S_MR,
    /* .  */ S_DOTL,
    /* /  */ S_DUR | S_DLL,
    /* 0  */ S_TL | S_TR | S_UL | S_UR | S_LL | S_LR | S_BL | S_BR,
    /* 1  */ S_UR | S_LR,
    /* 2  */ S_TL | S_TR | S_UR | S_ML | S_MR | S_LL | S_BL | S_BR,
    /* 3  */ S_TL | S_TR | S_UR | S_MR | S_LR | S_BL | S_BR,
    /* 4  */ S_UL | S_UR | S_ML | S_MR | S_LR,
    /* 5  */ S_TL | S_TR | S_UL | S_ML | S_MR | S_LR | S_BL | S_BR,
    /* 6  */ S_TL | S_TR | S_UL | S_ML | S_MR | S_LL | S_LR | S_BL | S_BR,
    /* 7  */ S_TL | S_TR | S_UR | S_LR,
    /* 8  */ S_TL | S_TR | S_UL | S_UR | S_ML | S_MR | S_LL | S_LR | S_BL | S_BR,
    /* 9  */ S_TL | S_TR | S_UL | S_UR | S_ML | S_MR | S_LR | S_BL | S_BR,
    /* :  */ S_DOTL | S_DOTM,
    /* ;  */ S_DOTL | S_DOTM,
    /* <  */ S_DUR | S_DLR,
    /* =  */ S_ML | S_MR | S_BL | S_BR,
    /* >  */ S_DUL | S_DLL,
    /* ?  */ S_TL | S_TR | S_UR | S_MR | S_LM | S_DOTL,
    /* @  */ S_TL | S_TR | S_UL | S_LL | S_BL | S_BR | S_UR | S_MR | S_LM,
    /* A  */ S_TL | S_TR | S_UL | S_UR | S_ML | S_MR | S_LL | S_LR,
    /* B  */ S_TL | S_TR | S_UR | S_LR | S_MR | S_BL | S_BR | S_UM | S_LM,
    /* C  */ S_TL | S_TR | S_UL | S_LL | S_BL | S_BR,
    /* D  */ S_TL | S_TR | S_UR | S_LR | S_BL | S_BR | S_UM | S_LM,
    /* E  */ S_TL | S_TR | S_UL | S_LL | S_ML | S_BL | S_BR,
    /* F  */ S_TL | S_TR | S_UL | S_LL | S_ML,
    /* G  */ S_TL | S_TR | S_UL | S_LL | S_LR | S_MR | S_BL | S_BR,
    /* H  */ S_UL | S_LL | S_UR | S_LR | S_ML | S_MR,
    /* I  */ S_TL | S_TR | S_BL | S_BR | S_UM | S_LM,
    /* J  */ S_TR | S_UR | S_LR | S_LL | S_BL | S_BR,
    /* K  */ S_UL | S_LL | S_ML | S_DUR | S_DLR,
    /* L  */ S_UL | S_LL | S_BL | S_BR,
    /* M  */ S_UL | S_LL | S_UR | S_LR | S_DUL | S_DUR,
    /* N  */ S_UL | S_LL | S_UR | S_LR | S_DUL | S_DLR,
    /* O  */ S_TL | S_TR | S_UL | S_UR | S_LL | S_LR | S_BL | S_BR,
    /* P  */ S_TL | S_TR | S_UL | S_LL | S_UR | S_ML | S_MR,
    /* Q  */ S_TL | S_TR | S_UL | S_UR | S_LL | S_LR | S_BL | S_BR | S_DLR,
    /* R  */ S_TL | S_TR | S_UL | S_LL | S_UR | S_ML | S_MR | S_DLR,
    /* S  */ S_TL | S_TR | S_UL | S_ML | S_MR | S_LR | S_BL | S_BR,
    /* T  */ S_TL | S_TR | S_UM | S_LM,
    /* U  */ S_UL | S_LL | S_UR | S_LR | S_BL | S_BR,
    /* V  */ S_UL | S_UR | S_LM,
    /* W  */ S_UL | S_LL | S_UR | S_LR | S_DLL | S_DLR,
    /* X  */ S_DUL | S_DUR | S_DLL | S_DLR,
    /* Y  */ S_UL | S_UR | S_LM,
    /* Z  */ S_TL | S_TR | S_DUR | S_DLL | S_BL | S_BR,
};

const segfont_t segfont_s  = { 10, 14,  2, 13 };
const segfont_t segfont_m  = { 16, 22,  3, 20 };
const segfont_t segfont_l  = { 22, 30,  4, 27 };
const segfont_t segfont_xl = { 34, 46,  6, 41 };

static uint32_t mask_of(unsigned ch)
{
    if (ch >= 'a' && ch <= 'z')
        ch -= 32;
    if (ch < FIRST || ch > LAST)
        ch = '?';
    return seg[ch - FIRST];
}

int segfont_text_w(const segfont_t *f, const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n ? n * f->adv - (f->adv - f->w) : 0;
}

/* --- Fuellen mit gebrochenen Kanten (1/16 Bildpunkt) --------------------- */
#define SUB 16

static void span(uint16_t *row, int row_w, int xa, int xb, uint16_t col)
{
    int ia, ib, x;
    if (xb <= xa) return;
    ia = xa >> 4;
    ib = (xb - 1) >> 4;
    if (ia == ib) {
        if (ia >= 0 && ia < row_w)
            row[ia] = pbc_blend565(row[ia], col, (unsigned)((xb - xa) * 255 / SUB));
        return;
    }
    if (ia >= 0 && ia < row_w) {
        unsigned a = (unsigned)(((ia + 1) * SUB - xa) * 255 / SUB);
        row[ia] = pbc_blend565(row[ia], col, a);
    }
    for (x = ia + 1; x < ib; x++)
        if (x >= 0 && x < row_w)
            row[x] = col;
    if (ib >= 0 && ib < row_w && ib != ia) {
        unsigned a = (unsigned)((xb - ib * SUB) * 255 / SUB);
        row[ib] = pbc_blend565(row[ib], col, a > 255 ? 255 : a);
    }
}

/* Waagerechter Balken: Zeilenband [ys, ys+t) */
static void hbar(uint16_t *row, int row_w, int py, int ox, int ys, int t,
                 int xa, int xb, uint16_t col)
{
    if (py < ys || py >= ys + t) return;
    span(row, row_w, (ox + xa) * SUB, (ox + xb) * SUB, col);
}

/* Senkrechter Balken bei Spalte xs, Zeilen [ya, yb) */
static void vbar(uint16_t *row, int row_w, int py, int ox, int xs, int t,
                 int ya, int yb, uint16_t col)
{
    if (py < ya || py >= yb) return;
    span(row, row_w, (ox + xs) * SUB, (ox + xs + t) * SUB, col);
}

/* Schraege von (ax,ay) nach (bx,by), Strichstaerke t -- mit weichen Kanten */
static void diag(uint16_t *row, int row_w, int py, int ox,
                 int ax, int ay, int bx, int by, int t, uint16_t col)
{
    int lo = ay < by ? ay : by, hi = ay < by ? by : ay;
    int cx;
    if (py < lo || py >= hi || ay == by) return;
    cx = ax * SUB + (bx - ax) * SUB * (py - ay) / (by - ay);
    span(row, row_w, ox * SUB + cx - t * SUB / 2, ox * SUB + cx + t * SUB / 2, col);
}

void segfont_row(uint16_t *row, int row_w, int py, int x, int y,
                 const segfont_t *f, const char *s, uint16_t col)
{
    int w = f->w, h = f->h, t = f->t;
    int xm = w / 2, ym = h / 2, ht = t / 2;
    int gy = py - y;

    if (gy < 0 || gy >= h)
        return;

    for (; *s; s++, x += f->adv) {
        uint32_t m = mask_of((unsigned char)*s);
        if (!m)
            continue;

        if (m & S_TL) hbar(row, row_w, gy, x, 0,        t, 0,      xm + ht, col);
        if (m & S_TR) hbar(row, row_w, gy, x, 0,        t, xm - ht, w,      col);
        if (m & S_ML) hbar(row, row_w, gy, x, ym - ht,  t, 0,      xm + ht, col);
        if (m & S_MR) hbar(row, row_w, gy, x, ym - ht,  t, xm - ht, w,      col);
        if (m & S_BL) hbar(row, row_w, gy, x, h - t,    t, 0,      xm + ht, col);
        if (m & S_BR) hbar(row, row_w, gy, x, h - t,    t, xm - ht, w,      col);

        if (m & S_UL) vbar(row, row_w, gy, x, 0,       t, 0,  ym + ht, col);
        if (m & S_LL) vbar(row, row_w, gy, x, 0,       t, ym - ht, h, col);
        if (m & S_UR) vbar(row, row_w, gy, x, w - t,   t, 0,  ym + ht, col);
        if (m & S_LR) vbar(row, row_w, gy, x, w - t,   t, ym - ht, h, col);
        if (m & S_UM) vbar(row, row_w, gy, x, xm - ht, t, 0,  ym + ht, col);
        if (m & S_LM) vbar(row, row_w, gy, x, xm - ht, t, ym - ht, h, col);

        if (m & S_DUL) diag(row, row_w, gy, x, ht,     ht,     xm, ym, t, col);
        if (m & S_DUR) diag(row, row_w, gy, x, w - ht, ht,     xm, ym, t, col);
        if (m & S_DLL) diag(row, row_w, gy, x, ht,     h - ht, xm, ym, t, col);
        if (m & S_DLR) diag(row, row_w, gy, x, w - ht, h - ht, xm, ym, t, col);

        if (m & S_DOTL) hbar(row, row_w, gy, x, h - t, t, xm - ht, xm + ht + (t & 1), col);
        if (m & S_DOTM) hbar(row, row_w, gy, x, ym - ht, t, xm - ht, xm + ht + (t & 1), col);
    }
}
