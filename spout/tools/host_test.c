/* host_test.c -- laeuft auf dem Buildrechner, nicht auf dem Geraet.
 *
 * Zweck (CLAUDE.md Abschnitt 10: "Was ohne Geraet pruefbar ist, auch pruefen"):
 *   - Spielkern und Renderer viele tausend Frames laufen lassen,
 *   - nach jedem Tick die Kornbuchhaltung pruefen (spout_selfcheck),
 *   - mit -fsanitize=address,undefined jeden Zugriff ausserhalb der Puffer
 *     und jeden Ueberlauf melden,
 *   - PPM-Bilder ausgeben, damit Layout und Farben ohne Hardware sichtbar sind.
 *
 * Aufruf: host_test [frames] [seed] [--shots verz] [--fuzz] [--dark]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "render.h"
#include "color565.h"

static uint16_t fb[SCR_W * SCR_H];

static void write_ppm(const char *dir, int n, const char *tag)
{
    char path[512];
    FILE *f;
    int i;
    snprintf(path, sizeof path, "%s/frame_%05d_%s.ppm", dir, n, tag);
    f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
    for (i = 0; i < SCR_W * SCR_H; i++) {
        uint32_t c = pbc_rgb24_of(fb[i]);   /* Panelwort ist BGR565 */
        unsigned char rgb[3];
        rgb[0] = (unsigned char)((c >> 16) & 0xff);
        rgb[1] = (unsigned char)((c >> 8) & 0xff);
        rgb[2] = (unsigned char)(c & 0xff);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* Autopilot: sucht die freie Spalte ueber sich, richtet die Duese dorthin aus
 * und dosiert den Schub.  Kein guter Spieler, aber er fliegt weit genug, um
 * Sperrmauern, Zeitbonus und enger werdende Hoehlen zu erreichen. */
static int free_run(int cx, int cy)
{
    int n = 0;
    while (n < 30) {
        int row = (spout.disp_pos + ((cy - n) & ROW_MASK)) & ROW_MASK;
        if (cx < VIEW_X0 || cx >= VIEW_X0 + VIEW_W)
            break;
        { uint8_t v = spout.cells[row * CELL_W + cx];
          if (v && !(v & CELL_GRAIN)) break; }
        n++;
    }
    return n;
}

static uint16_t bot(int frame, int fuzz)
{
    static uint16_t prev;
    static int lastx = 64;
    uint16_t pad = 0;

    if (fuzz) {
        uint32_t r = spout_rand();
        if (r & 1) pad |= PAD_B;               /* Schub */
        if ((r >> 1) & 1) pad |= PAD_LF;
        if ((r >> 2) & 1) pad |= PAD_RI;
        if (((r >> 3) & 0x1ff) == 0) pad |= PAD_A;   /* Pause / weiter */
        if (((r >> 4) & 0x3ff) == 0) pad |= PAD_C;
    } else if (spout.phase != PH_GAME || spout.gameover) {
        if ((frame & 31) == 0) pad |= PAD_B;   /* Titel/Game-Over quittieren */
    } else {
        int best = spout.ship_x, bestn = -1, dx, want, diff;

        /* beste Spalte im Fenster +-14 suchen */
        for (dx = -14; dx <= 14; dx++) {
            int cx = spout.ship_x + dx;
            int n = free_run(cx, spout.ship_y);
            n -= (dx < 0 ? -dx : dx) / 6;      /* nahe Spalten bevorzugen   */
            if (n > bestn) { bestn = n; best = cx; }
        }
        if (best < VIEW_X0 + 8)  best = VIEW_X0 + 8;
        if (best > VIEW_X0 + VIEW_W - 9) best = VIEW_X0 + VIEW_W - 9;
        lastx = best;

        /* Zielwinkel: 768 = senkrecht nach oben, seitlich nachfuehren */
        want = 768 - (best - spout.ship_x) * 12;
        if (want < 640) want = 640;
        if (want > 896) want = 896;

        diff = ((want - spout.ship_r) & 1023);
        if (diff > 512) diff -= 1024;
        if (diff > 8)       pad |= PAD_LF;     /* LF erhoeht mR             */
        else if (diff < -8) pad |= PAD_RI;

        if (spout.ship_y > 34 || (frame & 3) != 3)
            pad |= PAD_B;                      /* Schub liegt auf B */
    }

    (void)lastx;
    { uint16_t trg = (uint16_t)((pad & ~prev) << 8); prev = pad; return (uint16_t)(pad | trg); }
}

