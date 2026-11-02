#!/usr/bin/env python3
"""pbc_patch.py -- PicoBoy changes to redcode/Z80 (sources/Z80.c), all behind
compile-time switches; without them the file is the original.

  Z80_PBC_MAP      reads (opcodes, operands, data) straight from an 8 KB
                   page map, Z80_PBC_READMAP (default z80_readmap)
  Z80_PBC_FETCH8K  instead of the map: opcodes and operands from this 8 KB
                   array (pointer, mirrored), data reads through Z80_PBC_READ
                   (function); for a machine whose Z80 runs code from RAM only
  Z80_PBC_WRITE    writes through this function (address, value) instead of
                   the callback, e.g. the emulator's own write handler
  Z80_PBC_RAM      a section (string) for the code, e.g.
                   ".time_critical.redcode": RAM on the RP2350
  Z80_PBC_RAM_TABLES  the tables in RAM too (.data)
  Z80_PBC_SWITCH_XY  the DD/FD (IX/IY) opcodes through a switch as well, in
                   one function (more RAM, faster for IX/IY-heavy code)
  Z80_PBC_SWITCH   the unprefixed opcodes through a switch instead of the
                   function table in z80_execute and z80_run, the instruction
                   functions and the ALU/INC/DEC helpers inlined into it: with
                   the opcode a constant, the registers it names fold away

    pbc_patch.py <redcode sources/Z80.c> <output>
"""
import re, sys
s = open(sys.argv[1]).read()

old = '''#define FETCH_OPCODE(address) self->fetch_opcode(CONTEXT, address)
#define FETCH(address)	      self->fetch	(CONTEXT, address)
#define READ(address)	      self->read	(CONTEXT, address)
#define WRITE(address, value) self->write	(CONTEXT, address, value)'''
assert old in s
s = s.replace(old, '''#if defined(Z80_PBC_MAP)
	/* PicoBoy: reads straight from the emulator's 8 KB page map */
#	ifndef Z80_PBC_READMAP
#		define Z80_PBC_READMAP z80_readmap
#	endif
	extern zuint8 *Z80_PBC_READMAP[8];
	static Z_ALWAYS_INLINE zuint8 pbc_read(zuint16 a) { return Z80_PBC_READMAP[a >> 13][a & 0x1FFF]; }
#	define FETCH_OPCODE(address) pbc_read((zuint16)(address))
#	define FETCH(address)	      pbc_read((zuint16)(address))
#	define READ(address)	      pbc_read((zuint16)(address))
#elif defined(Z80_PBC_FETCH8K)
	/* PicoBoy: code only ever runs from an 8 KB RAM (mirrored), as on the
	 * Mega Drive: opcodes and operands straight from it, data through the
	 * emulator's read function */
	extern zuint8 *Z80_PBC_FETCH8K;
	extern zuint8 Z80_PBC_READ(zuint16 address);
#	define FETCH_OPCODE(address) Z80_PBC_FETCH8K[(address) & 0x1FFF]
#	define FETCH(address)	      Z80_PBC_FETCH8K[(address) & 0x1FFF]
#	define READ(address)	      Z80_PBC_READ((zuint16)(address))
#else
#	define FETCH_OPCODE(address) self->fetch_opcode(CONTEXT, address)
#	define FETCH(address)	      self->fetch	(CONTEXT, address)
#	define READ(address)	      self->read	(CONTEXT, address)
#endif
#ifdef Z80_PBC_WRITE
	/* PicoBoy: writes straight to the emulator's handler */
	extern void Z80_PBC_WRITE(int address, int value);
#	define WRITE(address, value) Z80_PBC_WRITE((zuint16)(address), (zuint8)(value))
#else
#	define WRITE(address, value) self->write	(CONTEXT, address, value)
#endif''')

old = '#define INSN(name)\t     static zuint8 name(Z80 *self)'
assert old in s
# code and tables into one section (RAM)
ram = ('\n#ifdef Z80_PBC_RAM\n#\tdefine PBC_FN __attribute__((section(Z80_PBC_RAM)))\n'
       '#else\n#\tdefine PBC_FN\n#endif\n'
       '#ifdef Z80_PBC_RAM_TABLES\n#\tdefine PBC_CONST __attribute__((section(".data.redcode")))\n'
       '#else\n#\tdefine PBC_CONST const\n#endif\n')
k = s.index('/* MARK: - Precomputed Values of AF for `daa` */')
s = s[:k] + ram + s[k:]
s = re.sub(r'static (\w+) const (\w+)(\s*)\[', r'static \1 PBC_CONST \2\3[', s)   # tables: plain .data = RAM
for sig in ('Z80_API void z80_power(', 'Z80_API void z80_instant_reset(', 'Z80_API void z80_int(',
            'Z80_API void z80_nmi(', 'Z80_API zusize z80_execute(', 'Z80_API zusize z80_run(',
            'static zuint8 ggg('):
    assert sig in s, sig
    s = s.replace(sig, sig.replace('Z80_API ', 'Z80_API PBC_FN ').replace('static zuint8 ', 'static PBC_FN zuint8 '))
