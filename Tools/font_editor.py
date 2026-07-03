#!/usr/bin/env python3
"""
Font Pixel Editor — 交互式像素编辑器
=====================================
Click pixels to toggle → 实时写入 fonts.cpp

用法:
    python Tools/font_editor.py

特性:
    - 支持 Font_Style1_7x10 和 Font_Style2_9x18
    - 点击像素切换亮/灭
    - 每次点击自动保存到 fonts.cpp
    - 实时预览 + Hex 数据显示
"""

import re
import sys
import tkinter as tk
from tkinter import ttk
from pathlib import Path

# ─── 文件路径 ───────────────────────────────────────────────────────
HERE = Path(__file__).resolve().parent
FONTS_CPP = HERE.parent / "LoveFinderLib" / "ST7735" / "fonts.cpp"

FONT_SPECS = {
    's1': {'rows': 10, 'cols': 7,  'cols_bit': 7,  'label': 'Style1 (7×10)',  'vpl': 5, 'bit_shift': 8},
    's2': {'rows': 18, 'cols': 7,  'cols_bit': 7,  'label': 'Style2 (9×18)',  'vpl': 6, 'bit_shift': 8},
    's3': {'rows': 16, 'cols': 16, 'cols_bit': 16, 'label': 'Style3 (16×16)', 'vpl': 4, 'bit_shift': 0},
}

# ─── 主题配色 ───────────────────────────────────────────────────────
C_BG      = '#1a1a2e'
C_GRID    = '#16213e'
C_ON      = '#00d4ff'
C_OFF     = '#2a2a4a'
C_OFF_HVR = '#3a3a5a'
C_ON_HVR  = '#33ddff'
C_TEXT    = '#e0e0e0'
C_BORDER  = '#0f3460'
C_SAVED   = '#00cc66'
C_MODIFIED = '#ff6633'

# ─── 正则 ───────────────────────────────────────────────────────────
RE_HEX   = re.compile(r'0x([0-9A-Fa-f]{4})')
RE_DECL  = re.compile(r'static constexpr uint16_t (s[123]_\w+)\[(\d+)\] = \{')
RE_AC    = re.compile(r'// ASCII (\d+) \(([^)]*)\)')
RE_UNI   = re.compile(r'// ([^\s]+) \(U\+([0-9A-Fa-f]+)\)')

# ═══════════════════════════════════════════════════════════════════
#  GlyphData — 单个字体的像素数据模型
# ═══════════════════════════════════════════════════════════════════
class GlyphData:
    def __init__(self, name, font_id):
        self.name = name
        self.font_id = font_id
        s = FONT_SPECS[font_id]
        self.rows = s['rows']
        self.cols = s['cols']
        self.ascii = None
        self.unicode = None
        self.char = ''
        self.pixels = [[False] * self.cols for _ in range(self.rows)]

    @property
    def label(self):
        label = self.name
        if self.char:
            label += f'  "{self.char}"'
        if self.ascii is not None:
            label += f'  (ASCII {self.ascii})'
        if self.unicode is not None:
            label += f'  (U+{self.unicode:04X})'
        return label

    def load_hex(self, hex_vals):
        """从 uint16_t 列表加载像素状态。"""
        spec = FONT_SPECS[self.font_id]
        cols_bit = spec['cols_bit']
        bit_shift = spec['bit_shift']
        for r in range(min(len(hex_vals), self.rows)):
            v = int(hex_vals[r], 16) << bit_shift
            for c in range(self.cols):
                self.pixels[r][c] = bool(v & (1 << (cols_bit - 1 - c)))

    def dump_hex(self):
        """返回 uint16_t 值列表。"""
        spec = FONT_SPECS[self.font_id]
        cols_bit = spec['cols_bit']
        bit_shift = spec['bit_shift']
        vals = []
        for r in range(self.rows):
            v = 0
            for c in range(self.cols):
                if self.pixels[r][c]:
                    v |= 1 << (cols_bit - 1 - c)
            vals.append(v >> bit_shift)
        return vals

    def format_hex_block(self):
        """格式化为 C++ 数组内容（缩进、逗号）。"""
        vpl = FONT_SPECS[self.font_id]['vpl']
        vals = self.dump_hex()
        lines = []
        for i in range(0, len(vals), vpl):
            chunk = vals[i:i + vpl]
            lines.append('    ' + ', '.join(f'0x{v:04X}' for v in chunk) + ',')
        return '\n'.join(lines)

    def render_ascii(self):
        """返回纯文本点阵预览（使用██使比例更真实）。"""
        rows = []
        for r in range(self.rows):
            row = ''.join('██' if p else '  ' for p in self.pixels[r])
            rows.append(row)
        return '\n'.join(rows)


