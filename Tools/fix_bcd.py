#!/usr/bin/env python3
"""Fix B, C, D glyphs in fonts.cpp."""

import re, os
FONTS_CPP = os.path.join(os.path.dirname(__file__),
                         "..", "LoveFinderLib", "ST7735", "fonts.cpp")

def px7(p7):
    return f'0x{(p7 & 0x7F) << 8:04X}'

def G(rows, name=""):
    assert len(rows) == 18, f"{name}: need 18 rows got {len(rows)}"
    v = [px7(r) for r in rows]
    return '\n'.join([
        f"    {v[0]},{v[1]},{v[2]},{v[3]},{v[4]},{v[5]},",
        f"    {v[6]},{v[7]},{v[8]},{v[9]},{v[10]},{v[11]},",
        f"    {v[12]},{v[13]},{v[14]},{v[15]},{v[16]},{v[17]},"])

_L=0x40;_R=0x01;_B=0x41;_C=0x08;_E=0x7F
C1=0x20;C2=0x10;C4=0x04;C5=0x02

# ── B: TWO clearly visible loops (not like 6 which has one) ─────────
# Upper: both cols for 4 rows, then left only 2 rows → right side drops before mid bar
# Lower: both cols 6 rows → full lower loop
# This creates visible upper and lower right loops
B = G([0,0,_E,
       _B,_B,_B,_B,    # both cols 4 rows (upper-right loop visible)
       _L,_L,            # left only 2 rows (right drops before mid)
       _E,                # mid bar
       _B,_B,_B,_B,_B,_B,  # both cols 6 rows (lower loop)
       _E,                # bottom
       0], "B")

# ── C: reduce gap from 2 blank rows to 1 ────────────────────────────
C = G([0,0,_E,
       _L,_L,_L,_L,_L,  # left 5 rows upper
       0,                 # 1 blank row gap
       _L,_L,_L,_L,_L,  # left 5 rows lower
       _E,                # bottom bar
       0,0,0], "C")

# ── D: TWO visible loops (not like flipped 6) ───────────────────────
# Mirror of B: upper both cols, then right only drops before mid
D = G([0,0,_E,
       _B,_B,_B,_B,    # both cols 4 rows
       _R,_R,            # right only 2 rows (left drops before mid)
       _E,                # mid bar
       _B,_B,_B,_B,_B,_B,  # both cols 6 rows (lower loop)
       _E,                # bottom
       0], "D")

print("B:", B, "\n")
print("C:", C, "\n")
print("D:", D, "\n")

with open(FONTS_CPP, 'r', encoding='utf-8') as f:
    cpp = f.read()

for ch, glyph_data in [('B', B), ('C', C), ('D', D)]:
    pat = rf'(static constexpr uint16_t s2_{ch}\[18\] = \{{)(.+?)(\}};)'
    new_cpp, count = re.subn(pat, rf'\1\n{glyph_data}\n\3', cpp, count=1, flags=re.DOTALL)
    if count:
        cpp = new_cpp
        print(f"Patched s2_{ch}")
    else:
        print(f"ERROR: s2_{ch} not found")

with open(FONTS_CPP, 'w', encoding='utf-8') as f:
    f.write(cpp)
print("Done!")
