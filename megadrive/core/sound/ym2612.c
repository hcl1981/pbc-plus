/* ym2612.c -- YM2612 (OPN2) FM sound chip for the Mega Drive emulator
 *
 * Written for the PicoBoy Color Plus port (GPL-3.0-or-later). It replaces
 * the YM2612 based on MAME's fm.c with Genesis Plus GX's changes, whose
 * licence does not allow use in a commercial product, and keeps its
 * interface (ym2612.h) and output level: six channels of 14 bits, summed and
 * halved into one 16-bit sample at the chip's rate (clock / 144).
 *
 * The chip, as documented by Yamaha, Sega's technical manual and the
 * measurements published on the SpritesMind forum:
 *   - 6 channels of 4 operators. An operator is a sine wave read from a
 *     20-bit phase counter (top 10 bits) through a log-sine table and an
 *     exponent table: attenuation adds in the log domain, the result is 13
 *     bits plus sign. Its phase can be offset by another operator's output
 *     (frequency modulation); 8 algorithms connect the four, operator 1 can
 *     modulate itself (feedback). The chip computes the operators in the
 *     order 1, 3, 2, 4 (for all channels): what feeds operator 3 from
 *     operator 2 (or from 1 in algorithms 1 and 5) comes from the previous
 *     sample (MEM).
 *   - phase step: F-number shifted by the block, plus detune (by key code),
 *     times MUL (0 = 1/2); detune can overflow the 17 bits. The block and
 *     F-number high bits (A4-A6) go to one latch shared by all channels and
 *     take effect with the next A0-A2 write.
 *   - envelope: 10-bit attenuation, attack / decay / sustain / release, rates
 *     from the registers plus key scaling, advanced every 3 samples by a
 *     global counter; SSG-EG (repeat, alternate, hold, invert)
 *   - LFO: amplitude (to operators with AM on, depth per channel) and pitch
 *     modulation (per channel), 8 speeds
 *   - channel 3 special mode: own frequencies for operators 1-3; CSM:
 *     timer A keys channel 3 on
 *   - DAC: channel 6 plays the 8-bit value of register 2A instead
 *   - timers A (1024 - N samples) and B (16 * (256 - N) samples), status
 *   - output: each carrier truncated to 9 bits (the DAC), each channel
 *     limited to 14 bits; channels muted unless panned left or right; the
 *     discrete YM2612's DAC adds an offset of 4 steps away from zero to
 *     every channel it outputs (crossover distortion, "ladder effect")
 * Pitch modulation follows the documented depths (in cents) instead of the
 * chip's bit pattern; SSG-EG follows the published behaviour.
 */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "ym2612.h"
#include "gwenesis_bus.h"
#include "gwenesis_savestate.h"
#ifdef PBC_YM_CORE1
#include "ym_core1.h"
#endif

/* PBC: synthesis and its tables in RAM */
#define YM_CODE __attribute__((section(".time_critical.ym2612")))
#define YM_DATA __attribute__((section(".data.ym2612")))

enum { EG_ATTACK, EG_DECAY, EG_SUSTAIN, EG_RELEASE };

typedef struct {
    uint32_t phase;         /* 20 bits */
    uint32_t inc;
    int32_t  att;           /* envelope: 0 (loudest) .. 1023 */
    uint8_t  state;         /* EG_* */
    uint8_t  key;           /* keyed on by register 28 */
    uint8_t  csm;           /* keyed on by CSM */
    uint8_t  ssg_inv;       /* SSG-EG: output inverted */
    uint8_t  dt, mul, ks, ar, d1r, d2r, rr, am, ssg;
    uint8_t  rate[4];       /* per EG state, with key scaling */
    uint16_t sl_att;        /* sustain level as attenuation */
    uint16_t tl_att;        /* total level as attenuation */
} ym_op;

typedef struct {
    ym_op    op[4];         /* operators 1, 2, 3, 4 (registers +0, +8, +4, +C) */
    uint16_t fnum;
    uint8_t  block, alg, fb, ams, pms, pan;
    uint8_t  active;        /* some operator still audible */
    int32_t  op1_out[2];    /* operator 1's last two outputs */
    int32_t  mem;           /* delayed modulator */
} ym_ch;

