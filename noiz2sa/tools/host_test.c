/* host_test.c -- laeuft auf dem Buildrechner, nicht auf dem Geraet.
 *
 * Spielkern und Renderer viele tausend Bilder laufen lassen, nach jedem Bild
 * die Buchhaltung pruefen und PPM-Bilder ausgeben.  Mit
 * -fsanitize=address,undefined faellt jeder Zugriff ausserhalb der Puffer auf.
 *
 * Aufruf: host_test [frames] [seed] [--shots verz] [--stage n] [--bot|--fuzz]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "render.h"
#include "color565.h"
#include "bml.h"

static uint16_t fbuf[SCR_W * SCR_H];

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
        uint32_t c = pbc_rgb24_of(fbuf[i]);
        unsigned char rgb[3];
        rgb[0] = (unsigned char)((c >> 16) & 0xff);
        rgb[1] = (unsigned char)((c >> 8) & 0xff);
        rgb[2] = (unsigned char)(c & 0xff);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* Autopilot: haelt sich unten in der Mitte, weicht seitlich aus, schiesst. */
static uint16_t bot(int frame, int fuzz, int start_stage)
{
    static uint16_t prev;
    uint16_t pad = 0;

    if (fuzz) {
        uint32_t r = noiz_rand();
        if (r & 1) pad |= PAD_LF;
        if ((r >> 1) & 1) pad |= PAD_RI;
        if ((r >> 2) & 1) pad |= PAD_UP;
        if ((r >> 3) & 1) pad |= PAD_DN;
        if ((r >> 4) & 1) pad |= PAD_B;
        if ((r >> 5) & 1) pad |= PAD_A;
        if (((r >> 6) & 0x1ff) == 0) pad |= PAD_C;
    } else if (noiz.status != ST_GAME) {
        /* Titel: gewuenschte Stufe anwaehlen, dann starten */
        if (noiz.status == ST_TITLE && noiz.stage != start_stage) {
            if ((frame & 7) == 0) pad |= PAD_RI;
        } else if ((frame & 15) == 0) {
            pad |= PAD_B;
        }
    } else {
        /* Haelt sich im unteren Drittel und pendelt seitlich -- kein guter
         * Spieler, aber er bleibt dort, wo ein Mensch auch spielen wuerde. */
        pad |= PAD_B;                          /* Dauerfeuer */
        if ((frame / 40) & 1) pad |= PAD_LF; else pad |= PAD_RI;
        if (noiz.ship_y < 200) pad |= PAD_DN;
        else if (noiz.ship_y > 250) pad |= PAD_UP;
        if ((frame % 97) < 30) pad |= PAD_A;   /* zwischendurch langsam */
    }

    { uint16_t trg = (uint16_t)((pad & ~prev) << 8); prev = pad; return (uint16_t)(pad | trg); }
}

int main(int argc, char **argv)
{
    int frames = 12000, i, fuzz = 0, shots_at = 0, stage = 0;
    uint32_t seed = 0xC0FFEEu;
    const char *shots = NULL;
    int peak_bullet = 0, peak_runner = 0, deaths = 0, max_ms_state = 0;
    long scenes = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots = argv[++i];
        else if (!strcmp(argv[i], "--fuzz")) fuzz = 1;
        else if (!strcmp(argv[i], "--stage") && i + 1 < argc) stage = atoi(argv[++i]);
        else if (i == 1) frames = atoi(argv[i]);
        else if (i == 2) seed = (uint32_t)strtoul(argv[i], NULL, 0);
    }

    noiz_init(seed, 123456);
    noiz_render_init();

    for (i = 0; i < frames; i++) {
        int prev_status = noiz.status, rc, prev_scene = noiz.scene;

        noiz_tick(bot(i, fuzz, stage));
        noiz_render.blink = i;

        rc = noiz_selfcheck();
        if (rc) {
            fprintf(stderr, "FEHLER: noiz_selfcheck() = %d in Bild %d "
                            "(status %d, Geschosse %d)\n", rc, i, noiz.status, noiz.n_bullet);
            return 1;
        }
        if (noiz.n_bullet > FOE_MAX) {
            fprintf(stderr, "FEHLER: n_bullet = %d in Bild %d\n", noiz.n_bullet, i);
            return 1;
        }
        if (noiz.n_bullet > peak_bullet) peak_bullet = noiz.n_bullet;
        if (noiz.n_runner > peak_runner) peak_runner = noiz.n_runner;
        if (noiz.interval > max_ms_state) max_ms_state = noiz.interval;
        if (prev_status == ST_GAME && noiz.status == ST_GAMEOVER) deaths++;
        if (noiz.scene != prev_scene) scenes++;

        {
            int y;
            for (y = 0; y < SCR_H; y += 8) {
                int rows = (y + 8 <= SCR_H) ? 8 : SCR_H - y;
                noiz_render_rows(fbuf + (size_t)y * SCR_W, y, rows);
            }
        }

        if (shots && i >= shots_at) {
            const char *tag = noiz.status == ST_TITLE ? "title" :
                              noiz.status == ST_GAMEOVER ? "over" :
                              noiz.status == ST_CLEAR ? "clear" : "game";
            write_ppm(shots, i, tag);
            fprintf(stderr, "shot %5d %-5s szene %2d geschosse %3d laeufer %3d schiff %3d,%3d inv %3d fb %02X\n",
                    i, tag, noiz.scene, noiz.n_bullet, noiz.n_runner,
                    noiz.ship_x, noiz.ship_y, noiz.ship_inv,
                    noiz.fb[noiz.ship_y * SCR_W + noiz.ship_x]);
            shots_at = i + (i < 400 ? 100 : 900);
        }
    }

    printf("ok: %d Bilder, seed 0x%08x%s\n", frames, seed, fuzz ? ", fuzz" : "");
    printf("    max. Geschosse %d/%d, max. Laeufer %d/%d, Szenen %ld, Tode %d\n",
           peak_bullet, FOE_MAX, peak_runner, BML_MAX_RUNNER, scenes, deaths);
    printf("    groesster Bildabstand %d (Grundwert 16), Punkte %d\n",
           max_ms_state, noiz.score);
    return 0;
}
