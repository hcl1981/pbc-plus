#!/usr/bin/env python3
"""
mkdata.py -- packt die Tyrian-2.1-Daten in ein Flash-Archiv und erzeugt daraus
ein UF2, das neben die Firmware ins Flash geladen wird.

Warum ein eigenes Archiv und kein Dateisystem: der PicoBoy hat kein Speicher-
medium ausser dem Flash, und das Flash ist ab 0x10000000 in den Adressraum
eingeblendet. Ein Verzeichnis plus fester Offsets reicht damit voellig -- eine
Datei zu "oeffnen" ist ein Zeiger, kein Lesevorgang. Ein Dateisystem wuerde
Code, RAM und Ladezeit kosten und nichts dafuer zurueckgeben, weil ohnehin nie
etwas hinzukommt.

Aufruf:
    ./mkdata.py <tyrian21-verzeichnis> [-o tyrian-data.uf2] [--addr 0x10100000]

Das erzeugte UF2 wird wie die Firmware auf das BOOTSEL-Laufwerk kopiert. Beide
liegen an verschiedenen Adressen und ueberschreiben sich nicht.

GPLv2, wie OpenTyrian.
"""

import argparse
import os
import struct
import sys

MAGIC = b"PBCTYR01"
NAME_LEN = 16
HEADER_LEN = 16
ENTRY_LEN = 24

# --- UF2 ---------------------------------------------------------------------
# Die Werte stammen aus der Doom-Portierung auf derselben Hardware
# (doomusb/wad2uf2.py) und sind dort im Einsatz erprobt.
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY = 0x00002000
UF2_FAMILY_DATA = 0xE48BFF58  # RP2350, "data"-Family: kein ausfuehrbarer Code
UF2_PAYLOAD = 256

# Diese Dateien braucht OpenTyrian. Ermittelt aus allen Dateinamen, die in den
# Quellen vorkommen (fest verdrahtet oder ueber Formatmuster wie "newsh%c.shp").
# Alles Uebrige im Original-ZIP ist DOS-Beiwerk: die Spiel-EXE, der Netzwerk-
# Treiber fuer Modem/IPX, Handbuecher, das Installationsprogramm.
NEEDED_EXACT = {
    "tyrian.shp", "tyrian.hdt", "tyrian.pic", "tyrian.snd", "tyrian.cdt",
    "voices.snd", "music.mus", "palette.dat", "estsc.shp", "exitmsg.bin",
    "tshp2.pcx",
    # Die Schlussanimation. Mit 3,3 MB der groesste Brocken ueberhaupt und nur
    # einmal am Ende einer Episode zu sehen -- sie passt aber, und ohne sie
    # fehlte ein Stueck Originalspiel. Mit --no-ending abwaehlbar.
    "tyrend.anm",
}
NEEDED_PREFIX = ("newsh", "shapes", "levels", "cubetxt", "tyrian1", "tyrian2",
                 "tyrian3", "tyrian4", "demo.")

# Weihnachtsfassung: rund 0,5 MB, ohne die laeuft das Spiel vollstaendig. Wird
# nur mit --xmas aufgenommen.
XMAS_FILES = {"tyrianc.shp", "voicesc.snd"}


def wanted(name, with_xmas, with_ending=True):
    n = name.lower()
    if n in XMAS_FILES:
        return with_xmas
    if n == "tyrend.anm":
        return with_ending
    if n in NEEDED_EXACT:
        return True
    return any(n.startswith(p) for p in NEEDED_PREFIX)


def prebake_palette(raw):
    """palette.dat -> fertiges Speicherabbild von SDL_Color[256] je Palette.

    Die Datei enthaelt VGA-Werte mit 6 Bit je Farbanteil. OpenTyrian streckt
    sie beim Laden auf 8 Bit, und zwar nicht durch simples Schieben (das ergaebe
    252 statt 255 als Hoechstwert), sondern indem die oberen zwei Bit unten
    wieder angehaengt werden. Genau diese Rechnung wird hier vorweggenommen,
    damit die 23 KB im Flash bleiben koennen statt beim Start ins RAM
    ausgepackt zu werden.

    Zielformat ist SDL_Color: r, g, b, a -- vier Byte, a bleibt null.
    """
    out = bytearray()
    for i in range(0, len(raw), 3):
        for c in raw[i:i + 3]:
            out.append(((c << 2) | (c >> 4)) & 0xFF)
        out.append(0)
    return bytes(out)