static struct {
    ym_ch    ch[6];
    uint16_t addr;          /* latched address, port 1 adds 0x100 */
    uint8_t  fhi;           /* A4-A6 latch, one for all channels (as in the chip) */
    uint8_t  fhi3;          /* AC-AE latch */
    uint16_t fnum3[3];      /* channel 3 special mode: A8 (op 3), A9 (op 1), AA (op 2) */
    uint8_t  block3[3];
    uint8_t  mode;          /* register 27 */
    uint8_t  status;
    uint16_t ta, tb;        /* timer registers */
    int32_t  ta_cnt, tb_cnt;
    uint8_t  lfo_on, lfo_freq, lfo_cnt, lfo_am;
    int8_t   lfo_pm;        /* -7 .. 7 */
    uint8_t  lfo_div;
    uint8_t  eg_div;
    uint32_t eg_cnt;
    int32_t  dac_out;
    uint8_t  dac_on;
    uint8_t  chip_type;
} ym;

/* ---- tables -------------------------------------------------------------- */

/* -log2(sin((i + 0.5) / 256 * pi / 2)) * 256: a quarter sine wave */
static const uint16_t YM_DATA logsin[256] = {
    2137, 1731, 1543, 1419, 1326, 1252, 1190, 1137, 1091, 1050, 1013,  979,  949,  920,  894,  869,
     846,  825,  804,  785,  767,  749,  732,  717,  701,  687,  672,  659,  646,  633,  621,  609,
     598,  587,  576,  566,  556,  546,  536,  527,  518,  509,  501,  492,  484,  476,  468,  461,
     453,  446,  439,  432,  425,  418,  411,  405,  399,  392,  386,  380,  375,  369,  363,  358,
     352,  347,  341,  336,  331,  326,  321,  316,  311,  307,  302,  297,  293,  289,  284,  280,
     276,  271,  267,  263,  259,  255,  251,  248,  244,  240,  236,  233,  229,  226,  222,  219,
     215,  212,  209,  205,  202,  199,  196,  193,  190,  187,  184,  181,  178,  175,  172,  169,
     167,  164,  161,  159,  156,  153,  151,  148,  146,  143,  141,  138,  136,  134,  131,  129,
     127,  125,  122,  120,  118,  116,  114,  112,  110,  108,  106,  104,  102,  100,   98,   96,
      94,   92,   91,   89,   87,   85,   83,   82,   80,   78,   77,   75,   74,   72,   70,   69,
      67,   66,   64,   63,   62,   60,   59,   57,   56,   55,   53,   52,   51,   49,   48,   47,
      46,   45,   43,   42,   41,   40,   39,   38,   37,   36,   35,   34,   33,   32,   31,   30,
      29,   28,   27,   26,   25,   24,   23,   23,   22,   21,   20,   20,   19,   18,   17,   17,
      16,   15,   15,   14,   13,   13,   12,   12,   11,   10,   10,    9,    9,    8,    8,    7,
       7,    7,    6,    6,    5,    5,    5,    4,    4,    4,    3,    3,    3,    2,    2,    2,
       2,    1,    1,    1,    1,    1,    1,    1,    0,    0,    0,    0,    0,    0,    0,    0
};

/* 2^((255 - i) / 256) * 1024: the fraction of the exponent */
static const uint16_t YM_DATA exptab[256] = {
    2042, 2037, 2031, 2026, 2020, 2015, 2010, 2004, 1999, 1993, 1988, 1983, 1977, 1972, 1966, 1961,
    1956, 1951, 1945, 1940, 1935, 1930, 1924, 1919, 1914, 1909, 1904, 1898, 1893, 1888, 1883, 1878,
    1873, 1868, 1863, 1858, 1853, 1848, 1843, 1838, 1833, 1828, 1823, 1818, 1813, 1808, 1803, 1798,
    1794, 1789, 1784, 1779, 1774, 1769, 1765, 1760, 1755, 1750, 1746, 1741, 1736, 1732, 1727, 1722,
    1717, 1713, 1708, 1704, 1699, 1694, 1690, 1685, 1681, 1676, 1672, 1667, 1663, 1658, 1654, 1649,
    1645, 1640, 1636, 1631, 1627, 1623, 1618, 1614, 1609, 1605, 1601, 1596, 1592, 1588, 1584, 1579,
    1575, 1571, 1566, 1562, 1558, 1554, 1550, 1545, 1541, 1537, 1533, 1529, 1525, 1520, 1516, 1512,
    1508, 1504, 1500, 1496, 1492, 1488, 1484, 1480, 1476, 1472, 1468, 1464, 1460, 1456, 1452, 1448,
    1444, 1440, 1436, 1433, 1429, 1425, 1421, 1417, 1413, 1409, 1406, 1402, 1398, 1394, 1391, 1387,
    1383, 1379, 1376, 1372, 1368, 1364, 1361, 1357, 1353, 1350, 1346, 1342, 1339, 1335, 1332, 1328,
    1324, 1321, 1317, 1314, 1310, 1307, 1303, 1300, 1296, 1292, 1289, 1286, 1282, 1279, 1275, 1272,
    1268, 1265, 1261, 1258, 1255, 1251, 1248, 1244, 1241, 1238, 1234, 1231, 1228, 1224, 1221, 1218,
    1214, 1211, 1208, 1205, 1201, 1198, 1195, 1192, 1188, 1185, 1182, 1179, 1176, 1172, 1169, 1166,
    1163, 1160, 1157, 1154, 1150, 1147, 1144, 1141, 1138, 1135, 1132, 1129, 1126, 1123, 1120, 1117,
    1114, 1111, 1108, 1105, 1102, 1099, 1096, 1093, 1090, 1087, 1084, 1081, 1078, 1075, 1072, 1069,
    1066, 1064, 1061, 1058, 1055, 1052, 1049, 1046, 1044, 1041, 1038, 1035, 1032, 1030, 1027, 1024
};

