#!/usr/bin/env python3
"""Generates src/library/pixoo_font.c, the built-in pixel font of pixoo.library.

The glyphs are drawn below as ASCII art ('#' = pixel); the output has the
layout of an Amiga TextFont (see generate()). Rows 0-4 are the
glyph above the baseline (capitals and digits use all five), row 5 is for
descenders (g, j, p, q, y, comma). Widths are 1-5 pixels, proportional;
digits are all 3 wide so that clocks do not jump. Characters are ISO-8859-1.

Own design for this project (MIT, like the library).

Usage: python3 tools/mkfont.py   (writes src/library/pixoo_font.c)
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


LOCHAR, HICHAR = 32, 255
BASELINE = 4    # row of the baseline (rows 0-4 are above it, inclusive)
YSIZE = 7       # line height: 6 glyph rows and one empty row


def generate():
    """The font in the layout of an Amiga struct TextFont: one bitmap strip
    (tf_CharData, tf_Modulo bytes per row), an offset/width pair per
    character (tf_CharLoc) and the advance per character (tf_CharSpace),
    for tf_LoChar..tf_HiChar plus the glyph for missing characters."""
    chars = sorted(GLYPHS, key=ord)
    # strip: every glyph once, side by side
    offset = {}
    pos = 0
    for c in chars:
        offset[c] = pos
        pos += rows_of(c)[0]
    modulo = ((pos + 15) // 16) * 2
    strip = [[0] * modulo for _ in range(YSIZE)]
    for c in chars:
        width, bits = rows_of(c)
        for y, b in enumerate(bits):
            for x in range(width):
                if b & (0x80 >> x):
                    bit = offset[c] + x
                    strip[y][bit >> 3] |= 0x80 >> (bit & 7)
    codes = [chr(n) for n in range(LOCHAR, HICHAR + 1)] + [UNKNOWN]
    loc, space = [], []
    for c in codes:
        g = c if c in GLYPHS else UNKNOWN
        width = rows_of(g)[0]
        loc.append((offset[g] << 16) | width)
        space.append(width + 1)

    def bytes_of(values, size):
        out = []
        for v in values:
            out += [(v >> (8 * i)) & 0xFF for i in reversed(range(size))]
        return out

    def c_array(name, data, comment):
        lines = ['/** %s */' % comment, 'static const unsigned char %s[%d] = {' % (name, len(data))]
        for i in range(0, len(data), 16):
            lines.append('    ' + ', '.join('0x%02X' % v for v in data[i:i + 16]) + ',')
        lines[-1] = lines[-1].rstrip(',')
        lines.append('};')
        return lines

    lines = [
        '/**',
        ' * @file pixoo_font.c',
        ' * @brief Built-in pixel font of pixoo.library (generated by tools/mkfont.py,',
        ' *        do not edit).',
        ' *',
        ' * %d glyphs, 5 pixels above the baseline plus one row for descenders,' % len(chars),
        ' * 1 to 5 pixels wide, ISO-8859-1. Stored like an Amiga TextFont so that',
        " * the same renderer draws it and disk fonts. Characters without a glyph",
        " * are shown as '%s'." % UNKNOWN,
        ' *',
        ' * Released under the MIT License, see LICENSE.',
        ' */',
        '#include "pixoo_core.h"',
        '',
    ]
    strip_bytes = [v for row in strip for v in row]
    lines += c_array('chardata', strip_bytes, 'Bitmap strip, %d rows of %d bytes (tf_CharData).' % (YSIZE, modulo))
    lines.append('')
    lines += c_array('charloc', bytes_of(loc, 4), 'Bit offset and width of each glyph, big-endian (tf_CharLoc).')
    lines.append('')
    lines += c_array('charspace', bytes_of(space, 2), 'Advance of each glyph, big-endian (tf_CharSpace).')
    lines += [
        '',
        'const PxFont px_builtin_font = {',
        '    { chardata, 0, 0, 0, 0, 0, 0, 0 },  /* planes */',
        '    1,                  /* nplanes */',
        '    %d,                 /* modulo */' % modulo,
        '    %d, %d, %d,          /* ysize, baseline, xsize */' % (YSIZE, BASELINE, 4),
        '    %d, %d,            /* lochar, hichar */' % (LOCHAR, HICHAR),
        '    1,                  /* proportional */',
        '    charloc, charspace, 0',
        '};',
    ]
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
        out = os.path.join(ROOT, 'src', 'library', 'pixoo_font.c')
        with open(out, 'w', encoding='latin-1') as f:
            f.write(generate())
        print('wrote', out)
