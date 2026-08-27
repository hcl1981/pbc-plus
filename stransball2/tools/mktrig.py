#!/usr/bin/env python3
"""Erzeugt src/trig.h: Sinus und Kosinus je Grad, in 1/1024."""
import math, os
sin = [int(round(math.sin(math.radians(d)) * 1024)) for d in range(360)]
cos = [int(round(math.cos(math.radians(d)) * 1024)) for d in range(360)]
out = ["/* trig.h -- erzeugt von tools/mktrig.py, nicht von Hand aendern. */",
       "#ifndef STB_TRIG_H", "#define STB_TRIG_H", "#include <stdint.h>", "",
       "/* Werte in 1/1024; Winkel in Grad 0..359 */",
       "static const int16_t stb_sin[360] = {"]
for i in range(0, 360, 12):
    out.append("    " + " ".join("%5d," % v for v in sin[i:i+12]))
out += ["};", "static const int16_t stb_cos[360] = {"]
for i in range(0, 360, 12):
    out.append("    " + " ".join("%5d," % v for v in cos[i:i+12]))
# Arkustangens fuer das Achtel 0..45 Grad: Index = Verhaeltnis * 64
atan = [int(round(math.degrees(math.atan(i / 64.0)))) for i in range(65)]
out += ["};", "",
        "/* atan(i/64) in Grad, fuer die Achtelzerlegung in stb_atan2 */",
        "static const uint8_t stb_atan[65] = {"]
for i in range(0, 65, 13):
    out.append("    " + " ".join("%3d," % v for v in atan[i:i+13]))
out += ["};", "",
        "/* Winkel von (dx,dy) in Grad 0..359, y nach unten wie im Bild. */",
        "static inline int stb_atan2(int dy, int dx)",
        "{",
        "    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, a;",
        "    if (ax == 0 && ay == 0) return 0;",
        "    a = (ax >= ay) ? stb_atan[(int)((int64_t)ay * 64 / ax)]",
        "                   : 90 - stb_atan[(int)((int64_t)ax * 64 / ay)];",
        "    if (dx >= 0) { if (dy >= 0) return a; return (360 - a) % 360; }",
        "    if (dy >= 0) return 180 - a;",
        "    return 180 + a;",
        "}", "",
        "#endif"]
p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "trig.h")
open(p, "w").write("\n".join(out) + "\n")
print("geschrieben: src/trig.h")