/* detune in phase steps by key code (Yamaha's table) */
static const uint8_t YM_DATA dt_tab[4][32] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
      2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8 },
    { 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
      5, 6, 6, 7, 8, 8, 9,10,11,12,13,14,16,16,16,16 },
    { 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
      8, 8, 9,10,11,12,13,14,16,17,19,20,22,22,22,22 }
};

/* the key code's two low bits from F-number bits 10..7 */
static const uint8_t YM_DATA kc_note[16] = { 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3 };

/* envelope step per update, eight updates in a row: rates 2-47 by rate & 3,
 * then 48-59 one row each, 60-63 */
static const uint8_t YM_DATA eg_inc[17][8] = {
    { 0, 1, 0, 1, 0, 1, 0, 1 }, { 0, 1, 0, 1, 1, 1, 0, 1 },
    { 0, 1, 1, 1, 0, 1, 1, 1 }, { 0, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 2, 1, 1, 1, 2 },
    { 1, 2, 1, 2, 1, 2, 1, 2 }, { 1, 2, 2, 2, 1, 2, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2 }, { 2, 2, 2, 4, 2, 2, 2, 4 },
    { 2, 4, 2, 4, 2, 4, 2, 4 }, { 2, 4, 4, 4, 2, 4, 4, 4 },
    { 4, 4, 4, 4, 4, 4, 4, 4 }, { 4, 4, 4, 8, 4, 4, 4, 8 },
    { 4, 8, 4, 8, 4, 8, 4, 8 }, { 4, 8, 8, 8, 4, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 }
};

/* LFO: samples per step, 128 steps per period (3.98 .. 72.2 Hz) */
static const uint8_t YM_DATA lfo_period[8] = { 108, 77, 71, 67, 62, 44, 8, 5 };

/* AM depth: shift of the LFO's 0..126 (0, 1.4, 5.9, 11.8 dB) */
static const uint8_t YM_DATA ams_shift[4] = { 8, 3, 1, 0 };

/* PM: F-number change in 1/2^20 per depth (0, 3.4, 6.7, 10, 14, 20, 40,
 * 80 cents at the peak) and LFO step 0..7 */
static const uint16_t YM_DATA pm_mul[8][8] = {
    {    0,     0,     0,     0,     0,     0,     0,     0},
    {    0,   294,   589,   883,  1177,  1472,  1767,  2061},
    {    0,   580,  1160,  1741,  2321,  2903,  3484,  4066},
    {    0,   866,  1732,  2599,  3467,  4335,  5204,  6074},
    {    0,  1212,  2426,  3640,  4857,  6074,  7293,  8514},
    {    0,  1732,  3467,  5204,  6945,  8688, 10435, 12184},
    {    0,  3467,  6945, 10435, 13936, 17449, 20973, 24509},
    {    0,  6945, 13936, 20973, 28057, 35188, 42366, 49591},
};

/* register slot (address bits 3..2) to operator: +0 op 1, +4 op 3,
 * +8 op 2, +C op 4 */
static const uint8_t YM_DATA slot_op[4] = { 0, 2, 1, 3 };

/* ---- frequency and rates ------------------------------------------------- */

static inline int keycode(int block, int fnum)
{
    return (block << 2) | kc_note[fnum >> 7];
}

