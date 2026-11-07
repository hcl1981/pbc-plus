/* pbc_game_opts.c -- see pbc_game_opts.h */
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pbc_game_opts.h"

/* The launcher's table, exactly as pbcemu/src/c64/c64_settings.c (and
 * c64/src/settings.c) lay it out. */
#define OPTS_OFFSET    0xFF2000u
#define OPTS_MAGIC     0x35364350u      /* "PC65" */
#define OPTS_GAMES     150
#define OPTS_FILE_MAX  16
#define OPTS_PORT_DEF  2

typedef struct {
    uint32_t hash;                      /* FNV-1a of the file name, 0 = unused */
    uint8_t  port;
    char     file[OPTS_FILE_MAX + 1];
    uint8_t  key;
    uint8_t  drive;
} opts_game_t;

typedef struct {
    uint32_t magic;
    uint8_t  sound;
    uint8_t  volume;
    uint16_t count;
    uint32_t check;
    opts_game_t games[OPTS_GAMES];
} opts_t;

_Static_assert(sizeof(opts_t) <= FLASH_SECTOR_SIZE, "one sector");

static uint32_t fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 16777619u;
    }
    return h ? h : 1;
}

static uint32_t header_check(const opts_t *s)
{
    return (s->magic ^ ((uint32_t)s->sound << 8) ^ ((uint32_t)s->volume << 16) ^ s->count)
           * 2654435761u;
}

static void write_sector(void *page)
{
    flash_range_erase(OPTS_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(OPTS_OFFSET, (const uint8_t *)page,
                        (sizeof(opts_t) + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1));
}

bool pbc_game_opts_set_key(const char *game, uint8_t key)
{
    const opts_t *now = (const opts_t *)(XIP_BASE + OPTS_OFFSET);
    bool valid = now->magic == OPTS_MAGIC && now->check == header_check(now) &&
                 now->count <= OPTS_GAMES;
    uint32_t h = fnv1a(game);
    if (valid)                          /* unchanged: spare the flash */
        for (int i = 0; i < now->count; i++)
            if (now->games[i].hash == h && now->games[i].key == key)
                return true;

    opts_t *s = malloc(FLASH_SECTOR_SIZE);
    if (!s)
        return false;
    memset(s, 0xFF, FLASH_SECTOR_SIZE);
    if (valid) {
        memcpy(s, now, sizeof *s);
    } else {
        memset(s, 0, sizeof *s);
        s->magic = OPTS_MAGIC;
    }
    int i;
    for (i = 0; i < s->count; i++)
        if (s->games[i].hash == h)
            break;
    if (i == s->count) {                /* a new entry, as the launcher makes one */
        if (s->count >= OPTS_GAMES) {
            memmove(&s->games[0], &s->games[1], sizeof(opts_game_t) * (OPTS_GAMES - 1));
            i = OPTS_GAMES - 1;
        } else {
            s->count++;
        }
        memset(&s->games[i], 0, sizeof s->games[i]);
        s->games[i].hash = h;
        s->games[i].port = OPTS_PORT_DEF;
    }
    s->games[i].key = key;
    s->check = header_check(s);
    bool ok = flash_safe_execute(write_sector, s, 100) == PICO_OK;
    free(s);
    return ok;
}
