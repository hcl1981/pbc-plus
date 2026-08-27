/* game.c -- Spout, Spielkern.
 *
 * Portiert aus spout-1.4 (Nick White, nach dem Original von Kuni), MIT-Lizenz,
 * siehe COPYING.  Die Physik (Schwerkraft, Schub, Kornbewegung, Aufschlag,
 * Hoehlenerzeugung, Punkte- und Zeitregeln) ist Zug um Zug uebernommen; die
 * Anpassungen fuer den PicoBoy Color Plus stehen in doc/PORT.md.
 */

#include <string.h>
#include "game.h"
#include "sintable.h"

#define ABS(v) ((v) < 0 ? -(v) : (v))

/* ---- Speicher ----------------------------------------------------------- */
static uint8_t  cells[RING_CELLS];         /* 32 KB Zellring                */
static uint16_t v2g[RING_CELLS];           /* Zelle -> Kornindex, 0xffff=frei */

typedef struct GRAIN {
    struct GRAIN *next, *prev;
    int16_t  sx, sy;                       /* Subzellenposition, 1/256 Zelle */
    int16_t  vx, vy;
    uint16_t pos;                          /* Ringindex                      */
    uint8_t  color;
} GRAIN;

static GRAIN grain[MAX_GRAIN];
static GRAIN *grain_use, *grain_free;

spout_state_t spout;

/* Spielinterne Groessen (im Original global) */
static int32_t mx, my, mvx, mvy;           /* Position/Tempo, 1/256 Zelle   */
static int     mR;                         /* Drehwinkel 0..1023            */
static int     upper_line, roll_count, since_start;
static int     title_slot;
static uint32_t rng;

