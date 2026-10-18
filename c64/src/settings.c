/* settings.c -- see settings.h */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/regs/addressmap.h"
#include "settings.h"

#define SETTINGS_MAGIC 0x35364350u   /* "PC65": layout 2, with a file name per game */
#define MAX_GAMES      150

typedef struct {
    uint32_t hash;                  /* FNV-1a of the file name, 0 = unused */
    uint8_t  port;
    char     file[SETTINGS_FILE_MAX + 1];   /* "" = first program */
    uint8_t  pad[2];
} game_t;

typedef struct {
    uint32_t magic;
    uint8_t  sound;
    uint8_t  volume;
    uint16_t count;
    uint32_t check;
    game_t   games[MAX_GAMES];
} stored_t;

_Static_assert(sizeof(stored_t) <= FLASH_SECTOR_SIZE, "settings must fit one sector");

/* Sound off by default: it is optional here (see README). */
pbc_settings_t g_settings = { false, VOLUME_DEFAULT };

/* Working copy: only allocated while the menu changes something. */
static stored_t *work;

static const stored_t *flash_copy(void)
{
    return (const stored_t *)(XIP_BASE + SETTINGS_OFFSET);
}

static uint32_t fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 16777619u;
    }
    return h ? h : 1;
}

static uint32_t header_check(const stored_t *s)
{
    return (s->magic ^ ((uint32_t)s->sound << 8) ^ ((uint32_t)s->volume << 16) ^ s->count)
           * 2654435761u;
}

static bool valid(const stored_t *s)
{
    return s->magic == SETTINGS_MAGIC && s->check == header_check(s) && s->count <= MAX_GAMES;
}

void settings_load(void)
{
    const stored_t *s = flash_copy();
    if (!valid(s))
        return;
    g_settings.sound = s->sound != 0;
    if (s->volume >= VOLUME_MIN && s->volume <= VOLUME_MAX)
        g_settings.volume = s->volume;
}

static const stored_t *current(void)
{
    if (work)
        return work;
    return valid(flash_copy()) ? flash_copy() : NULL;
}

static const game_t *find(const char *game)
{
    const stored_t *s = current();
    uint32_t h = fnv1a(game);
    for (int i = 0; s && i < s->count; i++)
        if (s->games[i].hash == h)
            return &s->games[i];
    return NULL;
}

int settings_port(const char *game)
{
    const game_t *g = find(game);
    return g ? (g->port == 1 ? 1 : 2) : PORT_DEFAULT;
}

void settings_file(const char *game, char *out)
{
    const game_t *g = find(game);
    out[0] = 0;
    if (g) {
        memcpy(out, g->file, SETTINGS_FILE_MAX);
        out[SETTINGS_FILE_MAX] = 0;
    }
}

static stored_t *writable(void)
{
    if (!work) {
        /* a whole sector: flash_range_program writes whole pages */
        work = malloc(FLASH_SECTOR_SIZE);
        if (!work)
            return NULL;
        memset(work, 0xFF, FLASH_SECTOR_SIZE);
        if (valid(flash_copy())) {
            memcpy(work, flash_copy(), sizeof *work);
        } else {
            memset(work, 0, sizeof *work);
            work->magic = SETTINGS_MAGIC;
        }
    }
    return work;
}

/* Entry for `game` in the working copy, created with defaults if new. */
static game_t *entry(const char *game)
{
    stored_t *s = writable();
    if (!s)
        return NULL;
    uint32_t h = fnv1a(game);
    int i;
    for (i = 0; i < s->count; i++)
        if (s->games[i].hash == h)
            break;
    if (i == s->count) {
        if (s->count >= MAX_GAMES) {
            /* full: forget the oldest entry */
            memmove(&s->games[0], &s->games[1], sizeof(game_t) * (MAX_GAMES - 1));
            i = MAX_GAMES - 1;
        } else {
            s->count++;
        }
        memset(&s->games[i], 0, sizeof s->games[i]);
        s->games[i].hash = h;
        s->games[i].port = PORT_DEFAULT;
    }
    return &s->games[i];
}

void settings_set_port(const char *game, int port)
{
    game_t *g = entry(game);
    if (g)
        g->port = (uint8_t)(port == 1 ? 1 : 2);
}

void settings_set_file(const char *game, const char *file)
{
    game_t *g = entry(game);
    if (g) {
        memset(g->file, 0, sizeof g->file);
        strncpy(g->file, file, SETTINGS_FILE_MAX);
    }
}

void settings_save(void)
{
    stored_t *s = writable();
    if (!s)
        return;
    s->sound = g_settings.sound;
    s->volume = (uint8_t)g_settings.volume;
    s->check = header_check(s);

    if (memcmp(flash_copy(), s, sizeof *s) != 0) {
        uint32_t irq = save_and_disable_interrupts();
        flash_range_erase(SETTINGS_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(SETTINGS_OFFSET, (const uint8_t *)s,
                            (sizeof *s + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1));
        restore_interrupts(irq);
    }
    free(work);
    work = NULL;
}
