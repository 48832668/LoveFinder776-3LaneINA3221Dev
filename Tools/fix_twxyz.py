#!/usr/bin/env python3
"""Fix T, W, X, Y, Z glyphs in fonts.cpp."""

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

# T: top bar + center stem (3 columns wide: cols 2,3,4)
T = G([0,0,_E, C2|_C|C4,C2|_C|C4,C2|_C|C4,C2|_C|C4,C2|_C|C4,
         C2|_C|C4,C2|_C|C4,C2|_C|C4,C2|_C|C4,C2|_C|C4,C2|_C|C4,0,0,0,0], "T")

# W: three vertical strokes (cols 0,3,6) = 0x49
# Clearly different from V (\ /) and X (crossing)
W = G([0,0, _B|_C,_B|_C,_B|_C,_B|_C,_B|_C,_B|_C,
         _B|_C,_B|_C,_B|_C,_B|_C,_B|_C,_B|_C,0,0,0,0], "W")

# X: crossing diagonals
# \ diag: row2->col0, row4->col1, row6->col2, row8->col3, row10->col4, row12->col5, row14->col6
# / diag: row3->col6, row5->col5, row7->col4, row9->col3, row11->col2, row13->col1, row15->col0
X = G([0,0, 0x40,0x01,0x20,0x02,0x10,0x04,0x08,0x08,
         0x04,0x10,0x02,0x20,0x01,0x40, 0,0], "X")

# Y: V-shape upper (wide→center) + center stem
# \: row2 col0, row4 col1, row6 col2
# /: row3 col6, row5 col5, row7 col4
# center: row8 col3, then stem down
Y = G([0,0, 0x40,0x01,0x20,0x02,0x10,0x04,0x08,
         0x08,0x08,0x08,0x08,0x08,0x08,0x08, 0,0], "Y")

# Z: top bar + right-to-left diagonal + bottom bar
# row2: E (full top)
# row3-4: _R (col6)
# row5-6: 0x02 (col5)
# row7-8: 0x04 (col4)
# row9-10: 0x08 (col3/center)
# row11-12: 0x10 (col2)
# row13-14: 0x20 (col1)
# row15: E (full bottom)
Z = G([0,0, _E, _R, C5,C5, C4,C4, _C,_C, C2,C2, C1,C1, _L,_E, 0,0], "Z")

print("T:", T)
print()
print("W:", W)
print()
print("X:", X)
print()
print("Y:", Y)
print()
print("Z:", Z)

# Read and patch
with open(FONTS_CPP, 'r', encoding='utf-8') as f:
    cpp = f.read()

patches = {'T': T, 'W': W, 'X': X, 'Y': Y, 'Z': Z}

for ch, glyph_data in patches.items():
    # Find the array: from "static constexpr uint16_t s2_X[18] = {" to "};"
    pattern = rf'(static constexpr uint16_t s2_{ch}\[18\] = \{{)(.+?)(\}};)'
    replacement = rf'\1\n{glyph_data}\n\3'
    new_cpp, count = re.subn(pattern, replacement, cpp, count=1, flags=re.DOTALL)
    if count == 0:
        print(f"ERROR: Could not find s2_{ch} in file!")
    else:
        cpp = new_cpp
        print(f"Patched s2_{ch}")

with open(FONTS_CPP, 'w', encoding='utf-8') as f:
    f.write(cpp)
print("\nDone!")
