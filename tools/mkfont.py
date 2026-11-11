#!/usr/bin/env python3
"""Generates src/pixoo_font.c, the built-in pixel font of pixoo.library.

The glyphs are drawn below as ASCII art ('#' = pixel). Rows 0-4 are the
glyph above the baseline (capitals and digits use all five), row 5 is for
descenders (g, j, p, q, y, comma). Widths are 1-5 pixels, proportional;
digits are all 3 wide so that clocks do not jump. Characters are ISO-8859-1.

Own design for this project (MIT, like the library).

Usage: python3 tools/mkfont.py   (writes src/pixoo_font.c)
         python3 tools/mkfont.py --preview out.png  (needs Pillow)
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

GLYPHS = {
    ' ': ["..", "..", "..", "..", ".."],
    '!': ["#", "#", "#", ".", "#"],
    '"': ["#.#", "#.#", "...", "...", "..."],
    '#': ["#.#.", "####", "#.#.", "####", "#.#."],
    '$': [".##", "##.", ".#.", ".##", "##."],
    '%': ["#.#", "..#", ".#.", "#..", "#.#"],
    '&': [".#.", "#.#", ".#.", "#.#", ".##"],
    "'": ["#", "#", ".", ".", "."],
    '(': [".#", "#.", "#.", "#.", ".#"],
    ')': ["#.", ".#", ".#", ".#", "#."],
    '*': ["...", "#.#", ".#.", "#.#", "..."],
    '+': ["...", ".#.", "###", ".#.", "..."],
    ',': ["..", "..", "..", "..", ".#", "#."],
    '-': ["...", "...", "###", "...", "..."],
    '.': [".", ".", ".", ".", "#"],
    '/': ["..#", "..#", ".#.", "#..", "#.."],
    '0': ["###", "#.#", "#.#", "#.#", "###"],
    '1': [".#.", "##.", ".#.", ".#.", "###"],
    '2': ["###", "..#", "###", "#..", "###"],
    '3': ["###", "..#", ".##", "..#", "###"],
    '4': ["#.#", "#.#", "###", "..#", "..#"],
    '5': ["###", "#..", "###", "..#", "###"],
    '6': ["###", "#..", "###", "#.#", "###"],
    '7': ["###", "..#", "..#", ".#.", ".#."],
    '8': ["###", "#.#", "###", "#.#", "###"],
    '9': ["###", "#.#", "###", "..#", "###"],
    ':': [".", "#", ".", "#", "."],
    ';': ["..", ".#", "..", ".#", "#."],
    '<': ["..#", ".#.", "#..", ".#.", "..#"],
    '=': ["...", "###", "...", "###", "..."],
    '>': ["#..", ".#.", "..#", ".#.", "#.."],
    '?': ["###", "..#", ".#.", "...", ".#."],
    '@': [".#.", "#.#", "###", "#..", ".##"],
    'A': [".#.", "#.#", "###", "#.#", "#.#"],
    'B': ["##.", "#.#", "##.", "#.#", "##."],
    'C': [".##", "#..", "#..", "#..", ".##"],
    'D': ["##.", "#.#", "#.#", "#.#", "##."],
    'E': ["###", "#..", "##.", "#..", "###"],
    'F': ["###", "#..", "##.", "#..", "#.."],
    'G': [".##", "#..", "#.#", "#.#", ".##"],
    'H': ["#.#", "#.#", "###", "#.#", "#.#"],
    'I': ["###", ".#.", ".#.", ".#.", "###"],
    'J': ["..#", "..#", "..#", "#.#", ".#."],
    'K': ["#.#", "#.#", "##.", "#.#", "#.#"],
    'L': ["#..", "#..", "#..", "#..", "###"],
    'M': ["#...#", "##.##", "#.#.#", "#...#", "#...#"],
    'N': ["#..#", "##.#", "#.##", "#..#", "#..#"],
    'O': [".#.", "#.#", "#.#", "#.#", ".#."],
    'P': ["##.", "#.#", "##.", "#..", "#.."],
    'Q': [".#.", "#.#", "#.#", "##.", ".##"],
    'R': ["##.", "#.#", "##.", "#.#", "#.#"],
    'S': [".##", "#..", ".#.", "..#", "##."],
    'T': ["###", ".#.", ".#.", ".#.", ".#."],
    'U': ["#.#", "#.#", "#.#", "#.#", "###"],
    'V': ["#.#", "#.#", "#.#", "#.#", ".#."],
    'W': ["#...#", "#...#", "#.#.#", "##.##", "#...#"],
    'X': ["#.#", "#.#", ".#.", "#.#", "#.#"],
    'Y': ["#.#", "#.#", ".#.", ".#.", ".#."],
    'Z': ["###", "..#", ".#.", "#..", "###"],
    '[': ["##", "#.", "#.", "#.", "##"],
    '\\': ["#..", "#..", ".#.", "..#", "..#"],
    ']': ["##", ".#", ".#", ".#", "##"],
    '^': [".#.", "#.#", "...", "...", "..."],
    '_': ["...", "...", "...", "...", "###"],
    '`': ["#.", ".#", "..", "..", ".."],
    'a': ["...", "##.", "..#", "#.#", ".##"],
    'b': ["#..", "##.", "#.#", "#.#", "##."],
    'c': ["...", ".##", "#..", "#..", ".##"],
    'd': ["..#", ".##", "#.#", "#.#", ".##"],
    'e': ["...", ".#.", "###", "#..", ".##"],
    'f': [".##", "#..", "##.", "#..", "#.."],
    'g': ["...", ".##", "#.#", ".##", "..#", "##."],
    'h': ["#..", "##.", "#.#", "#.#", "#.#"],
    'i': ["#", ".", "#", "#", "#"],
    'j': [".#", "..", ".#", ".#", ".#", "#."],
    'k': ["#..", "#.#", "##.", "##.", "#.#"],
    'l': ["#.", "#.", "#.", "#.", ".#"],
    'm': [".....", "####.", "#.#.#", "#.#.#", "#.#.#"],
    'n': ["...", "##.", "#.#", "#.#", "#.#"],
    'o': ["...", ".#.", "#.#", "#.#", ".#."],
    'p': ["...", "##.", "#.#", "#.#", "##.", "#.."],
    'q': ["...", ".##", "#.#", "#.#", ".##", "..#"],
    'r': ["..", "#.", "##", "#.", "#."],
    's': ["...", ".##", "##.", "..#", "##."],
    't': [".#.", "###", ".#.", ".#.", "..#"],
    'u': ["...", "#.#", "#.#", "#.#", ".##"],
    'v': ["...", "#.#", "#.#", "#.#", ".#."],
    'w': [".....", "#...#", "#.#.#", "#.#.#", ".#.#."],
    'x': ["...", "#.#", ".#.", ".#.", "#.#"],
    'y': ["...", "#.#", "#.#", ".##", "..#", "##."],
    'z': ["...", "###", "..#", ".#.", "###"],
    '{': [".##", ".#.", "##.", ".#.", ".##"],
    '|': ["#", "#", "#", "#", "#"],
    '}': ["##.", ".#.", ".##", ".#.", "##."],
    '~': ["....", ".#.#", "#.#.", "....", "...."],
    # ISO-8859-1
    '\xb0': [".#.", "#.#", ".#.", "...", "..."],          # degree
    '\xc4': ["#.#", ".#.", "#.#", "###", "#.#"],          # A umlaut
    '\xd6': ["#.#", ".#.", "#.#", "#.#", ".#."],          # O umlaut
    '\xdc': ["#.#", "...", "#.#", "#.#", "###"],          # U umlaut
    '\xdf': [".#.", "#.#", "##.", "#.#", "##.", "#.."],   # sharp s
    '\xe4': ["#.#", "...", ".##", "#.#", ".##"],          # a umlaut
    '\xf6': ["#.#", "...", ".#.", "#.#", ".#."],          # o umlaut
    '\xfc': ["#.#", "...", "#.#", "#.#", ".##"],          # u umlaut
}

ROWS = 6        # rows per glyph (5 + descender)
UNKNOWN = '?'   # shown for characters without a glyph


def rows_of(ch):
    art = GLYPHS[ch]
    width = len(art[0])
    assert 1 <= width <= 5, ch
    assert len(art) in (5, 6), ch
    for r in art:
        assert len(r) == width and set(r) <= set('#.'), (ch, art)
    art = art + ['.' * width] * (ROWS - len(art))
    bits = []
    for r in art:
        v = 0
        for i, c in enumerate(r):
            if c == '#':
                v |= 0x80 >> i
        bits.append(v)
    return width, bits


def generate():
    chars = sorted(GLYPHS, key=ord)
    index = {c: i for i, c in enumerate(chars)}
    lines = []
    lines.append('/**')
    lines.append(' * @file pixoo_font.c')
    lines.append(' * @brief Built-in pixel font of pixoo.library (generated by tools/mkfont.py,')
    lines.append(' *        do not edit).')
    lines.append(' *')
    lines.append(' * %d glyphs, 5 pixels above the baseline plus one row for descenders,' % len(chars))
    lines.append(' * 1 to 5 pixels wide; ISO-8859-1. Characters without a glyph are shown')
    lines.append(" * as '%s'." % UNKNOWN)
    lines.append(' *')
    lines.append(' * Released under the MIT License, see LICENSE.')
    lines.append(' */')
    lines.append('#include "pixoo_core.h"')
    lines.append('')
    lines.append('/** Glyphs: width, then %d rows, bit 7 is the leftmost pixel. */' % ROWS)
    lines.append('const unsigned char px_font_glyphs[%d][%d] = {' % (len(chars), ROWS + 1))
    for c in chars:
        w, bits = rows_of(c)
        name = c if 32 < ord(c) < 127 and c not in '\\*/' else '0x%02X' % ord(c)
        lines.append('    { %d, %s },  /* %s */' % (w, ', '.join('0x%02X' % b for b in bits), name))
    lines.append('};')
    lines.append('')
    lines.append('/** Glyph index of every ISO-8859-1 character. */')
    lines.append('const unsigned char px_font_map[256] = {')
    unk = index[UNKNOWN]
    for row in range(0, 256, 16):
        vals = [index.get(chr(row + i), unk) for i in range(16)]
        lines.append('    ' + ', '.join('%3d' % v for v in vals) + ',')
    lines[-1] = lines[-1].rstrip(',')
    lines.append('};')
    return '\n'.join(lines) + '\n'


def preview(path):
    from PIL import Image
    chars = sorted(GLYPHS, key=ord)
    per_line = 24
    scale = 8
    w = per_line * 6 + 1
    h = ((len(chars) + per_line - 1) // per_line) * 8 + 1
    im = Image.new('RGB', (w, h), (0, 0, 64))
    for n, c in enumerate(chars):
        width, bits = rows_of(c)
        ox, oy = 1 + (n % per_line) * 6, 1 + (n // per_line) * 8
        for y, b in enumerate(bits):
            for x in range(width):
                if b & (0x80 >> x):
                    im.putpixel((ox + x, oy + y), (255, 255, 255))
    im.resize((w * scale, h * scale), Image.NEAREST).save(path)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--preview':
        preview(sys.argv[2])
    else:
        out = os.path.join(ROOT, 'src', 'pixoo_font.c')
        with open(out, 'w', encoding='latin-1') as f:
            f.write(generate())
        print('wrote', out)
