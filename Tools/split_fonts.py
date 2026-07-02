#!/usr/bin/env python3
"""
Split flat font arrays into per-character arrays + lookup functions.
Reads existing fonts.cpp, extracts per-char data, writes new fonts.cpp.
"""
import re
import os

FONTS_CPP = os.path.join(os.path.dirname(__file__),
                         "..", "LoveFinderLib", "ST7735", "fonts.cpp")

def parse_array_entries(text: str, start_marker: str):
    """Find array name and extract hex values from C array."""
    # Find the array
    pattern = re.escape(start_marker) + r'\s*\[\]\s*=\s*\{'
    m = re.search(pattern, text)
    if not m:
        # Try without marker
        pass
    
    # Find array: optional static constexpr, uint16_t, name, [] = { ... };
    pattern = r'static\s+constexpr\s+uint16_t\s+(\w+)\[\]\s*=\s*\{(.*?)\};'
    for m in re.finditer(pattern, text, re.DOTALL):
        name = m.group(1)
        body = m.group(2)
        values = []
        for tok in re.split(r'[,\s]+', body):
            tok = tok.strip().strip(',')
            if tok.startswith('0x') or tok.startswith('0X'):
                try:
                    values.append(tok)
                except ValueError:
                    pass
            elif tok == '':
                continue
        if len(values) > 10:  # Has real data
            return name, values
    return None, []


def extract_glyph(data: list, ch: int, height: int) -> list:
    """Extract height entries for given char from flat array."""
    idx = (ch - 32) * height
    if idx < 0 or idx + height > len(data):
        return ['0x0000'] * height
    return data[idx:idx + height]


def is_blank(glyph: list) -> bool:
    """Check if all entries are 0x0000."""
    return all(v == '0x0000' for v in glyph)


def hex_to_pixels(hex_val: str, width: int) -> str:
    """Convert hex value to pixel string (# for on, . for off)."""
    val = int(hex_val, 16)
    pixels = ''
    for col in range(width):
        if (val >> (15 - col)) & 1:
            pixels += '#'
        else:
            pixels += '.'
    return pixels


def main():
    with open(FONTS_CPP, 'r', encoding='utf-8') as f:
        text = f.read()
    
    # Find Font7x10Data array
    m7 = re.search(r'static\s+constexpr\s+uint16_t\s+Font7x10Data\[\]\s*=\s*\{(.*?)\};', text, re.DOTALL)
    if not m7:
        print("ERROR: Font7x10Data not found")
        return
    f7_data = []
    body7 = m7.group(1)
    for tok in re.split(r'[,\s]+', body7):
        tok = tok.strip().strip(',')
        if tok.startswith('0x') or tok.startswith('0X'):
            f7_data.append(tok)
    
    # Find Font9x18Data array  
    m9 = re.search(r'static\s+constexpr\s+uint16_t\s+Font9x18Data\[\]\s*=\s*\{(.*?)\};', text, re.DOTALL)
    if not m9:
        print("ERROR: Font9x18Data not found")
        return
    f9_data = []
    body9 = m9.group(1)
    for tok in re.split(r'[,\s]+', body9):
        tok = tok.strip().strip(',')
        if tok.startswith('0x') or tok.startswith('0X'):
            f9_data.append(tok)
    
    print(f"Font7x10Data: {len(f7_data)} entries ({len(f7_data)//10} chars)")
    print(f"Font9x18Data: {len(f9_data)} entries ({len(f9_data)//18} chars)")
    
    # Verify sanity
    assert len(f7_data) == 950, f"Expected 950 entries, got {len(f7_data)}"
    assert len(f9_data) == 1710, f"Expected 1710 entries, got {len(f9_data)}"
    
    # ---- Char sets needed ----
    style2_chars = [32, 45, 46] + list(range(48, 58)) + [65, 67, 72, 86, 87, 104, 109]
    # Chars needed for Style1_7x10
    style1_chars = [32, 33] + list(range(48, 58)) + list(range(65, 71)) + [73, 78, 87] + [99, 100, 101, 105, 110, 111, 115, 116, 117, 118, 120]
    
    # Verify all have actual glyph data (not blank)
    print("\n=== Style2_9x18 chars ===")
    for ch in style2_chars:
        glyph = extract_glyph(f9_data, ch, 18)
        blank = is_blank(glyph)
        c = chr(ch) if 32 <= ch <= 126 else '?'
        print(f"  {ch:3d} '{c}': {'BLANK' if blank else 'ok'}")
    
    print("\n=== Style1_7x10 chars ===")
    for ch in style1_chars:
        glyph = extract_glyph(f7_data, ch, 10)
        blank = is_blank(glyph)
        c = chr(ch) if 32 <= ch <= 126 else '?'
        print(f"  {ch:3d} '{c}': {'BLANK' if blank else 'ok'}")
    
    # Generate new fonts.cpp
    generate(f7_data, f9_data, style1_chars, style2_chars)


