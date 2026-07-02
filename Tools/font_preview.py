#!/usr/bin/env python3
"""
Font Glyph Previewer — Pixel Grid Visualization
================================================
Reads per-character glyph arrays in both old (flat array) and new
(per-character arrays + lookup table) formats from fonts.cpp
and renders any character as a zoomed pixel grid.

Features:
   - Supports both flat FontXxxData[] and per-character s1_xxx/s2_xxx arrays
   - Select chars via dropdown (0-9, A-Z, a-z) or type any ASCII char (32-126)
   - Shows font name, char, ASCII code, dimensions, used-char count
   - Pixel cell colors adapt to dark theme
   - Handles disabled fonts (data = nullptr)
"""

import re
import os

# ──────────────────────────────────────────────────────────────────────────
# 1.  Parser — extracts font data from C++ source
#     Handles both old flat-array and new per-char-array formats.
# ──────────────────────────────────────────────────────────────────────────

FONTS_CPP = os.path.join(os.path.dirname(__file__),
                         "..", "LoveFinderLib", "ST7735", "fonts.cpp")


def parse_hex_array(body: str) -> list[int]:
    """Return list of ints from a C/C++ hex literal array body."""
    values = []
    for tok in re.split(r'[,\s{}]+', body):
        tok = tok.strip()
        if tok.startswith("0x") or tok.startswith("0X"):
            try:
                values.append(int(tok, 16) & 0xFFFF)
            except ValueError:
                pass
    return values


def parse_fonts_cpp(filepath: str):
    """
    Returns dict: font_name → {
        width, height, data: list[int] (flattened 95-char array),
        array_name, char_count, char_total
    }

    Supports two formats:
      OLD:  single flat array FontXxxData[] + FontDef = {W, H, ArrayName}
      NEW:  per-char arrays sX_NAME[h] + glyph_table_sX() + FontDef = {W, H, nullptr}
    """
    with open(filepath, "r", encoding="utf-8") as f:
        text = f.read()

    # ── Step 1: Find ALL uint16_t arrays ─────────────────────────────────
    # Match both name[] and name[N] declarations
    all_arrays = {}
    array_pattern = r'(?:static\s+constexpr\s+)?uint16_t\s+(\w+)\[\d*\]\s*=\s*\{(.*?)\};'
    for m in re.finditer(array_pattern, text, re.DOTALL):
        name = m.group(1)
        body = m.group(2)
        vals = parse_hex_array(body)
        if vals:
            all_arrays[name] = vals

    # ── Step 2: Find lookup functions (new format) ───────────────────────
    # Extract: case N: return ArrayName;
    lookup_tables = {}  # suffix → { ascii_code: glyph_data_list }
    lookup_pattern = r'static\s+const\s+uint16_t\*\s*glyph_table_(\w+)\(uint8_t\s+ch\).*?\{(.*?)\}'
    for m in re.finditer(lookup_pattern, text, re.DOTALL):
        suffix = m.group(1)  # 's1', 's2'
        body = m.group(2)
        char_map = {}
        case_pattern = r'case\s+(\d+):\s+return\s+(\w+);'
        for cm in re.finditer(case_pattern, body):
            ascii_code = int(cm.group(1))
            array_name = cm.group(2)
            if array_name in all_arrays:
                char_map[ascii_code] = all_arrays[array_name]
        lookup_tables[suffix] = char_map

    # ── Step 3: Find FontDef declarations ────────────────────────────────
    # Match: const FontDef Font_StyleX_YxZ = {W, H, ArrayName_or_nullptr};
    fonts = {}

    # Old format: Font_XXX = {W, H, FlatArrayName}
    old_pattern = r'const\s+FontDef\s+(Font_\w+)\s*=\s*\{(\d+)\s*,\s*(\d+)\s*,\s*(\w+)\s*\};'
    for m in re.finditer(old_pattern, text):
        font_name = m.group(1)
        width = int(m.group(2))
        height = int(m.group(3))
        array_name = m.group(4)
        data = all_arrays.get(array_name, [])
        fonts[font_name] = {
            "width": width,
            "height": height,
            "data": data,
            "array_name": array_name,
            "char_count": len(data) // height if height > 0 else 0,
            "char_total": 95,
        }

    # New format: Font_StyleN_... = {W, H, nullptr} → look up glyph_table_sN
    new_pattern = r'const\s+FontDef\s+(Font_Style(\d+)_\w+)\s*=\s*\{(\d+)\s*,\s*(\d+)\s*,\s*nullptr\s*\};'
    for m in re.finditer(new_pattern, text):
        font_name = m.group(1)
        style_num = m.group(2)  # '1' or '2'
        width = int(m.group(3))
        height = int(m.group(4))
        suffix = 's' + style_num  # 's1' or 's2'

        char_map = lookup_tables.get(suffix, {})

        # Reconstruct flat 95-char data array (blanks for missing chars)
        flat_data = []
        for ascii_code in range(32, 127):
            if ascii_code in char_map:
                flat_data.extend(char_map[ascii_code])
            else:
                flat_data.extend([0] * height)

        fonts[font_name] = {
            "width": width,
            "height": height,
            "data": flat_data,
            "array_name": f"<conditional: {len(char_map)} chars>",
            "char_count": len(char_map),
            "char_total": 95,
        }

    return fonts