static void op_freq(ym_op *o, int fnum, int block, int kc)
{
    uint32_t base = ((uint32_t)fnum << block) >> 1;
    int d = dt_tab[o->dt & 3][kc];
    base = (o->dt & 4) ? base - d : base + d;
    base &= 0x1ffff;                    /* detune overflow */
    o->inc = (o->mul ? base * o->mul : base >> 1) & 0xfffff;
}

static void op_rates(ym_op *o, int kc)
{
    int ksr = kc >> (3 - o->ks);
    int r;
    r = 2 * o->ar + ksr;  o->rate[EG_ATTACK]  = o->ar  ? (r > 63 ? 63 : r) : 0;
    r = 2 * o->d1r + ksr; o->rate[EG_DECAY]   = o->d1r ? (r > 63 ? 63 : r) : 0;
    r = 2 * o->d2r + ksr; o->rate[EG_SUSTAIN] = o->d2r ? (r > 63 ? 63 : r) : 0;
    r = 4 * o->rr + 2 + ksr; o->rate[EG_RELEASE] = r > 63 ? 63 : r;
}

static inline int pm_delta(int fnum, int pms, int pm)
{
    int d = (int)(((uint32_t)fnum * pm_mul[pms][pm < 0 ? -pm : pm]) >> 20);
    return pm < 0 ? -d : d;
}

/* phase steps and key-scaled rates of a channel's operators */
static void ch_freq(int c)
{
    ym_ch *ch = &ym.ch[c];
    bool special = c == 2 && (ym.mode & 0xc0);
    for (int i = 0; i < 4; i++) {
        int fnum = ch->fnum, block = ch->block;
        if (special && i < 3) {
            static const uint8_t src[3] = { 1, 2, 0 };
            fnum = ym.fnum3[src[i]];
            block = ym.block3[src[i]];
        }
        int kc = keycode(block, fnum);
        int f = fnum;
        if (ch->pms && ym.lfo_pm)
            f += pm_delta(fnum, ch->pms, ym.lfo_pm);
        op_freq(&ch->op[i], f, block, kc);
        op_rates(&ch->op[i], kc);
    }
}

/* ---- envelope ------------------------------------------------------------ */

static void key_on(ym_ch *ch, ym_op *o)
{
    if (o->key || o->csm)
        return;
    o->phase = 0;
    o->ssg_inv = 0;
    if (o->rate[EG_ATTACK] >= 62) {     /* instant attack */
        o->att = 0;
        o->state = EG_DECAY;
    } else {
        o->state = EG_ATTACK;
    }
    ch->active = 1;
}

static void key_off(ym_op *o)
{
    if (o->key || o->csm)
        return;
    if (o->state == EG_RELEASE)
        return;
    /* an inverted SSG-EG envelope continues from what was heard */
    if ((o->ssg & 8) && (o->ssg_inv ^ ((o->ssg >> 2) & 1)))
        o->att = (0x200 - o->att) & 0x3ff;
    o->ssg_inv = 0;
    o->state = EG_RELEASE;
}

static inline void eg_update(ym_op *o, uint32_t cnt)
{
    /* decay ends at the sustain level whatever the rate (SL 0 with D1R 0
     * goes straight on to D2R) */
    if (o->state == EG_DECAY && o->att >= o->sl_att)
        o->state = EG_SUSTAIN;
    int r = o->rate[o->state];
    if (r >= 2) {
        int sh = r < 48 ? 11 - (r >> 2) : 0;
        if (!(cnt & ((1u << sh) - 1))) {
            int inc = eg_inc[r < 48 ? (r & 3) : r < 60 ? 4 + (r - 48) : 16][(cnt >> sh) & 7];
            if (o->state == EG_ATTACK) {
                o->att = r >= 62 ? 0 : o->att + ((~o->att * inc) >> 4);
                if (o->att <= 0) {
                    o->att = 0;
                    o->state = EG_DECAY;
                }
            } else {
                if (o->ssg & 8) {               /* SSG-EG: 4 times as fast, up to 0x200 */
                    if (o->att < 0x200)
                        o->att += 4 * inc;
                } else {
                    o->att += inc;
                }
                if (o->att > 0x3ff)
                    o->att = 0x3ff;
                if (o->state == EG_DECAY && o->att >= o->sl_att)
                    o->state = EG_SUSTAIN;
            }
        }
    }
    /* SSG-EG at the end of a cycle */
    if ((o->ssg & 8) && o->att >= 0x200 && o->state != EG_RELEASE) {
        if (o->ssg & 1) {                       /* hold: loud if inverted, else silent */
            if (o->ssg & 2)
                o->ssg_inv = 1;
            if (o->state != EG_ATTACK)
                o->att = (o->ssg_inv ^ ((o->ssg >> 2) & 1)) ? 0x200 : 0x3ff;
        } else {
            if (o->ssg & 2)                     /* alternate */
                o->ssg_inv ^= 1;
            else                                /* repeat */
                o->phase = 0;
            if (o->state != EG_ATTACK) {
                if (o->rate[EG_ATTACK] >= 62) {
                    o->att = 0;
                    o->state = EG_DECAY;
                } else {
                    o->state = EG_ATTACK;
                }
            }
        }
    }
}

