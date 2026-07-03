#!/usr/bin/env python3
"""Generate 16x16 Chinese glyph bitmap data for embedded display."""
from PIL import Image, ImageDraw, ImageFont
import os, sys

CHARS = [
    (0x4F60, "你"),
    (0x597D, "好"),
]

SIZE = 16

def find_font(name):
    paths = {
        'msyh': "C:/Windows/Fonts/msyh.ttc",
        'simsun': "C:/Windows/Fonts/simsun.ttc",
        'simhei': "C:/Windows/Fonts/simhei.ttf",
    }
    fp = paths.get(name, paths['simsun'])
    if os.path.exists(fp):
        return fp
    return None

def render_glyph_direct(char: str, font_path: str, size: int) -> list[str]:
    """Render Chinese char at exact pixel size using font's built-in renderer."""
    # Render at 2x then threshold-downscale for better hinting
    big_size = 64
    big = Image.new("L", (big_size, big_size), 0)
    bdraw = ImageDraw.Draw(big)
    
    font_big = ImageFont.truetype(font_path, big_size)
    bbox = font_big.getbbox(char)
    if bbox:
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
        x = (big_size - tw) // 2 - bbox[0]
        y = (big_size - th) // 2 - bbox[1]
        bdraw.text((x, y), char, font=font_big, fill=255)
    
    # Downscale to size×size
    small = big.resize((size, size), Image.LANCZOS)
    
    # Adaptive threshold (Otsu-like simplified)
    pixels = list(small.getdata())
    threshold = 128
    
    result = []
    for y in range(size):
        row = ''
        for x in range(size):
            px = pixels[y * size + x]
            row += '1' if px >= threshold else '0'
        result.append(row)
    return result

def print_glyph(bits: list[str]):
    for row in bits:
        line = ''.join('██' if c == '1' else '  ' for c in row)
        print(f"  {line}")

def bits_to_hex(bits: list[str]) -> list[int]:
    vals = []
    for row in bits:
        v = 0
        for i, c in enumerate(row):
            if c == '1':
                v |= 1 << (15 - i)
        vals.append(v)
    return vals

def generate(font_name):
    font_path = find_font(font_name)
    print(f"\n{'='*60}", file=sys.stderr)
    print(f"Font: {font_name}  ({font_path})", file=sys.stderr)
    print(f"{'='*60}", file=sys.stderr)
    
    for code, char in CHARS:
        bits = render_glyph_direct(char, font_path, SIZE)
        hex_vals = bits_to_hex(bits)
        print(f"\n// {char} (U+{code:04X}) — {SIZE}×{SIZE}  ({font_name})")
        print(f"static constexpr uint16_t s3_{font_name}_{char}[{SIZE}] = {{")
        for i in range(0, len(hex_vals), 4):
            chunk = hex_vals[i:i+4]
            print(f"    0x{', 0x'.join(f'{v:04X}' for v in chunk)},")
        print(f"}};")
        # Also output comma-separated for easy copy
        print(f"// {char} hex: {', '.join(f'0x{v:04X}' for v in hex_vals)}")

if __name__ == '__main__':
    import sys
    font = sys.argv[1] if len(sys.argv) > 1 else 'simsun'
    generate(font)