int main(int argc, char **argv)
{
    int frames = 20000, i, fuzz = 0, dark = 0, shots_at = 0, nodeath = 0, pause_at = -1, titleonly = 0;
    uint32_t seed = 0xC0FFEEu;
    const char *shots = NULL;
    int hs[2] = { 123456, 4321 };
    int worst_grain = 0, deaths = 0, best_height = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots = argv[++i];
        else if (!strcmp(argv[i], "--fuzz")) fuzz = 1;
        else if (!strcmp(argv[i], "--dark")) dark = 1;
        else if (!strcmp(argv[i], "--nodeath")) nodeath = 1;
        else if (!strcmp(argv[i], "--title")) titleonly = 1;   /* auf dem Titelbild bleiben */
        else if (!strcmp(argv[i], "--pause-at") && i + 1 < argc) pause_at = atoi(argv[++i]);
        else if (i == 1) frames = atoi(argv[i]);
        else if (i == 2) seed = (uint32_t)strtoul(argv[i], NULL, 0);
    }

    spout_init(seed, hs);
    spout_render_init(dark);

    for (i = 0; i < frames; i++) {
        int prev_over = spout.gameover;
        int rc;

        spout_tick(titleonly ? 0 : (i == pause_at ? (uint16_t)(PAD_A | TRG_A) : bot(i, fuzz)));
        /* Nur fuer den Test: Tod unterdruecken, damit die Hoehle bis in die
         * engsten Stufen und ueber viele Sperrmauern hinweg geprueft wird. */
        if (nodeath && (spout.phase & 2)) { spout.gameover = 0; spout.msg[0] = 0; }
        spout_render.blink = i;

        rc = spout_selfcheck();
        if (rc) {
            fprintf(stderr, "FEHLER: spout_selfcheck() = %d in Frame %d "
                            "(phase %d, koerner %d)\n", rc, i, spout.phase, spout.n_grain);
            return 1;
        }
        if (spout.n_grain < 0 || spout.n_grain > MAX_GRAIN) {
            fprintf(stderr, "FEHLER: n_grain = %d in Frame %d\n", spout.n_grain, i);
            return 1;
        }
        if (spout.n_ovl > SPOUT_MAX_OVL) {
            fprintf(stderr, "FEHLER: n_ovl = %d in Frame %d\n", spout.n_ovl, i);
            return 1;
        }
        if (spout.n_dot > SPOUT_MAX_DOT) {
            fprintf(stderr, "FEHLER: n_dot = %d in Frame %d\n", spout.n_dot, i);
            return 1;
        }
        if (spout.n_grain > worst_grain) worst_grain = spout.n_grain;
        if (spout.height > best_height) best_height = spout.height;
        if (!prev_over && spout.gameover) deaths++;

        /* Renderer bei jedem Frame laufen lassen -- er greift auf denselben
         * Ringpuffer zu und wuerde Fehlgriffe unter ASan sofort melden.
         * Streifenweise wie auf dem Geraet (8 Zeilen), damit auch die
         * Streifengrenzen mitgeprueft werden. */
        {
            int y;
            for (y = 0; y < SCR_H; y += 8) {
                int rows = (y + 8 <= SCR_H) ? 8 : SCR_H - y;
                spout_render_rows(fb + (size_t)y * SCR_W, y, rows);
            }
        }

        if (shots && i >= shots_at) {
            const char *tag = spout.phase == PH_TITLE ? "title" :
                              spout.phase == PH_PAUSE ? "pause" :
                              spout.gameover ? "over" : "game";
            write_ppm(shots, i, tag);
            fprintf(stderr, "shot %5d %-5s schiff x=%3d y=%3d punkte=%d\n",
                    i, tag, spout.ship_x, spout.ship_y, spout.n_dot);
            shots_at = i + (i < 200 ? 40 : 900);
        }
    }

    printf("ok: %d frames, seed 0x%08x%s%s\n", frames, seed,
           fuzz ? ", fuzz" : "", dark ? ", dunkel" : "");
    printf("    max. Koerner %d/%d, Tode %d, beste Hoehe %d, Punkte %d\n",
           worst_grain, MAX_GRAIN, deaths, best_height, spout.score);
    printf("    Bestwert %d / Hoehe %d\n", spout.hiscore[0], spout.hiscore[1]);
    printf("    Ringpuffer %d B, Kornkarte %d B\n",
           RING_CELLS, RING_CELLS * 2);
    return 0;
}