/* attenuation heard: envelope (SSG inversion), total level, AM */
static inline int op_att(const ym_op *o, int am)
{
    int a = o->att;
    if ((o->ssg & 8) && o->state != EG_RELEASE && (o->ssg_inv ^ ((o->ssg >> 2) & 1)))
        a = (0x200 - a) & 0x3ff;
    a += o->tl_att + (o->am ? am : 0);
    return a;
}

/* ---- operators and channels ---------------------------------------------- */

/* one operator: phase (10 bits) plus modulation, attenuation 0..1023+ */
static inline int op_out(uint32_t phase, int mod, int att)
{
    if (att >= 832)                     /* below the exponent's range */
        return 0;
    uint32_t p = ((phase >> 10) + (uint32_t)mod) & 1023;
    uint32_t q = p & 255;
    if (p & 256)
        q ^= 255;
    uint32_t lv = logsin[q] + ((uint32_t)att << 2);
    uint32_t sh = lv >> 8;
    if (sh >= 13)
        return 0;
    int v = (int)((exptab[lv & 255] << 2) >> sh);
    return (p & 512) ? -v : v;
}

#define OUT9(x) ((x) & ~0x1f)           /* the DAC's 9 bits */

static inline int ch_out(ym_ch *ch, int am)
{
    ym_op *o = ch->op;
    int a1 = op_att(&o[0], am), a2 = op_att(&o[1], am);
    int a3 = op_att(&o[2], am), a4 = op_att(&o[3], am);

    /* operator 1, with feedback from its last two outputs */
    int fbm = ch->fb ? (ch->op1_out[0] + ch->op1_out[1]) >> (10 - ch->fb) : 0;
    int m1 = op_out(o[0].phase, fbm, a1);
    ch->op1_out[0] = ch->op1_out[1];
    ch->op1_out[1] = m1;

    int o2, o3, o4, out;
    switch (ch->alg) {
    case 0:     /* 1 > 2 > (MEM) > 3 > 4 */
        o3 = op_out(o[2].phase, ch->mem >> 1, a3);
        o2 = op_out(o[1].phase, m1 >> 1, a2);
        ch->mem = o2;
        o4 = op_out(o[3].phase, o3 >> 1, a4);
        out = OUT9(o4);
        break;
    case 1:     /* (1 + 2) > (MEM) > 3 > 4 */
        o3 = op_out(o[2].phase, ch->mem >> 1, a3);
        o2 = op_out(o[1].phase, 0, a2);
        ch->mem = m1 + o2;
        o4 = op_out(o[3].phase, o3 >> 1, a4);
        out = OUT9(o4);
        break;
    case 2:     /* (1 + (2 > (MEM) > 3)) > 4 */
        o3 = op_out(o[2].phase, ch->mem >> 1, a3);
        o2 = op_out(o[1].phase, 0, a2);
        ch->mem = o2;
        o4 = op_out(o[3].phase, (m1 + o3) >> 1, a4);
        out = OUT9(o4);
        break;
    case 3:     /* ((1 > 2 > (MEM)) + 3) > 4 */
        o3 = op_out(o[2].phase, 0, a3);
        o2 = op_out(o[1].phase, m1 >> 1, a2);
        o4 = op_out(o[3].phase, (o3 + ch->mem) >> 1, a4);
        ch->mem = o2;
        out = OUT9(o4);
        break;
    case 4:     /* (1 > 2) + (3 > 4) */
        o3 = op_out(o[2].phase, 0, a3);
        o2 = op_out(o[1].phase, m1 >> 1, a2);
        o4 = op_out(o[3].phase, o3 >> 1, a4);
        out = OUT9(o2) + OUT9(o4);
        break;
    case 5:     /* 1 > 2, 1 > (MEM) > 3, 1 > 4 */
        o3 = op_out(o[2].phase, ch->mem >> 1, a3);
        ch->mem = m1;
        o2 = op_out(o[1].phase, m1 >> 1, a2);
        o4 = op_out(o[3].phase, m1 >> 1, a4);
        out = OUT9(o2) + OUT9(o3) + OUT9(o4);
        break;
    case 6:     /* (1 > 2) + 3 + 4 */
        o3 = op_out(o[2].phase, 0, a3);
        o2 = op_out(o[1].phase, m1 >> 1, a2);
        o4 = op_out(o[3].phase, 0, a4);
        out = OUT9(o2) + OUT9(o3) + OUT9(o4);
        break;
    default:    /* 1 + 2 + 3 + 4 */
        o3 = op_out(o[2].phase, 0, a3);
        o2 = op_out(o[1].phase, 0, a2);
        o4 = op_out(o[3].phase, 0, a4);
        out = OUT9(m1) + OUT9(o2) + OUT9(o3) + OUT9(o4);
        break;
    }
    for (int i = 0; i < 4; i++)
        o[i].phase += o[i].inc;
    if (out > 8191) out = 8191;
    if (out < -8192) out = -8192;
    return out;
}

