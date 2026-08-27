#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
wad2uf2 -- Doom-WAD -> PicoBoy-Color-UF2 Konverter

Macht aus einer Doom-WAD (DOOM.WAD, DOOM2.WAD, ein eigenes PWAD-IWAD, ...)
eine flash-fertige UF2-Datei fuer die PicoBoy-Color-Doom-Firmware.

Bedienung -- so einfach wie moeglich:

  * Doppelklick (oder Start ohne Argumente): es oeffnet sich ein
    Datei-Dialog. WAD auswaehlen -> fertige .uf2 landet daneben.

  * Kommandozeile:
        python3 wad2uf2.py MEINE.WAD
        python3 wad2uf2.py MEINE.WAD -o mein-wad.uf2

Wenn eine Datei mit dem Zielnamen schon existiert, wird sie NICHT
ueberschrieben -- es wird an den Namen _1, _2, ... angehaengt.

Hinweise:
  * Braucht das Hilfsprogramm 'whd_gen' (liegt normalerweise neben diesem
    Skript). Ueber --whd-gen laesst sich ein anderer Pfad angeben.
  * whd_gen ist plattformspezifisch. Das beiliegende Binary ist fuer
    Linux x86-64. Auf anderen Systemen muss whd_gen dort neu gebaut werden.
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile

# ---------------------------------------------------------------------------
# Feste Werte fuer die PicoBoy-Color-Firmware (doom_tiny_nost)
# ---------------------------------------------------------------------------
WAD_FLASH_ADDR   = 0x10048000       # TINY_WAD_ADDR der Firmware
FLASH_END        = 0x11000000       # 16 MB Flash-Ende (0x10000000 + 16M)
UF2_FAMILY_DATA  = 0xE48BFF58       # RP2350 "data"-Family

# UF2-Formatkonstanten
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END    = 0x0AB16F30
UF2_FLAG_FAMILY  = 0x00002000       # familyID-Feld ist gueltig
UF2_PAYLOAD      = 256              # Nutzbytes pro 512-Byte-Block


class ConvertError(Exception):
    """Benutzerfreundlicher Fehler mit klarer Meldung."""


def _platform_binary_names():
    """Passende whd_gen-Dateinamen fuer das aktuelle System (bevorzugt zuerst)."""
    import platform
    sysname = platform.system().lower()      # 'linux' | 'windows' | 'darwin'
    machine = platform.machine().lower()      # 'x86_64' | 'amd64' | 'arm64' | 'aarch64'
    arm = machine in ("arm64", "aarch64")
    names = []
    if sysname.startswith("win"):
        names += ["whd_gen-windows-x86_64.exe", "whd_gen.exe", "whd_gen"]
    elif sysname == "darwin":
        if arm:
            names += ["whd_gen-macos-arm64", "whd_gen-macos-x86_64"]
        else:
            names += ["whd_gen-macos-x86_64", "whd_gen-macos-arm64"]
        names += ["whd_gen"]
    else:  # linux / sonstiges unixoides
        names += ["whd_gen-linux-x86_64", "whd_gen"]
    return names


def find_whd_gen(explicit):
    """whd_gen-Binary suchen: --whd-gen, dann plattformspezifisch in bin/ bzw.
    neben dem Skript, dann PATH."""
    from shutil import which
    candidates = []
    if explicit:
        candidates.append(explicit)
    here = os.path.dirname(os.path.abspath(__file__))
    names = _platform_binary_names()
    for d in (os.path.join(here, "bin"), here):
        for name in names:
            candidates.append(os.path.join(d, name))
    for name in names:
        p = which(name)
        if p:
            candidates.append(p)

    for c in candidates:
        if c and os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    # gefunden, aber nicht ausfuehrbar? -> hilfreiche Meldung (Unix: chmod)
    for c in candidates:
        if c and os.path.isfile(c):
            raise ConvertError(
                "whd_gen gefunden, aber nicht ausfuehrbar:\n  %s\n"
                "Bitte ausfuehrbar machen:  chmod +x \"%s\"" % (c, c))
    raise ConvertError(
        "whd_gen wurde fuer dieses System nicht gefunden.\n"
        "Gesucht (in ./bin und neben dem Skript): %s\n"
        "Passendes Binary dort ablegen, oder Pfad angeben:\n"
        "    --whd-gen /pfad/zu/whd_gen" % ", ".join(names))


def check_is_wad(path):
    """Grobpruefung: existiert die Datei und faengt sie mit IWAD/PWAD an?"""
    if not os.path.isfile(path):
        raise ConvertError("WAD-Datei nicht gefunden:\n  %s" % path)
    with open(path, "rb") as f:
        magic = f.read(4)
    if magic not in (b"IWAD", b"PWAD"):
        raise ConvertError(
            "Das sieht nicht nach einer Doom-WAD aus (Magic %r statt IWAD/PWAD):\n  %s"
            % (magic, path))
    return magic


def run_whd_gen(whd_gen, wad_path, whd_path):
    """whd_gen aufrufen; WHD im 'non super-tiny'-Format (fuer die nost-Firmware)."""
    cmd = [whd_gen, wad_path, whd_path, "-no-super-tiny"]
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True)
    except OSError as e:
        raise ConvertError("whd_gen konnte nicht gestartet werden:\n  %s\n  (%s)"
                          % (whd_gen, e))
    if proc.returncode != 0 or not os.path.isfile(whd_path):
        raise ConvertError(
            "whd_gen ist fehlgeschlagen (Code %s).\n"
            "Nicht jede WAD wird unterstuetzt (getestet v.a. mit Id-WADs).\n\n"
            "Ausgabe:\n%s" % (proc.returncode, (proc.stdout or "").strip()))
    # WHD-Magic pruefen
    with open(whd_path, "rb") as f:
        if f.read(4) != b"IWHD":
            raise ConvertError("whd_gen hat keine gueltige WHD-Datei erzeugt.")


