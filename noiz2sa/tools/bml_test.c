/* bml_test.c -- prueft Uebersetzer und Ausfuehrung ohne Geraet.
 *
 * Jedes der 73 Muster wird als Gegner gestartet und einige tausend Ticks lang
 * laufen gelassen.  Erzeugte Geschosse bekommen selbst Laeufer, so wie im
 * Spiel.  Mit -fsanitize=address,undefined faellt jeder Fehlgriff sofort auf.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bml.h"

#define MAXB 4096

typedef struct {
    int      used;
    int      runner;
    bml_state_t st;
    int      life;
} bullet_t;

static bullet_t bl[MAXB];
static int bullets_made, bullets_live, peak_live, peak_runner;
static uint32_t rng = 12345;

uint32_t bml_random(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

void bml_fire_bullet(void *owner, int32_t dir, int32_t spd,
                     uint16_t action, const int32_t *param, uint8_t pn)
{
    int i;
    (void)owner;
    bullets_made++;
    for (i = 0; i < MAXB; i++) {
        if (bl[i].used) continue;
        memset(&bl[i], 0, sizeof bl[i]);
        bl[i].used = 1;
        bl[i].st.dir = dir;
        bl[i].st.spd = spd;
        bl[i].st.rank = BML_ONE / 2;
        bl[i].st.aim = 180 << BML_FP;
        bl[i].life = 600;
        bl[i].runner = (action != BML_NONE) ? bml_runner_alloc(action, param, pn) : -1;
        bullets_live++;
        if (bullets_live > peak_live) peak_live = bullets_live;
        return;
    }
}

static void free_bullet(int i)
{
    if (bl[i].runner >= 0) bml_runner_free(bl[i].runner);
    bl[i].used = 0;
    bullets_live--;
}

int main(int argc, char **argv)
{
    int ticks = (argc > 1) ? atoi(argv[1]) : 2400;
    int b, i, t, bad = 0;
    long total_made = 0;

    printf("%-30s %7s %7s %7s\n", "Muster", "Schuss", "max.leb", "max.Lauf");
    for (b = 0; b < bml_brg_num; b++) {
        int rn[8], nrn = 0;
        int32_t rank = BML_ONE * 7 / 10;
        bml_state_t foe;

        bml_runner_init();
        memset(bl, 0, sizeof bl);
        bullets_made = bullets_live = peak_live = peak_runner = 0;

        memset(&foe, 0, sizeof foe);
        foe.rank = rank;
        foe.dir = 180 << BML_FP;           /* Gegner zeigt nach unten */
        foe.aim = 180 << BML_FP;
        foe.spd = 0;

        for (i = 0; i < bml_brg[b].tn && nrn < 8; i++) {
            int r = bml_runner_alloc(bml_top[bml_brg[b].toff + i], NULL, 0);
            if (r >= 0) rn[nrn++] = r;
        }
        if (nrn == 0) { printf("%-30s KEIN LAEUFER\n", bml_brg[b].name); bad++; continue; }

        for (t = 0; t < ticks; t++) {
            /* Gegner */
            for (i = 0; i < nrn; i++) {
                if (bml_runner_done(rn[i])) continue;
                foe.vanished = 0;
                bml_runner_step(rn[i], &foe, NULL);
                if (foe.vanished) { bml_runner_free(rn[i]); rn[i] = -1; }
            }
            /* Geschosse */
            for (i = 0; i < MAXB; i++) {
                if (!bl[i].used) continue;
                if (bl[i].runner >= 0 && !bml_runner_done(bl[i].runner)) {
                    bl[i].st.vanished = 0;
                    bml_runner_step(bl[i].runner, &bl[i].st, NULL);
                    if (bl[i].st.vanished) { free_bullet(i); continue; }
                }
                if (--bl[i].life <= 0) free_bullet(i);
            }
            if (bml_runner_used() > peak_runner) peak_runner = bml_runner_used();
        }
        total_made += bullets_made;
        printf("%-30s %7d %7d %7d%s\n", bml_brg[b].name, bullets_made,
               peak_live, peak_runner, bullets_made == 0 ? "   <-- FEUERT NICHT" : "");
        if (bullets_made == 0) bad++;
    }
    printf("\n%ld Schuesse insgesamt, %d Muster ohne Wirkung\n", total_made, bad);
    return bad ? 1 : 0;
}
