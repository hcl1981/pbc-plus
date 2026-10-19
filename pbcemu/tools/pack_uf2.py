#!/usr/bin/env python3
"""pack_uf2.py -- one UF2 out of the launcher and the emulators.

    pack_uf2.py <out.uf2> <launcher.uf2> <app.bin>@<flash offset> ...

The launcher's UF2 (made by picotool) is taken as it is, including the
extra "absolute" block picotool puts in front (RP2350-E10 workaround).
The emulators are .bin files linked to their offsets; their 256-byte blocks
are appended and all flash blocks numbered as one file.
"""
import struct
import sys

MAGIC0, MAGIC1, MAGIC_END = 0x0A324655, 0x9E5D5157, 0x0AB16F30
FLAG_FAMILY = 0x00002000
RP2350_ARM_S = 0xE48BFF59
XIP_BASE = 0x10000000


def blocks(data):
    for i in range(0, len(data), 512):
        yield data[i:i + 512]


def main():
    out, launcher, apps = sys.argv[1], sys.argv[2], sys.argv[3:]
    head, flash = [], []                 # (flags, addr, payload, family)
    for b in blocks(open(launcher, 'rb').read()):
        m0, m1, flags, addr, size, bno, nb, fam = struct.unpack('<8I', b[:32])
        assert m0 == MAGIC0 and m1 == MAGIC1
        if fam == RP2350_ARM_S:
            flash.append((flags, addr, b[32:32 + size], fam))
        else:
            head.append(b)
    end = max(a + len(p) for f, a, p, fam in flash)
    for spec in apps:
        path, off = spec.rsplit('@', 1)
        base = XIP_BASE + int(off, 0)
        if base < end:
            sys.exit(f'{path} at {off} overlaps the previous image (ends at {end - XIP_BASE:#x})')
        data = open(path, 'rb').read()
        for i in range(0, len(data), 256):
            flash.append((FLAG_FAMILY, base + i, data[i:i + 256], RP2350_ARM_S))
        end = base + len(data)
    with open(out, 'wb') as f:
        for b in head:
            f.write(b)
        for n, (flags, addr, payload, fam) in enumerate(flash):
            payload = payload.ljust(256, b'\0')
            f.write(struct.pack('<8I', MAGIC0, MAGIC1, flags, addr, 256, n, len(flash), fam))
            f.write(payload.ljust(476, b'\0'))
            f.write(struct.pack('<I', MAGIC_END))
    print(f'{out}: {len(flash)} blocks, flash used up to {end - XIP_BASE:#x}')


if __name__ == '__main__':
    main()
