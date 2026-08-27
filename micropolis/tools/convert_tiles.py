#!/usr/bin/env python3
"""
convert_tiles.py — turn the original Micropolis tile artwork into an
RGB565 C array that lives in flash, implementing the tile_get() seam.

The classic Micropolis tile sheet is a single column of 16x16 tiles
(tilewidth = 16, one tile per row in the image, ~960 tiles tall). Some
distributions ship a wider sheet; pass --cols to match.

Usage:
    python3 convert_tiles.py tiles.png --out ../src/tiles_micropolis.c
    python3 convert_tiles.py tiles.png --tile 16 --cols 1 --count 960

Output: a C file defining
    const uint16_t kMicropolisTiles[COUNT][256];   // in flash (.rodata)
    const uint16_t *tile_get(uint16_t id);

Swap tiles_placeholder.c for the generated file in CMakeLists.txt.
"""
import argparse, sys

def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image", help="path to the Micropolis tile sheet (PNG)")
    ap.add_argument("--tile", type=int, default=16, help="tile size in px (default 16)")
    ap.add_argument("--cols", type=int, default=1, help="tiles per row in the sheet")
    ap.add_argument("--count", type=int, default=0, help="number of tiles (0 = all)")
    ap.add_argument("--out", default="tiles_micropolis.c")
    args = ap.parse_args()

    try:
        from PIL import Image
    except ImportError:
        sys.exit("Pillow required:  pip install pillow")

    img = Image.open(args.image).convert("RGB")
    T = args.tile
    cols = args.cols
    rows = img.height // T
    total = cols * rows
    count = args.count or total
    if count > total:
        count = total

    with open(args.out, "w") as f:
        f.write('#include "tiles.h"\n\n')
        f.write(f"// Auto-generated from {args.image} ({count} tiles, {T}x{T}).\n")
        f.write(f"const uint16_t kMicropolisTiles[{count}][{T*T}] = {{\n")
        for i in range(count):
            cx = (i % cols) * T
            cy = (i // cols) * T
            f.write("  {")
            for y in range(T):
                for x in range(T):
                    r, g, b = img.getpixel((cx + x, cy + y))
                    f.write(f"0x{rgb565(r,g,b):04X},")
            f.write("},\n")
        f.write("};\n\n")
        f.write("void tiles_init(void) { /* nothing: tiles live in flash */ }\n\n")
        f.write("const uint16_t *tile_get(uint16_t id) {\n")
        f.write(f"    if (id >= {count}) id = 0;\n")
        f.write("    return kMicropolisTiles[id];\n}\n")

    print(f"Wrote {args.out}: {count} tiles, {count*T*T*2} bytes in flash.")

if __name__ == "__main__":
    main()