# ═══════════════════════════════════════════════════════════════════
#  FontFile — fonts.cpp 读写
# ═══════════════════════════════════════════════════════════════════
class FontFile:
    def __init__(self, path):
        self.path = Path(path)
        self.text = ''
        self.glyphs = {}      # name → GlyphData
        self.blocks = {}      # name → (start, end) 字符位置
        self._load()

    def _load(self):
        """解析 fonts.cpp。"""
        self.text = self.path.read_text(encoding='utf-8')
        self.glyphs.clear()
        self.blocks.clear()

        for m in RE_DECL.finditer(self.text):
            name = m.group(1)
            rows_decl = int(m.group(2))
            
            # Detect font ID from prefix
            if name.startswith('s1_'):
                font_id = 's1'
            elif name.startswith('s2_'):
                font_id = 's2'
            elif name.startswith('s3_'):
                font_id = 's3'
            else:
                continue
                
            spec = FONT_SPECS.get(font_id)
            if not spec or rows_decl != spec['rows']:
                continue

            # 提取 hex 值（找到声明行后的所有 hex 直到 };）
            rest = self.text[m.end():]
            end_m = re.search(r'\};', rest)
            if not end_m:
                continue
            block_text = rest[:end_m.end()]
            hex_vals = RE_HEX.findall(block_text)

            if len(hex_vals) != spec['rows']:
                continue

            gd = GlyphData(name, font_id)
            gd.load_hex(['0x' + h for h in hex_vals])

            # 向前找注释——ASCII 兼容 s1/s2，Unicode 兼容 s3
            pre = self.text[:m.start()]
            
            # Try Unicode comment first: "你 (U+4F60)"
            uni_matches = list(RE_UNI.finditer(pre))
            if uni_matches:
                last = uni_matches[-1]
                gap = pre[last.end():].count('\n')
                if gap <= 2:
                    gd.char = last.group(1).strip()
                    gd.unicode = int(last.group(2), 16)

            # Try ASCII comment (for s1/s2)
            acs = list(RE_AC.finditer(pre))
            if acs:
                last = acs[-1]
                gap = pre[last.end():].count('\n')
                if gap <= 2:
                    gd.ascii = int(last.group(1))
                    if not gd.char:
                        gd.char = last.group(2).strip('()')

            block_end = m.end() + end_m.end()
            self.glyphs[name] = gd
            self.blocks[name] = (m.start(), block_end)

    def save_glyph(self, name):
        """将 name 对应的 glyph 数据写回文件。"""
        if name not in self.glyphs or name not in self.blocks:
            return False

        gd = self.glyphs[name]
        new_inner = gd.format_hex_block()
        old_start, old_end = self.blocks[name]

        # 提取声明行（static constexpr ... {）
        decl_end = self.text.index('{', old_start) + 1
        # 找到声明行末尾的 '{'

        # 更简单：找到声明行的 { 然后取到 old_end
        brace_pos = self.text.index('{', old_start, old_end)
        before_brace = self.text[old_start:brace_pos + 1]

        new_block = before_brace + '\n' + new_inner + '\n};\n'

        # 替换
        self.text = self.text[:old_start] + new_block + self.text[old_end:]

        # 更新 blocks 偏移
        shift = len(new_block) - (old_end - old_start)
        for n in self.blocks:
            s, e = self.blocks[n]
            if n == name:
                self.blocks[n] = (old_start, old_start + len(new_block))
            elif s >= old_end:
                self.blocks[n] = (s + shift, e + shift)

        self.path.write_text(self.text, encoding='utf-8')
        return True

    def get_sorted_names(self, font_id):
        """按 ascii/unicode 码排序的 glyph 名称列表。"""
        names = [n for n in self.glyphs if n.startswith(font_id + '_')]
        def sort_key(n):
            g = self.glyphs[n]
            if g.unicode is not None:
                return (0, g.unicode)
            if g.ascii is not None:
                return (1, g.ascii)
            return (2, n)
        names.sort(key=sort_key)
        return names


