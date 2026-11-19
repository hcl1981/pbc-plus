/* music.c -- the car radio: three stations of looping chip music.
 *
 * Leave it out with RT_MUSIC=0; snd.c then mixes nothing in and the game
 * has no radio row in its menu. A station is four bars in sixteenths: a
 * bass line, a lead, and drums from a bit pattern. Notes are MIDI numbers,
 * 0 is a rest.
 */
#include "rt.h"

#if RT_MUSIC

typedef struct {
    const char *name;
    uint16_t step;                      /* samples per sixteenth */
    uint8_t bass[64], lead[64];
    uint16_t kick, snare, hat;          /* per bar: bit n set = a hit on sixteenth n */
} song_t;

#define R 0
static const song_t songs[] = {
    /* A minor, F, C, G: driving, a little sad */
    { "RUSH FM", 2480,
      { 45, R, 57, R, 45, R, 57, 45,  45, R, 57, R, 45, 57, R, 52,
        41, R, 53, R, 41, R, 53, 41,  41, R, 53, R, 41, 53, R, 48,
        48, R, 60, R, 48, R, 60, 48,  48, R, 60, R, 48, 60, R, 55,
        43, R, 55, R, 43, R, 55, 43,  43, R, 55, R, 47, 50, 55, 59 },
      { 69, R, R, 72, R, R, 76, R,  74, R, 72, R, 69, R, R, R,
        69, R, R, 72, R, R, 77, R,  76, R, 72, R, 69, R, R, R,
        67, R, R, 72, R, R, 76, R,  79, R, 76, R, 72, R, R, R,
        74, R, R, 71, R, R, 67, R,  71, R, 74, R, 79, R, 77, 76 },
      0x1111, 0x1010, 0x5555 },
    /* D major, slow and easy: for the beach */
    { "PALM RADIO", 3300,
      { 38, R, R, 45, R, R, 50, R,  38, R, R, 45, R, R, 42, R,
        43, R, R, 50, R, R, 55, R,  43, R, R, 50, R, R, 47, R,
        45, R, R, 52, R, R, 57, R,  45, R, R, 52, R, R, 49, R,
        43, R, R, 50, R, R, 45, R,  45, R, R, 52, R, R, 57, R },
      { R, R, 74, R, 78, R, 81, R,  R, R, 78, R, 74, R, R, R,
        R, R, 71, R, 74, R, 79, R,  R, R, 78, R, 74, R, R, R,
        R, R, 73, R, 76, R, 81, R,  R, R, 79, R, 76, R, R, R,
        R, R, 74, R, 71, R, 73, R,  76, R, R, R, 73, R, R, R },
      0x0101, 0x0010, 0x4444 },
    /* E minor on one riff: machines at work */
    { "IRON 99", 2100,
      { 40, 40, R, 40, 52, R, 40, R,  40, 40, R, 43, R, 45, 43, R,
        40, 40, R, 40, 52, R, 40, R,  40, 40, R, 47, R, 45, 43, R,
        36, 36, R, 36, 48, R, 36, R,  36, 36, R, 40, R, 43, 40, R,
        38, 38, R, 38, 50, R, 38, R,  38, 38, R, 42, 45, 50, 54, 57 },
      { 64, R, R, R, R, R, 67, R,  R, R, 71, R, R, R, 69, R,
        64, R, R, R, R, R, 67, R,  R, R, 74, R, 71, R, R, R,
        72, R, R, R, R, R, 67, R,  R, R, 76, R, R, R, 72, R,
        74, R, R, R, 69, R, R, R,  66, R, 69, R, 74, R, 78, R },
      0x1151, 0x1010, 0xffff },
    /* C minor, slow: the town after midnight */
    { "MIDNIGHT 88", 3000,
      { 36, R, R, R, R, R, 48, R, 36, R, R, R, 43, R, R, R,
        32, R, R, R, R, R, 44, R, 32, R, R, R, 39, R, R, R,
        39, R, R, R, R, R, 51, R, 39, R, R, R, 46, R, R, R,
        34, R, R, R, R, R, 46, R, 34, R, R, R, 41, R, R, R },
      { 79, R, R, R, 75, R, R, R, 72, R, R, R, R, R, 75, R,
        77, R, R, R, 75, R, R, R, 72, R, R, R, R, R, R, R,
        79, R, R, R, 82, R, R, R, 79, R, R, R, R, R, 75, R,
        77, R, R, R, 74, R, R, R, 70, R, R, R, R, R, R, R },
      0x0401, 0x0100, 0x4444 },
    /* C major, quick arpeggios: a toy that means it */
    { "SQUARE ONE", 1900,
      { 36, R, 48, R, 36, R, 48, R, 36, R, 48, R, 36, R, 48, R,
        31, R, 43, R, 31, R, 43, R, 31, R, 43, R, 31, R, 43, R,
        33, R, 45, R, 33, R, 45, R, 33, R, 45, R, 33, R, 45, R,
        29, R, 41, R, 29, R, 41, R, 29, R, 41, R, 31, R, 43, R },
      { 72, 76, 79, 84, 79, 76, 72, 76, 79, 84, 79, 76, 72, R, R, R,
        71, 74, 79, 83, 79, 74, 71, 74, 79, 83, 79, 74, 71, R, R, R,
        69, 72, 76, 81, 76, 72, 69, 72, 76, 81, 76, 72, 69, R, R, R,
        65, 69, 72, 77, 72, 69, 65, 69, 72, 77, 72, 69, 67, 71, 74, 79 },
      0x1111, 0x1010, 0x4444 },
    /* a blues in A with a walking bass */
    { "DOCK BLUES", 2700,
      { 45, R, R, R, 49, R, R, R, 52, R, R, R, 54, R, R, R,
        50, R, R, R, 54, R, R, R, 57, R, R, R, 59, R, R, R,
        45, R, R, R, 49, R, R, R, 52, R, R, R, 54, R, R, R,
        52, R, R, R, 56, R, R, R, 59, R, R, R, 56, R, R, R },
      { R, R, 81, R, 79, R, 76, R, 75, 74, 72, R, 69, R, R, R,
        R, R, 74, R, 77, R, 81, R, 79, R, 77, R, 74, R, R, R,
        R, R, 81, R, 79, R, 76, R, 72, R, 69, R, R, R, R, R,
        R, R, 76, R, 79, R, 83, R, 80, R, 76, R, 74, R, 71, R },
      0x0101, 0x1010, 0x5555 },
};
#undef R
#define SONGS ((int)(sizeof songs / sizeof songs[0]))