/* ---- timers -------------------------------------------------------------- */

static void csm_key_on(void)
{
    ym_ch *ch = &ym.ch[2];
    for (int i = 0; i < 4; i++) {
        ym_op *o = &ch->op[i];
        if (!o->key && !o->csm) {
            key_on(ch, o);
            o->csm = 1;
        }
    }
}

static void csm_key_off(void)
{
    ym_ch *ch = &ym.ch[2];
    for (int i = 0; i < 4; i++) {
        ym_op *o = &ch->op[i];
        if (o->csm) {
            o->csm = 0;
            key_off(o);
        }
    }
}

/* n samples of timers A and B; CSM only where the chip synthesises */
static void timers_run(int n, bool csm)
{
    if (ym.mode & 1) {
        ym.ta_cnt -= n;
        while (ym.ta_cnt <= 0) {
            ym.ta_cnt += 1024 - ym.ta;
            if (ym.mode & 4)
                ym.status |= 1;
            if (csm && (ym.mode & 0xc0) == 0x80)
                csm_key_on();
        }
    }
    if (ym.mode & 2) {
        ym.tb_cnt -= n;
        while (ym.tb_cnt <= 0) {
            ym.tb_cnt += (256 - ym.tb) << 4;
            if (ym.mode & 8)
                ym.status |= 2;
        }
    }
}

/* ---- LFO ----------------------------------------------------------------- */

static void lfo_set(int cnt)
{
    ym.lfo_cnt = (uint8_t)cnt;
    int t = cnt & 63;
    ym.lfo_am = (uint8_t)(2 * ((cnt & 64) ? t : 63 - t));
    int p = cnt >> 2;
    int mag = (p & 8) ? 7 - (p & 7) : (p & 7);
    int pm = (p & 16) ? -mag : mag;
    if (pm != ym.lfo_pm) {
        ym.lfo_pm = (int8_t)pm;
        for (int c = 0; c < 6; c++)
            if (ym.ch[c].pms)
                ch_freq(c);
    }
}

/* ---- synthesis ----------------------------------------------------------- */

static YM_CODE void render(int16_t *buf, int n)
{
    for (int s = 0; s < n; s++) {
        int am = ym.lfo_am;
        int mix = 0;
        const int ladder = ym.chip_type == YM2612_DISCRETE ? 4 << 5 : 0;
        for (int c = 0; c < 6; c++) {
            ym_ch *ch = &ym.ch[c];
            int out = 0;
            if (c == 5 && ym.dac_on) {
                out = ym.dac_out;
                if (ch->active)                 /* keep its operators running */
                    for (int i = 0; i < 4; i++)
                        ch->op[i].phase += ch->op[i].inc;
            } else if (ch->active) {
                out = ch_out(ch, am >> ams_shift[ch->ams]);
            }
            if (ch->pan & 0xc0)
                mix += out + (out < 0 ? -ladder : ladder);
        }
        mix >>= 1;
        buf[s] = (int16_t)(mix > 32767 ? 32767 : mix < -32768 ? -32768 : mix);

        /* LFO */
        if (ym.lfo_on && ++ym.lfo_div >= lfo_period[ym.lfo_freq]) {
            ym.lfo_div = 0;
            lfo_set((ym.lfo_cnt + 1) & 127);
        }

        /* envelopes, every 3 samples */
        if (++ym.eg_div >= 3) {
            ym.eg_div = 0;
            uint32_t cnt = ++ym.eg_cnt;
            for (int c = 0; c < 6; c++) {
                ym_ch *ch = &ym.ch[c];
                if (!ch->active)
                    continue;
                bool audible = false;
                for (int i = 0; i < 4; i++) {
                    ym_op *o = &ch->op[i];
                    eg_update(o, cnt);
                    if (o->state != EG_RELEASE || o->att < 0x3ff)
                        audible = true;
                }
                if (!audible) {                 /* all released and at the bottom: silent until the next key on */
                    ch->active = 0;
                    ch->op1_out[0] = ch->op1_out[1] = ch->mem = 0;
                }
            }
        }

        /* timers; a CSM key on lasts one sample unless timer A overflows again */
        csm_key_off();
        timers_run(1, true);
    }
}

