#!/usr/bin/env python3
"""Makes doc/Rushtown-Story.pdf: the story's texts and the screens between
the missions, in German and English.

    tools/host.sh && ~/.cache/pbcplus-venv/bin/python tools/storybook.py

Needs reportlab and ImageMagick's convert. The texts are read from
src/story.c, the screens come from build-host/host playing each mission.
"""
import os, re, subprocess, sys, tempfile
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, PageBreak, Image, Table,
                                TableStyle, KeepTogether)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
HOST = os.path.join(ROOT, "build-host", "host")
TMP = tempfile.mkdtemp(prefix="rushtown-story-")

pdfmetrics.registerFont(TTFont("Sans", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
pdfmetrics.registerFont(TTFont("Sans-Bold", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"))
pdfmetrics.registerFont(TTFont("Mono", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"))

def style(name, **kw):
    base = dict(fontName="Sans", fontSize=10, leading=14)
    base.update(kw)
    return ParagraphStyle(name, **base)
H0 = style("h0", fontName="Sans-Bold", fontSize=30, leading=36, alignment=1)
H1 = style("h1", fontName="Sans-Bold", fontSize=15, leading=19, spaceBefore=4, spaceAfter=2)
H2 = style("h2", fontName="Sans-Bold", fontSize=11, leading=14, textColor=colors.HexColor("#1d6f8a"))
BODY = style("body")
SMALL = style("small", fontSize=8, leading=10, textColor=colors.HexColor("#555555"))
QUOTE = style("quote", fontName="Mono", fontSize=8.5, leading=11.5, leftIndent=3 * mm)
CAP = style("cap", fontSize=7, leading=9, alignment=1, textColor=colors.HexColor("#555555"))

def shot(name, *steps):
    """runs the game through `steps` and returns the picture of its last frame"""
    ppm = os.path.join(TMP, name + ".ppm")
    png = os.path.join(TMP, name + ".png")
    subprocess.run([HOST, *steps, "s" + ppm], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["convert", ppm, "-filter", "point", "-resize", "200%", png], check=True)
    return png

def read_story():
    src = open(os.path.join(ROOT, "src", "story.c"), encoding="utf-8").read()
    steps = {}
    for name, body in re.findall(r"STEPS\((m\d\d),(.*?)\);", src, re.S):
        steps[name] = re.findall(r'\{ "([^"]*)", "([^"]*)" \}', body)
    people = dict(re.findall(r'\[(\w+)\] = \{ "([^"]+)"', src))
    missions = []
    pat = re.compile(r'\{ (\w+), \{ "([^"]*)", "([^"]*)" \},\s*\{ \{ (.*?) \},\s*\{ (.*?) \} \},\s*'
                     r'\{ "([^"]*)", "([^"]*)" \}, (\d+), (m\d\d) \}', re.S)
    for who, t_en, t_de, b_en, b_de, d_en, d_de, reward, st in pat.findall(src):
        lines = lambda b: re.findall(r'"([^"]*)"', b)
        missions.append(dict(who=people[who], title=(t_en, t_de), brief=(lines(b_en), lines(b_de)),
                             done=(d_en, d_de), reward=int(reward), steps=steps[st]))
    return missions

def pictures(row, width):
    cells = [[Image(p, width=width, height=width * 280 / 240) for p, _ in row],
             [Paragraph(c, CAP) for _, c in row]]
    t = Table(cells, colWidths=[width + 3 * mm] * len(row))
    t.setStyle(TableStyle([("ALIGN", (0, 0), (-1, -1), "CENTER"), ("TOPPADDING", (0, 0), (-1, -1), 1),
                           ("BOTTOMPADDING", (0, 0), (-1, -1), 1)]))
    return t

def main():
    missions = read_story()
    assert len(missions) == 20, "expected twenty missions, found %d" % len(missions)
    start = ["w10", "+A", "w2", "-A", "w5", "+A", "w2", "-A", "w100"]     # the second press puts the opening picture away
    out = os.path.join(ROOT, "doc", "Rushtown-Story.pdf")
    doc = SimpleDocTemplate(out, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm,
                            bottomMargin=16 * mm, title="Rushtown – Story", author="Jan Schulz")
    f = []
    f += [Spacer(1, 30 * mm), Paragraph("RUSHTOWN", H0), Spacer(1, 4 * mm),
          Paragraph("Die Story und ihre Zwischenbilder · The story and its screens", style("sub", alignment=1, fontSize=12)),
          Spacer(1, 10 * mm),
          pictures([(shot("title_de", "D", "w60"), "Titelbild, deutsch"), (shot("title_en", "w60"), "Title, English"),
                    (shot("map", "D", *start, "+C", "w40", "-C", "w10"), "Karte und Menü (Mitte halten)")], 46 * mm),
          Spacer(1, 10 * mm),
          Paragraph("Worum es geht", H1),
          Paragraph("Du kommst ohne Geld nach Rushtown und kannst fahren. Rosa Marchetti, Wirtin am großen Platz, braucht "
                    "einen Fahrer. Kommissar Hale kassiert vom Tower aus bei jedem Geschäft der Stadt mit. Über Rosa "
                    "lernst du Dave Hollis am Hafen kennen, dessen Fähre Hale „zur Reparatur“ stillgelegt hat, über ihn "
                    "Lola Vance auf Palm Key, die weiß, dass die Brücke nach Iron Isle zweimal bezahlt wurde, und über sie "
                    "Mr. Gray, der auf Iron Isle druckt, was Hale ausgibt. Am Ende holt ihr Hales Buch aus dem Tower.", BODY),
          Spacer(1, 3 * mm),
          Paragraph("Zwanzig Missionen in vier Abschnitten. Die Fähre nach Palm Key öffnet mit Mission 10, die Brücke nach "
                    "Iron Isle mit Mission 14. Die Texte unten stehen so im Spiel: erst deutsch, dann englisch. "
                    "Die Bilder sind aus dem PC-Testprogramm; die Missionen wurden dafür per Testbefehl gelöst, "
                    "nicht von Hand gefahren.", BODY),
          PageBreak()]

    for i, m in enumerate(missions):
        n = "%02d" % (i + 1)
        at = ["S%d" % i]
        past = ["G", "w3", "+B", "w2", "-B", "w160", "G", "w3"]    # past the picture, if there is one, to the briefing
        row = [(shot(n + "_brief_de", "D", *start, *at, *past), "Auftrag, deutsch"),
               (shot(n + "_run_de", "D", *start, *at, *past, "+A", "w2", "-A", "w30"), "nach dem Annehmen"),
               (shot(n + "_done_de", "D", *start, *at, "Q", "w4"), "erledigt"),
               (shot(n + "_brief_en", *start, *at, *past), "Briefing, English")]
        tasks_de = " → ".join(t[1] for t in m["steps"] if t[1])
        tasks_en = " → ".join(t[0] for t in m["steps"] if t[0])
        block = [Paragraph("Mission %d: %s" % (i + 1, m["title"][1]), H1),
                 Paragraph("%s · $%d · English: %s" % (m["who"].title(), m["reward"], m["title"][0]), SMALL),
                 Spacer(1, 1.5 * mm),
                 Table([[[Paragraph("Deutsch", H2), Paragraph("<br/>".join(m["brief"][1]), QUOTE),
                          Paragraph("Aufgabe: " + tasks_de, SMALL), Paragraph("Danach: „%s“" % m["done"][1], SMALL)],
                         [Paragraph("English", H2), Paragraph("<br/>".join(m["brief"][0]), QUOTE),
                          Paragraph("Task: " + tasks_en, SMALL), Paragraph("Afterwards: “%s”" % m["done"][0], SMALL)]]],
                       colWidths=[87 * mm, 87 * mm], style=TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP")])),
                 Spacer(1, 2 * mm), pictures(row, 39 * mm), Spacer(1, 5 * mm)]
        f.append(KeepTogether(block))
        if i % 2 == 1 and i != len(missions) - 1:
            f.append(PageBreak())

    art = os.path.join(ROOT, "art", "preview.png")
    if os.path.exists(art):
        f += [PageBreak(), Paragraph("Die Bilder", H1),
              Paragraph("Vier Porträts für die Briefings, neun Szenen an den Wendepunkten der Story "
                        "(Ankunft, Rosas Lokal, Tower, Hafen, erste Überfahrt, Palm Key, Brücke, Werk, Flucht) und "
                        "die Bilder für „gefasst“ und „K.O.“ (das Straßenbild in der zweiten Reihe links wird nicht benutzt, hinter dem Titel läuft die Stadt). So, wie das Spiel sie zeigt: 240 × 280 Pixel, "
                        "256 Farben je Bild. Erzeugt mit Gemini nach den Vorgaben aus diesem Projekt.", BODY),
              Spacer(1, 4 * mm), Image(art, width=150 * mm, height=150 * mm * 1136 / 976),
              PageBreak(), Paragraph("Bilder im Ablauf", H1), Spacer(1, 2 * mm),
              pictures([(shot("card0", "D", "w10", "+A", "w2", "-A", "w5"), "Beginn einer neuen Story"),
                        (shot("card1", "D", *start, "G", "w3"), "vor Mission 1"),
                        (shot("card5", "D", *start, "S4", "G", "w3"), "vor Mission 5"),
                        (shot("card7", "D", *start, "S6", "G", "w3"), "vor Mission 7")], 39 * mm),
              Spacer(1, 4 * mm),
              pictures([(shot("card10", "D", *start, "S9", "G", "w3"), "vor Mission 10"),
                        (shot("card11", "D", *start, "S10", "G", "w3"), "vor Mission 11"),
                        (shot("card14", "D", *start, "S13", "G", "w3"), "vor Mission 14"),
                        (shot("card16", "D", *start, "S15", "G", "w3"), "vor Mission 16")], 39 * mm),
              Spacer(1, 4 * mm),
              pictures([(shot("card20", "D", *start, "S19", "G", "w3"), "vor Mission 20"),
                        (shot("ko", "D", *start, "H", "U", "w70"), "K.O."),
                        (shot("busted2", "D", *start, "J7", "w2", "U", "w70"), "gefasst")], 39 * mm)]

    f += [PageBreak(), Paragraph("Weitere Zwischenbilder", H1),
          Paragraph("Was sonst zwischen den Missionen erscheint.", BODY), Spacer(1, 3 * mm),
          pictures([(shot("end_de", "D", *start, "S19", "Q", "w4"), "nach der letzten Mission"),
                    (shot("end_en", *start, "S19", "Q", "w4"), "after the last mission"),
                    (shot("ferry", "D", "M1", *start, "$50", "T" + ferry(), "w40"), "auf der Fähre"),
                    (shot("busted", "D", *start, "J7", "w2", "U", "w40"), "gefasst")], 39 * mm),
          Spacer(1, 4 * mm),
          pictures([(shot("fail", "D", *start, "S0", "G", "w3", "+A", "w2", "-A", "V", "w10"), "Zeit abgelaufen"),
                    (shot("bridge", "D", *start, "S13", "Q", "w4"), "die Brücke öffnet (Mission 14)"),
                    (shot("ferryopen", "D", *start, "S9", "G", "w3", "+A", "w2", "-A", "w20"), "die Fähre fährt wieder (Mission 10)"),
                    (shot("free", "D", "M1", *start), "freies Spiel")], 39 * mm)]
    doc.build(f)
    print(out)

def ferry():
    out = subprocess.run([HOST, "W"], check=True, capture_output=True, text=True).stdout
    return re.search(r"ferry (\d+,\d+)", out).group(1)

if __name__ == "__main__":
    sys.exit(main())
