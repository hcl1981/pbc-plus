#!/usr/bin/env python3
"""m68k_gen.py -- builds the 68000 opcode handlers and tables from m68k_in.c.

Musashi ships m68kmake.c, which turns m68k_in.c into handlers for every CPU
from the 68000 to the 68040 and a table the emulator expands into two
64K-entry arrays at start-up (handler pointer and cycle count per CPU type,
several hundred KB). This script reads the same input and writes what the
PicoBoy needs instead:

  core/cpus/musashi/m68kops.c
      only the handlers a 68000 can reach, as static functions, followed by
      the packed dispatch table:
        m68ki_ops[]    one {handler, cycles} pair per distinct combination,
                       cycles already in master clocks (x MUL)
        m68ki_blk[][]  distinct runs of 32 opcodes, as indices into m68ki_ops
        m68ki_top[]    opcode >> 5 -> run
      about 40 KB, placed in .data so it is copied into RAM at start-up.

The expansion follows m68kmake.c (version 4.10): every table line gives one
handler per allowed addressing mode (and per condition for Bcc, DBcc, Scc),
with the same names, masks and 68000 cycle counts (except for word ADD, SUB,
AND, OR, ADDA, SUBA with an immediate operand, see oper_cycles), and the
dispatch table
is filled the way m68ki_build_opcode_table() fills it for the 68000. Lines
the 68000 does not have ("." in its column) are left out: on a 68000 their
handlers only raise the illegal-instruction or line-F exception, which the
handler they would have replaced does as well (checked below).

    tools/m68k_gen.py                 regenerate m68kops.c
    tools/m68k_hot.py build/x.elf     then mark the most used handlers for RAM
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'core', 'cpus', 'musashi')
MUL = 7                     # master clocks per 68000 clock on the Mega Drive
RUN = 32
SEP = 'X' * 79

EA_INFO = [  # fname, ea, mask, match, cycles 000 (unsized, 8/16, 32)
    ('',     '',      0x00, 0x00, (0, 0, 0)),
    ('ai',   'AY_AI', 0x38, 0x10, (0, 4, 8)),
    ('pi',   'AY_PI', 0x38, 0x18, (0, 4, 8)),
    ('pi7',  'A7_PI', 0x3f, 0x1f, (0, 4, 8)),
    ('pd',   'AY_PD', 0x38, 0x20, (0, 6, 10)),
    ('pd7',  'A7_PD', 0x3f, 0x27, (0, 6, 10)),
    ('di',   'AY_DI', 0x38, 0x28, (0, 8, 12)),
    ('ix',   'AY_IX', 0x38, 0x30, (0, 10, 14)),
    ('aw',   'AW',    0x3f, 0x38, (0, 8, 12)),
    ('al',   'AL',    0x3f, 0x39, (0, 12, 16)),
    ('pcdi', 'PCDI',  0x3f, 0x3a, (0, 8, 12)),
    ('pcix', 'PCIX',  0x3f, 0x3b, (0, 10, 14)),
    ('i',    'I',     0x3f, 0x3c, (0, 4, 8)),
]
NONE, AI, PI, PI7, PD, PD7, DI, IX, AW, AL, PCDI, PCIX, I = range(13)
#                 NONE AI PI PI7 PD PD7 DI IX AW AL PCDI PCIX I
JMP_CYC   = (0, 4, 0, 0, 0, 0,  6, 10,  6,  8,  6, 10, 0)
JSR_CYC   = (0, 4, 0, 0, 0, 0,  6, 10,  6,  8,  6, 10, 0)
LEA_CYC   = (0, 4, 0, 0, 0, 0,  8, 12,  8, 12,  8, 12, 0)
PEA_CYC   = (0, 6, 0, 0, 0, 0, 10, 14, 10, 14, 10, 14, 0)
MOVEM_CYC = (0, 0, 0, 0, 0, 0,  4,  6,  4,  8,  0,  0, 0)
CC = ['t', 'f', 'hi', 'ls', 'cc', 'cs', 'ne', 'eq', 'vc', 'vs', 'pl', 'mi',
      'ge', 'lt', 'gt', 'le']


def size_index(size):
    return {0: 0, 8: 1, 16: 1, 32: 2}[size]


class Op:
    pass


def read_input(path):
    lines = open(path, encoding='latin-1').read().split('\n')
    lines = [l.rstrip('\r') for l in lines]
    start = lines.index('M68KMAKE_TABLE_START') + 1
    end = lines.index(SEP, start)
    table = []
    for l in lines[start:end]:
        if not l.strip():
            continue
        f = l.split()
        op = Op()
        op.name, op.size, op.spec_proc, op.spec_ea, pattern, op.ea_allowed = \
            f[0], int(f[1]), f[2], f[3], f[4], f[5]
        op.cpu_mode = f[6:11]
        c000 = f[11]
        op.has_000 = c000 != '.'
        op.cycles = int(c000) if op.has_000 else 0
        op.mask = sum((ch != '.') << (15 - i) for i, ch in enumerate(pattern))
        op.match = sum((ch == '1') << (15 - i) for i, ch in enumerate(pattern))
        table.append(op)
    body_start = lines.index('M68KMAKE_OPCODE_HANDLER_BODY') + 1
    prims, i = [], body_start
    while i < len(lines):
        l = lines[i]
        if l == SEP or l == 'M68KMAKE_END':
            break
        if 'M68KMAKE_OP(' in l:
            m = re.search(r'M68KMAKE_OP\(([^,]*),\s*(\d+),\s*([^,]*),\s*([^)]*)\)', l)
            body = []
            i += 1
            while True:
                body.append(lines[i])
                if lines[i].startswith('}'):
                    break
                i += 1
            prims.append((m.group(1), int(m.group(2)), m.group(3), m.group(4), body))
        i += 1
    return table, prims


def oper_cycles(op, ea):
    if not op.has_000:
        return 0
    s = size_index(op.size)
    base = op.cycles
    # m68kmake adds 2 to every size here; the 68000 manual has the 2 extra
    # clocks (8 instead of 6 base) only for long operations, as with Dn
    if ea == I and op.size == 32 and ((op.name in ('add', 'and', 'or', 'sub') and
                                       op.spec_proc == 'er') or op.name in ('adda', 'suba')):
        return base + EA_INFO[ea][4][s] + 2
    for name, tab in (('jmp', JMP_CYC), ('jsr', JSR_CYC), ('lea', LEA_CYC),
                      ('pea', PEA_CYC), ('movem', MOVEM_CYC)):
        if op.name == name:
            return base + tab[ea]
    return base + EA_INFO[ea][4][s]


def write_body(body, replace):
    out = []
    for line in body:
        if 'M68KMAKE' in line:
            found = False
            for a, b in replace:
                p = line.find(a)
                if p >= 0:
                    found = True
                    line = line[:p] + b + line[p + len(a):]
            if not found:
                sys.exit(f'unknown M68KMAKE directive: {line}')
        out.append(line)
    return out


def generate(table, prims):
    handlers = {}     # name -> (op, body lines)
    entries = []      # (name, mask, match, cycles, has_000)

    def handler(src, body, replace, ea):
        op = Op()
        op.__dict__.update(src.__dict__)
        op.cycles = oper_cycles(src, ea)
        if op.spec_ea == '.' and ea != NONE:
            op.spec_ea = EA_INFO[ea][0]
        op.mask = src.mask | EA_INFO[ea][2]
        op.match = src.match | EA_INFO[ea][3]
        name = f'm68k_op_{op.name}'
        if op.size > 0:
            name += f'_{op.size}'
        if op.spec_proc != '.':
            name += f'_{op.spec_proc}'
        if op.spec_ea != '.':
            name += f'_{op.spec_ea}'
        rep = list(replace)
        if ea != NONE:
            e = EA_INFO[ea][1]
            rep += [('M68KMAKE_GET_EA_AY_8', f'EA_{e}_8()'),
                    ('M68KMAKE_GET_EA_AY_16', f'EA_{e}_16()'),
                    ('M68KMAKE_GET_EA_AY_32', f'EA_{e}_32()'),
                    ('M68KMAKE_GET_OPER_AY_8', f'OPER_{e}_8()'),
                    ('M68KMAKE_GET_OPER_AY_16', f'OPER_{e}_16()'),
                    ('M68KMAKE_GET_OPER_AY_32', f'OPER_{e}_32()')]
        assert name not in handlers, name
        handlers[name] = (op, write_body(body, rep))
        entries.append((name, op.mask, op.match, op.cycles, op.has_000))

    def ea_variants(op, body, replace):
        allowed = op.ea_allowed
        if allowed == '..........':
            handler(op, body, replace, NONE)
            return
        order = [(0, 'A', AI), (1, '+', PI), (2, '-', PD), (3, 'D', DI), (4, 'X', IX),
                 (5, 'W', AW), (6, 'L', AL), (7, 'd', PCDI), (8, 'x', PCIX), (9, 'I', I)]
        for pos, ch, ea in order:
            if allowed[pos] == ch:
                handler(op, body, replace, ea)
                if ea in (PI, PD) and op.size == 8:
                    handler(op, body, replace, PI7 if ea == PI else PD7)

    def cc_variants(src, body, offset):
        for i in range(2, 16):
            op = Op()
            op.__dict__.update(src.__dict__)
            op.mask = src.mask | 0x0f00
            op.match = (src.match & 0xf0ff) | (i << 8)
            op.name = src.name[:offset] + CC[i]
            rep = [('M68KMAKE_CC', f'COND_{CC[i].upper()}()'),
                   ('M68KMAKE_NOT_CC', f'COND_NOT_{CC[i].upper()}()')]
            ea_variants(op, body, rep)

    for name, size, spec_proc, spec_ea, body in prims:
        matches = [o for o in table if (o.name, o.size, o.spec_proc, o.spec_ea) ==
                   (name, size, spec_proc, spec_ea)]
        if not matches:
            sys.exit(f'no table entry for {name} {size} {spec_proc} {spec_ea}')
        op = matches[0]
        if op.name in ('bcc', 'scc'):
            cc_variants(op, body, 1)
        elif op.name == 'dbcc':
            cc_variants(op, body, 2)
        elif op.name == 'trapcc':
            cc_variants(op, body, 4)
        else:
            ea_variants(op, body, [])
    return handlers, entries


def build_table(entries):
    """m68ki_build_opcode_table() for the 68000."""
    order = sorted(entries, key=lambda e: (bin(e[1]).count('1'), e[1], e[2]))
    # m68kmake's builder handles the entries from the first 0xff00 mask on
    # with special loops; they are equivalent to the generic one only if
    # what follows uses just those masks, in this order:
    first = next(i for i, e in enumerate(order) if e[1] == 0xff00)
    special = [0xff00, 0xf1f8, 0xfff0, 0xf1ff, 0xfff8, 0xffff]
    k = 0
    for e in order[first:]:
        while special[k] != e[1]:
            k += 1
            assert k < len(special), f'unexpected mask {e[1]:04x} for {e[0]}'
    name = ['m68k_op_illegal'] * 0x10000
    cyc = [0] * 0x10000
    has = [True] * 0x10000
    for n, mask, match, c, h in order:
        for i in range(0x10000):
            if i & mask == match:
                name[i], cyc[i], has[i] = n, c, h
    return name, cyc, has


def last_statement(body):
    code = [l.strip() for l in body
            if l.strip() and not l.strip().startswith(('/*', '*', '//', '#'))]
    code = [l for l in code if l not in ('{', '}', 'else')]   # an else block's end
    return code[-1]


def main():
    table, prims = read_input(os.path.join(SRC, 'm68k_in.c'))
    handlers, entries = generate(table, prims)
    name, cyc, has = build_table(entries)

    # Opcodes whose handler the 68000 does not have: replace them by what a
    # 68000 does there (the handler's last statement after its CPU checks).
    fallback = {'m68ki_exception_illegal();': 'm68k_op_illegal',
                'm68ki_exception_1111();': 'm68k_op_1111',
                'm68ki_exception_1010();': 'm68k_op_1010'}
    replaced = {}
    for i in range(0x10000):
        if not has[i]:
            last = last_statement(handlers[name[i]][1])
            if i >> 12 == 0xf:          # a 68000 traps all of line F
                fb = 'm68k_op_1111'     # (move16 has no CPU check at all)
            elif last in fallback:
                fb = fallback[last]
            else:
                sys.exit(f'{name[i]}: no 68000 fallback for "{last}"')
            replaced.setdefault(name[i], fb)
            name[i] = fb
            # the fallback handler's own cycle count (illegal: 4 + exception)
            # is what the 68000 would charge; m68kmake gives these opcodes 0
            # and the exception undoes the instruction's count, so 0 it stays
            cyc[i] = 0

    used = sorted(set(name), key=lambda n: list(handlers).index(n))

    out = []
    out.append('/* m68kops.c -- written by tools/m68k_gen.py from m68k_in.c; do not edit.')
    out.append(' *')
    out.append(' * Musashi 68000 opcode handlers (MIT licence, see readme.txt) as the')
    out.append(' * 68000 needs them, and the packed dispatch table (see m68k_gen.py). */')
    out.append('')
    out.append('#include "m68kcpu.h"')
    out.append('')
    out.append('/* PBC: the most used handlers run from RAM (tools/m68k_hot.py) */')
    out.append('#define M68K_HOT1 __attribute__((section(".time_critical.m68k_hot")))')
    out.append('#if PBC_MULTI')
    out.append('#define M68K_HOT2 M68K_HOT1')
    out.append('#else')
    out.append('#define M68K_HOT2')
    out.append('#endif')
    out.append('')
    hot = {}
    old = os.path.join(SRC, 'm68kops.c')
    if os.path.exists(old):      # keep the marks of tools/m68k_hot.py
        for m in re.finditer(r'^static (M68K_HOT[12]) void (m68k_op_\w+)\(void\)',
                             open(old).read(), re.M):
            hot[m.group(2)] = m.group(1)
    for n in used:
        tag = hot.get(n)
        out.append(f'static {tag} void {n}(void)' if tag else f'static void {n}(void)')
        out.extend(handlers[n][1])
        out.append('')
        out.append('')

    ops, op_index = [], {}
    for pair in zip(name, cyc):
        if pair not in op_index:
            op_index[pair] = len(ops)
            ops.append(pair)
    runs, run_index, top = [], {}, []
    for i in range(0, 0x10000, RUN):
        run = tuple(op_index[p] for p in zip(name[i:i + RUN], cyc[i:i + RUN]))
        if run not in run_index:
            run_index[run] = len(runs)
            runs.append(run)
        top.append(run_index[run])
    size = len(ops) * 8 + len(runs) * RUN * 2 + len(top) * 2
    out.append(f'/* {len(used)} handlers; {len(ops)} handler/cycle pairs, {len(runs)} runs of')
    out.append(f' * {RUN} opcodes, {size} bytes in RAM. Cycles are master clocks. */')
    out.append('')
    out.append('#define M68KI_IN_RAM __attribute__((section(".data.m68ki_dispatch")))')
    out.append('')
    out.append(f'M68KI_IN_RAM const m68ki_op_t m68ki_ops[{len(ops)}] = {{')
    for h, c in ops:
        out.append(f'  {{{h}, {c * MUL}}},')
    out.append('};')
    out.append('')
    out.append(f'M68KI_IN_RAM const unsigned short m68ki_blk[{len(runs)}][{RUN}] = {{')
    for r in runs:
        out.append('  {' + ','.join(map(str, r)) + '},')
    out.append('};')
    out.append('')
    out.append(f'M68KI_IN_RAM const unsigned short m68ki_top[{len(top)}] = {{')
    for i in range(0, len(top), 16):
        out.append('  ' + ','.join(map(str, top[i:i + 16])) + ',')
    out.append('};')
    out.append('')
    open(os.path.join(SRC, 'm68kops.c'), 'w').write('\n'.join(out))
    print(f'{len(handlers)} handlers generated, {len(used)} used by the 68000; '
          f'{len(ops)} pairs, {len(runs)} runs, {size} bytes')
    for k, v in sorted(replaced.items()):
        print(f'  not on 68000: {k} -> {v}')


main()