# ──────────────────────────────────────────────────────────────────────────
# 2.  Bitmap decoder
# ──────────────────────────────────────────────────────────────────────────

def get_glyph(font: dict, char_code: int) -> list[list[bool]]:
    """
    Return glyph as a 2D bool list [row][col].
    char_code: ASCII code (32-126).
    """
    w = font["width"]
    h = font["height"]
    data = font["data"]
    first = 32
    n_chars = 95  # ASCII 32-126

    idx = (char_code - first) * h
    if idx < 0 or idx + h > len(data):
        return [[False] * w for _ in range(h)]

    glyph = []
    for row in range(h):
        row_val = data[idx + row]
        pixels = []
        for col in range(w):
            bit_pos = 15 - col
            pixels.append(bool((row_val >> bit_pos) & 1))
        glyph.append(pixels)
    return glyph


# ──────────────────────────────────────────────────────────────────────────
# 3.  Tkinter GUI
# ──────────────────────────────────────────────────────────────────────────

PIXEL_SIZE = 20        # each "pixel" is 20×20 px
GRID_GAP = 1           # gap between cells
PAD_X = 30             # canvas padding
PAD_Y = 30

# Colors
BG = "#0D1117"
GRID_COLOR = "#21262D"
ON_COLOR = "#00E676"    # green ON pixels
OFF_COLOR = "#161B22"
ACCENT = "#58A6FF"
TEXT_FG = "#C9D1D9"
LABEL_FG = "#8B949E"