/* ---- registers ----------------------------------------------------------- */

static void write_op(int r, int v)
{
    int c = r & 3;
    if (c == 3)
        return;
    if (r & 0x100)
        c += 3;
    ym_ch *ch = &ym.ch[c];
    ym_op *o = &ch->op[slot_op[(r >> 2) & 3]];
    switch (r & 0xf0) {
    case 0x30:
        o->dt = (v >> 4) & 7;
        o->mul = v & 15;
        ch_freq(c);
        break;
    case 0x40:
        o->tl_att = (uint16_t)((v & 0x7f) << 3);
        break;
    case 0x50:
        o->ks = (v >> 6) & 3;
        o->ar = v & 0x1f;
        ch_freq(c);
        break;
    case 0x60:
        o->am = (v >> 7) & 1;
        o->d1r = v & 0x1f;
        ch_freq(c);
        break;
    case 0x70:
        o->d2r = v & 0x1f;
        ch_freq(c);
        break;
    case 0x80: {
        int sl = (v >> 4) & 15;
        o->sl_att = (uint16_t)(sl == 15 ? 0x3e0 : sl << 5);
        o->rr = v & 15;
        ch_freq(c);
        break;
    }
    case 0x90:
        o->ssg = v & 15;
        break;
    }
}

static void write_ch(int r, int v)
{
    int c = r & 3;
    if (c == 3)
        return;
    int cc = c + ((r & 0x100) ? 3 : 0);
    ym_ch *ch = &ym.ch[cc];
    switch (r & 0xfc) {
    case 0xa0:
        ch->fnum = (uint16_t)(((ym.fhi & 7) << 8) | v);
        ch->block = (ym.fhi >> 3) & 7;
        ch_freq(cc);
        break;
    case 0xa4:
        ym.fhi = (uint8_t)(v & 0x3f);
        break;
    case 0xa8:
        if (!(r & 0x100)) {
            ym.fnum3[c] = (uint16_t)(((ym.fhi3 & 7) << 8) | v);
            ym.block3[c] = (ym.fhi3 >> 3) & 7;
            ch_freq(2);
        }
        break;
    case 0xac:
        if (!(r & 0x100))
            ym.fhi3 = (uint8_t)(v & 0x3f);
        break;
    case 0xb0:
        ch->alg = v & 7;
        ch->fb = (v >> 3) & 7;
        break;
    case 0xb4:
        ch->pan = (uint8_t)(v & 0xc0);
        ch->ams = (v >> 4) & 3;
        ch->pms = v & 7;
        ch_freq(cc);
        break;
    }
}

static void write_reg(int r, int v)
{
    if (r < 0x30) {
        if (r & 0x100)
            return;                     /* 0x20-0x2F only on port 0 */
        switch (r) {
        case 0x22:
            ym.lfo_on = (v >> 3) & 1;
            ym.lfo_freq = v & 7;
            if (!ym.lfo_on) {
                ym.lfo_div = 0;
                lfo_set(0);
            }
            break;
        case 0x24:
            ym.ta = (uint16_t)((ym.ta & 3) | (v << 2));
            break;
        case 0x25:
            ym.ta = (uint16_t)((ym.ta & 0x3fc) | (v & 3));
            break;
        case 0x26:
            ym.tb = (uint16_t)v;
            break;
        case 0x27: {
            if ((v & 1) && !(ym.mode & 1))
                ym.ta_cnt = 1024 - ym.ta;
            if ((v & 2) && !(ym.mode & 2))
                ym.tb_cnt = (256 - ym.tb) << 4;
            if (v & 0x10)
                ym.status &= ~1;
            if (v & 0x20)
                ym.status &= ~2;
            bool special = (ym.mode ^ v) & 0xc0;
            ym.mode = (uint8_t)v;
            if (special)
                ch_freq(2);
            break;
        }
        case 0x28: {
            int c = v & 3;
            if (c == 3)
                break;
            if (v & 4)
                c += 3;
            ym_ch *ch = &ym.ch[c];
            for (int i = 0; i < 4; i++) {
                ym_op *o = &ch->op[i];
                if (v & (0x10 << i)) {
                    key_on(ch, o);
                    o->key = 1;
                } else if (o->key) {
                    o->key = 0;
                    key_off(o);
                }
            }
            break;
        }
        case 0x2a:
            ym.dac_out = (v - 0x80) << 6;
            break;
        case 0x2b:
            ym.dac_on = (v >> 7) & 1;
            break;
        }
        return;
    }
    if ((r & 0xff) < 0xa0)
        write_op(r, v);
    else
        write_ch(r, v);
}