s = s.replace(old, '#ifdef Z80_PBC_SWITCH\n#\tdefine INSN(name) static inline __attribute__((always_inline)) PBC_FN zuint8 name(Z80 *self)\n'
              '#else\n#\tdefine INSN(name) static PBC_FN zuint8 name(Z80 *self)\n#endif')
for sig in ('static void uuu(Z80 *self, zuint8 offset, zuint8 rhs)\n',
            'static zuint8 vvv(Z80 *self, zuint8 offset, zuint8 value)\n'):
    assert s.count(sig) == 1
    s = s.replace(sig, '#ifdef Z80_PBC_SWITCH\nstatic inline __attribute__((always_inline)) PBC_FN\n#else\nstatic PBC_FN\n#endif\n'
                  + sig[len('static '):])

i = s.index('static Insn PBC_CONST insn_table[256] = {'); j = s.index('};', i)
body = re.sub(r'/\*.*?\*/', '', s[i:j], flags=re.S)
names = [n.strip() for n in body.split('{', 1)[1].split(',') if n.strip()]
assert len(names) == 256
cases = '\n'.join('\t\tcase 0x%02X: DATA[0] = 0x%02X; return %s(self);' % (k, k, n) for k, n in enumerate(names))
s = s[:j + 2] + ('\n\n#ifdef Z80_PBC_SWITCH\n'
    '/* PicoBoy: the unprefixed opcodes through a switch instead of insn_table. */\n'
    'static Z_ALWAYS_INLINE zuint8 pbc_dispatch(Z80 *self, zuint8 opcode)\n'
    '\t{\n\tswitch (opcode)\n\t\t{\n' + cases + '\n\t\t}\n\treturn 0;\n\t}\n'
    '#\tdefine PBC_INSN(opcode) pbc_dispatch(self, opcode)\n'
    '#else\n#\tdefine PBC_INSN(opcode) insn_table[DATA[0] = (opcode)](self)\n#endif\n') + s[j + 2:]

# IX/IY instructions (DD/FD prefix) through a switch too, in one function
i = s.index('static Insn PBC_CONST xy_insn_table[256] = {'); j = s.index('};', i)
body = re.sub(r'/\*.*?\*/', '', s[i:j], flags=re.S)
xy_names = [n.strip() for n in body.split('{', 1)[1].split(',') if n.strip()]
assert len(xy_names) == 256
xy_cases = '\n'.join('\t\tcase 0x%02X: DATA[1] = 0x%02X; return %s(self);' % (k, k, n) for k, n in enumerate(xy_names))
s = s[:j + 2] + ('\n\n#ifdef Z80_PBC_SWITCH_XY\n'
    '/* PicoBoy: the DD/FD (IX/IY) opcodes through a switch instead of xy_insn_table,\n'
    ' * the instructions inlined; one function for both prefixes. */\n'
    'static PBC_FN __attribute__((noinline)) zuint8 pbc_xy(Z80 *self, zuint8 opcode)\n'
    '\t{\n\tswitch (opcode)\n\t\t{\n' + xy_cases + '\n\t\t}\n\treturn 0;\n\t}\n#endif\n') + s[j + 2:]
old_xy = 'cycles = xy_insn_table[DATA[1] = FETCH_OPCODE(PC + 1)](self); \\'
assert s.count(old_xy) == 1, 'xy macro'
s = s.replace(old_xy, 'cycles = PBC_XY_INSN(FETCH_OPCODE(PC + 1));		      \\')
k = s.index('#define XY_PREFIX(index_register)')
s = s[:k] + ('#ifdef Z80_PBC_SWITCH_XY\n\tstatic zuint8 pbc_xy(Z80 *self, zuint8 opcode);\n'
             '#\tdefine PBC_XY_INSN(opcode) pbc_xy(self, opcode)\n#else\n'
             '#\tdefine PBC_XY_INSN(opcode) xy_insn_table[DATA[1] = (opcode)](self)\n#endif\n\n') + s[k:]

old = 'self->cycles += insn_table[DATA[0] = FETCH_OPCODE(PC)](self);'
assert s.count(old) == 2, s.count(old)          # z80_execute and z80_run main loops
s = s.replace(old, 'self->cycles += PBC_INSN(FETCH_OPCODE(PC));')

s = ('/* PicoBoy Color Plus: changed by pbc_patch.py (page-map reads, direct\n'
     ' * writes, switch dispatch), all behind Z80_PBC_* switches. */\n') + s
open(sys.argv[2], 'w').write(s)
