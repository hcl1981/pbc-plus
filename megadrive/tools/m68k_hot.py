#!/usr/bin/env python3
"""m68k_hot.py -- marks the most used 68000 opcode handlers for RAM.

The handlers in core/cpus/M68K/m68kops.h run from flash and share the 16 KB
XIP cache with the game ROM. Not all of them fit in RAM, so this picks them
by instruction family (branches, moves, compares, ... first, the way 68000
game code is usually made up) and by addressing mode (registers and
(An)/(An)+/-(An) first), until a size budget is used up:

    M68K_HOT1   both firmwares (the stand-alone one has little RAM left)
    M68K_HOT2   only the combined firmware (PBCemu), see m68kcpu.c

It reads the handler sizes from a build without the marks and rewrites the
handler definitions in m68kops.h; running it again replaces the old marks.

    tools/m68k_hot.py build/pbc_megadrive.elf
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OPS = os.path.join(HERE, '..', 'core', 'cpus', 'M68K', 'm68kops.h')
BUDGET1, BUDGET2 = 17000, 30000

FAMILIES = ('move moveq bne beq bra bsr dbf dbra jsr rts tst cmp cmpi lea addq '
            'subq movea btst clr add sub andi ori bcc bcs bpl bmi bge blt bgt ble '
            'bhi bls bvc bvs dbcc dbne dbeq jmp rte nop swap ext cmpa lsl lsr asl '
            'asr adda suba and or eor eori not neg mulu muls bset bclr bchg rol '
            'ror roxl roxr movem pea link unlk addi subi scc sne seq exg divu divs '
            'addx subx negx').split()
EA_RANK = {'d': 0, 'a': 0, 'r': 0, 's': 0, 'er': 0, 'ai': 1, 'pi': 1, 'pd': 1,
           're': 1, 'di': 2, 'i': 2, 'aw': 3, 'al': 3, 'pcdi': 3, 'ix': 4,
           'pi7': 4, 'pd7': 4, 'pcix': 5}


def sizes(elf):
    out = subprocess.run(['arm-none-eabi-nm', '-S', elf], capture_output=True, text=True).stdout
    total = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in 'tT' and p[3].startswith('m68k_op_'):
            name = p[3].split('.')[0]               # count .part clones too
            total[name] = total.get(name, 0) + int(p[1], 16)
    return total


def main():
    size = sizes(sys.argv[1])
    rank = {f: i for i, f in enumerate(FAMILIES)}
    order = []
    for name, s in size.items():
        parts = name[len('m68k_op_'):].split('_')
        if parts[0] not in rank:
            continue
        ea = max((EA_RANK[x] for x in parts[1:] if x in EA_RANK), default=0)
        order.append((rank[parts[0]] // 6 + ea * 3, rank[parts[0]], name))
    order.sort()
    hot, used1, used2 = {}, 0, 0
    for _, _, name in order:
        if used1 + size[name] <= BUDGET1:
            hot[name], used1 = 'M68K_HOT1', used1 + size[name]
        elif used2 + size[name] <= BUDGET2:
            hot[name], used2 = 'M68K_HOT2', used2 + size[name]

    src = open(OPS, encoding='utf-8').read()
    src = re.sub(r'^static M68K_HOT[12] void ', 'static void ', src, flags=re.M)

    def mark(m):
        tag = hot.get(m.group(1))
        return f'static {tag} void {m.group(1)}(void)' if tag else m.group(0)
    src = re.sub(r'^static void (m68k_op_\w+)\(void\)', mark, src, flags=re.M)
    open(OPS, 'w', encoding='utf-8').write(src)
    n1 = sum(1 for t in hot.values() if t == 'M68K_HOT1')
    print(f'M68K_HOT1: {n1} handlers, {used1} bytes; M68K_HOT2: {len(hot) - n1}, {used2} bytes')


main()
