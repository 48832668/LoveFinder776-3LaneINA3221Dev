#!/usr/bin/env python3
"""Generate & inject glyph arrays for Font_Style2_9x18 (A-Z, a-z) + update lookup table."""

import re, os, sys

FONTS_CPP = os.path.join(os.path.dirname(__file__),
                         "..", "LoveFinderLib", "ST7735", "fonts.cpp")

def px7(p7):
    return f'0x{(p7 & 0x7F) << 8:04X}'

_L=0x40; C1=0x20; C2=0x10; _C=0x08; C4=0x04; C5=0x02; _R=0x01; _B=0x41; _E=0x7F

def G(rows):
    assert len(rows)==18, f"Need 18 rows got {len(rows)}"
    v=[px7(r) for r in rows]
    return '\n'.join([
        f"    {v[0]},{v[1]},{v[2]},{v[3]},{v[4]},{v[5]},",
        f"    {v[6]},{v[7]},{v[8]},{v[9]},{v[10]},{v[11]},",
        f"    {v[12]},{v[13]},{v[14]},{v[15]},{v[16]},{v[17]},"])

# Glyph data
A={}
A['A']=G([0,0,_E,_B,_B,_B,_B,_B,_B,_E,_B,_B,_B,_B,_B,_B,0,0])
A['B']=G([0,0,_E,_L,_L,_L,_L,_L,_L,_E,_B,_B,_B,_B,_B,_B,_E,0])
A['C']=G([0,0,_E,_L,_L,_L,_L,_L,0,0,_L,_L,_L,_L,_L,_E,0,0])
A['D']=G([0,0,_E,_R,_R,_R,_R,_R,_R,_E,_B,_B,_B,_B,_B,_B,_E,0])
A['E']=G([0,0,_E,_L,_L,_L,_L,_L,_L,_E,_L,_L,_L,_L,_L,_E,0,0])
A['F']=G([0,0,_E,_L,_L,_L,_L,_L,_L,_E,_L,_L,_L,_L,_L,_L,0,0])
A['G']=G([0,0,_E,_L,_L,_L,_L,_L,0,0,_B,_B,_B,_B,_B,_E,0,0])
A['H']=G([0,0,_B,_B,_B,_B,_B,_B,_E,_B,_B,_B,_B,_B,_B,0,0,0])
A['I']=G([0,0,_E,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,_E,0,0])
A['J']=G([0,0,_E,_R,_R,_R,_R,_R,0,0,_B,_B,_B,_B,_B,_E,0,0])
A['K']=G([0,0,_L,_L,_L|C5,_L|C4,_L|C2,_L|C1,_L|_R,
           _L|_R,_L|C1,_L|C2,_L|C4,_L|C5,_L,_L,0,0])
A['L']=G([0,0,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_E,0,0,0])
A['M']=G([0,0,_B,_B,_B|_C,_B|_C,_B|_C,_B|_C,
           _B|_C,_B,_B,_B,_B,_B,_B,_B,0,0])
A['N']=G([0,0,_B,_B,_L|C4,_L|C2,_L|C1,_L|_R,
           _L|_R,_L|_R,C1|_R,C2|_R,C4|_R,_B,_B,_B,0,0])
A['O']=G([0,0,_E,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_E,0,0])
A['P']=G([0,0,_E,_B,_B,_B,_B,_B,_B,_E,_L,_L,_L,_L,_L,_L,0,0])
A['Q']=G([0,0,_E,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_E,_L|C4,0])
A['R']=G([0,0,_E,_B,_B,_B,_B,_B,_B,_E,_L,_L|C1,_L|C2,_L|C4,_L|C5,_L,0,0])
A['S']=G([0,0,_E,_L,_L,_L,0,0,0,_E,0,0,_R,_R,_R,_E,0,0])
A['T']=G([0,0,_E,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,0,0,0])
A['U']=G([0,0,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_E,0,0,0])
A['V']=G([0,0,_B,_B,_B|C4,_B|C2,_L|C1|_R,_L|_R,
           _L|C5,_L|C4,_L|C2,_L|C1,_C,_C,0,0,0,0])
A['W']=G([0,0,_B|_C,_B|_C,_B|_C,_B,_B,_B|_C,
           _B|_C,_B|_C,_L|C4|_R,_L|C2,_B,_B,0,0,0,0])
A['X']=G([0,0,_B,_B,_L|C4,_L|C2,_L|C1,_L|_R,
           _C,_C,_L|_R,_L|C1,_L|C2,_L|C4,_B,_B,0,0])
A['Y']=G([0,0,_B,_B,_L|C4,_L|C2,_L|C1,_L|_R,
           _C,_C,_C,_C,_C,_C,_C,_C,0,0])
A['Z']=G([0,0,_E,_R,_R,_R|C4,_R|C2,_R|C1,
           _L|_R,_L|C1,_L|C2,_L|C4,_L,_L,_E,0,0,0])