# ═══════════════════════════════════════════════════════════════════
#  像素网格 Canvas 控件
# ═══════════════════════════════════════════════════════════════════
class PixelGrid(tk.Canvas):
    CELL = 34         # 单元格像素大小
    GAP = 2           # 间隙
    PAD = 8           # 边距

    def __init__(self, parent, on_toggle, **kw):
        super().__init__(parent, bg=C_BG, highlightthickness=0, **kw)
        self.on_toggle = on_toggle
        self.gd = None
        self._hover = None  # (r, c) 当前悬停
        self._rects = {}    # (r, c) → rect_id
        self._size = (0, 0)

        self.bind('<Button-1>', self._on_click)
        self.bind('<Motion>', self._on_motion)
        self.bind('<Leave>', self._on_leave)

    def set_glyph(self, gd):
        self.gd = gd
        self._hover = None
        self._rects.clear()
        self._draw()

    def _cell_coords(self, r, c):
        x0 = self.PAD + c * (self.CELL + self.GAP)
        y0 = self.PAD + r * (self.CELL + self.GAP)
        x1 = x0 + self.CELL
        y1 = y0 + self.CELL
        return x0, y0, x1, y1

    def _draw(self):
        self.delete('all')
        self._rects.clear()
        if not self.gd:
            return

        for r in range(self.gd.rows):
            for c in range(self.gd.cols):
                x0, y0, x1, y1 = self._cell_coords(r, c)
                on = self.gd.pixels[r][c]
                fill = C_ON if on else C_OFF
                rid = self.create_rectangle(x0, y0, x1, y1,
                                            fill=fill, outline=C_GRID, width=1)
                self._rects[(r, c)] = rid

        w = self.PAD * 2 + self.gd.cols * (self.CELL + self.GAP) - self.GAP
        h = self.PAD * 2 + self.gd.rows * (self.CELL + self.GAP) - self.GAP
        self.configure(width=w, height=h, scrollregion=(0, 0, w, h))
        self._size = (w, h)

    def _get_cell(self, event):
        if not self.gd:
            return None
        c = (event.x - self.PAD) // (self.CELL + self.GAP)
        r = (event.y - self.PAD) // (self.CELL + self.GAP)
        if 0 <= r < self.gd.rows and 0 <= c < self.gd.cols:
            return (r, c)
        return None

    def _on_click(self, event):
        cell = self._get_cell(event)
        if cell and self.on_toggle:
            self.on_toggle(cell[0], cell[1])

    def _on_motion(self, event):
        cell = self._get_cell(event)
        if cell != self._hover:
            # 恢复旧悬停
            if self._hover and self._hover in self._rects:
                r, c = self._hover
                on = self.gd.pixels[r][c]
                self.itemconfig(self._rects[(r, c)], fill=C_ON if on else C_OFF)
            # 设置新悬停
            self._hover = cell
            if cell and cell in self._rects:
                r, c = cell
                on = self.gd.pixels[r][c]
                hov = C_ON_HVR if on else C_OFF_HVR
                self.itemconfig(self._rects[(r, c)], fill=hov)

    def _on_leave(self, event):
        if self._hover and self._hover in self._rects:
            r, c = self._hover
            on = self.gd.pixels[r][c]
            self.itemconfig(self._rects[(r, c)], fill=C_ON if on else C_OFF)
        self._hover = None

    def refresh_cell(self, r, c):
        """重画单个单元格（切换后调用）。"""
        if (r, c) in self._rects:
            on = self.gd.pixels[r][c]
            self.itemconfig(self._rects[(r, c)], fill=C_ON if on else C_OFF)

    def refresh_all(self):
        """全部重绘。"""
        self._draw()


