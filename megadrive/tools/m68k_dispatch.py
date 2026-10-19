#!/usr/bin/env python3
"""m68k_dispatch.py -- packs the 68000 opcode tables so they fit in SRAM.

Musashi dispatches through two 64K-entry tables (handler pointer and cycle
count, 320 KB together). On this device they sat in flash and competed for
the 16 KB XIP cache with the emulator code and the game ROM; every opcode
was a likely cache miss.

The cycle count depends only on the handler (apart from m68k_op_illegal),
so the tables collapse into
    m68ki_ops[]    one {handler, cycles} pair per distinct combination
    m68ki_blk[][]  distinct runs of 32 opcodes, as indices into m68ki_ops
    m68ki_top[]    opcode >> 5 -> run
about 42 KB, placed in .data so the startup code copies it into RAM (as
plain statics GCC sees they are never written and moves them to flash).

    tools/m68k_dispatch.py   ->  core/cpus/M68K/m68ki_dispatch.h
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
M68K = os.path.join(HERE, '..', 'core', 'cpus', 'M68K')
RUN = 32


def body(name):
    return open(os.path.join(M68K, name), encoding='utf-8').read().split('{', 1)[1]


handlers = re.findall(r'm68k_op_\w+', body('m68ki_instruction_jump_table_full.h'))
cycles = [f'{a}*{b}' for a, b in re.findall(r'(\d+)\s*\*\s*(\d+)', body('m68ki_cycles_full.h'))]
assert len(handlers) == len(cycles) == 0x10000

ops, op_index = [], {}
for pair in zip(handlers, cycles):
    if pair not in op_index:
        op_index[pair] = len(ops)
        ops.append(pair)
assert len(ops) < 0x10000

runs, run_index, top = [], {}, []
for i in range(0, 0x10000, RUN):
    run = tuple(op_index[p] for p in zip(handlers[i:i + RUN], cycles[i:i + RUN]))
    if run not in run_index:
        run_index[run] = len(runs)
        runs.append(run)
    top.append(run_index[run])

out = []
out.append('/* m68ki_dispatch.h -- written by tools/m68k_dispatch.py from')
out.append(' * m68ki_instruction_jump_table_full.h and m68ki_cycles_full.h; do not edit.')
out.append(f' * {len(ops)} handler/cycle pairs, {len(runs)} runs of {RUN} opcodes,')
size = len(ops) * 8 + len(runs) * RUN * 2 + len(top) * 2
out.append(f' * {size} bytes in RAM. */')
out.append('')
out.append(f'#define M68KI_RUN_SHIFT {RUN.bit_length() - 1}')
out.append('')
out.append('typedef struct { void (*handler)(void); unsigned int cycles; } m68ki_op_t;')
out.append('')
out.append('#define M68KI_IN_RAM __attribute__((section(".data.m68ki_dispatch")))')
out.append('')
out.append(f'static M68KI_IN_RAM m68ki_op_t m68ki_ops[{len(ops)}] = {{')
for h, c in ops:
    out.append(f'  {{{h}, {c}}},')
out.append('};')
out.append('')
out.append(f'static M68KI_IN_RAM unsigned short m68ki_blk[{len(runs)}][{RUN}] = {{')
for r in runs:
    out.append('  {' + ','.join(map(str, r)) + '},')
out.append('};')
out.append('')
out.append(f'static M68KI_IN_RAM unsigned short m68ki_top[{len(top)}] = {{')
for i in range(0, len(top), 16):
    out.append('  ' + ','.join(map(str, top[i:i + 16])) + ',')
out.append('};')
out.append('')
out.append('static inline __attribute__((always_inline)) const m68ki_op_t *m68ki_decode(unsigned int ir)')
out.append('{')
out.append('  return &m68ki_ops[m68ki_blk[m68ki_top[ir >> M68KI_RUN_SHIFT]][ir & ((1u << M68KI_RUN_SHIFT) - 1)]];')
out.append('}')
out.append('')
out.append('static inline unsigned int m68ki_op_cycles(unsigned int ir)')
out.append('{')
out.append('  return m68ki_decode(ir)->cycles;')
out.append('}')
open(os.path.join(M68K, 'm68ki_dispatch.h'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')
print(f'm68ki_dispatch.h: {len(ops)} pairs, {len(runs)} runs, {size} bytes')