A['a']=G([0,0,0,0,0,0,0,0,_E,_B,_B,_B,_B,_B,_B,_E,0,0])
A['b']=G([0,0,_L,_L,_L,_L,_L,_L,_E,_B,_B,_B,_B,_B,_B,_E,0,0])
A['c']=G([0,0,0,0,0,0,0,0,0,_L,_L,_L,_L,_L,_E,0,0,0])
A['d']=G([0,0,_R,_R,_R,_R,_R,_R,_E,_B,_B,_B,_B,_B,_B,_E,0,0])
A['e']=G([0,0,0,0,0,0,0,0,_E,_L,_L,_L,_L,_L,_E,0,0,0])
A['f']=G([0,0,_E,_L,_L,_L,_L,_L,_L,_L,0,0,0,0,0,0,0,0])
A['g']=G([0,0,0,0,0,0,0,0,_E,_B,_B,_B,_B,_B,_B,_E,_C,0])
A['h']=G([0,0,_L,_L,_L,_L,_L,_L,_E,_L,_L,_L,_L,_L,_L,0,0,0])
A['i']=G([0,_R,0,0,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,_E,0,0,0])
A['j']=G([0,_R,0,0,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,_E,_C,0,0])
A['k']=G([0,0,_L,_L,_L,_L,_L,_L,_L|C4,_L|C2,_L|_R,_L|C2,_L|C4,_L,0,0,0,0])
A['l']=G([0,0,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,_R,_E,0,0,0])
A['m']=G([0,0,0,0,_B,_B|_C,_B|_C,_B|_C,_B|_C,_B,_B,_B,_B,_B,0,0,0,0])
A['n']=G([0,0,0,0,0,0,_E,_B,_B,_B,_B,_B,_B,_B,0,0,0,0])
A['o']=G([0,0,0,0,0,0,0,_E,_B,_B,_B,_B,_B,_B,_E,0,0,0])
A['p']=G([0,0,_L,_L,_L,_L,_L,_L,_E,_B,_B,_B,_B,_B,_B,_E,_L,_L])
A['q']=G([0,0,0,0,0,0,0,0,_E,_B,_B,_B,_B,_B,_B,_E,_R,_R])
A['r']=G([0,0,0,0,0,0,0,0,_E,_L,_C,0,0,0,0,0,0,0])
A['s']=G([0,0,0,0,0,0,0,_E,_L,_L,_E,_R,_R,_E,0,0,0,0])
A['t']=G([0,0,_L,_L,_L,_E,_R,_R,_R,_R,_R,_R,0,0,0,0,0,0])
A['u']=G([0,0,0,0,0,0,0,_B,_B,_B,_B,_B,_B,_E,0,0,0,0])
A['v']=G([0,0,0,0,0,0,0,_B,_B|C4,_L|C2,_L|C1,_C,_C,0,0,0,0,0])
A['w']=G([0,0,0,0,0,0,0,_B,_B,_B,_B|_C,_B|_C,_B|_C,_B,0,0,0,0])
A['x']=G([0,0,0,0,0,0,_B,_L|C4,_L|C2,_C,_L|C2,_L|C4,_B,0,0,0,0,0])
A['y']=G([0,0,0,0,0,0,_B,_L|C4,_L|C2,_C,_C,_C,_C,_C,_C,_C,0,0])
A['z']=G([0,0,0,0,0,0,_E,_R,_R|C4,_C,_L|C4,_L,_E,0,0,0,0,0])

for c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz':
    assert c in A, f"Missing {c}"
print(f"Validated {len(A)} glyphs")

# ── Read fonts.cpp ─────────────────────────────────────────────────
with open(FONTS_CPP, 'r', encoding='utf-8') as f:
    cpp = f.read()

# Find existing s2_ arrays
existing = set(re.findall(r's2_(\w+)\[18\]', cpp))
new_chars = [c for c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz' if c not in existing]
print(f"Existing arrays: {sorted(existing)}")
print(f"New arrays to add ({len(new_chars)}): {new_chars}")

# Insert arrays before glyph_table_s2 function
marker = 'static const uint16_t* glyph_table_s2'
idx = cpp.find(marker)
assert idx >= 0, f"'{marker}' not found"

block = []
for ch in new_chars:
    block.append(f"// ASCII {ord(ch)} ({ch})")
    block.append(f"static constexpr uint16_t s2_{ch}[18] = {{")
    block.append(A[ch])
    block.append("};")
    block.append("")

insert = '\n'.join(block)
prefix = cpp[:idx].rstrip() + '\n\n'
new_cpp = prefix + insert + '\n' + cpp[idx:]

with open(FONTS_CPP, 'w', encoding='utf-8') as f:
    f.write(new_cpp)
print(f"Injected {len(new_chars)} new glyph arrays")

# ── Update lookup table ───────────────────────────────────────────
with open(FONTS_CPP, 'r', encoding='utf-8') as f:
    cpp = f.read()

# Parse table body
table_start = cpp.find('static const uint16_t* glyph_table_s2')
assert table_start >= 0
brace_open = cpp.find('{', table_start)
brace_count = 1
i = brace_open + 1
while brace_count > 0 and i < len(cpp):
    if cpp[i] == '{': brace_count += 1
    elif cpp[i] == '}': brace_count -= 1
    i += 1
table_end = i - 1  # position of closing '}'

table_body = cpp[brace_open+1:table_end]

# Existing case codes
existing_codes = set()
for m in re.finditer(r'case\s+(\d+):', table_body):
    existing_codes.add(int(m.group(1)))

# Build new cases
new_cases = []
for c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz':
    code = ord(c)
    if code not in existing_codes:
        new_cases.append(f"        case {code}: return s2_{c};  // {c}")

if new_cases:
    # Insert before default
    lines = table_body.split('\n')
    def_idx = next(i for i, l in enumerate(lines) if 'default:' in l)
    lines = lines[:def_idx] + new_cases + lines[def_idx:]
    new_body = '\n'.join(lines)
    new_cpp = cpp[:brace_open+1] + new_body + cpp[table_end:]
    with open(FONTS_CPP, 'w', encoding='utf-8') as f:
        f.write(new_cpp)
    print(f"Added {len(new_cases)} new cases to glyph_table_s2")
else:
    print("All cases already present")
