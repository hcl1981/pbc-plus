#!/usr/bin/env python3
"""Uebersetzt die BulletML-Muster nach src/bml_data.c.

Die Vorlage laedt zur Laufzeit 73 XML-Dateien und wertet sie mit libBulletML
(C++, eigener XML-Parser) aus.  Auf dem Geraet ist beides unnoetig: die Muster
aendern sich nach dem Bauen nie mehr.  Also wird hier uebersetzt -- in flache
Tabellen mit vorgerechneten Ausdruecken in RPN, 16.16-Festkomma.

Aufbau der Ausgabe:
  bml_tok     RPN-Token aller Ausdruecke
  bml_expr    (Offset, Anzahl) je Ausdruck
  bml_node    Knoten aller Aktionen, je Aktion mit OP_END abgeschlossen
  bml_fire    Feuerbefehle
  bml_bullet  Geschossvorlagen
  bml_pidx    Parameterlisten (Indizes in bml_expr)
  bml_top     Einsprungaktionen je Muster
  bml_brg     die Muster selbst (Typ, Einsprungliste, Name)
"""
import os, re, sys
import xml.etree.ElementTree as ET

FP = 16                                     # 16.16-Festkomma
def fx(v):  return int(round(v * (1 << FP)))

# --- Knotenbefehle (muss zu src/bml.h passen) ---
OP_END, OP_WAIT, OP_FIRE, OP_CHDIR, OP_CHSPD, OP_ACCEL, OP_VANISH, \
    OP_REPEAT, OP_ACTION = range(9)
OPNAME = "END WAIT FIRE CHDIR CHSPD ACCEL VANISH REPEAT ACTION".split()

# --- Richtungs-/Geschwindigkeitsarten ---
T_AIM, T_ABS, T_REL, T_SEQ = range(4)
DIRTYPE = {"aim": T_AIM, "absolute": T_ABS, "relative": T_REL, "sequence": T_SEQ}
SPDTYPE = {"absolute": T_ABS, "relative": T_REL, "sequence": T_SEQ}

# --- RPN-Token ---
E_CONST, E_RANK, E_RAND, E_PARAM, E_ADD, E_SUB, E_MUL, E_DIV, E_NEG = range(9)

NONE = 0xFFFF


def strip_ns(tag):
    return tag.split('}')[-1] if '}' in tag else tag


class ExprError(Exception):
    pass


