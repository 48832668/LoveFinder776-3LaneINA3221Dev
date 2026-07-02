#!/usr/bin/env python3
"""Test the FontFile class from font_editor.py"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

# Import the module, avoiding GUI init
import importlib.util
spec = importlib.util.spec_from_file_location("font_editor", HERE / "font_editor.py")
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

# Test parsing
fnt = mod.FontFile(HERE.parent / "LoveFinderLib" / "ST7735" / "fonts.cpp")
s1 = sum(1 for g in fnt.glyphs if g.startswith("s1_"))
s2 = sum(1 for g in fnt.glyphs if g.startswith("s2_"))
print(f"Loaded {len(fnt.glyphs)} glyphs (s1={s1} s2={s2})")

# Test s2_B
gd = fnt.glyphs["s2_B"]
print(f's2_B: char={gd.char!r} ascii={gd.ascii} rows={gd.rows} cols={gd.cols}')

# Count lit pixels in s2_B
lit = sum(1 for r in range(gd.rows) for c in range(gd.cols) if gd.pixels[r][c])
print(f"s2_B lit pixels: {lit}")

# Test save (round-trip)
ok = fnt.save_glyph("s2_B")
print(f"Save s2_B: {ok}")

fnt2 = mod.FontFile(HERE.parent / "LoveFinderLib" / "ST7735" / "fonts.cpp")
gd2 = fnt2.glyphs["s2_B"]
match = all(
    gd.pixels[r][c] == gd2.pixels[r][c]
    for r in range(gd.rows)
    for c in range(gd.cols)
)
print(f"Round-trip match: {match}")

# Test char/ascii mapping
tests = ["s2_0", "s2_9", "s2_A", "s2_B", "s2_C", "s2_D", "s2_H",
         "s2_space", "s2_dash", "s2_dot",
         "s1_space", "s1_excl", "s1_A", "s1_I"]
for n in tests:
    g = fnt.glyphs.get(n)
    if g:
        print(f"  {n}: char={g.char!r} ascii={g.ascii}")

# Test rendering
print("\ns2_B preview:")
print(gd.render_ascii())

print("\nAll tests passed!" if match else "\nTEST FAILED!")
