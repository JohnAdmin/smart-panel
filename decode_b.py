"""Decode lv_font glyph for U+0042 'B' from generated source file."""
import re, sys

path = "src/lv_font_montserrat_14.c"
src = open(path, encoding="utf-8").read()

# Extract glyph_bitmap[] body
m = re.search(r"glyph_bitmap\[\]\s*=\s*\{(.*?)\};", src, re.S)
body = m.group(1)

# Find U+0042 B section
m2 = re.search(r"/\*\s*U\+0042\s*\"B\"\s*\*/(.*?)/\*\s*U\+0043", body, re.S)
b_section = m2.group(1)
b_bytes = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]+)", b_section)]
print(f"B bytes: {len(b_bytes)}")

# Compute byte offset of B from start of glyph_bitmap
prefix = body[:m2.start()]
prefix_bytes = re.findall(r"0x([0-9a-fA-F]+)", prefix)
print(f"B bitmap_index (computed): {len(prefix_bytes)}")

# Find glyph_dsc[35] (B)
m3 = re.search(r"glyph_dsc\[\]\s*=\s*\{(.*?)\};", src, re.S)
dsc_body = m3.group(1)
entries = re.findall(r"\{\.bitmap_index\s*=\s*(\d+),\s*\.adv_w\s*=\s*(\d+),\s*\.box_w\s*=\s*(\d+),\s*\.box_h\s*=\s*(\d+),\s*\.ofs_x\s*=\s*(-?\d+),\s*\.ofs_y\s*=\s*(-?\d+)", dsc_body)
print(f"glyph_dsc count: {len(entries)}")
print(f"glyph_dsc[35] (B): bitmap_index={entries[35][0]}, adv_w={entries[35][1]}, box_w={entries[35][2]}, box_h={entries[35][3]}, ofs_x={entries[35][4]}, ofs_y={entries[35][5]}")
print(f"glyph_dsc[34] (A): bitmap_index={entries[34][0]}, adv_w={entries[34][1]}, box_w={entries[34][2]}, box_h={entries[34][3]}, ofs_x={entries[34][4]}, ofs_y={entries[34][5]}")

# Decode B as ASCII art using glyph_dsc[35] params
bi, _, bw, bh, ox, oy = (int(x) for x in entries[35])
print(f"\nB dimensions: {bw}x{bh}, ofs_x={ox}, ofs_y={oy}")
nibbles = []
for byte in b_bytes:
    nibbles.append((byte >> 4) & 0xF)
    nibbles.append(byte & 0xF)
print(f"Total nibbles: {len(nibbles)}, expected: {bw*bh} = {bw*bh}")

palette = " .:-=+*#%@"
print("\nASCII rendering of B:")
for r in range(bh):
    row = ""
    for c in range(bw):
        idx = r*bw + c
        if idx < len(nibbles):
            v = nibbles[idx]
            row += palette[min(v * len(palette) // 16, len(palette)-1)]
        else:
            row += "?"
    print(f"  |{row}|")