# ═══════════════════════════════════════════════════════════════════
#  主应用
# ═══════════════════════════════════════════════════════════════════
class FontEditor(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title('Font Pixel Editor — LoveFinder776')
        self.configure(bg=C_BG)
        self.minsize(600, 480)

        # ─── 加载字体数据 ────────────────────────────────────────
        if not FONTS_CPP.exists():
            tk.messagebox.showerror('错误', f'找不到文件:\n{FONTS_CPP}\n\n请确认脚本在 Tools/ 目录下运行。')
            sys.exit(1)
        self.fnt = FontFile(FONTS_CPP)

        # ─── 状态 ────────────────────────────────────────────────
        self.current_font = 's2'
        self.current_name = ''
        self.modified = False

        # ─── 构建 UI ─────────────────────────────────────────────
        self._build_ui()

        # ─── 初始选择 ────────────────────────────────────────────
        self.font_combo.current(2)  # s3
        self._on_font_change()

    # ─── UI 构建 ────────────────────────────────────────────────────

    def _build_ui(self):
        style = ttk.Style(self)
        style.theme_use('vista' if 'vista' in style.theme_names() else 'clam')

        # 主容器
        main = ttk.Frame(self)
        main.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        # ── 工具栏 ──
        toolbar = ttk.Frame(main)
        toolbar.pack(fill=tk.X, pady=(0, 4))

        ttk.Label(toolbar, text='字体:').pack(side=tk.LEFT, padx=(0, 2))
        self.font_combo = ttk.Combobox(toolbar,
                                       values=[FONT_SPECS[f]['label'] for f in ['s1', 's2', 's3']],
                                       state='readonly', width=18)
        self.font_combo.pack(side=tk.LEFT, padx=(0, 8))
        self.font_combo.bind('<<ComboboxSelected>>', lambda e: self._on_font_change())

        ttk.Label(toolbar, text='字形:').pack(side=tk.LEFT, padx=(0, 2))
        self.glyph_combo = ttk.Combobox(toolbar, state='readonly', width=28)
        self.glyph_combo.pack(side=tk.LEFT, padx=(0, 8))
        self.glyph_combo.bind('<<ComboboxSelected>>', lambda e: self._on_glyph_select())

        self.info_var = tk.StringVar()
        ttk.Label(toolbar, textvariable=self.info_var,
                  foreground=C_TEXT, background=C_BG).pack(side=tk.LEFT, padx=(4, 0))

        # ── 主编辑区（网格 + 预览） ──
        paned = ttk.PanedWindow(main, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True)

        # 左：像素网格（可滚动）
        grid_frame = ttk.LabelFrame(paned, text='点阵编辑 (点击切换)', padding=2)
        self.grid_canvas_frame = ttk.Frame(grid_frame)
        self.grid_canvas_frame.pack(fill=tk.BOTH, expand=True)

        self.grid_canvas = PixelGrid(self.grid_canvas_frame, self._on_toggle)
        self.grid_scroll_y = ttk.Scrollbar(self.grid_canvas_frame, orient=tk.VERTICAL,
                                           command=self.grid_canvas.yview)
        self.grid_scroll_x = ttk.Scrollbar(self.grid_canvas_frame, orient=tk.HORIZONTAL,
                                           command=self.grid_canvas.xview)
        self.grid_canvas.configure(yscrollcommand=self.grid_scroll_y.set,
                                   xscrollcommand=self.grid_scroll_x.set)
        self.grid_scroll_y.pack(side=tk.RIGHT, fill=tk.Y)
        self.grid_scroll_x.pack(side=tk.BOTTOM, fill=tk.X)
        self.grid_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        paned.add(grid_frame, weight=3)

        # 右：预览
        right_frame = ttk.Frame(paned)
        paned.add(right_frame, weight=2)

        # 文本预览
        preview_frame = ttk.LabelFrame(right_frame, text='文本预览', padding=2)
        preview_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 4))

        self.preview_text = tk.Text(preview_frame, font=('Consolas', 10),
                                     bg=C_BG, fg=C_ON, relief=tk.FLAT,
                                     cursor='arrow', width=20, height=10)
        self.preview_text.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)
        self.preview_text.configure(state=tk.DISABLED)

        # Hex 数据
        hex_frame = ttk.LabelFrame(right_frame, text='Hex 数据', padding=2)
        hex_frame.pack(fill=tk.BOTH, expand=True)

        self.hex_text = tk.Text(hex_frame, font=('Consolas', 9),
                                 bg=C_BG, fg=C_TEXT, relief=tk.FLAT,
                                 cursor='arrow', height=6, wrap=tk.NONE)
        self.hex_text.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)
        self.hex_text.configure(state=tk.DISABLED)

        # ── 状态栏 ──
        self.status_var = tk.StringVar(value='就绪')
        status_bar = ttk.Label(main, textvariable=self.status_var, relief=tk.SUNKEN,
                               anchor=tk.W, padding=(4, 1))
        status_bar.pack(fill=tk.X, pady=(4, 0))

        # ── 键盘快捷键 ──
        self.bind('<Up>', lambda e: self._adj_glyph(-1))
        self.bind('<Down>', lambda e: self._adj_glyph(1))
        self.bind('<Left>', lambda e: self._adj_font(-1))
        self.bind('<Right>', lambda e: self._adj_font(1))

    # ─── 事件处理 ───────────────────────────────────────────────────

    def _on_font_change(self):
        idx = self.font_combo.current()
        font_ids = ['s1', 's2', 's3']
        self.current_font = font_ids[idx]

        names = self.fnt.get_sorted_names(self.current_font)
        display = []
        for n in names:
            g = self.fnt.glyphs[n]
            display.append(g.label)
        self.glyph_combo['values'] = display

        if display:
            self.glyph_combo.current(0)
            self._on_glyph_select()
        else:
            self.current_name = ''
            self._update_info()

        self.status_var.set(f'{FONT_SPECS[self.current_font]["label"]} — {len(names)} 个字型')

    def _on_glyph_select(self):
        idx = self.glyph_combo.current()
        names = self.fnt.get_sorted_names(self.current_font)
        if idx < 0 or idx >= len(names):
            return
        self.current_name = names[idx]
        self.modified = False
        self._update_info()
        self._update_grid()
        self._update_preview()

    def _adj_glyph(self, delta):
        names = self.fnt.get_sorted_names(self.current_font)
        idx = self.glyph_combo.current()
        new_idx = max(0, min(len(names) - 1, idx + delta))
        if new_idx != idx:
            self.glyph_combo.current(new_idx)
            self._on_glyph_select()

    def _adj_font(self, delta):
        idx = self.font_combo.current()
        new_idx = max(0, min(1, idx + delta))
        if new_idx != idx:
            self.font_combo.current(new_idx)
            self._on_font_change()

    def _on_toggle(self, r, c):
        gd = self.fnt.glyphs[self.current_name]
        gd.pixels[r][c] = not gd.pixels[r][c]
        self.modified = True

        # 实时保存
        if self.fnt.save_glyph(self.current_name):
            self.status_var.set(f'✓ 已保存 [{self.current_name}] row={r} col={c}')
        else:
            self.status_var.set(f'✗ 保存失败')

        # 更新界面
        self.grid_canvas.refresh_cell(r, c)
        self._update_preview()

    # ─── 界面更新 ───────────────────────────────────────────────────

    def _update_info(self):
        if not self.current_name:
            self.info_var.set('')
            return
        gd = self.fnt.glyphs[self.current_name]
        self.info_var.set(gd.label)

    def _update_grid(self):
        gd = self.fnt.glyphs.get(self.current_name)
        self.grid_canvas.set_glyph(gd)

    def _update_preview(self):
        gd = self.fnt.glyphs.get(self.current_name)
        if not gd:
            return

        # 文本预览
        self.preview_text.configure(state=tk.NORMAL)
        self.preview_text.delete('1.0', tk.END)
        # 渲染字符
        art = gd.render_ascii()
        # 加一点标注
        label = f'--- {self.current_name} ---'
        if gd.char:
            label += f'  "{gd.char}"'
        self.preview_text.insert(tk.END, label + '\n')
        self.preview_text.insert(tk.END, art + '\n')
        self.preview_text.configure(state=tk.DISABLED)

        # Hex 数据
        self.hex_text.configure(state=tk.NORMAL)
        self.hex_text.delete('1.0', tk.END)
        self.hex_text.insert(tk.END, gd.format_hex_block())
        self.hex_text.configure(state=tk.DISABLED)


# ═══════════════════════════════════════════════════════════════════
#  启动
# ═══════════════════════════════════════════════════════════════════
if __name__ == '__main__':
    app = FontEditor()
    app.mainloop()