/* ---- Zufall: xorshift32, damit Host und Geraet identisch laufen --------- */
uint32_t spout_rand(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

void spout_reseed(uint32_t seed)
{
    rng ^= seed ? seed : 1u;
    (void)spout_rand();
}

/* ---- Kornverwaltung (freie/benutzte Liste wie upstream) ----------------- */
static void grain_init(void)
{
    int i;
    for (i = 0; i < MAX_GRAIN - 1; i++)
        grain[i].next = &grain[i + 1];
    grain[MAX_GRAIN - 1].next = NULL;
    grain_free = grain;
    grain_use  = NULL;
}

static GRAIN *grain_alloc(void)
{
    GRAIN *c = grain_free;
    if (c) {
        grain_free = c->next;
        c->next = grain_use;
        c->prev = NULL;
        if (c->next)
            c->next->prev = c;
        grain_use = c;
    }
    return c;
}

static GRAIN *grain_release(GRAIN *c)
{
    GRAIN *next = c->next;
    if (next)
        next->prev = c->prev;
    if (c->prev)
        c->prev->next = next;
    else
        grain_use = next;
    c->next = grain_free;
    grain_free = c;
    return next;
}

static inline uint16_t grain_index(const GRAIN *g) { return (uint16_t)(g - grain); }

/* ---- Korn erzeugen ------------------------------------------------------ */
static void emit(int t, int x, int y)
{
    t &= RING_MASK;
    if (cells[t] != 0)
        return;
    if (spout.n_grain >= MAX_GRAIN)
        return;
    {
        GRAIN *g = grain_alloc();
        if (!g)
            return;
        g->vx = (int16_t)x;
        g->vy = (int16_t)y;
        g->sx = 0;
        g->sy = 0;
        /* Farbe 2 oder 3, Kornbit, drei Aufschlaege Lebensdauer */
        g->color = (uint8_t)((2 + (spout_rand() & 1)) + CELL_GRAIN + 0xc0);
        g->pos = (uint16_t)t;
        cells[t] = g->color;
        v2g[t] = grain_index(g);
        spout.n_grain++;
    }
}

/* ---- unterste Zeile mit c1, die darunter mit c2 fuellen ----------------- */
static void sweep(uint8_t c1, uint8_t c2)
{
    int base = ((upper_line + VIEW_H - 1) & ROW_MASK) * CELL_W + VIEW_X0;
    int i;

    for (i = 0; i < VIEW_W; i++) {
        int p = (base + i) & RING_MASK;
        if (cells[p] & CELL_GRAIN) {
            uint16_t gi = v2g[p];
            if (gi != 0xffffu) {
                grain_release(&grain[gi]);
                v2g[p] = 0xffffu;
                spout.n_grain--;
            }
        }
        cells[p] = c1;
    }

    base = (base + VIEW_W + (CELL_W - VIEW_W)) & RING_MASK;   /* naechste Zeile */
    for (i = 0; i < VIEW_W; i++)
        cells[(base + i) & RING_MASK] = c2;
}

/* ---- Overlaypunkt ablegen ---------------------------------------------- */
static void ovl_put(int x, int y, uint8_t c)
{
    if (x < VIEW_X0 || x >= VIEW_X0 + VIEW_W || y < 0 || y >= VIEW_H)
        return;
    if (spout.n_ovl >= SPOUT_MAX_OVL)
        return;
    spout.ovl[spout.n_ovl].x = (uint8_t)(x - VIEW_X0);
    spout.ovl[spout.n_ovl].y = (uint8_t)y;
    spout.ovl[spout.n_ovl].c = c;
    spout.n_ovl++;
}

/* Zielhilfe: Zellkoordinaten in 1/256 -> Bildpunkte in 1/64, bezogen auf den
 * sichtbaren Ausschnitt.  Ein Punkt deckt ein Quadrat von zwei Bildpunkten ab,
 * genau wie vorher eine Zelle -- nur eben nicht mehr aufs Raster gerundet. */
static void ovl_dot(int32_t x256, int32_t y256)
{
    int32_t fx = (x256 - (int32_t)VIEW_X0 * 256) / 2;
    int32_t fy = y256 / 2;

    if (spout.n_dot >= SPOUT_MAX_DOT)
        return;
    if (fx < -64 || fx > (int32_t)(VIEW_W * 2) * 64 + 64)
        return;
    if (fy < -64 || fy > (int32_t)(VIEW_H * 2) * 64 + 64)
        return;
    spout.dot[spout.n_dot].x = (uint16_t)(fx < 0 ? 0 : fx);
    spout.dot[spout.n_dot].y = (uint16_t)(fy < 0 ? 0 : fy);
    spout.n_dot++;
}

/* ---- Titelbild: einen Textblock in den Ring schreiben ------------------- */
#define TITLE_X 7
#define TITLE_Y 95

static void push_banner(int slot, int row)
{
    static int next;
    spout_banner_t *b = &spout.banner[next & (SPOUT_MAX_BANNER - 1)];
    next++;
    b->row  = (int16_t)(row & ROW_MASK);
    b->slot = (uint8_t)slot;
    b->used = 1;
}

/* ---- Start / Reset ------------------------------------------------------ */
void spout_init(uint32_t seed, const int hiscore[2])
{
    memset(&spout, 0, sizeof spout);
    spout.cells = cells;
    rng = seed ? seed : 0x12345678u;
    if (hiscore) {
        spout.hiscore[0] = hiscore[0];
        spout.hiscore[1] = hiscore[1];
    }
    spout.phase = PH_TITLE_INIT;
    spout.timeleft = 60 * FRAMERATE;
    spout.height = 0;
}

static void phase_reset(void)
{
    int i;

    if (spout.phase == PH_TITLE_INIT) {
        /* Bestwert festhalten, bevor der Titel neu aufgebaut wird */
        if (spout.score > spout.hiscore[0] ||
            (spout.score == spout.hiscore[0] && spout.height > spout.hiscore[1])) {
            spout.hiscore[0] = spout.score;
            spout.hiscore[1] = spout.height;
        }
    } else {
        spout.score = 0;
        spout.dispscore = 0;
        spout.height = -(SCROLL_Y + 18);
        spout.timeleft = 60 * FRAMERATE;
    }

    for (i = 0; i < RING_CELLS; i++)
        v2g[i] = 0xffffu;
    grain_init();
    spout.n_grain = 0;

    if (spout.phase & 2) {
        memset(cells, CELL_ROCK, RING_CELLS);
        memset(cells, 0, (size_t)CELL_W * VIEW_H);
        memset(cells + (size_t)CELL_W * (RING_H - 32), 0, (size_t)CELL_W * 32);
    } else {
        memset(cells, 0, RING_CELLS);
    }

    /* Randspalten: unzerstoerbar, damit Koerner dort abprallen */
    for (i = 0; i < RING_H; i++) {
        uint8_t *p = cells + i * CELL_W;
        int j;
        for (j = 0; j < VIEW_X0; j++) {
            p[j] = CELL_BORDER;
            p[CELL_W - 1 - j] = CELL_BORDER;
        }
    }

    since_start = 0;

    mx = 40 * 256;
    my = 0;
    mvx = 0;
    mvy = 0;
    mR = 256 + (spout.phase & 2) * 224;

    upper_line = 0;
    spout.disp_pos = 0;
    spout.gameover = 0;
    roll_count = 0;
    spout.msg[0] = 0;

    memset(spout.banner, 0, sizeof spout.banner);
    spout.msg2[0] = 0;

    if (!(spout.phase & 2)) {
        /* Titel: ersten Block sofort sichtbar setzen (upstream laesst hier
         * knapp vier Sekunden lang ein leeres Bild stehen) */
        push_banner(0, (upper_line - 24) & ROW_MASK);
        upper_line = (upper_line - 24 - 10) & ROW_MASK;
        spout.disp_pos = upper_line;
        title_slot = 0;
    }

    spout.phase++;
}

/* ---- ein Spielschritt --------------------------------------------------- */
void spout_tick(uint16_t pad)
{
    static int flame_phase;
    static int over_frames;
    int i;

    spout.n_ovl = 0;
    spout.n_dot = 0;
    spout.audio.thrust = 0;
    spout.audio.hits = 0;
    spout.audio.breaks = 0;
    spout.audio.ev = 0;

    /* --- Pause ---------------------------------------------------------- */
    if (spout.phase == PH_PAUSE) {
        if (pad & (TRG_A | TRG_B)) {       /* beide Tasten setzen fort */
            spout.phase = PH_GAME;
            spout.msg[0] = 0;
            spout.msg2[0] = 0;
            spout.audio.ev |= SPOUT_EV_UI;
        }
        return;                            /* Bild bleibt unveraendert stehen */
    }

    /* --- Phasenwechsel -------------------------------------------------- */
    if (!(spout.phase & 1))
        phase_reset();

    /* Pause liegt auf A.  Die Sperre der ersten Bilder verhindert, dass ein
     * noch gehaltenes A vom Startbild direkt wieder in die Pause faellt. */
    if (since_start < 1000)
        since_start++;
    if ((pad & TRG_A) && spout.phase == PH_GAME && spout.gameover == 0 &&
        since_start > 6) {
        strcpy(spout.msg, "PAUSE");
        strcpy(spout.msg2, "A OR B = RESUME");
        spout.phase = PH_PAUSE;
        spout.audio.ev |= SPOUT_EV_UI;
        return;
    }

    /* --- Schiff --------------------------------------------------------- */
    if (spout.phase & 2) {
        if (spout.gameover == 0) {
            if (pad & PAD_RI)
                mR = (mR - 16) & 1023;
            else if (pad & PAD_LF)
                mR = (mR + 16) & 1023;

            if (pad & PAD_B) {             /* Schub liegt auf B */
                mvx -= sintable[(256 + mR) & 1023] / 128;
                mvy += sintable[mR] / 128;
                spout.audio.thrust = 1;
            }
            mvy += 8;                      /* Schwerkraft */

            if (mvx < -256 * 4) mvx = -256 * 4;
            else if (mvx > 256 * 4) mvx = 256 * 4;
            if (mvy < -256 * 4) mvy = -256 * 4;
            else if (mvy > 256 * 4) mvy = 256 * 4;

            mx += mvx / 16;
            my += mvy / 16;

            /* Seitenwaende: im Original 2/125, hier die tatsaechlich
             * sichtbaren Randspalten, damit das Schiff nicht ausserhalb des
             * Bildes stirbt */
            if (mx >= (VIEW_X0 + VIEW_W - 1) * 256) {
                mx = (VIEW_X0 + VIEW_W - 2) * 256;
                spout.gameover = 1;
            } else if (mx <= VIEW_X0 * 256) {
                mx = (VIEW_X0 + 1) * 256;
                spout.gameover = 1;
            }
            if (my >= VIEW_H * 256) {
                my = (VIEW_H - 1) * 256;
                spout.gameover = 1;
            }

            if (my < SCROLL_Y * 256) {
                /* Welt mitscrollen: eine Zeile Hoehe */
                my += 256;
                upper_line = (upper_line - 1) & ROW_MASK;
                spout.height++;

                if (spout.height > 0) {
                    spout.score++;
                    if ((spout.height & 127) == 0) {
                        spout.score += (spout.timeleft + FRAMERATE - 1) / FRAMERATE * 10;
                        spout.timeleft += 60 * FRAMERATE;
                        if (spout.timeleft > 99 * FRAMERATE)
                            spout.timeleft = 99 * FRAMERATE;
                        spout.audio.ev |= SPOUT_EV_BONUS;
                    }
                }

                /* Sperrmauer so einsetzen, dass sie das Schiff genau beim
                 * naechsten Zeitbonus erreicht (upstream: upperLine == 111) */
                if (spout.height > 0 && ((spout.height + SCROLL_Y + 1) & 127) == 0) {
                    int r;
                    for (r = 1; r <= 3; r++) {
                        int row = (upper_line - r) & ROW_MASK;
                        uint8_t v = (r == 2) ? CELL_BAR : CELL_EMPTY;
                        int j;
                        for (j = 0; j < VIEW_W; j++)
                            cells[row * CELL_W + VIEW_X0 + j] = v;
                    }
                    spout.audio.ev |= SPOUT_EV_BARRIER;
                }

                /* Hoehle graben: Kaesten werden mit der Hoehe kleiner */
                {
                    int bw = 20 - (spout.height + 40) / 64;
                    int bh = bw;
                    int w, h, x1, row, y;
                    if (bw < 4) bw = 4;
                    if (bh < 4) bh = 4;
                    w = 4 + (int)(spout_rand() % (unsigned)bw);
                    h = 4 + (int)(spout_rand() % (unsigned)bh);
                    x1 = VIEW_X0 + (int)(spout_rand() % (unsigned)(VIEW_W - w));
                    row = (upper_line - 20 - (int)(spout_rand() & 7)) & ROW_MASK;
                    for (y = 0; y < h; y++) {
                        uint8_t *p = cells + row * CELL_W;
                        int j;
                        for (j = 0; j < w; j++)
                            p[x1 + j] = CELL_EMPTY;
                        row = (row - 1) & ROW_MASK;
                    }
                }

                sweep(CELL_FLOOR, CELL_ROCK);
            }
        }
    } else {
        /* --- Titelbild ---------------------------------------------------- */
        mx = TITLE_X * 256;
        my = TITLE_Y * 256;
        mR = 0;

        if ((roll_count & 7) == 0) {
            if ((upper_line & 63) == 0) {
                title_slot = (upper_line >> 6) & 3;
                push_banner(title_slot, (upper_line - 24) & ROW_MASK);
            }
            upper_line = (upper_line - 1) & ROW_MASK;
            sweep(CELL_FLOOR, CELL_EMPTY);
        }
    }

    roll_count++;

    /* --- Koerner ausstossen --------------------------------------------- */
    {
        static int gx[5] = { -2, 2, -1, 1, 0 };
        int r = (int)(spout_rand() & 3);
        int t = gx[r]; gx[r] = gx[r + 1]; gx[r + 1] = (int)t;

        if (spout.phase & 2) {
            if (spout.gameover == 0 && (pad & PAD_B)) {
                for (i = 0; i < 5; i++) {
                    int p = (int)(mx / 256) + gx[i] +
                            (((int)(my / 256) - 1 + ABS(gx[i]) + spout.disp_pos) & ROW_MASK) * CELL_W;
                    emit(p, (int)(mvx / 16) + sintable[(256 + mR) & 1023] / 8,
                            (int)(mvy / 16) - sintable[mR] / 8);
                }
            }
        } else {
            for (i = -1; i <= 2; i++) {
                int p = TITLE_X + i + (((TITLE_Y - 1) + spout.disp_pos) & ROW_MASK) * CELL_W;
                emit(p, 512, -384);
            }
        }
    }

    /* --- Koerner bewegen -------------------------------------------------- */
    {
        GRAIN *g = grain_use;
        while (g) {
            int dead = 0;
            uint8_t *c;

            g->vy += 8;                    /* Schwerkraft */
            g->sx = (int16_t)(g->sx + g->vx);
            g->sy = (int16_t)(g->sy + g->vy);

            cells[g->pos] = 0;
            v2g[g->pos] = 0xffffu;

            /* --- senkrecht --- */
            if (g->sy >= 256) {
                do {
                    g->sy -= 256;
                    g->pos = (uint16_t)((g->pos + CELL_W) & RING_MASK);
                    c = cells + g->pos;
                    if (*c) {
                        if (*c & CELL_GRAIN) {
                            uint16_t gi = v2g[g->pos];
                            if (gi != 0xffffu) {
                                GRAIN *o = &grain[gi];
                                int rr = 31 - (int)(spout_rand() & 63);
                                int16_t tx = g->vx, ty = g->vy;
                                g->vx = o->vx; g->vy = o->vy;
                                o->vx = tx;    o->vy = ty;
                                g->vx = (int16_t)(g->vx + rr);
                                o->vx = (int16_t)(o->vx - rr);
                            }
                        } else {
                            g->vy = (int16_t)(-g->vy / 2);
                            g->vx = (int16_t)(g->vx + 15 - (int)(spout_rand() & 31));
                            spout.audio.hits++;
                            if (*c & 0xc0) {
                                *c -= 0x40;
                                if (!(*c & 0xc0)) {
                                    *c = 0;
                                    spout.audio.breaks++;
                                }
                            }
                            if (g->color & 0xc0)
                                g->color -= 0x40;
                            else {
                                g->color = 0;
                                dead = 1;
                            }
                        }
                        g->pos = (uint16_t)((g->pos - CELL_W) & RING_MASK);
                        break;
                    }
                } while (g->sy >= 256);
            } else {
                while (g->sy <= -256) {
                    g->sy += 256;
                    g->pos = (uint16_t)((g->pos - CELL_W) & RING_MASK);
                    c = cells + g->pos;
                    if (*c) {
                        if (*c & CELL_GRAIN) {
                            uint16_t gi = v2g[g->pos];
                            if (gi != 0xffffu) {
                                GRAIN *o = &grain[gi];
                                int16_t tx = g->vx, ty = g->vy;
                                g->vx = o->vx; g->vy = o->vy;
                                o->vx = tx;    o->vy = ty;
                            }
                        } else {
                            g->vy = (int16_t)(-g->vy / 2);
                            spout.audio.hits++;
                            if (*c & 0xc0) {
                                *c -= 0x40;
                                if (!(*c & 0xc0)) {
                                    *c = 0;
                                    spout.audio.breaks++;
                                }
                            }
                            if (g->color & 0xc0)
                                g->color -= 0x40;
                            else {
                                g->color = 0;
                                dead = 1;
                            }
                        }
                        g->pos = (uint16_t)((g->pos + CELL_W) & RING_MASK);
                        break;
                    }
                }
            }

            /* --- waagerecht --- */
            if (g->sx >= 256) {
                do {
                    g->sx -= 256;
                    g->pos = (uint16_t)((g->pos + 1) & RING_MASK);
                    c = cells + g->pos;
                    if (*c) {
                        if (*c & CELL_GRAIN) {
                            uint16_t gi = v2g[g->pos];
                            if (gi != 0xffffu) {
                                GRAIN *o = &grain[gi];
                                int16_t tx = g->vx, ty = g->vy;
                                g->vx = o->vx; g->vy = o->vy;
                                o->vx = tx;    o->vy = ty;
                            }
                        } else {
                            g->vx = (int16_t)(-g->vx / 2);
                            spout.audio.hits++;
                            if (*c & 0xc0) {
                                *c -= 0x40;
                                if (!(*c & 0xc0)) {
                                    *c = 0;
                                    spout.audio.breaks++;
                                }
                            }
                            if (g->color & 0xc0)
                                g->color -= 0x40;
                            else {
                                g->color = 0;
                                dead = 1;
                            }
                        }
                        g->pos = (uint16_t)((g->pos - 1) & RING_MASK);
                        break;
                    }
                } while (g->sx >= 256);
            } else {
                while (g->sx <= -256) {
                    g->sx += 256;
                    g->pos = (uint16_t)((g->pos - 1) & RING_MASK);
                    c = cells + g->pos;
                    if (*c) {
                        if (*c & CELL_GRAIN) {
                            uint16_t gi = v2g[g->pos];
                            if (gi != 0xffffu) {
                                GRAIN *o = &grain[gi];
                                int16_t tx = g->vx, ty = g->vy;
                                g->vx = o->vx; g->vy = o->vy;
                                o->vx = tx;    o->vy = ty;
                            }
                        } else {
                            g->vx = (int16_t)(-g->vx / 2);
                            spout.audio.hits++;
                            if (*c & 0xc0) {
                                *c -= 0x40;
                                if (!(*c & 0xc0)) {
                                    *c = 0;
                                    spout.audio.breaks++;
                                }
                            }
                            if (g->color & 0xc0)
                                g->color -= 0x40;
                            else {
                                g->color = 0;
                                dead = 1;
                            }
                        }
                        g->pos = (uint16_t)((g->pos + 1) & RING_MASK);
                        break;
                    }
                }
            }

            if (dead) {
                cells[g->pos] = g->color;  /* color ist hier 0 */
                v2g[g->pos] = 0xffffu;
                spout.n_grain--;
                g = grain_release(g);
            } else {
                cells[g->pos] = g->color;
                v2g[g->pos] = grain_index(g);
                g = g->next;
            }
        }
    }

    spout.disp_pos = upper_line;
    spout.ship_x = (int)(mx / 256);
    spout.ship_y = (int)(my / 256);
    spout.ship_r = mR;

    /* --- Schiff gegen Fels pruefen -------------------------------------- */
    {
        int p = ((int)(mx / 256) + (((int)(my / 256) + spout.disp_pos) & ROW_MASK) * CELL_W) & RING_MASK;
        uint8_t v = cells[p];
        if ((spout.phase & 2) && v != 0 && (v & CELL_GRAIN) == 0 && spout.gameover == 0)
            spout.gameover = v;
    }

    /* --- Duesenstrahl und Schiff als Overlay ---------------------------- */
    {
        int sx = (int)(mx / 256), sy = (int)(my / 256);

        if (spout.gameover == 0 && (spout.phase & 2)) {
            int32_t x = mx + (int32_t)sintable[(256 + mR) & 1023] * flame_phase / 64;
            int32_t y = my - (int32_t)sintable[mR] * flame_phase / 64;
            int step;
            for (step = 0; step < 9; step++) {
                if (y >= VIEW_H * 256)
                    break;
                ovl_dot(x, y);
                {
                    int mul = ((step % 3) == 2) ? 2 : 1;
                    x += (int32_t)sintable[(256 + mR) & 1023] * mul / 16;
                    y -= (int32_t)sintable[mR] * mul / 16;
                }
            }
            flame_phase = (flame_phase + 1) & 15;
        }

        ovl_put(sx - 1, sy - 1, 0x03); ovl_put(sx, sy - 1, 0x03); ovl_put(sx + 1, sy - 1, 0x03);
        ovl_put(sx - 1, sy,     0x03); ovl_put(sx, sy,     0x00); ovl_put(sx + 1, sy,     0x03);
        ovl_put(sx - 1, sy + 1, 0x03); ovl_put(sx, sy + 1, 0x03); ovl_put(sx + 1, sy + 1, 0x03);
    }

    /* --- Phasenwechsel per Taste ---------------------------------------- */
    if (spout.phase == PH_TITLE) {
        if (pad & (TRG_A | TRG_B)) {
            spout.phase = PH_GAME_INIT;
            spout.audio.ev |= SPOUT_EV_START;
        }
    } else if (spout.gameover) {
        if (spout.msg[0] == 0) {
            strcpy(spout.msg, "GAME OVER");
            spout.audio.ev |= SPOUT_EV_OVER;
            over_frames = 0;
        }
        /* knappe Sekunde Sperre, sonst wischt ein gehaltener Schubknopf das
         * Bild sofort weg */
        if (over_frames < FRAMERATE)
            over_frames++;
        else if (pad & (TRG_A | TRG_B))
            spout.phase = PH_TITLE_INIT;
    }

    /* --- Zeit ------------------------------------------------------------ */
    if ((spout.phase & 2) && spout.timeleft && spout.gameover == 0) {
        spout.timeleft--;
        if ((spout.timeleft % FRAMERATE) == 0 && spout.timeleft <= 5 * FRAMERATE)
            spout.audio.ev |= SPOUT_EV_TICK;
        if (spout.timeleft == 0)
            spout.gameover = 1;
    }

    /* --- Punkteanzeige laeuft nach --------------------------------------- */
    if (spout.dispscore < spout.score) {
        spout.dispscore++;
        if (spout.dispscore < spout.score)
            spout.dispscore++;
    }
}

/* ---- Selbsttest der Kornbuchhaltung ------------------------------------- */
int spout_selfcheck(void)
{
    int cells_with_grain = 0, listed = 0, i;
    GRAIN *g;

    for (i = 0; i < RING_CELLS; i++) {
        if (cells[i] & CELL_GRAIN) {
            cells_with_grain++;
            if (v2g[i] == 0xffffu)
                return 1;                  /* Kornzelle ohne Ruecklink       */
            if (v2g[i] >= MAX_GRAIN)
                return 2;
            if (grain[v2g[i]].pos != (uint16_t)i)
                return 3;                  /* Ruecklink zeigt woanders hin   */
        }
    }
    for (g = grain_use; g; g = g->next) {
        listed++;
        if (listed > MAX_GRAIN)
            return 4;                      /* Zyklus in der Liste            */
        if ((cells[g->pos] & CELL_GRAIN) == 0)
            return 5;                      /* Korn ohne Zelle                */
    }
    if (listed != cells_with_grain)
        return 6;
    if (listed != spout.n_grain)
        return 7;
    return 0;
}