/* ---- interface ----------------------------------------------------------- */

void YM2612Init(void)
{
    memset(&ym, 0, sizeof ym);
}

void YM2612Config(unsigned char type)
{
    ym.chip_type = type;        /* discrete: with the DAC's ladder effect */
}

void YM2612ResetChip(void)
{
    uint8_t type = ym.chip_type;
    memset(&ym, 0, sizeof ym);
    ym.chip_type = type;
    for (int c = 0; c < 6; c++)
        for (int i = 0; i < 4; i++) {
            ym.ch[c].op[i].att = 0x3ff;
            ym.ch[c].op[i].state = EG_RELEASE;
        }
    lfo_set(0);
    for (int r = 0xb6; r >= 0xb4; r--) {
        write_reg(r, 0xc0);
        write_reg(r | 0x100, 0xc0);
    }
    for (int r = 0xb2; r >= 0x30; r--) {
        write_reg(r, 0);
        write_reg(r | 0x100, 0);
    }
}

/* PBC: the YM2612 entry points run from RAM; the Z80 calls them for
 * every register access (Sonic 2 streams DAC samples through them). */
YM_CODE void ym2612_run(int target)
{
    if (ym2612_clock >= target)
        return;

#if defined(PBC_YM_CORE1) && !defined(PBC_YM_SYNTH)
    /* PBC: core 0's copy only keeps the timers; with sound on, core 1
     * synthesises from the write log (src/ym_core1.c). */
    if (true) {
#else
    /* with sound off only the timers run: sound drivers poll their flags */
    extern bool audio_enabled;
    if (!audio_enabled) {
#endif
        int n = (target - ym2612_clock) / AUDIO_FREQ_DIVISOR;
        if (n <= 0)
            return;
        timers_run(n, false);
        ym2612_clock += n * AUDIO_FREQ_DIVISOR;
        return;
    }

    int prev = ym2612_index;
    ym2612_index += (target - ym2612_clock) / AUDIO_FREQ_DIVISOR;
    if (ym2612_index > GWENESIS_AUDIO_BUFFER_MAX)
        ym2612_index = GWENESIS_AUDIO_BUFFER_MAX;
    if (ym2612_index > prev) {
        render(gwenesis_ym2612_buffer + prev, ym2612_index - prev);
        ym2612_clock = ym2612_index * AUDIO_FREQ_DIVISOR;
    } else {
        ym2612_index = prev;
    }
}

YM_CODE void YM2612Write(unsigned int a, unsigned int v, int target)
{
#if defined(PBC_YM_CORE1) && !defined(PBC_YM_SYNTH)
    {
        extern bool audio_enabled;
        if (audio_enabled)
            ym_core1_log_ym(a, v, target);   /* core 1 replays it into its copy */
    }
#endif
    if (GWENESIS_AUDIO_ACCURATE == 1)
        ym2612_run(target);

    v &= 0xff;
    switch (a & 3) {
    case 0:
        ym.addr = (uint16_t)v;
        break;
    case 2:
        ym.addr = (uint16_t)(v | 0x100);
        break;
    default:                            /* either data port writes the latched address */
        write_reg(ym.addr, (int)v);
        break;
    }
}

YM_CODE unsigned int YM2612Read(int target)
{
    if (GWENESIS_AUDIO_ACCURATE == 1)
        ym2612_run(target);
    return ym.status;
}

void gwenesis_ym2612_save_state()
{
    SaveState *state = saveGwenesisStateOpenForWrite("ym2612");
    saveGwenesisStateSetBuffer(state, "ym2612", &ym, sizeof ym);
}

void gwenesis_ym2612_load_state()
{
    SaveState *state = saveGwenesisStateOpenForRead("ym2612");
    saveGwenesisStateGetBuffer(state, "ym2612", &ym, sizeof ym);
}
