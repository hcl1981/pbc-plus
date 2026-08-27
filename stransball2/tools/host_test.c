/* host_test.c -- laeuft auf dem Buildrechner, nicht auf dem Geraet.
 *
 * Spielkern und Renderer viele Bilder laufen lassen, nach jedem Bild die
 * Buchhaltung pruefen und PPM-Bilder ausgeben.  Mit
 * -fsanitize=address,undefined faellt jeder Fehlgriff auf.
 *
 * Aufruf: host_test [frames] [seed] [--shots verz] [--level n] [--fuzz]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "color565.h"

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

/* Autopilot: haelt das Schiff in der Luft, dreht, schiesst, zieht am Ball. */
static uint16_t bot(int frame, int fuzz, int want_level)
{
    static uint16_t prev;
    uint16_t pad = 0;

    if (fuzz) {
        uint32_t r = stb_rand();
        if (r & 1) pad |= PAD_LF;
        if ((r >> 1) & 1) pad |= PAD_RI;
        if ((r >> 2) & 3) pad |= PAD_B;
        if ((r >> 4) & 1) pad |= PAD_A;
        if (((r >> 5) & 0x1f) == 0) pad |= PAD_C;
    } else if (stb.status == ST_TITLE) {
        if (stb.level != want_level) { if ((frame & 7) == 0) pad |= PAD_RI; }
        else if ((frame & 15) == 0) pad |= PAD_B;
    } else if (stb.status != ST_GAME) {
        if ((frame & 15) == 0) pad |= PAD_B;
    } else {
        /* Grob schweben: Schub nur, wenn zu tief -- sonst klebt das Schiff
         * an der Decke und die Bilder zeigen nichts. */
        if (stb.ship_y > 120) pad |= PAD_B;
        if (stb.ship_angle > 8 && stb.ship_angle < 180) pad |= PAD_LF;
        else if (stb.ship_angle >= 180 && stb.ship_angle < 352) pad |= PAD_RI;
        if ((frame % 211) < 40) pad |= PAD_A;
        if ((frame % 97) == 0) pad |= PAD_C;
    }
    { uint16_t trg = (uint16_t)((pad & ~prev) << 8); prev = pad; return (uint16_t)(pad | trg); }
}

int main(int argc, char **argv)
{
    int frames = 6000, i, fuzz = 0, shots_at = 0, level = 0;
    uint32_t seed = 0xC0FFEEu;
    const char *shots = NULL;
    int deaths = 0, done = 0, peak_en = 0, shot_step = 100;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots = argv[++i];
        else if (!strcmp(argv[i], "--fuzz")) fuzz = 1;
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--every") && i + 1 < argc) shot_step = atoi(argv[++i]);
        else if (i == 1) frames = atoi(argv[i]);
        else if (i == 2) seed = (uint32_t)strtoul(argv[i], NULL, 0);
    }

    stb_init(seed, 0);
    stb_render_init();

    for (i = 0; i < frames; i++) {
        int prev_status = stb.status, rc;

        stb_tick(bot(i, fuzz, level));
        stb_render.blink = i;

        rc = stb_selfcheck();
        if (rc) {
            fprintf(stderr, "FEHLER: stb_selfcheck() = %d in Bild %d (status %d)\n",
                    rc, i, stb.status);
            return 1;
        }
        if (stb.n_enemies > peak_en) peak_en = stb.n_enemies;
        if (prev_status == ST_GAME && stb.status == ST_DEAD) deaths++;
        if (prev_status == ST_GAME && stb.status == ST_LEVELDONE) done++;

        {
            int y;
            for (y = 0; y < SCR_H; y += 8) {
                int rows = (y + 8 <= SCR_H) ? 8 : SCR_H - y;
                stb_render_rows(fbuf + (size_t)y * SCR_W, y, rows);
            }
        }

        if (shots && i >= shots_at) {
            const char *tag = stb.status == ST_TITLE ? "title" :
                              stb.status == ST_SHIPSEL ? "ships" :
                              stb.status == ST_GAMEOVER ? "over" : "game";
            write_ppm(shots, i, tag);
            fprintf(stderr, "shot %5d %-5s level %2d schiff %3d,%3d ansicht %3d,%3d "
                            "sprit %4d gegner %3d ball %3d\n",
                    i, tag, stb.level, stb.ship_x, stb.ship_y, stb.map_x, stb.map_y,
                    stb.fuel, stb.n_enemies, stb.ball_state);
            shots_at = i + shot_step;
        }
    }

    printf("ok: %d Bilder, seed 0x%08x%s\n", frames, seed, fuzz ? ", fuzz" : "");
    printf("    Tode %d, Level geschafft %d, max. Gegner %d/%d, Level %d\n",
           deaths, done, peak_en, MAX_ENEMIES, stb.level);
    return 0;
}
