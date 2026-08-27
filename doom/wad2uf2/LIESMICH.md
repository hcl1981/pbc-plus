# WAD → UF2 Konverter für PicoBoy Color Plus Doom

Macht aus einer eigenen Doom-**WAD** eine flash-fertige **UF2** für die
PicoBoy-Color-Doom-Firmware. Weitergeben darfst du nur das **Werkzeug** —
jede/r nutzt die **eigene** WAD (die WADs selbst nicht weitergeben).

`whd_gen` macht jetzt **alles allein** — **kein Python, kein picotool nötig.**

## Windows: einfach Doppelklick
Unter Windows kannst du `whd_gen-windows-x86_64.exe` **doppelklicken** —
es öffnet sich ein „WAD auswählen"-Dialog, danach liegt die fertige `.uf2`
neben der WAD. Der Dialog nutzt nur eingebaute Windows-Funktionen
(comdlg32/user32); **nichts muss nachinstalliert werden.** Alternativ eine
WAD-Datei direkt **auf die .exe ziehen**.

(Linux/macOS haben bewusst keinen Dialog — dort läuft es über die
Kommandozeile, siehe unten. So braucht dort niemand Zusatzpakete wie zenity.)

## Benutzung (Kommandozeile)

Passendes Binary für dein System nehmen. **Kurzform** — nur die WAD angeben,
der Ausgabename wird automatisch gebildet (`DOOM.WAD` → `DOOM.uf2`):

```
# Linux
bin/whd_gen-linux-x86_64        DOOM.WAD

# Windows
bin\whd_gen-windows-x86_64.exe  DOOM.WAD

# macOS
bin/whd_gen-macos-<arch>        DOOM.WAD   # muss erst gebaut werden, siehe unten
```

**Langform** mit eigenem Ausgabenamen (das `-no-super-tiny` wird bei der
Kurzform automatisch gesetzt):

```
bin/whd_gen-linux-x86_64  DOOM.WAD  MEIN-NAME.uf2  -no-super-tiny
```

- Endet der Zielname auf **`.uf2`** → es wird eine flash-fertige UF2
  geschrieben (Ladeadresse 0x10048000, RP2350-`data`-Family).
- Endet er auf **`.whd`** (oder sonst) → die rohe WHD wie bisher.
- Existiert die `.uf2` schon, wird sie **nicht überschrieben** — es wird
  `_1`, `_2`, … an den Namen angehängt.
- `-no-super-tiny` ist wichtig (passend zur `doom_tiny_nost`-Firmware).

Die erzeugte `.uf2` dann auf den PicoBoy bringen — siehe [`../FLASHEN.md`](../FLASHEN.md).

## Binaries
`bin/` enthält `whd_gen` fertig gebaut für:
- `whd_gen-linux-x86_64` (statisch, keine Abhängigkeiten)
- `whd_gen-windows-x86_64.exe` (nur Windows-System-DLLs)

**Für macOS liegt kein fertiges Binary bei** — es muss auf einem Mac gebaut
werden (siehe unten), das geht auf einem anderen System nicht.

Fehlt eins für dein System, einmal bauen mit `./build-whd_gen.sh`:
- **Windows-.exe** entsteht unter Linux mit `mingw-w64`
  (`sudo apt-get install -y mingw-w64`).
- **macOS-Binary** muss **auf einem Mac** gebaut werden (dort das Skript
  ausführen; nutzt clang++).

## Optional: wad2uf2.py (GUI-Komfort)
`wad2uf2.py` ist ein optionaler Python-Aufsatz mit Datei-Dialog (Doppelklick)
und automatischer Binary-Wahl. **Nicht erforderlich** — `whd_gen` allein
genügt. Wer es nicht braucht, kann `wad2uf2.py` ignorieren oder löschen.

## Gut zu wissen
- Getestet vor allem mit den Id-WADs (Doom 1 / Doom 2). Andere/größere WADs
  können funktionieren, sind aber nicht garantiert; zu große WADs passen
  nicht in den 16-MB-Flash.