static const uint32_t top[12] = { 815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u, 1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u };    /* MIDI 108..119 at SND_RATE */
static uint32_t inc(int note)
{
    return note ? top[note % 12] >> (9 - note / 12) : 0;
}

static int station = -1, pos, left;
static uint32_t ph_bass, in_bass, ph_lead, in_lead, ph_kick, in_kick, lfsr = 0x7a3c91u;
static int env_bass, env_lead, env_kick, env_noise, noise_fall;

int music_stations(void) { return SONGS; }
const char *music_name(int n) { return n >= 0 && n < SONGS ? songs[n].name : "OFF"; }

void music_station(int n)
{
    if (n == station)
        return;
    station = n < SONGS || n == MUSIC_POLICE ? n : -1;
    pos = 0;
    left = 0;
    env_bass = env_lead = env_kick = env_noise = 0;
}

/* one sample, about -40 .. 40 */
/* The police band: hiss, and now and then a beep and somebody saying
 * something nobody can make out -- a tone that jumps about under the hiss. */
static int police_sample(void)
{
    static int wait = SND_RATE, talk, hop;
    static uint32_t ph, in;
    lfsr ^= lfsr << 13;
    lfsr ^= lfsr >> 17;
    lfsr ^= lfsr << 5;
    int s = (int)(lfsr & 7) - 4;                /* the hiss is always there */
    if (talk > 0) {
        talk--;
        if (talk > SND_RATE * 3 / 2 - SND_RATE / 10)
            in = (uint32_t)(((uint64_t)1400 << 32) / SND_RATE);     /* the beep that opens the channel */
        else if (--hop <= 0) {
            hop = SND_RATE / 14 + (int)(lfsr & 1023);
            in = lfsr & 3 ? (uint32_t)(((uint64_t)(280 + (lfsr >> 8) % 500) << 32) / SND_RATE) : 0;
        }
        ph += in;
        if (in)
            s += ph & 0x80000000u ? 9 : -9;
        s += (int)(lfsr >> 28) - 8;             /* and louder noise while it is open */
    } else if (--wait <= 0) {
        wait = SND_RATE * 2 + (int)(lfsr % (SND_RATE * 4));
        talk = SND_RATE * 3 / 2;
        hop = 0;
    }
    return s;
}

int music_sample(void)
{
    if (station == MUSIC_POLICE)
        return police_sample();
    if (station < 0)
        return 0;
    const song_t *g = &songs[station];
    if (--left <= 0) {                          /* the next sixteenth */
        int bar = pos & 15;
        left = g->step;
        if (g->bass[pos]) { in_bass = inc(g->bass[pos]); env_bass = 255; }
        if (g->lead[pos]) { in_lead = inc(g->lead[pos]); env_lead = 255; }
        if (g->kick >> bar & 1) { in_kick = inc(48); env_kick = 255; }
        if (g->snare >> bar & 1) { env_noise = 255; noise_fall = 3; }
        else if (g->hat >> bar & 1) { env_noise = 130; noise_fall = 12; }
        pos = (pos + 1) & 63;
    }
    int s = 0;
    ph_bass += in_bass;
    s += ((ph_bass >> 24 < 96 ? 14 : -14) * env_bass) >> 8;             /* a narrow pulse */
    ph_lead += in_lead;
    s += ((ph_lead & 0x80000000u ? 11 : -11) * env_lead) >> 8;
    if (env_kick) {                             /* a tone that drops */
        ph_kick += in_kick;
        in_kick -= in_kick >> 9;
        s += ((ph_kick & 0x80000000u ? 18 : -18) * env_kick) >> 8;
    }
    if (env_noise) {
        lfsr ^= lfsr << 13;
        lfsr ^= lfsr >> 17;
        lfsr ^= lfsr << 5;
        s += (((int)(lfsr & 31) - 16) * env_noise) >> 9;
    }
    if ((left & 31) == 0) {                     /* everything rings off */
        env_bass -= env_bass >> 5;
        env_lead -= (env_lead >> 5) + (env_lead > 0);
        env_kick -= (env_kick >> 2) + (env_kick > 0);
        env_noise -= env_noise > noise_fall ? noise_fall : env_noise;
    }
    return s;
}

#else
int music_stations(void) { return 0; }
const char *music_name(int n) { (void)n; return "OFF"; }
void music_station(int n) { (void)n; }
int music_sample(void) { return 0; }
#endif
