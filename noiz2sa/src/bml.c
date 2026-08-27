/* bml.c -- Ausfuehrung der uebersetzten BulletML-Muster.
 *
 * Nachgebaut ist das Verhalten von BulletMLRunner: je Tick werden zuerst die
 * laufenden Aenderungen (changeDirection, changeSpeed, accel) fortgeschrieben,
 * danach laeuft der Aktionsstapel bis zum naechsten <wait>.
 *
 * Ein Laeufer entspricht genau EINER Einsprungaktion.  Ein Gegner mit mehreren
 * top-Aktionen bekommt entsprechend mehrere Laeufer -- in der Vorlage macht
 * libBulletML dasselbe mit mehreren Runner-Instanzen.
 */
#include <string.h>
#include "bml.h"

static bml_runner_t runner[BML_MAX_RUNNER];
static int runner_used;

void bml_runner_init(void)
{
    memset(runner, 0, sizeof runner);
    runner_used = 0;
}

int bml_runner_used(void) { return runner_used; }

int bml_runner_alloc(uint16_t action, const int32_t *param, uint8_t pn)
{
    int i;
    if (action == BML_NONE)
        return -1;
    for (i = 0; i < BML_MAX_RUNNER; i++) {
        bml_runner_t *r = &runner[i];
        if (r->used)
            continue;
        memset(r, 0, sizeof *r);
        r->used = 1;
        r->depth = 1;
        r->st[0].node = bml_action[action];
        r->st[0].start = r->st[0].node;
        r->st[0].rep_left = 1;
        r->st[0].pn = pn;
        if (param && pn) {
            if (pn > BML_MAX_PARAM) pn = BML_MAX_PARAM;
            memcpy(r->st[0].param, param, (size_t)pn * sizeof(int32_t));
            r->st[0].pn = pn;
        }
        runner_used++;
        return i;
    }
    return -1;                             /* Vorrat erschoepft: kein Laeufer */
}

void bml_runner_free(int r)
{
    if (r < 0 || r >= BML_MAX_RUNNER || !runner[r].used)
        return;
    runner[r].used = 0;
    runner_used--;
}

int bml_runner_done(int r)
{
    return (r < 0 || r >= BML_MAX_RUNNER || !runner[r].used || runner[r].depth == 0);
}

/* ---- Ausdruecke --------------------------------------------------------- */
static int32_t eval(uint16_t e, const bml_frame_t *fr, int32_t rank)
{
    int32_t stack[16];
    int sp = 0;
    unsigned i;
    const bml_expr_t *ex;

    if (e == BML_NONE)
        return 0;
    ex = &bml_expr[e];
    for (i = 0; i < ex->n; i++) {
        const bml_tok_t *t = &bml_tok[ex->off + i];
        switch (t->op) {
        case BE_CONST: if (sp < 16) stack[sp++] = t->val; break;
        case BE_RANK:  if (sp < 16) stack[sp++] = rank; break;
        case BE_RAND:  if (sp < 16) stack[sp++] = (int32_t)(bml_random() & 0xffffu); break;
        case BE_PARAM:
            if (sp < 16)
                stack[sp++] = (fr && t->val < fr->pn) ? fr->param[t->val] : 0;
            break;
        case BE_NEG:   if (sp >= 1) stack[sp - 1] = -stack[sp - 1]; break;
        case BE_ADD:   if (sp >= 2) { stack[sp - 2] += stack[sp - 1]; sp--; } break;
        case BE_SUB:   if (sp >= 2) { stack[sp - 2] -= stack[sp - 1]; sp--; } break;
        case BE_MUL:   if (sp >= 2) { stack[sp - 2] = bml_mul(stack[sp - 2], stack[sp - 1]); sp--; } break;
        case BE_DIV:   if (sp >= 2) { stack[sp - 2] = bml_div(stack[sp - 2], stack[sp - 1]); sp--; } break;
        default: break;
        }
    }
    return sp ? stack[sp - 1] : 0;
}

/* Parameterliste auswerten, im Geltungsbereich des aufrufenden Rahmens */
static uint8_t eval_params(uint16_t off, uint8_t n, const bml_frame_t *fr,
                           int32_t rank, int32_t *out)
{
    uint8_t i;
    if (off == BML_NONE || n == 0)
        return 0;
    if (n > BML_MAX_PARAM)
        n = BML_MAX_PARAM;
    for (i = 0; i < n; i++)
        out[i] = eval(bml_pidx[off + i], fr, rank);
    return n;
}

/* ---- Winkel ------------------------------------------------------------- */
#define DEG360 (360 << BML_FP)
#define DEG180 (180 << BML_FP)

static int32_t norm180(int32_t d)
{
    while (d > DEG180)  d -= DEG360;
    while (d <= -DEG180) d += DEG360;
    return d;
}

