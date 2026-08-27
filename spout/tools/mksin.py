#!/usr/bin/env python3
"""Erzeugt src/sintable.h: 1024 Eintraege, Amplitude 4096 (wie im Original)."""
import math, os
vals = [int(round(4096.0 * math.sin(2.0 * math.pi * i / 1024.0))) for i in range(1024)]
vals = [max(-4096, min(4096, v)) for v in vals]
out = ["/* sintable.h -- erzeugt von tools/mksin.py; 4096*sin(2*pi*i/1024) */",
       "#ifndef SINTABLE_H", "#define SINTABLE_H", "#include <stdint.h>", "",
       "#define SIN_ONE 4096", "", "static const int16_t sintable[1024] = {"]
for i in range(0, 1024, 8):
    out.append("    " + ", ".join("%6d" % v for v in vals[i:i+8]) + ",")
out += ["};", "", "#endif /* SINTABLE_H */"]
p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "sintable.h")
open(p, "w").write("\n".join(out) + "\n")
print("geschrieben:", os.path.normpath(p), "min", min(vals), "max", max(vals))
