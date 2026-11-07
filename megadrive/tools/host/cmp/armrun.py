#!/usr/bin/env python3
"""armrun.py ELF ROM 'ARGS' [SKIP] -- hostcmp (built by ab.sh) on an emulated
Cortex-M33 (unicorn), with the firmware's compiler flags and the VDP's ARM
assembly.

ARGS as for hostcmp after the ROM name, e.g. '1330 -q -z 0x0D01'. From frame
SKIP on it counts the instructions executed per frame, by part (68k, z80,
vdp_line = drawing lines, convert = scaler, sound_c1 = core 1's sound loop,
other); HC_DETAIL=N breaks part N down by function. Flash wait states are
not modelled: this is the work, not the device's time. hash= covers every
frame (picture, VDP buffer, sound, RAM, VRAM) and must match between two
states. HC_FOLD=1 prints the per-frame hashes as hostcmp does with HC_FOLD=1
(to compare PC and M33).

    pip install unicorn pyelftools
"""
import bisect
import os
import struct
import sys
from elftools.elf.elffile import ELFFile
from unicorn import *
from unicorn.arm_const import *

elf_path, rom_path, args = sys.argv[1], sys.argv[2], sys.argv[3]
skip = int(sys.argv[4]) if len(sys.argv) > 4 else 0      # frames not counted
BASE, SIZE = 0x20000000, 48 << 20
elf = ELFFile(open(elf_path, 'rb'))
symtab = elf.get_section_by_name('.symtab')
syms = {s.name: s['st_value'] for s in symtab.iter_symbols()}
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(BASE, SIZE)
for seg in elf.iter_segments():
    if seg['p_type'] == 'PT_LOAD' and seg['p_filesz']:
        uc.mem_write(seg['p_paddr'], seg.data())
rom = open(rom_path, 'rb').read()[:0x500000]
uc.mem_write(syms['hc_rom'], rom)                         # .bss: armglue.c leaves it
uc.mem_write(syms['arm_rom_len'], struct.pack('<I', len(rom)))
uc.mem_write(syms['arm_args'], ('x rom ' + args).encode()[:255] + b'\0')

names = ['68k', 'z80', 'vdp_line', 'convert', 'sound_c1', 'frame_rest', '-', 'other']
counts = [0] * 8
cur = [7]
cache = {}
fsyms = sorted((s['st_value'] & ~1, s.name) for s in symtab.iter_symbols()
               if s['st_info']['type'] == 'STT_FUNC' and s['st_value'])
faddr = [a for a, n in fsyms]
detail = int(os.environ.get('HC_DETAIL', '-1'))
fcount = {}
counting = [False]


def on_block(uc, addr, size, user):
    k = (addr, size)
    n = cache.get(k)
    if n is None:                       # Thumb: 32-bit if the first halfword says so
        b = uc.mem_read(addr, size)
        n = i = 0
        while i < size:
            i += 4 if ((b[i] | b[i + 1] << 8) >> 11) in (0x1d, 0x1e, 0x1f) else 2
            n += 1
        cache[k] = n
    counts[cur[0]] += n
    if cur[0] == detail:
        fn = fsyms[bisect.bisect_right(faddr, addr) - 1][1]
        fcount[fn] = fcount.get(fn, 0) + n


def on_mark(uc, addr, size, user):      # arm_mark(part)
    cur[0] = uc.reg_read(UC_ARM_REG_R0)


def on_frame(uc, addr, size, user):     # arm_frame(frame): count from SKIP on
    if not counting[0] and uc.reg_read(UC_ARM_REG_R0) == skip:
        counting[0] = True
        uc.hook_add(UC_HOOK_BLOCK, on_block)
        uc.hook_add(UC_HOOK_CODE, on_mark, begin=syms['arm_mark'] & ~1, end=syms['arm_mark'] & ~1)
        uc.ctl_remove_cache(BASE, BASE + SIZE)


uc.hook_add(UC_HOOK_CODE, on_frame, begin=syms['arm_frame'] & ~1, end=syms['arm_frame'] & ~1)
uc.reg_write(UC_ARM_REG_SP, BASE + SIZE - 64)
try:
    uc.emu_start(syms['_start'] | 1, 0xFFFFFFFF)
except UcError as e:                    # the BKPT at the end, or a fault
    if not struct.unpack('<I', uc.mem_read(syms['arm_done'], 4))[0]:
        print('stop:', e, hex(uc.reg_read(UC_ARM_REG_PC)), file=sys.stderr)
done = struct.unpack('<I', uc.mem_read(syms['arm_done'], 4))[0]
nfr = int(args.split()[0])
hashes = struct.unpack('<%dQ' % nfr, uc.mem_read(syms['arm_hash'], 8 * nfr))
if os.environ.get('HC_FOLD'):
    m = min(nfr, 1024)
    comp = struct.unpack('<%dQ' % (9 * m), uc.mem_read(syms['arm_comp'], 72 * m))
    for i in range(m):
        print('%05d ' % i + ' '.join('%016x' % comp[9 * i + j] for j in range(8)))
h = 0
for x in hashes:
    h = (h * 1000003 ^ x) & 0xFFFFFFFFFFFFFFFF
fr = max(nfr - skip, 1)
tot = sum(counts) - counts[6]           # '-': the harness's own hashing
for fn, c in sorted(fcount.items(), key=lambda x: -x[1])[:25]:
    print('   %-40s %8.1f k/frame' % (fn, c / fr / 1e3))
print('done=%d frames=%d hash=%016x  per frame (k instr): total %.0f  ' % (done, fr, h, tot / fr / 1e3) +
      '  '.join('%s %.0f' % (names[i], counts[i] / fr / 1e3) for i in range(8) if names[i] != '-'))