/* ---- Feuern ------------------------------------------------------------- */
static void do_fire(const bml_node_t *nd, bml_runner_t *r, bml_frame_t *fr,
                    bml_state_t *st, void *owner)
{
    const bml_fire_t *f = &bml_fire[nd->a];
    const bml_bullet_t *b = (f->bullet != BML_NONE) ? &bml_bullet[f->bullet] : 0;
    bml_frame_t scope;                     /* Geltungsbereich fuer die Ausdruecke */
    int32_t bparam[BML_MAX_PARAM];
    uint8_t bpn;
    int32_t dir, spd;
    uint16_t dexp, sexp;
    uint8_t dtype, stype;

    /* fireRef bringt eigene Parameter mit; ein eingebettetes <fire> erbt den
     * Geltungsbereich der umgebenden Aktion. */
    if (f->fpoff != BML_NONE && f->fpn) {
        memset(&scope, 0, sizeof scope);
        scope.pn = eval_params(f->fpoff, f->fpn, fr, st->rank, scope.param);
    } else {
        scope = *fr;
    }

    /* Parameter des bulletRef, im Geltungsbereich des Feuerbefehls */
    bpn = eval_params(f->bpoff, f->bpn, &scope, st->rank, bparam);

    /* Richtung: erst der Feuerbefehl, dann die Geschossvorlage, sonst die
     * aktuelle Richtung des Schuetzen. */
    if (f->dir != BML_NONE) {
        dexp = f->dir; dtype = f->dtype;
        dir = eval(dexp, &scope, st->rank);
    } else if (b && b->dir != BML_NONE) {
        bml_frame_t bs;
        memset(&bs, 0, sizeof bs);
        bs.pn = bpn;
        memcpy(bs.param, bparam, sizeof bparam);
        dexp = b->dir; dtype = b->dtype;
        dir = eval(dexp, &bs, st->rank);
    } else {
        dtype = 0xff; dir = 0;
    }
    switch (dtype) {
    case BT_AIM: dir = st->aim + dir; break;
    case BT_ABS: break;
    case BT_REL: dir = st->dir + dir; break;
    case BT_SEQ: dir = r->prev_dir + dir; break;
    default:     dir = st->dir; break;
    }

    /* Geschwindigkeit: analog, Vorgabe ist 1. */
    if (f->spd != BML_NONE) {
        sexp = f->spd; stype = f->stype;
        spd = eval(sexp, &scope, st->rank);
    } else if (b && b->spd != BML_NONE) {
        bml_frame_t bs;
        memset(&bs, 0, sizeof bs);
        bs.pn = bpn;
        memcpy(bs.param, bparam, sizeof bparam);
        sexp = b->spd; stype = b->stype;
        spd = eval(sexp, &bs, st->rank);
    } else {
        stype = 0xff; spd = 0;
    }
    switch (stype) {
    case BT_ABS: break;
    case BT_REL: spd = st->spd + spd; break;
    case BT_SEQ: spd = r->prev_spd + spd; break;
    default:     spd = BML_ONE; break;
    }

    r->prev_dir = dir;
    r->prev_spd = spd;

    /* Hat das Geschoss eine eigene Aktion, laeuft sie mit den Parametern des
     * bulletRef; ein actionRef darin bringt eigene mit. */
    {
        uint16_t act = b ? b->action : BML_NONE;
        int32_t aparam[BML_MAX_PARAM];
        uint8_t apn = 0;
        if (b && b->poff != BML_NONE && b->pn) {
            bml_frame_t bs;
            memset(&bs, 0, sizeof bs);
            bs.pn = bpn;
            memcpy(bs.param, bparam, sizeof bparam);
            apn = eval_params(b->poff, b->pn, &bs, st->rank, aparam);
        } else {
            apn = bpn;
            memcpy(aparam, bparam, sizeof aparam);
        }
        bml_fire_bullet(owner, dir, spd, act, aparam, apn);
    }
}