def make_uf2(data, base_addr):
    """Rohdaten -> UF2-Bytes (RP2350 data-Family, feste Ladeadresse)."""
    total = (len(data) + UF2_PAYLOAD - 1) // UF2_PAYLOAD
    out = bytearray()
    for i in range(total):
        chunk = data[i * UF2_PAYLOAD:(i + 1) * UF2_PAYLOAD]
        header = struct.pack("<IIIIIIII",
                            UF2_MAGIC_START0, UF2_MAGIC_START1, UF2_FLAG_FAMILY,
                            base_addr + i * UF2_PAYLOAD, len(chunk),
                            i, total, UF2_FAMILY_DATA)
        block = header + chunk + b"\x00" * (476 - len(chunk))
        block += struct.pack("<I", UF2_MAGIC_END)
        assert len(block) == 512
        out += block
    return bytes(out)


def default_output(wad_path):
    base = os.path.basename(wad_path)
    stem = os.path.splitext(base)[0]
    return os.path.join(os.path.dirname(os.path.abspath(wad_path)), stem + "-wad.uf2")


def unique_path(path):
    """Existiert 'path' schon, wird NICHT ueberschrieben: haenge _1, _2, ...
    vor die Endung, bis ein freier Name gefunden ist."""
    if not os.path.exists(path):
        return path
    root, ext = os.path.splitext(path)
    i = 1
    while True:
        cand = "%s_%d%s" % (root, i, ext)
        if not os.path.exists(cand):
            return cand
        i += 1


def convert(wad_path, out_path=None, whd_gen_path=None, log=print):
    """Kompletter Ablauf. Gibt den Pfad zur erzeugten UF2 zurueck."""
    whd_gen = find_whd_gen(whd_gen_path)
    check_is_wad(wad_path)
    if out_path is None:
        out_path = default_output(wad_path)
    out_path = unique_path(out_path)   # vorhandene Dateien nicht ueberschreiben

    log("whd_gen: %s" % os.path.basename(whd_gen))
    log("Konvertiere WAD -> WHD (kann ein paar Sekunden dauern) ...")

    with tempfile.TemporaryDirectory() as td:
        whd_path = os.path.join(td, "out.whd")
        run_whd_gen(whd_gen, wad_path, whd_path)
        whd = open(whd_path, "rb").read()

    end_addr = WAD_FLASH_ADDR + len(whd)
    log("WHD-Groesse: %.2f MB" % (len(whd) / (1024 * 1024)))
    if end_addr > FLASH_END:
        raise ConvertError(
            "Diese WAD ist zu gross fuer den 16-MB-Flash "
            "(braucht bis 0x%08X, Flash endet bei 0x%08X)."
            % (end_addr, FLASH_END))
    if end_addr > FLASH_END - 0x40000:   # < 256 KB fuer Savegames uebrig
        log("WARNUNG: sehr wenig Platz fuer Savegames uebrig.")

    uf2 = make_uf2(whd, WAD_FLASH_ADDR)
    with open(out_path, "wb") as f:
        f.write(uf2)
    log("Fertig: %s  (%.2f MB, %d Bloecke)"
        % (out_path, len(uf2) / (1024 * 1024), len(uf2) // 512))
    return out_path


# ---------------------------------------------------------------------------
# GUI-Modus (Doppelklick / ohne Argumente)
# ---------------------------------------------------------------------------
def run_gui():
    try:
        import tkinter as tk
        from tkinter import filedialog, messagebox
    except Exception:
        return False   # keine GUI verfuegbar -> Aufrufer zeigt CLI-Hilfe

    root = tk.Tk()
    root.withdraw()
    wad = filedialog.askopenfilename(
        title="Doom-WAD auswaehlen",
        filetypes=[("Doom-WAD", "*.wad *.WAD"), ("Alle Dateien", "*.*")])
    if not wad:
        return True
    try:
        out = convert(wad, log=lambda m: None)
        messagebox.showinfo("Fertig", "UF2 erstellt:\n\n%s" % out)
    except ConvertError as e:
        messagebox.showerror("Fehler", str(e))
    except Exception as e:  # noqa
        messagebox.showerror("Unerwarteter Fehler", repr(e))
    return True


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if not argv:
        # Ohne Argumente: GUI versuchen, sonst Hilfe.
        if run_gui():
            return 0
        print(__doc__)
        return 0

    ap = argparse.ArgumentParser(
        description="Doom-WAD in eine PicoBoy-Color-UF2 umwandeln.")
    ap.add_argument("wad", help="Eingabe-WAD (z.B. DOOM.WAD)")
    ap.add_argument("-o", "--output", help="Ausgabe-UF2 (Standard: <name>-wad.uf2)")
    ap.add_argument("--whd-gen", help="Pfad zum whd_gen-Binary (falls nicht daneben)")
    args = ap.parse_args(argv)

    try:
        convert(args.wad, args.output, args.whd_gen)
    except ConvertError as e:
        print("\nFEHLER: %s" % e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
