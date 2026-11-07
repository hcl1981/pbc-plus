# Gerätebilder für PBCemu (Starter) und pbcplus.de

Liegt in `pbcemu/art/werkzeug/`; die fertigen Bilder (200 × 128, RGBA) sind die
`pbcemu/art/*.png` eine Ebene höher. pbcplus.de nutzt dieselben Bilder als
`bilder/konsole-<name>.png` (ohne `usb`).

- `quellen/` – Ausgaben von `pbcemu/tools/import_icons.py` (mit dessen Ellipsenschatten).
- `kontaktschatten.py`: rechnet den Ellipsenschatten exakt heraus,
  skaliert jedes Gerät in höchstens 188 × 108, setzt es mittig und gibt jedem Teil
  (Konsole, Controller) einen eigenen Kontaktschatten an seiner Unterkante.
  `python3 kontaktschatten.py .. quellen/*.png` (aus diesem Ordner; schreibt nach `pbcemu/art/`)
- `gemini-roh/` – Rohbilder von der Gemini-API (`gemini-3-pro-image`), freigestellt mit
  `../../tools/import_icons.py <bild> quellen 1x1 <name>`.
- `gemini_bild.py` – ein API-Aufruf; Key aus `~/.config/gemini.env` (`GEMINI_API_KEY=…`).
  `reihe.py`, `geraete.json`, `prompt_vorlage.txt` – Prompts für NES, PC Engine, Mega Drive,
  C64, Master System (Controller leicht vor der rechten Ecke, wie beim Atari 7800);
  `prompt_msx_v5.txt` (Joystick daneben wie beim ZX Spectrum), `prompt_cv.txt`, `prompt_cpc.txt`,
  `prompt_arduboy.txt` (Ergebnis: `../arduboy.jpeg`, Name `ard`).
  Stilvorlagen: `../zx.jpeg` und `../sheet.jpeg`; die Referenzbilder `ref_*.png`, die
  `reihe.py` außerdem mitschickt, liegen nicht bei.
- Kein API-Key im Repo: `gemini_bild.py` liest ihn nur aus `~/.config/gemini.env`.

Neues Gerät: Gemini-Bild erzeugen → `import_icons.py … quellen 1x1 <name>`
→ `kontaktschatten.py .. quellen/<name>.png` → `pbcemu/art/<name>.png` (danach
`pbcemu/build.sh`) und als `konsole-<name>.png` auf die Website.
