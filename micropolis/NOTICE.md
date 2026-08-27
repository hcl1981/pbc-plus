# NOTICE

Dieses Projekt enthält den Simulationskern von **Micropolis**, der
quelloffenen Freigabe des ursprünglichen SimCity, von Electronic Arts und
Don Hopkins. Er steht unter der **GNU GPL v3** mit der zusätzlichen
**Micropolis Public Name License** (siehe `vendor/micropolis/`). Weil dieses
Projekt jenen Quelltext einbindet, steht es als Ganzes unter GPL-3.0
(`LICENSE`).

## Was in `vendor/micropolis/` liegt

Der reine C-Simulationskern, wortgleich aus dem Ursprungsbaum
`micropolis-activity/src/sim/` übernommen:

- `s_*.c` — die Simulation: Speicherverwaltung, Simulationsschleife, Zonen,
  Verkehr, Strom, Bewertung, Kartendurchlauf, Katastrophen, Stadtentstehung,
  Initialisierung, Meldungen, Dateiausgabe
- `w_tool.c`, `w_con.c` — Werkzeuge samt Kosten und Grundflächen sowie das
  selbsttätige Verbinden von Straßen, Schienen und Leitungen
- `rand.c`, `random.c` — Zufallszahlen
- `headers/` — `sim.h`, `macros.h` und weitere: Weltgröße, Bitmasken der
  Kacheln, globale Deklarationen

Die X11/Tk-Oberfläche (`w_*.c` der Oberfläche, `g_*.c`, `sim.c`) ist bewusst
**nicht** enthalten — an ihrer Stelle stehen der ST7789-Renderer und die
Eingabe dieses Projekts.

## Stand der Einbindung

Der Kern ist **eingebunden und wird übersetzt.** `CMakeLists.txt` nimmt
`vendor/micropolis/sim/s_*.c`, `rand*.c`, `w_tool.c` und `w_con.c` mit auf;
`src/micropolis_glue.c` liefert die rund 90 Symbole, die der Kern von der
entfernten Oberfläche erwartet, und `src/engine_micropolis.c` setzt darüber
die Schnittstelle aus `engine.h`.

Einen Attrappen-Kern gibt es nicht mehr — was gebaut wird, ist die echte
Simulation. Die Einzelheiten der Portierung stehen in `README.md`.