/* ---- ein Tick ----------------------------------------------------------- */
int bml_runner_step(int r, bml_state_t *st, void *owner)
{
    bml_runner_t *rn;
    int guard = 0;

    if (r < 0 || r >= BML_MAX_RUNNER || !runner[r].used)
        return 0;
    rn = &runner[r];

    /* Laufende Aenderungen fortschreiben -- auch waehrend eines <wait>. */
    if (rn->chg_dir_turns > 0) { st->dir += rn->chg_dir_vel; rn->chg_dir_turns--; }
    if (rn->chg_spd_turns > 0) { st->spd += rn->chg_spd_vel; rn->chg_spd_turns--; }
    if (rn->acc_turns > 0) {
        st->vx += rn->acc_x_vel;
        st->vy += rn->acc_y_vel;
        rn->acc_turns--;
    }

    if (rn->depth == 0)
        return 0;
    if (rn->wait > 0) {
        rn->wait--;
        return 1;
    }

    while (rn->depth > 0) {
        bml_frame_t *fr = &rn->st[rn->depth - 1];
        const bml_node_t *nd = &bml_node[fr->node];

        if (++guard > 2000)                /* Notbremse gegen Endlosschleifen */
            break;

        switch (nd->op) {
        case BOP_END:
            if (fr->rep_left > 1) {
                fr->rep_left--;
                fr->node = fr->start;
            } else {
                rn->depth--;
            }
            continue;

        case BOP_WAIT: {
            int32_t w = eval(nd->a, fr, st->rank) >> BML_FP;
            fr->node++;
            if (w > 0) {
                rn->wait = (int16_t)(w > 32767 ? 32767 : w);
                rn->wait--;                /* dieser Tick zaehlt mit */
                return 1;
            }
            continue;
        }

        case BOP_FIRE:
            do_fire(nd, rn, fr, st, owner);
            fr->node++;
            continue;

        case BOP_CHDIR: {
            int32_t v = eval(nd->a, fr, st->rank);
            int32_t term = eval(nd->b, fr, st->rank) >> BML_FP;
            if (term < 1) term = 1;
            if (nd->flags == BT_SEQ) {
                rn->chg_dir_vel = v;
            } else {
                int32_t target = v;
                if (nd->flags == BT_AIM) target = st->aim + v;
                else if (nd->flags == BT_REL) target = st->dir + v;
                rn->chg_dir_vel = norm180(target - st->dir) / term;
            }
            rn->chg_dir_turns = (int16_t)term;
            fr->node++;
            continue;
        }

        case BOP_CHSPD: {
            int32_t v = eval(nd->a, fr, st->rank);
            int32_t term = eval(nd->b, fr, st->rank) >> BML_FP;
            if (term < 1) term = 1;
            if (nd->flags == BT_SEQ)
                rn->chg_spd_vel = v;
            else if (nd->flags == BT_REL)
                rn->chg_spd_vel = v / term;
            else
                rn->chg_spd_vel = (v - st->spd) / term;
            rn->chg_spd_turns = (int16_t)term;
            fr->node++;
            continue;
        }

        case BOP_ACCEL: {
            int32_t term = eval(nd->c, fr, st->rank) >> BML_FP;
            uint8_t ht = nd->flags & 3, vt = (nd->flags >> 2) & 3;
            if (term < 1) term = 1;
            rn->acc_x_vel = rn->acc_y_vel = 0;
            if (nd->a != BML_NONE) {
                int32_t v = eval(nd->a, fr, st->rank);
                if (ht == BT_SEQ)      rn->acc_x_vel = v;
                else if (ht == BT_REL) rn->acc_x_vel = v / term;
                else                   rn->acc_x_vel = (v - st->vx) / term;
            }
            if (nd->b != BML_NONE) {
                int32_t v = eval(nd->b, fr, st->rank);
                if (vt == BT_SEQ)      rn->acc_y_vel = v;
                else if (vt == BT_REL) rn->acc_y_vel = v / term;
                else                   rn->acc_y_vel = (v - st->vy) / term;
            }
            rn->acc_turns = (int16_t)term;
            fr->node++;
            continue;
        }

        case BOP_VANISH:
            st->vanished = 1;
            fr->node++;
            rn->depth = 0;
            return 0;

        case BOP_REPEAT:
        case BOP_ACTION: {
            int32_t times = (nd->op == BOP_REPEAT)
                            ? (eval(nd->a, fr, st->rank) >> BML_FP) : 1;
            uint16_t act = (nd->op == BOP_REPEAT) ? nd->b : nd->a;
            uint16_t poff = (nd->op == BOP_REPEAT) ? nd->c : nd->b;
            uint8_t  pn = nd->flags;
            int32_t param[BML_MAX_PARAM];
            uint8_t n = eval_params(poff, pn, fr, st->rank, param);

            fr->node++;
            if (times <= 0 || act == BML_NONE || rn->depth >= BML_MAX_DEPTH)
                continue;
            {
                bml_frame_t *nf = &rn->st[rn->depth];
                memset(nf, 0, sizeof *nf);
                nf->node = bml_action[act];
                nf->start = nf->node;
                nf->rep_left = (uint16_t)(times > 65535 ? 65535 : times);
                nf->pn = n;
                if (n)
                    memcpy(nf->param, param, sizeof param);
                else if (nd->op == BOP_REPEAT || poff == BML_NONE) {
                    /* ohne eigene Parameter bleibt der Geltungsbereich erhalten */
                    memcpy(nf->param, fr->param, sizeof nf->param);
                    nf->pn = fr->pn;
                }
                rn->depth++;
            }
            continue;
        }

        default:
            fr->node++;
            continue;
        }
    }

    return rn->depth > 0;
}