def tokenize(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c.isspace():
            i += 1
        elif c in "+-*/()":
            out.append(c); i += 1
        elif c == '$':
            m = re.match(r'\$(rank|rand|\d)', s[i:])
            if not m:
                raise ExprError("unbekannte Variable in %r" % s)
            out.append('$' + m.group(1)); i += m.end()
        else:
            m = re.match(r'\d*\.?\d+', s[i:])
            if not m:
                raise ExprError("unlesbar: %r in %r" % (s[i:], s))
            out.append(m.group(0)); i += m.end()
    return out


class Parser:
    """Rekursiver Abstieg: Ausdruck -> RPN."""
    def __init__(self, toks):
        self.t, self.i = toks, 0

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else None

    def take(self):
        v = self.peek(); self.i += 1; return v

    def parse(self):
        r = self.expr()
        if self.i != len(self.t):
            raise ExprError("Rest: %r" % self.t[self.i:])
        return r

    def expr(self):
        out = self.term()
        while self.peek() in ('+', '-'):
            op = self.take()
            out = out + self.term() + [(E_ADD if op == '+' else E_SUB, 0)]
        return out

    def term(self):
        out = self.unary()
        while self.peek() in ('*', '/'):
            op = self.take()
            out = out + self.unary() + [(E_MUL if op == '*' else E_DIV, 0)]
        return out

    def unary(self):
        if self.peek() == '-':
            self.take()
            return self.unary() + [(E_NEG, 0)]
        if self.peek() == '+':
            self.take()
            return self.unary()
        return self.atom()

    def atom(self):
        tk = self.take()
        if tk is None:
            raise ExprError("Ausdruck bricht ab")
        if tk == '(':
            r = self.expr()
            if self.take() != ')':
                raise ExprError("Klammer nicht geschlossen")
            return r
        if tk == '$rank':
            return [(E_RANK, 0)]
        if tk == '$rand':
            return [(E_RAND, 0)]
        if tk.startswith('$'):
            return [(E_PARAM, int(tk[1:]) - 1)]
        return [(E_CONST, fx(float(tk)))]


class Compiler:
    def __init__(self):
        self.tok = []
        self.expr = []          # (off, n)
        self.expr_cache = {}
        self.node = []
        self.action = []        # Offset in self.node
        self.fire = []
        self.bullet = []
        self.pidx = []
        self.top = []
        self.brg = []

    # ---- Ausdruecke ----
    def add_expr(self, text):
        if text is None:
            return NONE
        text = text.strip()
        if text == '':
            return NONE
        if text in self.expr_cache:
            return self.expr_cache[text]
        rpn = Parser(tokenize(text)).parse()
        off = len(self.tok)
        self.tok.extend(rpn)
        idx = len(self.expr)
        self.expr.append((off, len(rpn)))
        self.expr_cache[text] = idx
        return idx

    # ---- Hilfen ----
    def child(self, el, name):
        for c in el:
            if strip_ns(c.tag) == name:
                return c
        return None

    def children(self, el, *names):
        return [c for c in el if strip_ns(c.tag) in names]

    def params_of(self, el):
        ps = [self.add_expr(c.text) for c in el if strip_ns(c.tag) == 'param']
        if not ps:
            return NONE, 0
        off = len(self.pidx)
        self.pidx.extend(ps)
        return off, len(ps)

    # ---- Aktionen ----
    def compile_action(self, el, lb, idx=None):
        """Uebersetzt eine Aktion.  idx kann vorab vergeben sein -- noetig,
        damit sich selbst aufrufende Aktionen nicht endlos rekursieren."""
        if idx is None:
            idx = len(self.action)
            self.action.append(None)
        nodes = []
        for c in el:
            t = strip_ns(c.tag)
            if t == 'wait':
                nodes.append((OP_WAIT, 0, self.add_expr(c.text), NONE, NONE))
            elif t == 'vanish':
                nodes.append((OP_VANISH, 0, NONE, NONE, NONE))
            elif t in ('fire', 'fireRef'):
                nodes.append((OP_FIRE, 0, self.compile_fire(c, lb), NONE, NONE))
            elif t == 'changeDirection':
                d = self.child(c, 'direction')
                nodes.append((OP_CHDIR, DIRTYPE.get(d.get('type', 'aim'), T_AIM),
                              self.add_expr(d.text),
                              self.add_expr(self.child(c, 'term').text), NONE))
            elif t == 'changeSpeed':
                sp = self.child(c, 'speed')
                nodes.append((OP_CHSPD, SPDTYPE.get(sp.get('type', 'absolute'), T_ABS),
                              self.add_expr(sp.text),
                              self.add_expr(self.child(c, 'term').text), NONE))
            elif t == 'accel':
                h, v = self.child(c, 'horizontal'), self.child(c, 'vertical')
                ht = SPDTYPE.get(h.get('type', 'absolute'), T_ABS) if h is not None else 0
                vt = SPDTYPE.get(v.get('type', 'absolute'), T_ABS) if v is not None else 0
                nodes.append((OP_ACCEL, ht | (vt << 2),
                              self.add_expr(h.text) if h is not None else NONE,
                              self.add_expr(v.text) if v is not None else NONE,
                              self.add_expr(self.child(c, 'term').text)))
            elif t == 'repeat':
                times = self.add_expr(self.child(c, 'times').text)
                body = self.child(c, 'action')
                if body is not None:
                    bi, po, pn = self.compile_action(body, lb), NONE, 0
                else:
                    ref = self.child(c, 'actionRef')
                    bi = lb.action(ref.get('label'))
                    po, pn = self.params_of(ref)
                nodes.append((OP_REPEAT, pn, times, bi, po))
            elif t in ('action', 'actionRef'):
                if t == 'action':
                    ai, po, pn = self.compile_action(c, lb), NONE, 0
                else:
                    ai = lb.action(c.get('label'))
                    po, pn = self.params_of(c)
                nodes.append((OP_ACTION, pn, ai, po, NONE))
        nodes.append((OP_END, 0, NONE, NONE, NONE))
        off = len(self.node)
        self.node.extend(nodes)
        self.action[idx] = off
        return idx

    def compile_bullet(self, el, lb, idx=None):
        if idx is None:
            idx = len(self.bullet)
            self.bullet.append(None)
        d = self.child(el, 'direction')
        sp = self.child(el, 'speed')
        acts = self.children(el, 'action', 'actionRef')
        if len(acts) == 0:
            ai, po, pn = NONE, NONE, 0
        elif len(acts) == 1 and strip_ns(acts[0].tag) == 'action':
            ai, po, pn = self.compile_action(acts[0], lb), NONE, 0
        elif len(acts) == 1:
            ai = lb.action(acts[0].get('label'))
            po, pn = self.params_of(acts[0])
        else:
            wrap = ET.Element('action')           # mehrere Aktionen einhuellen
            for a in acts:
                wrap.append(a)
            ai, po, pn = self.compile_action(wrap, lb), NONE, 0
        self.bullet[idx] = dict(
            dir=self.add_expr(d.text) if d is not None else NONE,
            dtype=DIRTYPE.get(d.get('type', 'aim'), T_AIM) if d is not None else T_AIM,
            spd=self.add_expr(sp.text) if sp is not None else NONE,
            stype=SPDTYPE.get(sp.get('type', 'absolute'), T_ABS) if sp is not None else T_ABS,
            action=ai, poff=po, pn=pn)
        return idx

    def compile_fire(self, el, lb):
        if strip_ns(el.tag) == 'fireRef':
            src = lb.fire_el[el.get('label')]
            fpo, fpn = self.params_of(el)
        else:
            src, fpo, fpn = el, NONE, 0
        d = self.child(src, 'direction')
        sp = self.child(src, 'speed')
        b = self.child(src, 'bullet')
        if b is not None:
            bi, bpo, bpn = self.compile_bullet(b, lb), NONE, 0
        else:
            br = self.child(src, 'bulletRef')
            bi = lb.bullet(br.get('label'))
            bpo, bpn = self.params_of(br)
        idx = len(self.fire)
        self.fire.append(dict(
            dir=self.add_expr(d.text) if d is not None else NONE,
            dtype=DIRTYPE.get(d.get('type', 'aim'), T_AIM) if d is not None else T_AIM,
            spd=self.add_expr(sp.text) if sp is not None else NONE,
            stype=SPDTYPE.get(sp.get('type', 'absolute'), T_ABS) if sp is not None else T_ABS,
            bullet=bi, bpoff=bpo, bpn=bpn, fpoff=fpo, fpn=fpn))
        return idx

    # ---- eine Datei ----
    def compile_file(self, path, btype):
        root = ET.parse(path).getroot()
        act_els, bul_els, fire_els = {}, {}, {}
        for c in root:
            t, lab = strip_ns(c.tag), c.get('label')
            if not lab:
                continue
            if t == 'action':
                act_els[lab] = c
            elif t == 'bullet':
                bul_els[lab] = c
            elif t == 'fire':
                fire_els[lab] = c

        lb = Labels(self, act_els, bul_els, fire_els)

        tops = [lb.action(lab) for lab in sorted(act_els) if lab.startswith('top')]
        if not tops:
            raise ExprError("keine Aktion mit Label top*")
        toff = len(self.top)
        self.top.extend(tops)
        self.brg.append(dict(type=btype, toff=toff, tn=len(tops),
                             name=os.path.basename(path)[:-4]))


class Labels:
    """Loest Labelverweise auf und merkt sich das Ergebnis.  Der Index wird
    VOR dem Uebersetzen vergeben, sonst haengt sich eine Aktion auf, die sich
    selbst oder ueber Umwege wieder aufruft."""
    def __init__(self, comp, act_els, bul_els, fire_els):
        self.c = comp
        self.act_els, self.bul_els, self.fire_el = act_els, bul_els, fire_els
        self.ai, self.bi = {}, {}

    def action(self, lab):
        if lab not in self.ai:
            if lab not in self.act_els:
                raise ExprError("unbekanntes actionRef-Label %r" % lab)
            idx = len(self.c.action)
            self.c.action.append(None)
            self.ai[lab] = idx
            self.c.compile_action(self.act_els[lab], self, idx)
        return self.ai[lab]

    def bullet(self, lab):
        if lab not in self.bi:
            if lab not in self.bul_els:
                raise ExprError("unbekanntes bulletRef-Label %r" % lab)
            idx = len(self.c.bullet)
            self.c.bullet.append(None)
            self.bi[lab] = idx
            self.c.compile_bullet(self.bul_els[lab], self, idx)
        return self.bi[lab]


def emit(c, out_path):
    L = []
    A = L.append
    A("/* bml_data.c -- erzeugt von tools/mkbml.py, nicht von Hand aendern.")
    A(" *")
    A(" * Uebersetzte BulletML-Muster aus noiz2sa 0.52 von Kenta Cho.")
    A(" * Die Muster selbst sind Teil des BSD-lizenzierten Pakets, siehe COPYING.")
    A(" */")
    A('#include "bml.h"')
    A("")
    A("const bml_tok_t bml_tok[%d] = {" % len(c.tok))
    for i in range(0, len(c.tok), 4):
        A("    " + " ".join("{%d,%11d}," % t for t in c.tok[i:i + 4]))
    A("};")
    A("const bml_expr_t bml_expr[%d] = {" % len(c.expr))
    for i in range(0, len(c.expr), 8):
        A("    " + " ".join("{%5d,%2d}," % e for e in c.expr[i:i + 8]))
    A("};")
    A("const bml_node_t bml_node[%d] = {" % len(c.node))
    for n in c.node:
        A("    {%d,%2d,%5d,%5d,%5d}, /* %s */" % (n[0], n[1], n[2], n[3], n[4], OPNAME[n[0]]))
    A("};")
    A("const uint16_t bml_action[%d] = {" % len(c.action))
    for i in range(0, len(c.action), 12):
        A("    " + " ".join("%5d," % a for a in c.action[i:i + 12]))
    A("};")
    A("const bml_fire_t bml_fire[%d] = {" % len(c.fire))
    for f in c.fire:
        A("    {%5d,%5d,%5d,%5d,%5d,%d,%d,%d,%d}," % (
            f['dir'], f['spd'], f['bullet'], f['bpoff'], f['fpoff'],
            f['dtype'], f['stype'], f['bpn'], f['fpn']))
    A("};")
    A("const bml_bullet_t bml_bullet[%d] = {" % len(c.bullet))
    for b in c.bullet:
        A("    {%5d,%5d,%5d,%5d,%d,%d,%d}," % (
            b['dir'], b['spd'], b['action'], b['poff'], b['dtype'], b['stype'], b['pn']))
    A("};")
    A("const uint16_t bml_pidx[%d] = {" % max(1, len(c.pidx)))
    if c.pidx:
        for i in range(0, len(c.pidx), 12):
            A("    " + " ".join("%5d," % p for p in c.pidx[i:i + 12]))
    else:
        A("    0,")
    A("};")
    A("const uint16_t bml_top[%d] = {" % len(c.top))
    for i in range(0, len(c.top), 12):
        A("    " + " ".join("%5d," % t for t in c.top[i:i + 12]))
    A("};")
    A("const bml_brg_t bml_brg[%d] = {" % len(c.brg))
    for b in c.brg:
        A('    {%d,%5d,%2d,"%s"},' % (b['type'], b['toff'], b['tn'], b['name'][:23]))
    A("};")
    A("const uint16_t bml_brg_num = %d;" % len(c.brg))
    A("")
    open(out_path, "w").write("\n".join(L) + "\n")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.join(here, "..")
    src = os.path.join(root, "data_bml_src")
    c = Compiler()
    counts = []
    for btype, d in enumerate(("zako", "middle", "boss")):
        files = sorted(f for f in os.listdir(os.path.join(src, d)) if f.endswith(".xml"))
        for f in files:
            try:
                c.compile_file(os.path.join(src, d, f), btype)
            except Exception as e:
                sys.exit("%s/%s: %s" % (d, f, e))
        counts.append((d, len(files)))
    out = os.path.join(root, "src", "bml_data.c")
    emit(c, out)
    print("geschrieben: src/bml_data.c")
    for d, n in counts:
        print("  %-7s %2d Muster" % (d, n))
    print("  %5d Token  %4d Ausdruecke  %5d Knoten  %4d Aktionen" %
          (len(c.tok), len(c.expr), len(c.node), len(c.action)))
    print("  %5d Feuerbefehle  %4d Geschossvorlagen  %4d Parameter  %3d Einspruenge"
          % (len(c.fire), len(c.bullet), len(c.pidx), len(c.top)))
    b = (len(c.tok) * 8 + len(c.expr) * 4 + len(c.node) * 8 + len(c.action) * 2 +
         len(c.fire) * 14 + len(c.bullet) * 12 + len(c.pidx) * 2 + len(c.top) * 2)
    print("  rund %d B Flash" % b)


if __name__ == "__main__":
    main()
