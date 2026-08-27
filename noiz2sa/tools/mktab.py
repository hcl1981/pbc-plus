#!/usr/bin/env python3
"""Erzeugt src/tables.h: Sinus- und Arkustangenstabelle wie in der Vorlage.

sctbl[i] = sin(i*2pi/1024)*256, mit einem Viertel Ueberhang, damit
sctbl[i+256] den Kosinus liefert.  tantbl dient getDeg() (Koordinate ->
Winkel) und ist genauso aufgebaut wie degutil.c der Vorlage.
"""
import math, os

DIV = 1024
TAN = 1024

sc = [int(math.sin(i * (6.28 / DIV)) * 256) for i in range(DIV + DIV // 4)]

tan = [0] * (TAN + 2)
d = 0
for i in range(TAN):
    while int(math.sin(d * 6.28 / DIV) / math.cos(d * 6.28 / DIV) * TAN) < i:
        d += 1
    tan[i] = d
tan[TAN] = tan[TAN + 1] = 128

out = ["/* tables.h -- erzeugt von tools/mktab.py, nicht von Hand aendern. */",
       "#ifndef NOIZ_TABLES_H", "#define NOIZ_TABLES_H", "#include <stdint.h>", "",
       "#define TAN_TABLE_SIZE %d" % TAN, "",
       "static const int16_t sctbl[%d] = {" % len(sc)]
for i in range(0, len(sc), 12):
    out.append("    " + " ".join("%5d," % v for v in sc[i:i + 12]))
out += ["};", "", "static const int16_t tantbl[%d] = {" % len(tan)]
for i in range(0, len(tan), 12):
    out.append("    " + " ".join("%5d," % v for v in tan[i:i + 12]))
out += ["};", "", "#endif /* NOIZ_TABLES_H */"]

p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "tables.h")
open(p, "w").write("\n".join(out) + "\n")
print("geschrieben: src/tables.h  (%d Sinus-, %d Tangenswerte)" % (len(sc), len(tan)))
