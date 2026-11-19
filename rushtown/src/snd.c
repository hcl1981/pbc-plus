/* snd.c -- the sound, computed sample by sample: an engine, tyres, a
 * siren and two channels of short effects. The platform asks for samples
 * with snd_render() from the same loop that runs the game, so nothing here
 * needs a lock. */
#include "rt.h"

typedef struct { uint16_t hz; uint8_t ms, vol; } step_t;    /* hz 0: noise, hz 1: silence */
#define END { 0, 0, 0 }
static const step_t sfx_table[][7] = {
    [SFX_STEP]   = { { 0, 12, 30 }, END },
    [SFX_DOOR]   = { { 0, 25, 110 }, { 140, 40, 80 }, END },
    [SFX_CRASH]  = { { 0, 60, 255 }, { 0, 90, 150 }, { 0, 120, 70 }, END },
    [SFX_BUMP]   = { { 0, 40, 160 }, { 90, 50, 90 }, END },
    [SFX_COIN]   = { { 1319, 50, 120 }, { 1760, 110, 120 }, END },
    [SFX_JOB]    = { { 659, 70, 120 }, { 784, 70, 120 }, { 1047, 140, 120 }, END },
    [SFX_DONE]   = { { 523, 80, 130 }, { 659, 80, 130 }, { 784, 80, 130 }, { 1047, 80, 130 }, { 1319, 220, 130 }, END },
    [SFX_FAIL]   = { { 392, 130, 130 }, { 330, 130, 130 }, { 262, 250, 130 }, END },
    [SFX_BUSTED] = { { 440, 110, 140 }, { 1, 40, 0 }, { 440, 110, 140 }, { 1, 40, 0 }, { 294, 250, 140 }, END },
    [SFX_SPLASH] = { { 0, 80, 200 }, { 0, 200, 110 }, { 0, 250, 50 }, END },
    [SFX_HORN]   = { { 370, 180, 130 }, END },
    [SFX_PHONE]  = { { 1568, 45, 70 }, { 1245, 45, 70 }, { 1568, 45, 70 }, { 1245, 45, 70 }, END },
    [SFX_STAR]   = { { 880, 60, 120 }, { 660, 120, 120 }, END },
    [SFX_UI]     = { { 988, 35, 90 }, END },
    [SFX_YELP]   = { { 700, 40, 110 }, { 950, 70, 110 }, END },
    [SFX_SHOT]   = { { 0, 18, 255 }, { 0, 40, 120 }, END },
    [SFX_WHISTLE] = { { 1900, 60, 110 }, { 2400, 60, 110 }, { 3000, 140, 110 }, END },
    [SFX_BUY]    = { { 784, 60, 120 }, { 988, 60, 120 }, { 1175, 60, 120 }, { 1568, 160, 120 }, END },
    [SFX_RANK]   = { { 523, 90, 130 }, { 784, 90, 130 }, { 1047, 90, 130 }, { 784, 90, 130 }, { 1047, 250, 130 }, END },
};

static struct {
    const step_t *s;
    int left, vol;
    uint32_t phase, inc;
} ch[2];

static uint32_t eng_phase, eng_inc, lfsr = 0x1d872b41u;
static int eng_vol, eng_target, skid_vol, skid_target, skid_lp, siren_vol;
static bool siren_on, muted;
static uint32_t siren_phase, siren_t;

void snd_mute(bool m) { muted = m; }

/* a spoken shout: 11025 Hz samples, so each is played twice */
#if RT_SPEECH
static const uint8_t *say_pcm;
static uint32_t say_pos, say_len;
void snd_say(int id)
{
    if (say_pcm && say_pos < say_len / 2)       /* let the one before finish its word */
        return;
    say_pcm = speech[rt_lang][id].pcm;
    say_len = speech[rt_lang][id].len * 2;
    say_pos = 0;
}
#else
void snd_say(int id) { (void)id; }
#endif

static void start(int c)
{
    ch[c].left = ch[c].s->ms * SND_RATE / 1000;
    ch[c].inc = (uint32_t)(((uint64_t)ch[c].s->hz << 32) / SND_RATE);
    ch[c].phase = 0;
}

void snd_sfx(int id, int vol)
{
    /* the channel that is free, else the one that is further along */
    int c = !ch[0].s ? 0 : !ch[1].s ? 1 : ch[0].left < ch[1].left ? 0 : 1;
    ch[c].s = sfx_table[id];
    ch[c].vol = vol;
    start(c);
}

void snd_engine(int hz, int vol)
{
    eng_inc = (uint32_t)(((uint64_t)(uint32_t)hz << 32) / SND_RATE);
    eng_target = vol;
}

void snd_skid(int vol) { skid_target = vol; }
void snd_siren(bool on) { siren_on = on; }
bool snd_siren_on(void) { return siren_on; }    /* for the tests */

void snd_render(uint8_t *out, int n)
{
    for (int i = 0; i < n; i++) {
        int s = 0;
        lfsr ^= lfsr << 13;
        lfsr ^= lfsr >> 17;
        lfsr ^= lfsr << 5;
        int nz = (int)((lfsr >> 9) & 255) - 128;

        if ((i & 15) == 0) {                    /* envelopes glide */
            eng_vol += (eng_target - eng_vol) >> 3;
            skid_vol += (skid_target - skid_vol) >> 2;
            siren_vol += ((siren_on ? 26 : 0) - siren_vol) >> 3;
        }
        if (eng_vol > 0) {                      /* a saw and its octave below */
            eng_phase += eng_inc;
            int saw = (int)(eng_phase >> 24) - 128;
            int sub = (eng_phase >> 1) & 0x80000000u ? 60 : -60;
            s += ((saw + sub) * eng_vol) >> 9;
        }
        if (skid_vol > 0) {
            skid_lp += (nz - skid_lp) >> 1;
            s += ((nz - skid_lp) * skid_vol) >> 8;
        }
        if (siren_vol > 0) {                    /* two tones, half a second each */
            siren_t++;
            uint32_t hz = (siren_t / (SND_RATE / 2)) & 1 ? 870 : 650;
            siren_phase += (uint32_t)(((uint64_t)hz << 32) / SND_RATE);
            s += siren_phase & 0x80000000u ? siren_vol : -siren_vol;
        }
        for (int c = 0; c < 2; c++) {
            if (!ch[c].s)
                continue;
            const step_t *st = ch[c].s;
            int a = st->vol * ch[c].vol / 255 * 40 / 255;
            if (st->hz == 0)
                s += (nz * a) >> 6;
            else if (st->hz > 1) {
                ch[c].phase += ch[c].inc;
                s += ch[c].phase & 0x80000000u ? a : -a;
            }
            if (--ch[c].left <= 0) {
                ch[c].s++;
                if (ch[c].s->ms == 0)
                    ch[c].s = 0;
                else
                    start(c);
            }
        }
        s += music_sample();
#if RT_SPEECH
        if (say_pcm) {
            s += ((int)say_pcm[say_pos >> 1] - 128) * 3 / 4;
            if (++say_pos >= say_len)
                say_pcm = 0;
        }
#endif
        if (muted)
            s = 0;
        s += 128;
        out[i] = (uint8_t)(s < 0 ? 0 : s > 255 ? 255 : s);
    }
}