def glyph_name(prefix: str, ch: int) -> str:
    """Generate a readable C++ identifier for a character glyph."""
    if ch == 32:
        return f"{prefix}_space"
    elif ch == 33:
        return f"{prefix}_excl"
    elif ch == 45:
        return f"{prefix}_dash"
    elif ch == 46:
        return f"{prefix}_dot"
    elif 48 <= ch <= 57:
        return f"{prefix}_{chr(ch)}"
    elif 65 <= ch <= 90:
        return f"{prefix}_{chr(ch)}"
    elif 97 <= ch <= 122:
        return f"{prefix}_{chr(ch)}"
    else:
        return f"{prefix}_{ch}"


def generate(f7_data, f9_data, s1_chars, s2_chars):
    """Generate the new fonts.cpp content."""
    lines = []
    
    lines.append('''/**
 * @file fonts.cpp
 * @brief Font Bitmap Data — Per-Character Conditional Compilation
 *
 * Each used character is a separate static constexpr array.
 * A lookup function (glyph_table_*) maps ASCII code → glyph data pointer.
 * Unused characters are simply not declared — zero flash cost.
 *
 * Based on: https://github.com/afiskon/stm32-st7735
 */

#include "fonts.hpp"
#include "fonts_config.hpp"

/*============================================================================
 * Character Set: Which chars are actually used in the firmware?
 *
 * Analyzed from Core/Src/main.cpp:
 *
 * Font_Style2_9x18 (main display — all 4 rows):
 *   - "---" (no input):                      dash (-)
 *   - "CH%d" → "CH1"/"CH2"/"CH3":           C, H, 1-3
 *   - "%02u.%02uV" → "05.23V":              0-9, dot (.), V
 *   - "%03u.%uW" → "012.3W":                0-9, dot (.), W
 *   - "%05umAh" → "00123mAh":               0-9, m, A, h
 *   - "%02u.%02u" → "05.23":                0-9, dot (.)
 *   - space for padding                      32
 *   => 19 chars: space, -, ., 0-9, A, C, H, V, W, h, m
 *
 * Font_Style1_7x10 (debug/badge screens):
 *   - "INA3221 Not Found!":                  I,N,A,3,2,1, space,o,t,F,u,n,d,!
 *   - "I2C Devices":                         I,2,C, space,D,e,v,i,c,s
 *   - "0x%02X" → hex addresses:             0,x,0-9,A-F
 *   - Badges "A" and "W":                    A, W
 *   => 32 chars: space,!,0-9,A-F,I,N,W,c,d,e,i,n,o,s,t,u,v,x
 *============================================================================*/

''')
    
    # ==================================================================
    # Font_Style1_7x10 — Per-character arrays
    # ==================================================================
    lines.append('/*============================================================================\n')
    lines.append(' * Font_Style1_7x10 — Per-Character Glyph Arrays\n')
    lines.append(' *   7px wide × 10px tall, one uint16_t per row\n')
    lines.append(' *   Bit 15 = leftmost pixel\n')
    lines.append(' *============================================================================*/\n')
    lines.append('\n')
    lines.append('#if USE_FONT_STYLE1_7X10\n')
    lines.append('\n')
    
    for ch in s1_chars:
        glyph = extract_glyph(f7_data, ch, 10)
        name = glyph_name('s1', ch)
        c = chr(ch) if 32 <= ch <= 126 else f'\\x{ch:02x}'
        label = repr(c)[1:-1] if ch >= 32 else f'\\x{ch:02x}'
        lines.append(f'// ASCII {ch} ({label})\n')
        lines.append(f'static constexpr uint16_t {name}[10] = {{\n')
        # 5 entries per line
        for i in range(0, 10, 5):
            chunk = glyph[i:i+5]
            lines.append('    ' + ', '.join(chunk) + ',\n')
        lines.append('};\n\n')
    
    # Lookup function for Style1
    lines.append('// ── Glyph Lookup Table ────────────────────────────────────\n')
    lines.append('static const uint16_t* glyph_table_s1(uint8_t ch) {\n')
    lines.append('    switch (ch) {\n')
    for ch in s1_chars:
        name = glyph_name('s1', ch)
        c = chr(ch) if 32 <= ch <= 126 else f'\\x{ch:02x}'
        label = repr(c)[1:-1]
        lines.append(f'        case {ch}: return {name};  // {label}\n')
    lines.append('        default: return nullptr;\n')
    lines.append('    }\n')
    lines.append('}\n\n')
    
    lines.append('const FontDef Font_Style1_7x10 = {7, 10, nullptr};\n')
    lines.append('#endif /* USE_FONT_STYLE1_7X10 */\n\n')
    
    # ==================================================================
    # Font_Style2_9x18 — Per-character arrays
    # ==================================================================
    lines.append('/*============================================================================\n')
    lines.append(' * Font_Style2_9x18 — Per-Character Glyph Arrays\n')
    lines.append(' *   9px wide × 18px tall, one uint16_t per row\n')
    lines.append(' *   Bit 15 = leftmost pixel (bits 15-7 = 9px)\n')
    lines.append(' *============================================================================*/\n')
    lines.append('\n')
    lines.append('#if USE_FONT_STYLE2_9X18\n')
    lines.append('\n')
    
    for ch in s2_chars:
        glyph = extract_glyph(f9_data, ch, 18)
        name = glyph_name('s2', ch)
        c = chr(ch) if 32 <= ch <= 126 else f'\\x{ch:02x}'
        label = repr(c)[1:-1]
        lines.append(f'// ASCII {ch} ({label})\n')
        lines.append(f'static constexpr uint16_t {name}[18] = {{\n')
        for i in range(0, 18, 6):
            chunk = glyph[i:i+6]
            lines.append('    ' + ', '.join(chunk) + ',\n')
        lines.append('};\n\n')
    
    # Lookup function for Style2
    lines.append('// ── Glyph Lookup Table ────────────────────────────────────\n')
    lines.append('static const uint16_t* glyph_table_s2(uint8_t ch) {\n')
    lines.append('    switch (ch) {\n')
    for ch in s2_chars:
        name = glyph_name('s2', ch)
        c = chr(ch) if 32 <= ch <= 126 else f'\\x{ch:02x}'
        label = repr(c)[1:-1]
        lines.append(f'        case {ch}: return {name};  // {label}\n')
    lines.append('        default: return nullptr;\n')
    lines.append('    }\n')
    lines.append('}\n\n')
    
    lines.append('const FontDef Font_Style2_9x18 = {9, 18, nullptr};\n')
    lines.append('#endif /* USE_FONT_STYLE2_9X18 */\n')
    
    # ==================================================================
    # Global glyph lookup dispatcher
    # ==================================================================
    lines.append('\n')
    lines.append('/*============================================================================\n')
    lines.append(' * Global Glyph Lookup — called by ST7735::writeCharDMA\n')
    lines.append(' *============================================================================*/\n')
    lines.append('\n')
    lines.append('const uint16_t* font_get_glyph(const FontDef& font, uint8_t ch) {\n')
    lines.append('#if USE_FONT_STYLE1_7X10\n')
    lines.append('    if (&font == &Font_Style1_7x10) return glyph_table_s1(ch);\n')
    lines.append('#endif\n')
    lines.append('#if USE_FONT_STYLE2_9X18\n')
    lines.append('    if (&font == &Font_Style2_9x18) return glyph_table_s2(ch);\n')
    lines.append('#endif\n')
    lines.append('    return nullptr;\n')
    lines.append('}\n')
    
    output = ''.join(lines)
    output_path = os.path.join(os.path.dirname(FONTS_CPP), 'fonts.new.cpp')
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(output)
    
    print(f"\nGenerated: {output_path}")
    print(f"  Style1_7x10: {len(s1_chars)} glyph arrays + lookup")
    print(f"  Style2_9x18: {len(s2_chars)} glyph arrays + lookup")
    
    # Also show before/after size comparison
    s1_old_size = 950 * 2  # 950 uint16_t = 1900 bytes
    s1_new_size = len(s1_chars) * 10 * 2  # per-char arrays
    s2_old_size = 1710 * 2
    s2_new_size = len(s2_chars) * 18 * 2
    lookup_overhead = 200  # rough estimate for switch/jump table
    
    print(f"\n=== Flash Size Comparison ===")
    print(f"Style1_7x10: {s1_old_size} bytes → {s1_new_size + lookup_overhead} bytes (save {s1_old_size - s1_new_size - lookup_overhead})")
    print(f"Style2_9x18: {s2_old_size} bytes → {s2_new_size + lookup_overhead} bytes (save {s2_old_size - s2_new_size - lookup_overhead})")
    print(f"Total:       {s1_old_size + s2_old_size} bytes → {s1_new_size + s2_new_size + lookup_overhead*2} bytes (save {s1_old_size + s2_old_size - s1_new_size - s2_new_size - lookup_overhead*2})")


if __name__ == '__main__':
    main()