def build_archive(src_dir, with_xmas, with_ending):
    names = sorted(n for n in os.listdir(src_dir) if wanted(n, with_xmas, with_ending))
    if not names:
        sys.exit(f"fehler: in {src_dir} keine Tyrian-Daten gefunden")

    for n in names:
        if len(n) >= NAME_LEN:
            sys.exit(f"fehler: Dateiname zu lang fuer das Archivformat: {n}")

    payloads = []
    offset = HEADER_LEN + ENTRY_LEN * len(names)
    entries = []

    for n in names:
        with open(os.path.join(src_dir, n), "rb") as f:
            data = f.read()
        if n.lower() == "palette.dat":
            data = prebake_palette(data)
        entries.append((n.lower().encode("ascii"), offset, len(data)))
        payloads.append(data)
        # Auf 4 ausrichten: die Sprite-Tabellen werden spaeter direkt aus dem
        # Flash gelesen, und ausgerichtete Zugriffe sind auf dem Cortex-M
        # schneller (und bei 32-Bit-Feldern ueberhaupt erst zulaessig).
        offset += (len(data) + 3) & ~3

    total = offset
    out = bytearray()
    out += MAGIC
    out += struct.pack("<II", len(names), total)
    for name, off, size in entries:
        out += name.ljust(NAME_LEN, b"\0")
        out += struct.pack("<II", off, size)

    for data in payloads:
        out += data
        out += b"\0" * ((-len(data)) % 4)

    assert len(out) == total, (len(out), total)
    return bytes(out), list(zip(names, (e[2] for e in entries)))


def to_uf2(data, base_addr):
    blocks = (len(data) + UF2_PAYLOAD - 1) // UF2_PAYLOAD
    out = bytearray()
    for i in range(blocks):
        chunk = data[i * UF2_PAYLOAD:(i + 1) * UF2_PAYLOAD]
        block = struct.pack(
            "<IIIIIIII",
            UF2_MAGIC_START0, UF2_MAGIC_START1, UF2_FLAG_FAMILY,
            base_addr + i * UF2_PAYLOAD, UF2_PAYLOAD, i, blocks, UF2_FAMILY_DATA,
        )
        # UF2-Block: 32 Byte Kopf + 476 Byte Datenfeld + 4 Byte Endkennung.
        # Genutzt werden davon UF2_PAYLOAD Byte, der Rest bleibt null.
        block += chunk.ljust(476, b"\0")
        block += struct.pack("<I", UF2_MAGIC_END)
        assert len(block) == 512
        out += block
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src", help="Verzeichnis mit den entpackten Tyrian-2.1-Daten")
    ap.add_argument("-o", "--out", default="tyrian-data.uf2")
    ap.add_argument("--bin", help="zusaetzlich das rohe Archiv hierhin schreiben")
    ap.add_argument("--addr", default="0x10100000",
                    help="Flash-Adresse des Archivs (muss zu PBC_XIPFS_ADDR passen)")
    ap.add_argument("--xmas", action="store_true",
                    help="Weihnachtsfassung mit aufnehmen (+0,5 MB)")
    ap.add_argument("--no-ending", action="store_true",
                    help="Schlussanimation tyrend.anm weglassen (-3,3 MB)")
    args = ap.parse_args()

    addr = int(args.addr, 0)
    archive, listing = build_archive(args.src, args.xmas, not args.no_ending)

    # 16 MB Flash, davon gehoert alles unterhalb von addr der Firmware und der
    # oberste Sektor den Spielstaenden.
    limit = 0x11000000 - 0x10000 - addr
    if len(archive) > limit:
        sys.exit(f"fehler: Archiv ist {len(archive)} Byte, es passen nur {limit}")

    with open(args.out, "wb") as f:
        f.write(to_uf2(archive, addr))

    if args.bin:
        with open(args.bin, "wb") as f:
            f.write(archive)

    print(f"{len(listing)} Dateien, {len(archive):,} Byte "
          f"({len(archive) / 1024 / 1024:.2f} MB) ab {addr:#x}")
    print(f"-> {args.out}")
    print(f"   Firmware hat damit {(addr - 0x10000000) / 1024:.0f} KB, "
          f"frei bleiben {(limit - len(archive)) / 1024 / 1024:.2f} MB")


if __name__ == "__main__":
    main()