# GUI class is only defined/used when running interactively (not --headless)
def _build_gui_class():
    import tkinter as tk
    from tkinter import ttk

    class FontPreviewer(tk.Tk):
        def __init__(self, fonts: dict):
            super().__init__()
            self.fonts = fonts
            self.font_names = sorted(fonts.keys())

            self.title("Font Glyph Previewer")
            self.configure(bg=BG)
            self.resizable(False, False)

            self._current_font = tk.StringVar(value=self.font_names[0] if self.font_names else "")
            self._current_char = tk.StringVar(value="A")

            self._build_ui()
            self._refresh()

        # ── UI layout ──────────────────────────────────────────────────────

        def _build_ui(self):
            top = tk.Frame(self, bg=BG)
            top.pack(fill=tk.X, padx=16, pady=(16, 8))

            tk.Label(top, text="Font:", bg=BG, fg=LABEL_FG,
                     font=("Consolas", 11)).pack(side=tk.LEFT, padx=(0, 6))
            font_menu = ttk.Combobox(top, textvariable=self._current_font,
                                     values=self.font_names, state="readonly",
                                     width=22, font=("Consolas", 11))
            font_menu.pack(side=tk.LEFT, padx=(0, 20))
            font_menu.bind("<<ComboboxSelected>>", lambda e: self._refresh())

            tk.Label(top, text="Character:", bg=BG, fg=LABEL_FG,
                     font=("Consolas", 11)).pack(side=tk.LEFT, padx=(0, 6))

            char_list = (
                [chr(c) for c in range(0x30, 0x3A)] +
                [chr(c) for c in range(0x41, 0x5B)] +
                [chr(c) for c in range(0x61, 0x7B)] +
                [" ", ".", "!", "?", "-", "+", "/", ":", "=", "@", "#", "$", "%",
                 "&", "*", "(", ")", "[", "]", "{", "}", "<", ">", "|", "~", "^",
                 "_", "`", "'", "\"", ";", ","]
            )
            seen = set()
            uniq = []
            for ch in char_list:
                if ch not in seen:
                    seen.add(ch)
                    uniq.append(ch)

            char_menu = ttk.Combobox(top, textvariable=self._current_char,
                                     values=uniq, state="normal",
                                     width=6, font=("Consolas", 11))
            char_menu.pack(side=tk.LEFT, padx=(0, 12))
            char_menu.bind("<<ComboboxSelected>>", lambda e: self._refresh())
            char_menu.bind("<Return>", lambda e: self._refresh())
            char_menu.bind("<KeyRelease>", lambda e: self.after(200, self._refresh))

            self._ascii_label = tk.Label(top, text="", bg=BG, fg=ACCENT,
                                         font=("Consolas", 11))
            self._ascii_label.pack(side=tk.LEFT, padx=(0, 20))

            self._info_label = tk.Label(top, text="", bg=BG, fg=TEXT_FG,
                                        font=("Consolas", 11))
            self._info_label.pack(side=tk.LEFT)

            canvas_frame = tk.Frame(self, bg=BG, highlightthickness=0)
            canvas_frame.pack(padx=16, pady=(4, 16))

            self._canvas = tk.Canvas(canvas_frame, bg=BG, highlightthickness=0)
            self._canvas.pack()

            legend = tk.Frame(self, bg=BG)
            legend.pack(fill=tk.X, padx=16, pady=(0, 16))

            swatch_on = tk.Canvas(legend, width=16, height=16, bg=ON_COLOR,
                                  highlightthickness=0, relief=tk.FLAT)
            swatch_on.pack(side=tk.LEFT, padx=(0, 4))
            tk.Label(legend, text="Pixel ON", bg=BG, fg=TEXT_FG,
                     font=("Consolas", 10)).pack(side=tk.LEFT, padx=(0, 20))

            swatch_off = tk.Canvas(legend, width=16, height=16, bg=OFF_COLOR,
                                   highlightthickness=1, highlightbackground=GRID_COLOR)
            swatch_off.pack(side=tk.LEFT, padx=(0, 4))
            tk.Label(legend, text="Pixel OFF", bg=BG, fg=TEXT_FG,
                     font=("Consolas", 10)).pack(side=tk.LEFT, padx=(0, 20))

            tk.Label(legend, text="Cell size: {}×{}px".format(PIXEL_SIZE, PIXEL_SIZE),
                     bg=BG, fg=LABEL_FG, font=("Consolas", 10)).pack(side=tk.RIGHT)

        def _refresh(self, *args):
            font_name = self._current_font.get()
            char_str = self._current_char.get()

            if not char_str or not font_name:
                return

            char_code = ord(char_str[0])
            if char_code < 32 or char_code > 126:
                self._ascii_label.config(text="OUT OF RANGE")
                return

            font = self.fonts.get(font_name)
            if font is None:
                return

            glyph = get_glyph(font, char_code)
            w = font["width"]
            h = font["height"]

            self._ascii_label.config(
                text="ASCII {}  (0x{:02X})".format(char_code, char_code))

            cc = font.get("char_count", 0)
            ct = font.get("char_total", 95)
            if cc < ct:
                data_status = "  [{}/{} chars available]".format(cc, ct)
            elif font["data"]:
                data_status = "  [full 95-char set, {} entries]".format(len(font["data"]))
            else:
                data_status = "  [NO DATA - disabled]"

            self._info_label.config(
                text="{}  {}×{}px{}".format(font_name, w, h, data_status))

            cw = w * (PIXEL_SIZE + GRID_GAP) + GRID_GAP + PAD_X * 2
            ch = h * (PIXEL_SIZE + GRID_GAP) + GRID_GAP + PAD_Y * 2

            self._canvas.config(width=cw, height=ch)
            self._canvas.delete("all")

            x0 = PAD_X
            y0 = PAD_Y

            for row in range(h):
                for col in range(w):
                    on = glyph[row][col]
                    fill = ON_COLOR if on else OFF_COLOR
                    outline = GRID_COLOR if not on else ""
                    x = x0 + col * (PIXEL_SIZE + GRID_GAP) + GRID_GAP
                    y = y0 + row * (PIXEL_SIZE + GRID_GAP) + GRID_GAP
                    self._canvas.create_rectangle(
                        x, y,
                        x + PIXEL_SIZE, y + PIXEL_SIZE,
                        fill=fill, outline=outline, width=1 if on else 0)

    return FontPreviewer


# ──────────────────────────────────────────────────────────────────────────
# 4.  Entry point
# ──────────────────────────────────────────────────────────────────────────

def main():
    import sys
    fp = FONTS_CPP
    headless = False
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    flags = [a for a in sys.argv[1:] if a.startswith('--')]

    if '--headless' in flags or '--dump' in flags:
        headless = True

    if args:
        fp = args[0]

    if not os.path.exists(fp):
        print("ERROR: fonts.cpp not found at", fp)
        print("Usage: python font_preview.py [path/to/fonts.cpp] [--headless]")
        sys.exit(1)

    fonts = parse_fonts_cpp(fp)
    if not fonts:
        print("ERROR: no fonts found in", fp)
        sys.exit(1)

    print("Loaded fonts:")
    for name, f in sorted(fonts.items()):
        cc = f.get("char_count", 0)
        ct = f.get("char_total", 95)
        if cc < ct and cc > 0:
            status = "OK  {}/{} chars".format(cc, ct)
        elif cc == ct:
            status = "OK  full set"
        else:
            status = "DISABLED"
        print("  {name:20s}  {w}×{h}px  {status}".format(
            name=name, w=f["width"], h=f["height"], status=status))

    if not headless:
        FontPreviewer = _build_gui_class()
        app = FontPreviewer(fonts)
        app.mainloop()


if __name__ == "__main__":
    main()
